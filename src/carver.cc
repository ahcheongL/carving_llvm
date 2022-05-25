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

static map<void *, char *> func_ptrs;
static vector<IVAR *> inputs;
static vector<PTR> carved_ptrs;
static map<void *, int> alloced_ptrs;
static vector<PTR_IDX> array_index;
static char * put_ptr_index(char * name);

void Carv_char(char input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<char> * inputv = new VAR<char>(input, updated_name, INPUT_TYPE::CHAR);
  inputs.push_back((IVAR *) inputv);
}

void Carv_short(short input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<short> * inputv = new VAR<short>(input, updated_name, INPUT_TYPE::SHORT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_int(int input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<int> * inputv = new VAR<int>(input, updated_name, INPUT_TYPE::INT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_long(long input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<long> * inputv = new VAR<long>(input, updated_name, INPUT_TYPE::LONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_longlong(long long input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<long long> * inputv
    = new VAR<long long>(input, updated_name, INPUT_TYPE::LONGLONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_float(float input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<float> * inputv = new VAR<float>(input, updated_name, INPUT_TYPE::FLOAT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_double(double input, char * name) {
  char * updated_name = put_ptr_index(name);
  VAR<double> * inputv = new VAR<double>(input, updated_name, INPUT_TYPE::DOUBLE);
  inputs.push_back((IVAR *) inputv);
}

int Carv_pointer(void * ptr, char * name) {
  char * updated_name = put_ptr_index(name);
  if (ptr == NULL) {
    VAR<void *> * inputv = new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    inputs.push_back((IVAR *) inputv);
    return 0;
  }

  //Find already carved ptr
  int index = 0;
  int num_carved_ptrs = carved_ptrs.size();
  while (index < num_carved_ptrs) {
    PTR * carved_ptr = carved_ptrs[index];
    char * carved_addr = (char *) carved_ptr->addr;
    int carved_ptr_size = carved_ptr->alloc_size;
    if ((carved_addr <= ptr) && (ptr < (carved_addr + carved_ptr_size))) {
      int offset = ((char *) ptr) - carved_addr;
      VAR<int> * inputv = new VAR<int>(
        index, updated_name, offset, INPUT_TYPE::POINTER);
      inputs.push_back((IVAR *) inputv);
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
    if ((alloced_addr <= ptr) && (ptr < (alloced_addr + alloced_size))) {
      int size = alloced_addr + alloced_size - ((char *) ptr);
      int new_carved_ptr_index = carved_ptrs.size();
      carved_ptrs.push_back(PTR(ptr, size));

      std::cerr << "new ptr input : " << ptr << " : " << size << "\n";
      VAR<int> * inputv = new VAR<int>(
        new_carved_ptr_index, updated_name, 0, INPUT_TYPE::POINTER);
      inputs.push_back((IVAR *) inputv);

      array_index.push_back(PTR_IDX(ptr, 0));
    
      return size;
    }
    index ++;
  }

  VAR<void *> * inputv = new VAR<void *>(
        ptr, updated_name, INPUT_TYPE::UNKNOWN_PTR);
  inputs.push_back((IVAR *) inputv);
  return 0;
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.insert(ptr, name);
}

void __Carv_func_ptr(void * ptr, char * varname) {
  char * updated_name = put_ptr_index(varname);
  auto search = func_ptrs.find(ptr);
  if ((ptr == NULL) || (search == NULL)) {
    if (ptr != NULL) {
      std::cerr << "Warn : Unknown func ptr : " << varname << "\n";
    }
    VAR<void *> * inputv = new VAR<void *>(NULL
      , updated_name, INPUT_TYPE::NULLPTR);
    inputs.push_back((IVAR *) inputv);
    return;
  }

  VAR<char *> * inputv = new VAR<char *> (*search
    , updated_name, INPUT_TYPE::FUNCPTR);
  inputs.push_back(inputv);
  return;
}

void __carv_pointer_idx_update(void * ptr) {
  if (array_index.size() == 0) return;

  if (array_index.back()->addr == ptr) {
    array_index.back()->index++;
  }
}

void __carv_pointer_done(void * ptr) {
  if (array_index.size() == 0) return;
  
  if (array_index.back()->addr == ptr) {
    array_index.pop_back();
  }
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

//Insert at the begining of 
void __write_carved(char * func_name, int func_id) {
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
  
  //Open file
  char outfile_name[256];
  snprintf(outfile_name, 256, "%s/%s_%d_%d", outdir_name, func_name
    , carved_index++, num_func_calls[func_id]);

  std::ofstream outfile(outfile_name);  

  //Write carved pointers
  int idx = 0;
  int num_carved_ptrs = carved_ptrs.size();
  while (idx < num_carved_ptrs) {
    PTR * carved_ptr = carved_ptrs[idx];
    outfile << idx << ":" << carved_ptr->addr << ":" << carved_ptr->alloc_size << "\n";
    idx++;
  }

  outfile << "####\n";

  idx = 0;
  int num_inputs = inputs.size();
  while (idx < num_inputs) {
    IVAR * elem = *(inputs[idx]);
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
      outfile << elem->name << ":NULL\n";
    } else if (elem->type == INPUT_TYPE::POINTER) {
      VAR<int> * input = (VAR<int>*) elem;
      outfile << elem->name << ":PTR:" << input->input << ":"
              << input->pointer_offset << "\n";
    } else if (elem->type == INPUT_TYPE::FUNCPTR) {
      VAR<char *> * input = (VAR<char *>*) elem;
      outfile << elem->name << ":FUNCPTR:" << input->input << "\n";
    } else if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
      outfile << elem->name << ":UNKNOWN_PTR:" << ((VAR<void *>*) elem)->input << "\n";
    }

    delete elem;
    idx++;
  }

  int filesize = (int) outfile.tellp();
  outfile.close();

  inputs.clear();
  carved_ptrs.clear();
  array_index.clear();

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

  return;
}

static char * put_ptr_index(char * name) {
  char * buf = (char *) malloc(sizeof(char) * 256);

  strncpy(buf, name, 256);

  char * search = strstr(buf, "[]");
  int depth_index = 0;

  while (search != NULL) {
    int carving_index = 0;
    if (depth_index < array_index.size()) {
      carving_index = array_index[depth_index]->index;
    }

    char * tmp = strdup(search + 2);
    snprintf(search, 256 - (search - buf), "[%d]%s", carving_index, tmp);
    free(tmp);

    search = strstr(buf, "[]");
    depth_index++;
  }

  return buf;
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