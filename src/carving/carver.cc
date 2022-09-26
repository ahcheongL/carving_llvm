#ifndef __CROWN_CARVER_DEF
#define __CROWN_CARVER_DEF

#include "utils.hpp"
#include <errno.h>
#include <unistd.h>

#define MAX_NUM_FILE 8
#define MINSIZE 3
#define MAXSIZE 24
#define CARV_PROB 100
#define CARV_PROB_TYPE 100

static char *outdir_name = NULL;

static int *num_func_calls;
static int num_func_calls_size;

static int **func_carved_filesize;

static int num_type_carved;
static int **type_carved_inputssize;

static int *callseq;
static int callseq_size;
static int callseq_index;

static int carved_index = 0;

// Function pointer names
static boost::container::map<void *, char *> func_ptrs;

static map<void *, char> no_stub_funcs;

// inputs, work as similar as function call stack
static vector<FUNC_CONTEXT> inputs;
vector<IVAR *> *__carve_cur_inputs = NULL;
static vector<PTR> *cur_carved_ptrs = NULL;

// memory info
static boost::container::map<void *, struct typeinfo> alloced_ptrs;

// variable naming
static vector<char *> __carv_base_names;
static vector<bool> __need_to_free_carv_base_names;

int __carv_cur_class_index = -1;
int __carv_cur_class_size = -1;

bool __carv_ready0 = false;
bool __carv_ready = false;
char __carv_depth = 0;

static map<char *, classinfo> class_info;

