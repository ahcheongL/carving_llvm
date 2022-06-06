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

static int carved_index = 1;

static map<void *, char *> func_ptrs;
static vector<vector<IVAR *> *> inputs;
static vector<int> carving_index;
static vector<int> func_call_idx;
static vector<vector<PTR> *> carved_ptrs;
static vector<int> carved_ptr_idx;
static map<void *, int> alloced_ptrs;

//names
static vector<char * > __carv_base_names;

void Carv_char(char input) {
  VAR<char> * inputv = new VAR<char>(input
    , *__carv_base_names.back(), INPUT_TYPE::CHAR);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_short(short input) {
  VAR<short> * inputv = new VAR<short>(input
    , *__carv_base_names.back(), INPUT_TYPE::SHORT);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_int(int input) {
  VAR<int> * inputv = new VAR<int>(input
    , *__carv_base_names.back(), INPUT_TYPE::INT);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_longtype(long input) {
  VAR<long> * inputv = new VAR<long>(input
    , *__carv_base_names.back(), INPUT_TYPE::LONG);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_longlong(long long input) {
  VAR<long long> * inputv = new VAR<long long>(input
    , *__carv_base_names.back(), INPUT_TYPE::LONGLONG);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_float(float input) {
  VAR<float> * inputv = new VAR<float>(input
    , *__carv_base_names.back(), INPUT_TYPE::FLOAT);
  (*inputs.back())->push_back((IVAR *) inputv);
}

void Carv_double(double input) {
  VAR<double> * inputv = new VAR<double>(input
    , *__carv_base_names.back(), INPUT_TYPE::DOUBLE);
  (*inputs.back())->push_back((IVAR *) inputv);
}

int Carv_pointer(void * ptr) {
  char * updated_name = *(__carv_base_names.back());
  if (ptr == NULL) {
    VAR<void *> * inputv = new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    (*inputs.back())->push_back((IVAR *) inputv);
    return 0;
  }

  //Find already carved ptr
  vector<PTR> * cur_carved_ptrs = *(carved_ptrs.back());
  int index = *(carved_ptr_idx.back());
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
      (*inputs.back())->push_back((IVAR *) inputv);
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
      (*inputs.back())->push_back((IVAR *) inputv);

      return size;
    }
    index ++;
  }

  VAR<void *> * inputv = new VAR<void *>(
        ptr, updated_name, INPUT_TYPE::UNKNOWN_PTR);
  (*inputs.back())->push_back((IVAR *) inputv);
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
      std::cerr << "Warn : Unknown func ptr : " << updated_name << "\n";
    }
    VAR<void *> * inputv = new VAR<void *>(NULL
      , updated_name, INPUT_TYPE::NULLPTR);
    (*inputs.back())->push_back((IVAR *) inputv);
    return;
  }

  VAR<char *> * inputv = new VAR<char *> (*search
    , updated_name, INPUT_TYPE::FUNCPTR);
  (*inputs.back())->push_back(inputv);
  return;
}

void __carv_ptr_name_update(int idx) {
  char * base_name = *(__carv_base_names.back());
  char * update_name = (char *) malloc(sizeof(char) * 512);
  snprintf(update_name, 512, "%s[%d]",base_name, idx);
  __carv_base_names.push_back(update_name);
  return;
}

void __carv_name_push(char * name) {
  __carv_base_names.push_back(strdup(name));
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
  __carv_base_names.push_back(update_name);
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

  num_func_calls[func_id] += 1;

  inputs.push_back(new vector<IVAR *>());
  carved_ptrs.push_back(new vector<PTR>());
  carving_index.push_back(carved_index++);
  func_call_idx.push_back(num_func_calls[func_id]);
  carved_ptr_idx.push_back(0);
  return;
}

void __update_carved_ptr_idx() {
  *(carved_ptr_idx.back()) = (*(carved_ptrs.back()))->size();
}

void __carv_func_ret_probe(char * func_name, int func_id) {
  vector<IVAR * > * cur_inputs = *(inputs.back());
  inputs.pop_back();
  vector<PTR> * cur_carved_ptrs = *(carved_ptrs.back());
  carved_ptrs.pop_back();
  int cur_carving_index = *(carving_index.back());
  carving_index.pop_back();
  int cur_func_call_idx = *(func_call_idx.back());
  func_call_idx.pop_back();
  carved_ptr_idx.pop_back();

  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%d_%d", outdir_name, func_name
    , cur_carving_index, cur_func_call_idx);
  
  std::ofstream outfile(outfile_name);
  //Write carved pointers
  int idx = 0;
  int num_carved_ptrs = cur_carved_ptrs->size();
  while (idx < num_carved_ptrs) {
    PTR * carved_ptr = cur_carved_ptrs->get(idx);
    outfile << idx << ":" << carved_ptr->addr << ":" << carved_ptr->alloc_size << "\n";
    idx++;
  }

  outfile << "####\n";

  idx = 0;
  int num_inputs = cur_inputs->size();
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
    return;
  }

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

  delete cur_inputs;
  delete cur_carved_ptrs;
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

  //Write argc, argv values


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