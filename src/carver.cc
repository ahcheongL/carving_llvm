#ifndef __CROWN_CARVER_DEF
#define __CROWN_CARVER_DEF

#include "utils.hpp"

static char * outdir_name = NULL;

static int * num_func_calls;
static int num_func_calls_size;

static int ** func_carved_filesize;

static int * callseq;
static int callseq_size;
static int callseq_index;

static int carved_index = 0;

//Function pointer names
static map<void *, char *> func_ptrs;

//inputs, work as similar as function call stack
static vector<FUNC_CONTEXT> inputs;
static vector<IVAR *> * cur_inputs = NULL;
static vector<PTR> * cur_carved_ptrs = NULL;

//memory info
static map<void *, int> alloced_ptrs;
static map<void *, char *> alloced_type;
static vector<char *> class_names;

//variable naming
static vector<char * > __carv_base_names;

//
static vector<void *> carving_obj_addrs;

void Carv_char(char input) {
  VAR<char> * inputv = new VAR<char>(input
    , *__carv_base_names.back(), INPUT_TYPE::CHAR);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_short(short input) {
  VAR<short> * inputv = new VAR<short>(input
    , *__carv_base_names.back(), INPUT_TYPE::SHORT);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_int(int input) {
  VAR<int> * inputv = new VAR<int>(input
    , *__carv_base_names.back(), INPUT_TYPE::INT);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_longtype(long input) {
  VAR<long> * inputv = new VAR<long>(input
    , *__carv_base_names.back(), INPUT_TYPE::LONG);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_longlong(long long input) {
  VAR<long long> * inputv = new VAR<long long>(input
    , *__carv_base_names.back(), INPUT_TYPE::LONGLONG);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_float(float input) {
  VAR<float> * inputv = new VAR<float>(input
    , *__carv_base_names.back(), INPUT_TYPE::FLOAT);
  cur_inputs->push_back((IVAR *) inputv);
}

void Carv_double(double input) {
  VAR<double> * inputv = new VAR<double>(input
    , *__carv_base_names.back(), INPUT_TYPE::DOUBLE);
  cur_inputs->push_back((IVAR *) inputv);
}

int Carv_pointer(void * ptr) {
  char * updated_name = *(__carv_base_names.back());
  if (ptr == NULL) {
    VAR<void *> * inputv = new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    cur_inputs->push_back((IVAR *) inputv);
    return 0;
  }

  //Find already carved ptr
  int index = inputs.back()->carved_ptr_begin_idx;
  int num_carved_ptrs = cur_carved_ptrs->size();
  while (index < num_carved_ptrs) {
    PTR * carved_ptr = cur_carved_ptrs->get(index);
    char * carved_addr = (char *) carved_ptr->addr;
    int carved_ptr_size = carved_ptr->alloc_size;
    char * carved_addr_end = carved_addr + carved_ptr_size;
    if ((carved_addr <= ptr) && (ptr < carved_addr_end)) {
      int offset = ((char *) ptr) - carved_addr;
      VAR<int> * inputv = new VAR<int>(
        index, updated_name, offset, INPUT_TYPE::POINTER);
      cur_inputs->push_back((IVAR *) inputv);
      //Won't carve again.
      return 0;
    }
    index ++;
  }

  //Check whether it is alloced area
  index = 0;
  int num_alloced_ptrs = alloced_ptrs.size();
  while (index < num_alloced_ptrs) {
    auto alloced_ptr = alloced_ptrs[index];
    char * alloced_addr = (char *) alloced_ptr->key;
    int alloced_size = alloced_ptr->elem;
    char * alloced_addr_end = alloced_addr + alloced_size;
    if ((alloced_addr <= ptr) && (ptr < alloced_addr_end)) {
      int size = alloced_addr_end - ((char *) ptr);
      int new_carved_ptr_index = cur_carved_ptrs->size();
      cur_carved_ptrs->push_back(PTR(ptr, size));

      VAR<int> * inputv = new VAR<int>(
        new_carved_ptr_index, updated_name, 0, INPUT_TYPE::POINTER);
      cur_inputs->push_back((IVAR *) inputv);

      return size;
    }
    index ++;
  }

  VAR<void *> * inputv = new VAR<void *>(
        ptr, updated_name, INPUT_TYPE::UNKNOWN_PTR);
  cur_inputs->push_back((IVAR *) inputv);
  return 0;
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.insert(ptr, name);
}

void __Carv_func_ptr(void * ptr) {
  char * updated_name = *__carv_base_names.back();
  auto search = func_ptrs.find(ptr);
  if ((ptr == NULL) || (search == NULL)) {
    if (ptr != NULL) {
      //std::cerr << "Warn : Unknown func ptr : " << updated_name << "\n";
    }
    VAR<void *> * inputv = new VAR<void *>(NULL
      , updated_name, INPUT_TYPE::NULLPTR);
    cur_inputs->push_back((IVAR *) inputv);
    return;
  }

  VAR<char *> * inputv = new VAR<char *> (*search
    , updated_name, INPUT_TYPE::FUNCPTR);
  cur_inputs->push_back((IVAR *) inputv);
  return;
}

void __carv_ptr_name_update(int idx) {
  char * base_name = *(__carv_base_names.back());
  char * update_name = (char *) malloc(sizeof(char) * 512);
  snprintf(update_name, 512, "%s[%d]",base_name, idx);
  __carv_base_names.push_back_copy(update_name);
  return;
}

void __keep_class_name(char * name) {
  class_names.push_back_copy(name);
}

int __get_class_name_idx(void * obj_ptr, int default_idx) {
  char ** name_ptr = alloced_type.find(obj_ptr);
  if (name_ptr == NULL) {
    std::cerr << "Could not found name of " << obj_ptr  << "\n";
    return default_idx;
  }
  std::cerr << "Found class name of " << obj_ptr << " : " << *name_ptr << "\n";
  int carved_idx = carving_obj_addrs.get_idx(obj_ptr);
  if (carved_idx == -1) {
    carving_obj_addrs.push_back_copy(obj_ptr);
    return class_names.get_idx(*name_ptr);
  }
  return default_idx;
}

void __pop_carving_obj() {
  carving_obj_addrs.pop_back();
  return;  
}

void __carv_name_push(char * name) {
  __carv_base_names.push_back(strdup(name));
  return;
}

void __carv_name_free_pop() {
  free(*__carv_base_names.back());
  __carv_base_names.pop_back();
  return;
}

void __carv_name_pop() {
  __carv_base_names.pop_back();
  return;
}

void __carv_struct_name_update(char * field_name) {
  char * base_name = *(__carv_base_names.back());
  char * update_name = (char *) malloc(sizeof(char) * 512);
  snprintf(update_name, 512, "%s.%s",base_name, field_name);
  __carv_base_names.push_back(std::move(update_name));
  return;
}

void __mem_allocated_probe(void * ptr, int size) {
  int * search = alloced_ptrs.find(ptr);

  if (search != NULL) {
    *search = size;
    return;
  }

  alloced_ptrs.insert(ptr, size);
  return;
}

void __mem_alloc_type(void * ptr, char * type_name) {
  std::cerr << "obj " << ptr << " has type : " << type_name << "\n";
  alloced_type.insert(ptr, type_name);
}

void __remove_mem_allocated_probe(void * ptr) {  
  alloced_ptrs.remove(ptr);
}

void __carv_func_call_probe(int func_id) {
  //Write call sequence
  callseq[callseq_index++] = func_id;
  if (callseq_index >= callseq_size) {
    callseq_size *= 2;
    callseq = (int *) realloc(callseq, callseq_size * sizeof(int));
  }

  //Write # of function call 
  while (func_id >= num_func_calls_size) {
    int tmp = num_func_calls_size;
    num_func_calls_size *= 2;
    num_func_calls = (int*) realloc (num_func_calls
      , num_func_calls_size * sizeof(int));
    memset(num_func_calls + tmp, 0, tmp * sizeof(int));
    func_carved_filesize = (int **) realloc(func_carved_filesize
      , num_func_calls_size * sizeof(int *));
    memset(func_carved_filesize + tmp, 0, tmp * sizeof(int *));
  }

  FUNC_CONTEXT new_ctx
    = FUNC_CONTEXT(carved_index++, num_func_calls[func_id]);
  num_func_calls[func_id] += 1;

  
  inputs.push_back(std::move(new_ctx));
  cur_inputs = &(inputs.back()->inputs);
  cur_carved_ptrs = &(inputs.back()->carved_ptrs);

  return;
}

void __update_carved_ptr_idx() {
  inputs.back()->update_carved_ptr_begin_idx();
}

static void carved_ptr_postprocessing(int begin_idx, int end_idx) {
  int idx1, idx2, idx3, idx4, idx5;
  bool changed = true;
  while (changed) {
    changed = false;
    idx1 = begin_idx;
    while (idx1 < end_idx) {
      int idx2 = idx1 + 1;
      PTR * cur_carved_ptr = cur_carved_ptrs->get(idx1);
      char * addr1 = (char*) cur_carved_ptr->addr;
      int size1 = cur_carved_ptr->alloc_size;
      if (size1 == 0) { idx1++; continue; }
      char * end_addr1 = addr1 + size1;

      while (idx2 < end_idx) {
        PTR * cur_carved_ptr2 = cur_carved_ptrs->get(idx2);
        char * addr2 = (char*) cur_carved_ptr2->addr;
        int size2 = cur_carved_ptr2->alloc_size;
        if (size2 == 0) { idx2++; continue; }
        char * end_addr2 = addr2 + size2;
        int offset = -1;
        int remove_ptr_idx;
        int replacing_ptr_idx;
        PTR * remove_ptr;
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
          //remove remove_ptr in inputs;
          int idx3 = 0;
          int num_inputs = cur_inputs->size();
          while (idx3 < num_inputs) {
            IVAR * tmp_input = *(cur_inputs->get(idx3));
            if (tmp_input->type == INPUT_TYPE::POINTER) {
              VAR<int> * tmp_inputt = (VAR<int> *) tmp_input;
              if (tmp_inputt->input == remove_ptr_idx) {
                int old_offset = tmp_inputt->pointer_offset;
                tmp_inputt->input = replacing_ptr_idx;
                tmp_inputt->pointer_offset = offset + old_offset;
                if (old_offset == 0) {
                  //remove element carved results
                  char * var_name = tmp_input->name;
                  size_t var_name_len = strlen(var_name);
                  char * check_name = (char *) malloc(var_name_len + 2);
                  memcpy(check_name, var_name, var_name_len);
                  check_name[var_name_len] = '[';
                  check_name[var_name_len + 1] = 0;
                  int idx4 = idx3 + 1;
                  while (idx4 < num_inputs) {
                    IVAR * next_input = *(cur_inputs->get(idx4));
                    if (strncmp(check_name, next_input->name, var_name_len + 1) != 0) {
                      break;
                    }
                    idx4++;
                  }
                  free(check_name);
                  int idx5 = idx3 + 1;
                  while (idx5 < idx4) {
                    delete *(cur_inputs->get(idx3 + 1));
                    cur_inputs->remove(idx3 + 1);
                    idx5++;
                  }
                  num_inputs = cur_inputs->size();
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

      if (changed) break;
      idx1++;
    }
  }
  return ;
}

void __carv_func_ret_probe(char * func_name, int func_id) {
  class FUNC_CONTEXT * cur_context = inputs.back();
  inputs.pop_back();
  int idx = 0;
  const int cur_carving_index = cur_context->carving_index;
  const int cur_func_call_idx = cur_context->func_call_idx;  
  const int num_carved_ptrs = cur_carved_ptrs->size();
  const int carved_ptrs_init_idx = cur_context->carved_ptr_begin_idx;

  //check memory overlap
  carved_ptr_postprocessing(0, carved_ptrs_init_idx);
  carved_ptr_postprocessing(carved_ptrs_init_idx, num_carved_ptrs);
  const int num_inputs = cur_inputs->size();
  
  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%d_%d", outdir_name, func_name
    , cur_carving_index, cur_func_call_idx);
  
  std::ofstream outfile(outfile_name);
  //Write carved pointers
  idx = 0;  
  while (idx < num_carved_ptrs) {
    PTR * carved_ptr = cur_carved_ptrs->get(idx);
    outfile << idx << ":" << carved_ptr->addr << ":" << carved_ptr->alloc_size << "\n";
    idx++;
  }

  outfile << "####\n";

  idx = 0;
  while (idx < num_inputs) {
    IVAR * elem = *(cur_inputs->get(idx));
    if (elem->type == INPUT_TYPE::CHAR) {
      outfile << elem->name << ":CHAR:"
              << (int) (((VAR<char>*) elem)->input) << "\n";
    } else if (elem->type == INPUT_TYPE::SHORT) {
      outfile << elem->name << ":SHORT:"
              << ((VAR<short>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::INT) {
      outfile << elem->name << ":INT:"
              << ((VAR<int>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::LONG) {
      outfile << elem->name << ":LONG:"
              << ((VAR<long>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::LONGLONG) {
      outfile << elem->name << ":lONGLONG:"
              << ((VAR<long long>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::FLOAT) {
      outfile << elem->name << ":FLOAT:"
              << ((VAR<float>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::DOUBLE) {
      outfile << elem->name << ":SHORT:"
              << ((VAR<double>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::NULLPTR) {
      outfile << elem->name << ":NULL:0\n";
    } else if (elem->type == INPUT_TYPE::POINTER) {
      VAR<int> * input = (VAR<int>*) elem;
      outfile << elem->name << ":PTR:" << input->input << ":"
              << input->pointer_offset << "\n";
    } else if (elem->type == INPUT_TYPE::FUNCPTR) {
      VAR<char *> * input = (VAR<char *>*) elem;
      outfile << elem->name << ":FUNCPTR:" << input->input << "\n";
    } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
      void * addr = ((VAR<void *>*) elem)->input;
      
      //address might be the end point of carved pointers
      int carved_idx = 0;
      int offset;
      while (carved_idx < num_carved_ptrs) {
        PTR * carved_ptr = cur_carved_ptrs->get(carved_idx);
        char * end_addr = (char *) carved_ptr->addr + carved_ptr->alloc_size;
        if (end_addr == addr) {
          offset = carved_ptr->alloc_size;
          break;
        } 
        carved_idx++;
      }

      if (carved_idx == num_carved_ptrs) {
        outfile << elem->name << ":UNKNOWN_PTR:"
          << ((VAR<void *>*) elem)->input << "\n";
      } else {
        outfile << elem->name << ":PTR:" << carved_idx << ":"
          << offset << "\n";
      }
    }

    delete elem;
    idx++;
  }

  int filesize = (int) outfile.tellp();
  outfile.close();

  if ((filesize <= 64) || (filesize > 1048576)) {
    //remove(outfile_name);
  } else {
    int tmp = 128;
    int index = 0;
    while (filesize > tmp) {
      tmp *= 2;
      index += 1;
    }

    if (func_carved_filesize[func_id] == 0) {
      func_carved_filesize[func_id] = (int *) calloc(20, sizeof(int));
    }

    if (func_carved_filesize[func_id][index] >= 8) {
      remove(outfile_name);
    } else {
      func_carved_filesize[func_id][index] += 1;
    }
  }

  //delete cur_context;
  class FUNC_CONTEXT * next_ctx = inputs.back();
  if (next_ctx == NULL) {
    cur_inputs = NULL;
    cur_carved_ptrs = NULL;
  } else {
    cur_inputs = &(next_ctx->inputs);
    cur_carved_ptrs = &(next_ctx->carved_ptrs);
  }
  
  return;
}

void __carver_argv_modifier(int * argcptr, char *** argvptr) {
  int argc = (*argcptr) - 1;
  *argcptr = argc;

  outdir_name = (*argvptr)[argc];

  (*argvptr)[argc] = 0;
  __mem_allocated_probe(*argvptr, sizeof(char *) * argc);
  int idx;
  for (idx = 0; idx < argc; idx++) {
    char * argv_str = (*argvptr)[idx];
    __mem_allocated_probe(argv_str, strlen(argv_str) + 1);
  }

  num_func_calls_size = 256;
  num_func_calls = (int *) calloc(num_func_calls_size, sizeof(int));
  func_carved_filesize = (int **) calloc(num_func_calls_size, sizeof(int *));
  callseq_size = 16384;
  callseq = (int *) malloc(callseq_size * sizeof(int));
  callseq_index = 0;

  //Write argc, argv values, TODO


  return;
}

void __carv_FINI() {
  char buffer[256];
  snprintf(buffer, 256, "%s/call_seq", outdir_name);
  FILE * __call_seq_file = fopen(buffer, "w");
  if (__call_seq_file == NULL) {
    return;
  }
  fwrite(callseq, sizeof(int), callseq_index, __call_seq_file);
  fclose(__call_seq_file);
}

#endif