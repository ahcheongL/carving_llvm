#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include "utils/data_utils.hpp"
#include "utils/ptr_map.hpp"

#define SHM_ID_ENV "CARVING_SHM_ID"
#define NUM_SHM_ENTRY 1024

#define MAX_NUM_FILE 8
#define MINSIZE 3
#define MAXSIZE 24

#define HASH_MAX_NUM_FILE 8192

static char *outdir_name = NULL;

static int carved_index = 0;

// Function pointer names
static map<void *, char *> func_ptrs;

// inputs, work as similar as function call stack
static vector<FUNC_CONTEXT> inputs;
static vector<IVAR *> *carved_objs = NULL;
static vector<POINTER> *carved_ptrs = NULL;

typedef struct shm_entry_ {
  char *ptr;
  int size;
  char is_malloc;
} shm_entry;

char *ptr_alloc_shm_map = NULL;

#define LOCK_SHM_MAP() ((char *)ptr_alloc_shm_map)[4] = 1
#define UNLOCK_SHM_MAP() ((char *)ptr_alloc_shm_map)[4] = 0

// memory info
ptr_map alloced_ptrs;
// map<void *, struct typeinfo> alloced_ptrs;

int __carv_cur_class_index = -1;
int __carv_cur_class_size = -1;

bool __carv_opened = false;
bool __carv_ready = false;
char __carv_depth = 0;

static map<char *, classinfo> class_info;

static map<const char *, int *> func_result_hash;

