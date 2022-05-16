#ifndef __CROWN_CARVER_DEF
#define __CROWN_CARVER_DEF

#include "carver.hpp"

static char * outdir_name = NULL;

static int * num_func_calls;
static int num_func_calls_size;

static int ** func_carved_filesize;

static int * callseq;
static int callseq_size;
static int callseq_index;

static std::vector<IVAR *> inputs;
static std::vector<PTR> carved_ptrs;
static std::map<void *, int> alloced_ptrs;
static std::vector<std::pair<void *, int>> array_index;
static std::string put_ptr_index(char * name);

void Carv_char(char input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<char> * inputv = new VAR<char>(input, updated_name, INPUT_TYPE::CHAR);
  inputs.push_back((IVAR *) inputv);
}

void Carv_short(short input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<short> * inputv = new VAR<short>(input, updated_name, INPUT_TYPE::SHORT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_int(int input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<int> * inputv = new VAR<int>(input, updated_name, INPUT_TYPE::INT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_long(long input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<long> * inputv = new VAR<long>(input, updated_name, INPUT_TYPE::LONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_longlong(long long input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<long long> * inputv
    = new VAR<long long>(input, updated_name, INPUT_TYPE::LONGLONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_float(float input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<float> * inputv = new VAR<float>(input, updated_name, INPUT_TYPE::FLOAT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_double(double input, char * name) {
  std::string updated_name = put_ptr_index(name);
  VAR<double> * inputv = new VAR<double>(input, updated_name, INPUT_TYPE::DOUBLE);
  inputs.push_back((IVAR *) inputv);
}

int Carv_pointer(void * ptr, char * name) {
  std::string updated_name = put_ptr_index(name);
  if (ptr == NULL) {
    VAR<void *> * inputv = new VAR<void *>(NULL, updated_name, INPUT_TYPE::NULLPTR);
    inputs.push_back((IVAR *) inputv);
    return 0;
  }

  //Find already carved ptr
  int index = 0;
  for (auto iter = carved_ptrs.begin(); iter != carved_ptrs.end(); iter++) {
    char * carved_addr = (char *) iter->addr;
    if ((carved_addr <= ptr) && (ptr < (carved_addr + iter->alloc_size))) {
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
  for (auto iter = alloced_ptrs.begin(); iter != alloced_ptrs.end(); iter++) {
    char * alloced_ptr = (char *) iter->first;
    if ((alloced_ptr <= ptr) && (ptr < (alloced_ptr + iter->second))) {
      int size = alloced_ptr + iter->second - ((char *) ptr);
      index = carved_ptrs.size();
      carved_ptrs.push_back(PTR(ptr, size));

      VAR<int> * inputv = new VAR<int>(
        index, updated_name, 0, INPUT_TYPE::POINTER);
      inputs.push_back((IVAR *) inputv);

      array_index.push_back(std::make_pair(ptr, 0));
      std::cerr << "size : " << array_index.size() << "\n";

      return size;
    }
  }
  return 0;
}

void __carv_pointer_idx_update(void * ptr) {
  if (array_index.back().first == ptr) {
    array_index.back().second++;
  }
}

void __carv_pointer_done(void * ptr) {
  if (array_index.back().first == ptr) {
    array_index.pop_back();
  }
  std::cerr << "size : " << array_index.size() << "\n";
}

void __mem_allocated_probe(void * ptr, int size) {
  auto search = alloced_ptrs.find(ptr);

  if (search != alloced_ptrs.end()) {
    search->second = size;
    return;
  }

  alloced_ptrs.insert(std::make_pair(ptr, size));
  return;
}

void __remove_mem_allocated_probe(void * ptr) {
  auto search = alloced_ptrs.find(ptr);
  if (search != alloced_ptrs.end()) {
    alloced_ptrs.erase(search);
  }
}

void __carv_init() {
  num_func_calls_size = 256;
  num_func_calls = (int *) calloc(num_func_calls_size, sizeof(int));
  func_carved_filesize = (int **) calloc(num_func_calls_size, sizeof(int *));
  callseq_size = 16384;
  callseq = (int *) malloc(callseq_size * sizeof(int));
  callseq_index = 0;
  return;
}

int carved_index = 0;

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
    memset(func_carved_filesize + tmp, 0, tmp * sizeof(int));
  }

  num_func_calls[func_id] += 1;
  
  //Open file
  std::string outfile_name = std::string(outdir_name) + "/" + func_name
    + "_" + std::to_string(carved_index++) + "_" 
    + std::to_string(num_func_calls[func_id]);

  std::ofstream outfile(outfile_name);  

  //Write carved pointers
  for (auto iter = carved_ptrs.begin(); iter != carved_ptrs.end(); iter++) {
    outfile << iter->addr << ":" << iter->alloc_size << "\n";
  }

  outfile << "####\n";

  for (auto iter = inputs.begin(); iter != inputs.end(); iter++) {
    IVAR * elem = *iter;
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
    }
  }

  int filesize = (int) outfile.tellp();
  outfile.close();

  for (auto iter = inputs.begin(); iter != inputs.end(); iter++) {
    IVAR * elem = *iter;
    delete elem;
  }

  inputs.clear();
  carved_ptrs.clear();

  if ((filesize <= 64) || (filesize > 1048576)) {
    //remove(outfile_name.c_str());
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
    remove(outfile_name.c_str());
  } else {
    func_carved_filesize[func_id][index] += 1;
  } 

  array_index.clear();
  return;
}

static std::string put_ptr_index(char * name) {
  std::string res = std::string(name);
  auto search = res.find("[]");
  std::string ptr_name;
  int depth_index = 0;

  std::cerr << name << "\n";
  while (search != std::string::npos) {
    std::string index_string
      = "[" + std::to_string(array_index[depth_index].second) + "]";

    res.replace(search, 2, index_string);

    search = res.find("[]");
    depth_index++;
  }

  return res;
}

void __argv_modifier(int * argcptr, char *** argvptr) {
  int argc = (*argcptr) - 1;
  *argcptr = argc;

  outdir_name = (*argvptr)[argc];

  (*argvptr)[argc] = 0;
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