void Carv_char(char input) {
  VAR<char> *inputv =
      new VAR<char>(input, strdup(*__carv_base_names.back()), INPUT_TYPE::CHAR);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_short(short input) {
  VAR<short> *inputv = new VAR<short>(input, strdup(*__carv_base_names.back()),
                                      INPUT_TYPE::SHORT);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_int(int input) {
  VAR<int> *inputv =
      new VAR<int>(input, strdup(*__carv_base_names.back()), INPUT_TYPE::INT);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_longtype(long input) {
  VAR<long> *inputv =
      new VAR<long>(input, strdup(*__carv_base_names.back()), INPUT_TYPE::LONG);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_longlong(long long input) {
  VAR<long long> *inputv = new VAR<long long>(
      input, strdup(*__carv_base_names.back()), INPUT_TYPE::LONGLONG);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_float(float input) {
  VAR<float> *inputv = new VAR<float>(input, strdup(*__carv_base_names.back()),
                                      INPUT_TYPE::FLOAT);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

void Carv_double(double input) {
  VAR<double> *inputv = new VAR<double>(
      input, strdup(*__carv_base_names.back()), INPUT_TYPE::DOUBLE);
  __carve_cur_inputs->push_back((IVAR *)inputv);
}

int Carv_pointer(void *ptr, char *type_name, int default_idx,
                 int default_size) {
  char *updated_name = strdup(*(__carv_base_names.back()));

  if (ptr == NULL) {
    VAR<void *> *inputv =
        new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    __carve_cur_inputs->push_back((IVAR *)inputv);
    return 0;
  }

  // Find already carved ptr
  int index = inputs.back()->carved_ptr_begin_idx;
  int num_carved_ptrs = cur_carved_ptrs->size();
  while (index < num_carved_ptrs) {
    PTR *carved_ptr = cur_carved_ptrs->get(index);
    char *carved_addr = (char *)carved_ptr->addr;
    int carved_ptr_size = carved_ptr->alloc_size;
    char *carved_addr_end = carved_addr + carved_ptr_size;
    if ((carved_addr <= ptr) && (ptr < carved_addr_end)) {
      int offset = ((char *)ptr) - carved_addr;
      VAR<int> *inputv =
          new VAR<int>(index, updated_name, offset, INPUT_TYPE::POINTER);
      __carve_cur_inputs->push_back((IVAR *)inputv);
      // Won't carve again.
      return 0;
    }
    index++;
  }

  // Check whether it is alloced area
  // if (alloced_ptrs.size() == 0) { return 0; } // no way

  auto closest_alloc = alloced_ptrs.upper_bound(ptr);
  if (closest_alloc == alloced_ptrs.begin()) {
    VAR<void *> *inputv =
        new VAR<void *>(ptr, updated_name, INPUT_TYPE::UNKNOWN_PTR);
    __carve_cur_inputs->push_back((IVAR *)inputv);
    return 0;
  }

  closest_alloc--;

  char *closest_alloc_ptr_addr = (char *)closest_alloc->first;
  typeinfo *closest_alloced_info = &closest_alloc->second;

  char *alloced_addr_end = closest_alloc_ptr_addr + closest_alloced_info->size;

  if (alloced_addr_end < (char *)ptr) {
    VAR<void *> *inputv =
        new VAR<void *>(ptr, updated_name, INPUT_TYPE::UNKNOWN_PTR);
    __carve_cur_inputs->push_back((IVAR *)inputv);
    return 0;
  }

  int ptr_alloc_size = alloced_addr_end - ((char *)ptr);
  int new_carved_ptr_index = cur_carved_ptrs->size();

  __carv_cur_class_index = default_idx;
  __carv_cur_class_size = default_size;

  char *name_ptr = closest_alloced_info->type_name;
  if (name_ptr != NULL) {
    auto search = class_info.find(name_ptr);
    if ((search != NULL) && ((ptr_alloc_size % search->size) == 0)) {
      __carv_cur_class_index = search->class_index;
      __carv_cur_class_size = search->size;
      type_name = name_ptr;
    }
  }

  cur_carved_ptrs->push_back(PTR(ptr, type_name, ptr_alloc_size));

  VAR<int> *inputv =
      new VAR<int>(new_carved_ptr_index, updated_name, 0, INPUT_TYPE::POINTER);
  __carve_cur_inputs->push_back((IVAR *)inputv);

  return ptr_alloc_size;
}

void __record_func_ptr(void *ptr, char *name) { func_ptrs[ptr] = name; }

void __add_no_stub_func(void *ptr) { no_stub_funcs.insert(ptr, 0); }

bool __is_no_stub_func(void *ptr) { return no_stub_funcs.find(ptr) != NULL; }

void __Carv_func_ptr(void *ptr) {
  char *updated_name = strdup(*__carv_base_names.back());
  auto search = func_ptrs.find(ptr);
  if ((ptr == NULL) || (search == func_ptrs.end())) {
    VAR<void *> *inputv =
        new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    __carve_cur_inputs->push_back((IVAR *)inputv);
    return;
  }

  VAR<char *> *inputv =
      new VAR<char *>(search->second, updated_name, INPUT_TYPE::FUNCPTR);
  __carve_cur_inputs->push_back((IVAR *)inputv);
  return;
}

void __carv_ptr_name_update(int idx) {
  char *base_name = *(__carv_base_names.back());
  char *update_name = (char *)malloc(sizeof(char) * 512);
  snprintf(update_name, 512, "%s[%d]", base_name, idx);
  __carv_base_names.push_back(update_name);
  __need_to_free_carv_base_names.push_back(true);
  return;
}

void __keep_class_info(char *class_name, int size, int index) {
  classinfo tmp(index, size);
  class_info.insert(class_name, tmp);
}

int __get_class_idx() { return __carv_cur_class_index; }

int __get_class_size() { return __carv_cur_class_size; }

void __carv_name_push(char *name) {
  __carv_base_names.push_back(name);
  __need_to_free_carv_base_names.push_back(false);
  return;
}

void __carv_name_pop() {
  if (*__need_to_free_carv_base_names.back()) {
    free(*__carv_base_names.back());
  }
  __carv_base_names.pop_back();
  __need_to_free_carv_base_names.pop_back();
  return;
}

void __carv_struct_name_update(char *field_name) {
  char *base_name = *(__carv_base_names.back());
  char *update_name = (char *)malloc(sizeof(char) * 512);
  snprintf(update_name, 512, "%s.%s", base_name, field_name);
  __carv_base_names.push_back(update_name);
  __need_to_free_carv_base_names.push_back(true);
  return;
}

// Save ptr with size and type_name into memory `allocted_ptrs`.
void __mem_allocated_probe(void *ptr, int size, char *type_name) {
  if (!__carv_ready0) {
    return;
  }
  struct typeinfo tmp {
    type_name, size
  };
  alloced_ptrs[ptr] = tmp;
  return;
}

void __remove_mem_allocated_probe(void *ptr) {
  if (!__carv_ready0) {
    return;
  }
  alloced_ptrs.erase(ptr);
}

void __carv_func_call_probe(int func_id) {
  if (!__carv_ready0) {
    return;
  }

  // Write call sequence
  callseq[callseq_index++] = func_id;
  if (callseq_index >= callseq_size) {
    callseq_size *= 2;
    callseq = (int *)realloc(callseq, callseq_size * sizeof(int));
  }

  // Write # of function call
  while (func_id >= num_func_calls_size) {
    int tmp = num_func_calls_size;
    num_func_calls_size *= 2;
    num_func_calls =
        (int *)realloc(num_func_calls, num_func_calls_size * sizeof(int));
    memset(num_func_calls + tmp, 0, tmp * sizeof(int));
    func_carved_filesize = (int **)realloc(func_carved_filesize,
                                           num_func_calls_size * sizeof(int *));
    memset(func_carved_filesize + tmp, 0, tmp * sizeof(int *));
  }

  FUNC_CONTEXT new_ctx =
      FUNC_CONTEXT(carved_index++, num_func_calls[func_id], func_id);
  num_func_calls[func_id] += 1;

  inputs.push_back(new_ctx);

  if ((rand() % 100) < CARV_PROB) {
    __carve_cur_inputs = &(inputs.back()->inputs);
    cur_carved_ptrs = &(inputs.back()->carved_ptrs);
    __carv_ready = true;
  } else {
    inputs.back()->is_carved = false;
    __carve_cur_inputs = NULL;
    __carv_ready = false;
  }
  return;
}

void __update_carved_ptr_idx() { inputs.back()->update_carved_ptr_begin_idx(); }

static void carved_ptr_postprocessing(int begin_idx, int end_idx) {
  int idx1, idx2, idx3, idx4, idx5;
  bool changed = true;
  while (changed) {
    changed = false;
    idx1 = begin_idx;
    while (idx1 < end_idx) {
      int idx2 = idx1 + 1;
      PTR *cur_carved_ptr = cur_carved_ptrs->get(idx1);
      char *addr1 = (char *)cur_carved_ptr->addr;
      int size1 = cur_carved_ptr->alloc_size;
      if (size1 == 0) {
        idx1++;
        continue;
      }
      char *end_addr1 = addr1 + size1;
      const char *type1 = cur_carved_ptr->pointee_type;

      while (idx2 < end_idx) {
        PTR *cur_carved_ptr2 = cur_carved_ptrs->get(idx2);
        char *addr2 = (char *)cur_carved_ptr2->addr;
        int size2 = cur_carved_ptr2->alloc_size;
        if (size2 == 0) {
          idx2++;
          continue;
        }
        char *end_addr2 = addr2 + size2;
        const char *type2 = cur_carved_ptr2->pointee_type;
        if (type1 != type2) {
          idx2++;
          continue;
        }
        int offset = -1;
        int remove_ptr_idx;
        int replacing_ptr_idx;
        PTR *remove_ptr;
        if ((addr1 <= addr2) && (addr2 < end_addr1)) {
          offset = addr2 - addr1;
          remove_ptr_idx = idx2;
          replacing_ptr_idx = idx1;
          remove_ptr = cur_carved_ptr2;
        } else if ((addr2 <= addr1) && (addr1 < end_addr2)) {
          offset = addr1 - addr2;
          remove_ptr_idx = idx1;
          replacing_ptr_idx = idx2;
          remove_ptr = cur_carved_ptr;
        }

        if (offset != -1) {
          // remove remove_ptr in inputs;
          int idx3 = 0;
          int num_inputs = __carve_cur_inputs->size();
          while (idx3 < num_inputs) {
            IVAR *tmp_input = *(__carve_cur_inputs->get(idx3));
            if (tmp_input->type == INPUT_TYPE::POINTER) {
              VAR<int> *tmp_inputt = (VAR<int> *)tmp_input;
              if (tmp_inputt->input == remove_ptr_idx) {
                int old_offset = tmp_inputt->pointer_offset;
                tmp_inputt->input = replacing_ptr_idx;
                tmp_inputt->pointer_offset = offset + old_offset;
                if (old_offset == 0) {
                  // remove element carved results
                  char *var_name = tmp_input->name;
                  size_t var_name_len = strlen(var_name);
                  char *check_name = (char *)malloc(var_name_len + 2);
                  memcpy(check_name, var_name, var_name_len);
                  check_name[var_name_len] = '[';
                  check_name[var_name_len + 1] = 0;
                  int idx4 = idx3 + 1;
                  while (idx4 < num_inputs) {
                    IVAR *next_input = *(__carve_cur_inputs->get(idx4));
                    if (strncmp(check_name, next_input->name,
                                var_name_len + 1) != 0) {
                      break;
                    }
                    idx4++;
                  }
                  free(check_name);
                  int idx5 = idx3 + 1;
                  while (idx5 < idx4) {
                    delete *(__carve_cur_inputs->get(idx3 + 1));
                    __carve_cur_inputs->remove(idx3 + 1);
                    idx5++;
                  }
                  num_inputs = __carve_cur_inputs->size();
                }
              }
            }
            idx3++;
          }
          remove_ptr->alloc_size = 0;
          changed = true;
          break;
        }
        idx2++;
      }

      if (changed)
        break;
      idx1++;
    }
  }
  return;
}

static int num_excluded = 0;

void __carv_func_ret_probe(char *func_name, int func_id) {
  if (__carv_ready0 == false) {
    return;
  }

  if (__carve_cur_inputs == NULL) {
    inputs.pop_back();

    class FUNC_CONTEXT *next_ctx = inputs.back();
    if ((next_ctx == NULL) || (!next_ctx->is_carved)) {
      __carve_cur_inputs = NULL;
      cur_carved_ptrs = NULL;
      __carv_ready = false;
    } else {
      __carve_cur_inputs = &(next_ctx->inputs);
      cur_carved_ptrs = &(next_ctx->carved_ptrs);
      __carv_ready = true;
    }
    return;
  }

  class FUNC_CONTEXT *cur_context = inputs.back();
  inputs.pop_back();
  int idx = 0;
  const int cur_carving_index = cur_context->carving_index;
  const int cur_func_call_idx = cur_context->func_call_idx;
  const int num_carved_ptrs = cur_carved_ptrs->size();
  const int carved_ptrs_init_idx = cur_context->carved_ptr_begin_idx;

  if (func_id != cur_context->func_id) {
    std::cerr << "Error: Returning func_id != cur_context->func_id\n";
    std::cerr << "Returning func : " << func_name << "," << func_id << "\n";
    std::cerr << "Missing func id : " << cur_context->func_id << "\n";
    exit(1);
  }

  // check memory overlap
  // std::cerr << "Start processing...\n";
  // carved_ptr_postprocessing(0, carved_ptrs_init_idx);
  // carved_ptr_postprocessing(carved_ptrs_init_idx, num_carved_ptrs);
  // std::cerr << "Processing done\n";
  const int num_inputs = __carve_cur_inputs->size();

  bool skip_write = false;

  if (num_inputs <= (1 << MINSIZE)) {
    skip_write = true;
  } else {
    int tmp = (1 << (MINSIZE + 1));
    int index = 0;
    while (num_inputs > tmp) {
      tmp *= 2;
      index += 1;
    }

    if (func_carved_filesize[func_id] == 0) {
      func_carved_filesize[func_id] =
          (int *)calloc(MAXSIZE - MINSIZE + 1, sizeof(int));
    }

    if (func_carved_filesize[func_id][index] >= MAX_NUM_FILE) {
      skip_write = true;
    } else {
      func_carved_filesize[func_id][index] += 1;
    }
  }

#ifdef SMALL
  skip_write = false;
#endif

  if (skip_write) {
    idx = 0;
    while (idx < num_inputs) {
      delete *(__carve_cur_inputs->get(idx));
      idx++;
    }

    class FUNC_CONTEXT *next_ctx = inputs.back();
    if ((next_ctx == NULL) || (!next_ctx->is_carved)) {
      __carve_cur_inputs = NULL;
      cur_carved_ptrs = NULL;
      __carv_ready = false;
    } else {
      __carve_cur_inputs = &(next_ctx->inputs);
      cur_carved_ptrs = &(next_ctx->carved_ptrs);
      __carv_ready = true;
    }

    num_excluded++;
    return;
  }

  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%d_%d", outdir_name, func_name,
           cur_carving_index, cur_func_call_idx);

  FILE *outfile = fopen(outfile_name, "w");

  if (outfile == NULL) {
    std::cerr << "Error: Failed to open file : " << outfile_name
              << ", errno : " << strerror(errno) << "\n";

    idx = 0;
    while (idx < num_inputs) {
      delete *(__carve_cur_inputs->get(idx));
      idx++;
    }

    class FUNC_CONTEXT *next_ctx = inputs.back();
    if ((next_ctx == NULL) || (!next_ctx->is_carved)) {
      __carve_cur_inputs = NULL;
      cur_carved_ptrs = NULL;
      __carv_ready = false;
    } else {
      __carve_cur_inputs = &(next_ctx->inputs);
      cur_carved_ptrs = &(next_ctx->carved_ptrs);
      __carv_ready = true;
    }
    return;
  }

  // Write carved pointers
  idx = 0;
  while (idx < num_carved_ptrs) {
    PTR *carved_ptr = cur_carved_ptrs->get(idx);
    fprintf(outfile, "%d:%p:%d:%s\n", idx, carved_ptr->addr,
            carved_ptr->alloc_size, carved_ptr->pointee_type);
    idx++;
  }

  fprintf(outfile, "####\n");

  idx = 0;
  while (idx < num_inputs) {
    IVAR *elem = *(__carve_cur_inputs->get(idx));
    if (elem->type == INPUT_TYPE::CHAR) {
      fprintf(outfile, "%s:CHAR:%d\n", elem->name,
              (int)(((VAR<char> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::SHORT) {
      fprintf(outfile, "%s:SHORT:%d\n", elem->name,
              (int)(((VAR<short> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::INT) {
      fprintf(outfile, "%s:INT:%d\n", elem->name,
              (int)(((VAR<int> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::LONG) {
      fprintf(outfile, "%s:LONG:%ld\n", elem->name, ((VAR<long> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::LONGLONG) {
      fprintf(outfile, "%s:LONGLONG:%lld\n", elem->name,
              ((VAR<long long> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::FLOAT) {
      fprintf(outfile, "%s:FLOAT:%f\n", elem->name,
              ((VAR<float> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::DOUBLE) {
      fprintf(outfile, "%s:DOUBLE:%lf\n", elem->name,
              ((VAR<double> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::NULLPTR) {
      fprintf(outfile, "%s:NULL:0\n", elem->name);
    } else if (elem->type == INPUT_TYPE::POINTER) {
      VAR<int> *input = (VAR<int> *)elem;
      fprintf(outfile, "%s:PTR:%d:%d\n", elem->name, input->input,
              input->pointer_offset);
    } else if (elem->type == INPUT_TYPE::FUNCPTR) {
      VAR<char *> *input = (VAR<char *> *)elem;
      fprintf(outfile, "%s:FUNCPTR:%s\n", elem->name, input->input);
    } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
      void *addr = ((VAR<void *> *)elem)->input;

      // address might be the end point of carved pointers
      int carved_idx = 0;
      int offset;
      while (carved_idx < num_carved_ptrs) {
        PTR *carved_ptr = cur_carved_ptrs->get(carved_idx);
        char *end_addr = (char *)carved_ptr->addr + carved_ptr->alloc_size;
        if (end_addr == addr) {
          offset = carved_ptr->alloc_size;
          break;
        }
        carved_idx++;
      }

      if (carved_idx == num_carved_ptrs) {
        fprintf(outfile, "%s:UNKNOWN_PTR:%p\n", elem->name,
                ((VAR<void *> *)elem)->input);
      } else {
        fprintf(outfile, "%s:PTR:%d:%d\n", elem->name, carved_idx, offset);
      }
    } else {
      std::cerr << "Warning : unknown element type : " << elem->type << ", "
                << elem->name << "\n";
    }

    delete elem;
    idx++;
  }

  fclose(outfile);

  class FUNC_CONTEXT *next_ctx = inputs.back();
  if ((next_ctx == NULL) || (!next_ctx->is_carved)) {
    __carve_cur_inputs = NULL;
    cur_carved_ptrs = NULL;
    __carv_ready = false;
  } else {
    __carve_cur_inputs = &(next_ctx->inputs);
    cur_carved_ptrs = &(next_ctx->carved_ptrs);
    __carv_ready = true;
  }

  return;
}

void __carver_argv_modifier(int *argcptr, char ***argvptr) {

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
  }

  (*argvptr)[argc] = 0;
  __mem_allocated_probe(*argvptr, sizeof(char *) * argc, 0);
  int idx;
  for (idx = 0; idx < argc; idx++) {
    char *argv_str = (*argvptr)[idx];
    __mem_allocated_probe(argv_str, strlen(argv_str) + 1, 0);
  }

  num_func_calls_size = 256;
  num_func_calls = (int *)calloc(num_func_calls_size, sizeof(int));
  func_carved_filesize = (int **)calloc(num_func_calls_size, sizeof(int *));

  num_type_carved = 2048;
  type_carved_inputssize = (int **)calloc(num_type_carved, sizeof(int *));

  callseq_size = 16384;
  callseq = (int *)malloc(callseq_size * sizeof(int));
  callseq_index = 0;

  // Write argc, argv values, TODO

  __carv_ready0 = true;
  return;
}

void __carv_FINI() {
  char buffer[256];
  snprintf(buffer, 256, "%s/call_seq", outdir_name);
  FILE *__call_seq_file = fopen(buffer, "w");
  if (__call_seq_file == NULL) {
    return;
  }
  fwrite(callseq, sizeof(int), callseq_index, __call_seq_file);
  fclose(__call_seq_file);
  free(callseq);
  free(num_func_calls);

  int idx;
  for (idx = 0; idx < num_func_calls_size; idx++) {
    free(func_carved_filesize[idx]);
  }
  free(func_carved_filesize);

  for (idx = 0; idx < num_type_carved; idx++) {
    free(type_carved_inputssize[idx]);
  }
  free(type_carved_inputssize);
  free(outdir_name);

  __carv_ready = false;
}

void __carv_open() {
  if (!__carv_ready) {
    return;
  }

  if ((rand() % 100) < CARV_PROB_TYPE) {
    FUNC_CONTEXT new_ctx = FUNC_CONTEXT(carved_index++, 0, 0);
    inputs.push_back(new_ctx);
    __carve_cur_inputs = &(inputs.back()->inputs);
    cur_carved_ptrs = &(inputs.back()->carved_ptrs);
    return;
  } else {
    __carve_cur_inputs = NULL;
  }
}

static map<const char *, unsigned int> type_counter;
static map<const char *, unsigned int> type_idx;
static unsigned int max_type_idx = 0;

void __carv_close(const char *type_name, const char *func_name) {
  if (__carve_cur_inputs == NULL) {
    return;
  }

  FUNC_CONTEXT *cur_context = inputs.back();
  inputs.pop_back();

  int idx = 0;
  const int num_carved_ptrs = cur_carved_ptrs->size();
  const int num_inputs = __carve_cur_inputs->size();

  bool skip_write = false;

  unsigned int cur_type_idx = 0;

  unsigned int *type_idx_ptr = type_idx[type_name];
  if (type_idx_ptr == NULL) {
    cur_type_idx = max_type_idx;
    type_idx.insert(type_name, max_type_idx++);
  } else {
    cur_type_idx = *type_idx_ptr;
  }

  if (num_inputs <= (1 << MINSIZE)) {
    skip_write = true;
  } else {
    int tmp = (1 << (MINSIZE + 1));
    int index = 0;
    while (num_inputs > tmp) {
      tmp *= 2;
      index += 1;
    }

    if (cur_type_idx >= num_type_carved) {
      int tmp = num_type_carved;
      while (cur_type_idx >= num_type_carved) {
        num_type_carved *= 2;
      }

      type_carved_inputssize = (int **)realloc(type_carved_inputssize,
                                               sizeof(int *) * num_type_carved);
      memset(type_carved_inputssize + tmp, 0,
             sizeof(int *) * (num_type_carved - tmp));
    }

    if (type_carved_inputssize[cur_type_idx] == 0) {
      type_carved_inputssize[cur_type_idx] =
          (int *)calloc(MAXSIZE - MINSIZE + 1, sizeof(int));
    }

    if (type_carved_inputssize[cur_type_idx][index] >= MAX_NUM_FILE) {
      skip_write = true;
    } else {
      type_carved_inputssize[cur_type_idx][index] += 1;
    }
  }

  if (skip_write) {
    idx = 0;
    while (idx < num_inputs) {
      delete *(__carve_cur_inputs->get(idx));
      idx++;
    }

    num_excluded += 1;
    return;
  }

  unsigned int *type_count = type_counter[type_name];
  if (type_count == NULL) {
    type_counter.insert(type_name, 1);
    type_count = type_counter[type_name];
  } else {
    (*type_count)++;
  }

  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%d_%s", outdir_name, type_name,
           (*type_count), func_name);
  FILE *outfile = fopen(outfile_name, "w");

  if (outfile == NULL) {
    idx = 0;
    while (idx < num_inputs) {
      delete *(__carve_cur_inputs->get(idx));
      idx++;
    }
    return;
  }

  idx = 0;
  while (idx < num_carved_ptrs) {
    PTR *carved_ptr = cur_carved_ptrs->get(idx);
    fprintf(outfile, "%d:%p:%d:%s\n", idx, carved_ptr->addr,
            carved_ptr->alloc_size, carved_ptr->pointee_type);
    idx++;
  }

  fprintf(outfile, "####\n");

  idx = 0;
  while (idx < num_inputs) {
    IVAR *elem = *(__carve_cur_inputs->get(idx));
    if (elem->type == INPUT_TYPE::CHAR) {
      fprintf(outfile, "%s:CHAR:%d\n", elem->name,
              (int)(((VAR<char> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::SHORT) {
      fprintf(outfile, "%s:SHORT:%d\n", elem->name,
              (int)(((VAR<short> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::INT) {
      fprintf(outfile, "%s:INT:%d\n", elem->name,
              (int)(((VAR<int> *)elem)->input));
    } else if (elem->type == INPUT_TYPE::LONG) {
      fprintf(outfile, "%s:LONG:%ld\n", elem->name, ((VAR<long> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::LONGLONG) {
      fprintf(outfile, "%s:LONGLONG:%lld\n", elem->name,
              ((VAR<long long> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::FLOAT) {
      fprintf(outfile, "%s:FLOAT:%f\n", elem->name,
              ((VAR<float> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::DOUBLE) {
      fprintf(outfile, "%s:DOUBLE:%lf\n", elem->name,
              ((VAR<double> *)elem)->input);
    } else if (elem->type == INPUT_TYPE::NULLPTR) {
      fprintf(outfile, "%s:NULL:0\n", elem->name);
    } else if (elem->type == INPUT_TYPE::POINTER) {
      VAR<int> *input = (VAR<int> *)elem;
      fprintf(outfile, "%s:PTR:%d:%d\n", elem->name, input->input,
              input->pointer_offset);
    } else if (elem->type == INPUT_TYPE::FUNCPTR) {
      VAR<char *> *input = (VAR<char *> *)elem;
      fprintf(outfile, "%s:FUNCPTR:%s\n", elem->name, input->input);
    } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
      void *addr = ((VAR<void *> *)elem)->input;

      // address might be the end point of carved pointers
      int carved_idx = 0;
      int offset;
      while (carved_idx < num_carved_ptrs) {
        PTR *carved_ptr = cur_carved_ptrs->get(carved_idx);
        char *end_addr = (char *)carved_ptr->addr + carved_ptr->alloc_size;
        if (end_addr == addr) {
          offset = carved_ptr->alloc_size;
          break;
        }
        carved_idx++;
      }

      if (carved_idx == num_carved_ptrs) {
        fprintf(outfile, "%s:UNKNOWN_PTR:%p\n", elem->name,
                ((VAR<void *> *)elem)->input);
      } else {
        fprintf(outfile, "%s:PTR:%d:%d\n", elem->name, carved_idx, offset);
      }
    } else {
      std::cerr << "Warning : unknown element type : " << elem->type << ", "
                << elem->name << "\n";
    }

    delete elem;
    idx++;
  }

  fclose(outfile);
  return;
}

#endif