extern "C" {

static void dump_result(const char *func_name, char remove_dup);

void __insert_obj_info(char *name, char *type_name) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<char *> *inputv = new VAR<char *>(type_name, name, INPUT_TYPE::OBJ_INFO);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void __insert_ptr_idx(int idx) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<int> *inputv = new VAR<int>(idx, 0, INPUT_TYPE::PTR_IDX);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

static vector<int> carved_ptr_index_stack;

void __insert_ptr_end() {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  int ptr_idx = *carved_ptr_index_stack.back();
  carved_ptr_index_stack.pop_back();
  VAR<int> *inputv = new VAR<int>(ptr_idx, 0, INPUT_TYPE::PTR_END);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void __insert_struct_begin() {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<int> *inputv = new VAR<int>(0, 0, INPUT_TYPE::STRUCT_BEGIN);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void __insert_struct_end() {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<int> *inputv = new VAR<int>(0, 0, INPUT_TYPE::STRUCT_END);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_char(char input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<char> *inputv = new VAR<char>(input, 0, INPUT_TYPE::CHAR);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_short(short input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<short> *inputv = new VAR<short>(input, 0, INPUT_TYPE::SHORT);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_int(int input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<int> *inputv = new VAR<int>(input, 0, INPUT_TYPE::INT);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_longtype(long input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<long> *inputv = new VAR<long>(input, 0, INPUT_TYPE::LONG);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_longlong(long long input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<long long> *inputv = new VAR<long long>(input, 0, INPUT_TYPE::LONGLONG);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_float(float input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<float> *inputv = new VAR<float>(input, 0, INPUT_TYPE::FLOAT);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

void Carv_double(double input) {
  if (!__carv_opened) {
    return;
  }
  LOCK_SHM_MAP();
  VAR<double> *inputv = new VAR<double>(input, 0, INPUT_TYPE::DOUBLE);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
}

int Carv_pointer(void *ptr, char *type_name, int default_idx,
                 int default_size) {
  if (!__carv_opened) {
    return 0;
  }

  LOCK_SHM_MAP();

  if (ptr == NULL) {
    VAR<void *> *inputv = new VAR<void *>(NULL, type_name, INPUT_TYPE::NULLPTR);
    carved_objs->push_back((IVAR *)inputv);
    UNLOCK_SHM_MAP();
    return 0;
  }

  // Find already carved ptr
  int index = 0;
  int num_carved_ptrs = carved_ptrs->size();

  int end_index = -1;
  int end_offset = -1;
  while (index < num_carved_ptrs) {
    POINTER *carved_ptr = carved_ptrs->get(index);
    char *carved_addr = (char *)carved_ptr->addr;
    int carved_ptr_size = carved_ptr->alloc_size;
    char *carved_addr_end = carved_addr + carved_ptr_size;
    if ((carved_addr <= ptr) && (ptr < carved_addr_end)) {
      int offset = ((char *)ptr) - carved_addr;
      VAR<int> *inputv = new VAR<int>(index, 0, offset, INPUT_TYPE::PTR);
      carved_objs->push_back((IVAR *)inputv);
      // Won't carve again.
      UNLOCK_SHM_MAP();
      return 0;
    } else if (ptr == carved_addr_end) {
      end_index = index;
      end_offset = carved_ptr_size;
    }
    index++;
  }

  if (end_index != -1) {
    VAR<int> *inputv = new VAR<int>(end_index, 0, end_offset, INPUT_TYPE::PTR);
    carved_objs->push_back((IVAR *)inputv);
    UNLOCK_SHM_MAP();
    return 0;
  }

  ptr_map::rbtree_node *ptr_node = alloced_ptrs.find(ptr);
  if (ptr_node == NULL) {
    // We could not found the memory info.

    auto search = func_ptrs.find(ptr);
    if (search != NULL) {
      VAR<char *> *inputv = new VAR<char *>(*search, 0, INPUT_TYPE::FUNCPTR);
      carved_objs->push_back((IVAR *)inputv);
      UNLOCK_SHM_MAP();
      return 0;
    }

    VAR<void *> *inputv =
        new VAR<void *>(ptr, type_name, INPUT_TYPE::UNKNOWN_PTR);

    carved_objs->push_back((IVAR *)inputv);
    UNLOCK_SHM_MAP();
    return 0;
  }

  char *alloc_ptr = (char *)ptr_node->key_;
  int ptr_alloc_size = ptr_node->alloc_size_;

  int new_carved_ptr_index = carved_ptrs->size();

  __carv_cur_class_index = default_idx;
  __carv_cur_class_size = default_size;

  char *name_ptr = ptr_node->type_name_;
  if (name_ptr != NULL) {
    auto search = class_info.find(name_ptr);
    if ((search != NULL) && ((ptr_alloc_size % search->size) == 0)) {
      __carv_cur_class_index = search->class_index;
      __carv_cur_class_size = search->size;
      type_name = name_ptr;
    }
  }

  carved_ptrs->push_back(POINTER(ptr, type_name, ptr_alloc_size, default_size));

  VAR<int> *inputv = new VAR<int>(new_carved_ptr_index, 0, 0, INPUT_TYPE::PTR);
  carved_objs->push_back((IVAR *)inputv);

  VAR<int> *inputv2 =
      new VAR<int>(new_carved_ptr_index, 0, INPUT_TYPE::PTR_BEGIN);
  carved_objs->push_back((IVAR *)inputv2);

  carved_ptr_index_stack.push_back(new_carved_ptr_index);

  UNLOCK_SHM_MAP();
  return ptr_alloc_size;
}

void __Carv_func_ptr_name(void *ptr) {
  if (!__carv_opened) {
    return;
  }

  LOCK_SHM_MAP();

  auto search = func_ptrs.find(ptr);
  if ((ptr == NULL) || (search == NULL)) {
    VAR<void *> *inputv = new VAR<void *>(NULL, NULL, INPUT_TYPE::NULLPTR);
    carved_objs->push_back((IVAR *)inputv);
    UNLOCK_SHM_MAP();
    return;
  }

  VAR<char *> *inputv = new VAR<char *>(*search, NULL, INPUT_TYPE::FUNCPTR);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
  return;
}

void __record_func_ptr(void *ptr, char *name) {
  LOCK_SHM_MAP();
  func_ptrs.insert(ptr, name);
  UNLOCK_SHM_MAP();
}

void __keep_class_info(char *class_name, int size, int index) {
  LOCK_SHM_MAP();
  classinfo tmp(index, size);
  class_info.insert(class_name, tmp);
  UNLOCK_SHM_MAP();
}

int __get_class_idx() { return __carv_cur_class_index; }

int __get_class_size() { return __carv_cur_class_size; }

// Save ptr with size and type_name into memory `allocted_ptrs`.
void __mem_allocated_probe(void *ptr, int alloc_size, char *type_name) {
  if (!__carv_ready) {
    return;
  }

  LOCK_SHM_MAP();
  alloced_ptrs.insert(ptr, type_name, alloc_size);
  UNLOCK_SHM_MAP();
  return;
}

void __remove_mem_allocated_probe(void *ptr) {
  if (!__carv_ready) {
    return;
  }
  LOCK_SHM_MAP();
  alloced_ptrs.remove(ptr);
  UNLOCK_SHM_MAP();
}

// Fetch memory allocation and deallocation info from Pin tool
void __fetch_mem_alloc() {
  int cur_num_entry = ((int *)ptr_alloc_shm_map)[0];
  int idx = 0;
  LOCK_SHM_MAP();

  for (idx = 0; idx < cur_num_entry; idx++) {
    shm_entry *entry = &((shm_entry *)ptr_alloc_shm_map)[idx + 1];
    if (entry->is_malloc) {
      alloced_ptrs.insert(entry->ptr, 0, entry->size);
    } else {
      alloced_ptrs.remove(entry->ptr);
    }
  }
  ((int *)ptr_alloc_shm_map)[0] = 0;

  UNLOCK_SHM_MAP();
}

static map<char *, char *> file_save_hash_map;
static map<char *, unsigned int> file_save_idx_map;

void __carv_file(char *file_name) {
  if (!__carv_opened) {
    return;
  }

  LOCK_SHM_MAP();

  unsigned int file_idx = 0;
  FILE *target_file = fopen(file_name, "rb");
  if (target_file == NULL) {
    UNLOCK_SHM_MAP();
    return;
  }

  if (file_save_hash_map.find(file_name) == NULL) {
    char *hash_vec = (char *)malloc(sizeof(char) * 256);
    memset(hash_vec, 0, sizeof(char) * 256);
    file_save_hash_map.insert(file_name, hash_vec);
  }

  unsigned int *file_idx_ptr = file_save_idx_map.find(file_name);
  if (file_idx_ptr == NULL) {
    file_save_idx_map.insert(file_name, 0);
  } else {
    (*file_idx_ptr)++;
    file_idx = *file_idx_ptr;
  }

  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/carved_file_%s_%d", outdir_name, file_name,
           file_idx);

  FILE *outfile = fopen(outfile_name, "wb");
  if (outfile == NULL) {
    fclose(target_file);
    UNLOCK_SHM_MAP();
    return;
  }

  char buf[4096];
  int read_size;
  while ((read_size = fread(buf, 1, 4096, target_file)) > 0) {
    fwrite(buf, 1, read_size, outfile);
  }

  fclose(target_file);
  fclose(outfile);

  VAR<int> *inputv = new VAR<int>(file_idx, file_name, INPUT_TYPE::INPUTFILE);
  carved_objs->push_back((IVAR *)inputv);
  UNLOCK_SHM_MAP();
  return;
}

void *shmat(int, const void *, int);

void __carver_argv_modifier(int *argcptr, char ***argvptr) {
  // Get shared memory pointer
  pid_t pid = getpid();
  std::string shm_id_fn = "/tmp/pin_shm_id_" + std::to_string(pid);
  std::ifstream shm_id_file(shm_id_fn);

  if (!shm_id_file.is_open()) {
    std::cerr << "Error: Can't open shm_id file\n";
    exit(1);
  }

  char shm_id_str[256];
  shm_id_file >> shm_id_str;
  shm_id_file.close();

  unlink(shm_id_fn.c_str());

  int shm_id = atoi(shm_id_str);

  ptr_alloc_shm_map = (char *)shmat(shm_id, 0, 0);

  if (ptr_alloc_shm_map == NULL) {
    std::cerr << "Error: Failed to attach shared memory, errno : "
              << strerror(errno) << "\n";
    exit(1);
  }

  LOCK_SHM_MAP();

  int argc = (*argcptr) - 1;
  *argcptr = argc;

  char *tmp_outdir_name = (*argvptr)[argc];

  if (tmp_outdir_name[0] != '/') {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      std::cerr << "Error: Failed to get current working directory, errno : "
                << strerror(errno) << "\n";
      exit(1);
    } else {
      outdir_name = (char *)malloc(sizeof(char) * 512);
      snprintf(outdir_name, 512, "%s/%s", cwd, tmp_outdir_name);
    }
  } else {
    outdir_name = (char *)malloc(sizeof(char) * 512);
    snprintf(outdir_name, 512, "%s", tmp_outdir_name);
  }

  (*argvptr)[argc] = 0;
  __mem_allocated_probe(*argvptr, sizeof(char *) * argc, 0);
  int idx;
  for (idx = 0; idx < argc; idx++) {
    char *argv_str = (*argvptr)[idx];
    __mem_allocated_probe(argv_str, strlen(argv_str) + 1, 0);
  }

  // Write argc, argv values, TODO

  __carv_ready = true;
  UNLOCK_SHM_MAP();
  return;
}

void __carv_FINI() {
  char buffer[256];
  free(outdir_name);

  int idx = 0;
  int num_hash_objs = func_result_hash.size();
  for (idx = 0; idx < num_hash_objs; idx++) {
    auto search = func_result_hash.get_by_idx(idx);
    int *hash_ptr = search->elem;
    free(hash_ptr);
  }

  __carv_ready = false;
}

static map<const char *, unsigned int> func_file_counter;

void __carv_open(const char *func_name) {
  if (!__carv_ready) {
    return;
  }

  LOCK_SHM_MAP();

  unsigned int cur_cnt = 0;
  unsigned int *func_count = func_file_counter.find(func_name);
  if (func_count == NULL) {
    func_file_counter.insert(func_name, 1);
    cur_cnt = 1;
  } else {
    cur_cnt = *func_count;
    (*func_count)++;
  }

  FUNC_CONTEXT new_ctx = FUNC_CONTEXT(carved_index++, cur_cnt, func_name);

  inputs.push_back(new_ctx);

  carved_objs = &(inputs.back()->inputs);
  carved_ptrs = &(inputs.back()->carved_ptrs);
  __carv_opened = true;

  assert(carved_objs->size() == 0);
  assert(carved_ptrs->size() == 0);
  UNLOCK_SHM_MAP();
  return;
}

void __carv_mark_address(const char *ptr, const char is_crash) {
  if (carved_ptrs == nullptr) {
    return;
  }

  if (!is_crash && carved_ptrs->size() == 0) {
    return;
  }

  if (ptr == nullptr) {
    return;
  }

  LOCK_SHM_MAP();

  int idx = 0;
  vector<void *> *used_ptrs = &(inputs.back()->used_ptrs);
  const int size = used_ptrs->size();

  for (idx = 0; idx < size; idx++) {
    if (used_ptrs->data[idx] == ptr) {
      UNLOCK_SHM_MAP();
      return;
    }
  }

  used_ptrs->push_back((void *)ptr);

  UNLOCK_SHM_MAP();

  if (is_crash) {
    dump_result(inputs.back()->func_name, 0);
  }

  return;
}

// Count # of objs of each type
void __carv_close(const char *func_name) {
  if (!__carv_ready) {
    return;
  }

  LOCK_SHM_MAP();

  if (!(carved_objs == NULL || (carved_objs->size() == 0))) {
    UNLOCK_SHM_MAP();
    dump_result(func_name, 1);
    LOCK_SHM_MAP();
  }

  if (carved_objs != NULL) {
    unsigned int idx = 0;
    const unsigned int num_objs = carved_objs->size();
    while (idx < num_objs) {
      delete *(carved_objs->get(idx));
      idx++;
    }
    carved_objs->clear();
    carved_ptrs->clear();
  }

  inputs.pop_back();

  class FUNC_CONTEXT *next_ctx = inputs.back();
  if ((next_ctx == NULL) || (!next_ctx->is_carved)) {
    carved_objs = NULL;
    carved_ptrs = NULL;
  } else {
    carved_objs = &(next_ctx->inputs);
    carved_ptrs = &(next_ctx->carved_ptrs);
  }

  UNLOCK_SHM_MAP();
  return;
}

static void dump_result(const char *func_name, char remove_dup) {
  LOCK_SHM_MAP();

  class FUNC_CONTEXT *cur_context = inputs.back();
  const unsigned int cur_carving_index = cur_context->carving_index;
  const unsigned int cur_func_call_idx = cur_context->func_call_idx;

  if (func_name != cur_context->func_name) {
    std::cerr << "Error: Returning func_name != cur_context->func_name\n";
    std::cerr << "Returning func : " << func_name << "\n";
    std::cerr << "Missing func name : " << cur_context->func_name << "\n";
    UNLOCK_SHM_MAP();
    return;
  }

  const unsigned num_objs = carved_objs->size();
  const unsigned int num_carved_ptrs = carved_ptrs->size();

  int **hash_ptr = func_result_hash.find(func_name);

  if (hash_ptr == nullptr) {
    int *res_hash = (int *)malloc(sizeof(int) * HASH_MAX_NUM_FILE);
    memset(res_hash, 0, sizeof(int) * HASH_MAX_NUM_FILE);
    func_result_hash.insert(func_name, res_hash);
  }

  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%u_%u", outdir_name, func_name,
           cur_func_call_idx, cur_carving_index);

  std::ofstream outfile(outfile_name);

  if (!outfile.is_open()) {
    UNLOCK_SHM_MAP();
    return;
  }

  // raw

  vector<void *> *used_ptrs = &(cur_context->used_ptrs);
  const unsigned int num_used_ptrs = used_ptrs->size();

  // outfile << "read pointers : " << num_used_ptrs << "\n";
  // unsigned int idx = 0;
  // while (idx < num_used_ptrs) {
  //   outfile << "  [" << idx << "] : " << used_ptrs->data[idx] << "\n";
  //   idx++;
  // }
  // outfile << "###############\n";

  vector<void *> visit_ptr_stack;
  vector<int> visit_elem_size_stack;

  bool print_obj = true;
  int depth = 0;
  auto format_with_indent = [&depth, &outfile](std::string str, bool reached) {
    std::string indent(2 * depth, ' ');
    outfile << (reached ? '%' : '-') << ' ' << indent << str << '\n';
  };

  for (unsigned int idx = 0; idx < num_objs; ++idx) {
    IVAR *elem = *(carved_objs->get(idx));

    if (elem->type == INPUT_TYPE::PTR_BEGIN) {
      VAR<int> *input = (VAR<int> *)elem;
      int ptr_idx = input->input;
      format_with_indent("PTR_BEGIN " + std::to_string(ptr_idx), false);    // Postprocessing will handle reached info
      depth++;
    } else if (elem->type == INPUT_TYPE::PTR_END) {
      VAR<int> *input = (VAR<int> *)elem;
      int ptr_idx = input->input;
      depth--;
      format_with_indent("PTR_END " + std::to_string(ptr_idx), false);      // Postprocessing will handle reached info
    } else if (elem->type == INPUT_TYPE::STRUCT_BEGIN) {
      format_with_indent("STRUCT_BEGIN", print_obj);
      depth++;
    } else if (elem->type == INPUT_TYPE::STRUCT_END) {
      depth--;
      format_with_indent("STRUCT_END", print_obj);
    } else {
      std::stringstream ss;
      if (elem->type == INPUT_TYPE::CHAR) {
        ss << "i8" << ' ' << (int)(((VAR<char> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::SHORT) {
        ss << "i16" << ' ' << (int)(((VAR<short> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::INT) {
        ss << "i32" << ' ' << (int)(((VAR<int> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::LONG) {
        ss << "i32" << ' ' << ((VAR<long> *)elem)->input;
      } else if (elem->type == INPUT_TYPE::LONGLONG) {
        ss << "i64" << ' ' << ((VAR<long long> *)elem)->input;
      } else if (elem->type == INPUT_TYPE::FLOAT) {
        ss << "f32" << ' ' << ((VAR<float> *)elem)->input;
      } else if (elem->type == INPUT_TYPE::DOUBLE) {
        ss << "f64" << ' ' << ((VAR<double> *)elem)->input;
      } else if (elem->type == INPUT_TYPE::NULLPTR) {
        ss << (elem->name == NULL ? "func" : elem->name) << ' ' << "nullptr";
      } else if (elem->type == INPUT_TYPE::PTR) {
        VAR<int> *input = (VAR<int> *)elem;
        int ptr_idx = input->input;
        POINTER *carved_ptr = carved_ptrs->get(ptr_idx);

        if (input->pointer_offset == 0) {
          ss << carved_ptr->pointee_type << ' ' << "p" << ptr_idx << '['
             << carved_ptr->alloc_size << ']';
          visit_ptr_stack.push_back((char *)carved_ptr->addr -
                                    carved_ptr->elem_size);
          visit_elem_size_stack.push_back(carved_ptr->elem_size);
        } else {
          ss << carved_ptr->pointee_type << " *p" << ptr_idx << '+'
             << input->pointer_offset;
        }
      } else if (elem->type == INPUT_TYPE::FUNCPTR) {
        VAR<char *> *input = (VAR<char *> *)elem;
        ss << "func" << ' ' << input->input;
      } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
        ss << elem->name << ' ' << "?";
      } else if (elem->type == INPUT_TYPE::PTR_IDX) {
        VAR<int> *input = (VAR<int> *)elem;

        void **cur_ptr = visit_ptr_stack.back();
        *cur_ptr = (char *)*cur_ptr + *(visit_elem_size_stack.back());

        void *cur_ptr_val = *cur_ptr;
        print_obj = false;
        for (unsigned int idx2 = 0; idx2 < num_used_ptrs; idx2++) {
          if (used_ptrs->data[idx2] == cur_ptr_val) {
            print_obj = true;
            break;
          }
        }

        ss << "PTR_IDX" << ' ' << input->input;
      } else if (elem->type == INPUT_TYPE::PTR_END) {
        ss << "PTR_END" << ' ' << ((VAR<int> *)elem)->input;
        visit_ptr_stack.pop_back();
        visit_elem_size_stack.pop_back();
        if (visit_ptr_stack.size() == 0) {
          print_obj = true;
        }
      } else {
        continue;
      }
      format_with_indent(ss.str(), print_obj);
      ss.clear();
    }
    outfile.flush();
  }

  outfile.close();

  // // raw end
  /*
  fprintf(outfile, "####################\n");

  vector<int> ptr_stack;

  // result
  idx = 0;
  while (idx < num_objs) {
    IVAR *elem = *(carved_objs->get(idx));

    if (ptr_stack.size() == 0) {
      if (elem->type == INPUT_TYPE::CHAR) {
        fprintf(outfile, "char %d\n", (int)(((VAR<char> *)elem)->input));
      } else if (elem->type == INPUT_TYPE::SHORT) {
        fprintf(outfile, "short %d\n", (int)(((VAR<short> *)elem)->input));
      } else if (elem->type == INPUT_TYPE::INT) {
        fprintf(outfile, "int %d\n", (int)(((VAR<int> *)elem)->input));
      } else if (elem->type == INPUT_TYPE::LONG) {
        fprintf(outfile, "long %ld\n", ((VAR<long> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::LONGLONG) {
        fprintf(outfile, "long long %lld\n", ((VAR<long long> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::FLOAT) {
        fprintf(outfile, "float %f\n", ((VAR<float> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::DOUBLE) {
        fprintf(outfile, "double %lf\n", ((VAR<double> *)elem)->input);
      } else if (elem->type == INPUT_TYPE::NULLPTR) {
        fprintf(outfile, "nullptr\n");
      } else if (elem->type == INPUT_TYPE::PTR) {
        VAR<int> *input = (VAR<int> *)elem;
        int ptr_idx = input->input;
        POINTER *carved_ptr = carved_ptrs->get(ptr_idx);

        if (input->pointer_offset == 0) {
          fprintf(outfile, "%s p%d[%d]\n", carved_ptr->pointee_type, ptr_idx,
                  carved_ptr->alloc_size);
        } else {
          fprintf(outfile, "%s * p%d + %d\n", carved_ptr->pointee_type, ptr_idx,
                  input->pointer_offset);
        }
      } else if (elem->type == INPUT_TYPE::FUNCPTR) {
        VAR<char *> *input = (VAR<char *> *)elem;
        fprintf(outfile, "%s\n", input->input);
      } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
        fprintf(outfile, "?\n");
      }
    }

    if (elem->type == INPUT_TYPE::PTR_BEGIN) {
      VAR<int> *input = (VAR<int> *)elem;
      ptr_stack.push_back(input->input);
    } else if (elem->type == INPUT_TYPE::PTR_END) {
      ptr_stack.pop_back();
    }

    idx++;
  }

  ptr_stack.clear();

  // print pointers

  int cur_ptr_idx = 0;
  int prev_obj_idx = 0;

  int inner_ptr_idx = -1;

  print_obj = false;

  int cur_ptr_elem_size = 1;
  char *cur_ptr_addr = 0;

  // Print loaded ptrs
  // fprintf(stderr, "loaded ptrs : %d\n", loaded_ptrs->size());
  // for (idx = 0; idx < num_loaded_ptrs; idx++) {
  //   fprintf(stderr, "  [%d] : %p\n", idx, loaded_ptrs->data[idx]);
  // }

  idx = 0;
  while (idx < num_objs) {
    IVAR *elem = *(carved_objs->get(idx));

    if (!print_obj) {
      if (elem->type == INPUT_TYPE::PTR_BEGIN) {
        VAR<int> *input = (VAR<int> *)elem;
        if (input->input == cur_ptr_idx) {
          print_obj = true;
          prev_obj_idx = idx;
          POINTER *carved_ptr = carved_ptrs->get(cur_ptr_idx);
          cur_ptr_elem_size = carved_ptr->elem_size;
          cur_ptr_addr = (char *)carved_ptr->addr;
          cur_ptr_addr -= cur_ptr_elem_size;
          fprintf(outfile, "p%d = {", cur_ptr_idx);
        }
      } else if (elem->type == INPUT_TYPE::PTR_END) {
        VAR<int> *input = (VAR<int> *)elem;
        if (input->input == inner_ptr_idx) {
          print_obj = true;
          inner_ptr_idx = -1;
        }
      }
    } else {
      if (elem->type == INPUT_TYPE::PTR_BEGIN) {
        VAR<int> *input = (VAR<int> *)elem;
        inner_ptr_idx = input->input;
        print_obj = false;
      } else if (elem->type == INPUT_TYPE::PTR_END) {
        VAR<int> *input = (VAR<int> *)elem;
        assert(input->input == cur_ptr_idx);
        print_obj = false;
        cur_ptr_idx++;
        idx = prev_obj_idx;
        fprintf(outfile, "}\n");
      } else if (elem->type == INPUT_TYPE::PTR_IDX) {
        VAR<int> *input = (VAR<int> *)elem;
        int ptr_offset = input->input;
        cur_ptr_addr = cur_ptr_addr + cur_ptr_elem_size;
        // fprintf(outfile, "\np%d[%d] = ", cur_ptr_idx, ptr_offset);
      } else if (elem->type == INPUT_TYPE::STRUCT_BEGIN) {
        fprintf(outfile, "{");
      } else if (elem->type == INPUT_TYPE::STRUCT_END) {
        fprintf(outfile, "}");
      } else {
        int idx = 0;
        for (idx = 0; idx < num_loaded_ptrs; idx++) {
          if (loaded_ptrs->data[idx] == cur_ptr_addr) {
            break;
          }
        }

        if (idx != num_loaded_ptrs) {
          if (elem->type == INPUT_TYPE::CHAR) {
            fprintf(outfile, "%d, ", (int)(((VAR<char> *)elem)->input));
          } else if (elem->type == INPUT_TYPE::SHORT) {
            fprintf(outfile, "%d, ", (int)(((VAR<short> *)elem)->input));
          } else if (elem->type == INPUT_TYPE::INT) {
            fprintf(outfile, "%d, ", (int)(((VAR<int> *)elem)->input));
          } else if (elem->type == INPUT_TYPE::LONG) {
            fprintf(outfile, "%ld, ", ((VAR<long> *)elem)->input);
          } else if (elem->type == INPUT_TYPE::LONGLONG) {
            fprintf(outfile, "%lld, ", ((VAR<long long> *)elem)->input);
          } else if (elem->type == INPUT_TYPE::FLOAT) {
            fprintf(outfile, "%f, ", ((VAR<float> *)elem)->input);
          } else if (elem->type == INPUT_TYPE::DOUBLE) {
            fprintf(outfile, "%lf, ", ((VAR<double> *)elem)->input);
          } else if (elem->type == INPUT_TYPE::NULLPTR) {
            fprintf(outfile, "nullptr, ");
          } else if (elem->type == INPUT_TYPE::PTR) {
            VAR<int> *input = (VAR<int> *)elem;
            int ptr_idx = input->input;
            POINTER *carved_ptr = carved_ptrs->get(ptr_idx);

            if (input->pointer_offset != 0) {
              fprintf(outfile, "p%d+%d, ", ptr_idx, input->pointer_offset);
            } else {
              fprintf(outfile, "p%d, ", ptr_idx);
            }
          } else if (elem->type == INPUT_TYPE::FUNCPTR) {
            // fprintf(outfile, "Funcptr, ");
            fprintf(outfile, "%s, ", ((VAR<char *> *)elem)->input);
          }
        }
      }
    }
    idx++;
  }
  */

  // compute hash and remove duplicates
  if (remove_dup) {
    int *res_hash = *(func_result_hash.find(func_name));
    int hash_val = 0;
    FILE *hashfile = fopen(outfile_name, "rb");
    if (hashfile == NULL) {
      UNLOCK_SHM_MAP();
      return;
    }

    char buf[4096];
    int read_size;
    while ((read_size = fread(buf, 1, 4096, hashfile)) > 0) {
      int idx = 0;
      while (idx < read_size) {
        hash_val += buf[idx];
        hash_val = hash_val % HASH_MAX_NUM_FILE;
        idx += 16;
      }
    }

    fclose(hashfile);

    if (res_hash[hash_val] == 0) {
      res_hash[hash_val] = 1;
    } else {
      unlink(outfile_name);
    }
  }

  UNLOCK_SHM_MAP();
  return;
}
}  // extern "C"
