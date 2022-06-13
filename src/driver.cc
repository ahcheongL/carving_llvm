#include "utils.hpp"

static vector<PTR> carved_ptrs;

static vector<int> replayed_index;

static vector<VAR<int>> ptr_inputs;
static vector<void *> func_ptr_inputs;

//assert(func_ptr.size() == funcnames.size())
static vector<void *> func_ptrs;
static vector<char *> funcnames;

static vector<const char *> class_names;
static map<const char *, int> class_size;


static FILE * input_fp;

void __driver_inputf_open(char ** argv) {
  char * inputfilename = argv[1];
  input_fp = fopen(inputfilename, "r");
  if (input_fp == NULL) {
    fprintf(stderr, "Can't read input file\n");
    std::abort();
  }
  return;
}

static int cur_input_idx = 0;

char Replay_char() {
  char buf[sizeof(char) + 1];
  if(!fread(buf, sizeof(char), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

short Replay_short() {
  short buf[sizeof(short) + 1];
  if(!fread(buf, sizeof(short), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

int Replay_int() {
  int buf[sizeof(int) + 1];
  if(!fread(buf, sizeof(int), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

long Replay_longtype() {
  long buf[sizeof(long) + 1];
  if(!fread(buf, sizeof(long), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

long long Replay_longlong() {
  long long buf[sizeof(long long) + 1];
  if(!fread(buf, sizeof(long long), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

float Replay_float() {
  float buf[sizeof(float) + 1];
  if(!fread(buf, sizeof(float), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

double Replay_double() {
  double buf[sizeof(double) + 1];
  if(!fread(buf, sizeof(double), 1, input_fp))
  {
      return 0;
  } else {
      return buf[0];
  }
}

static int cur_class_index;
static int cur_class_size;
static int cur_pointer_size;
static int cur_ptr_idx = 0;

void * Replay_pointer(char * default_class_name) {

  cur_class_index = -1;
  cur_class_size = 0;
  cur_pointer_size = 0;

  if (cur_ptr_idx >= ptr_inputs.size()) { return 0; }

  VAR<int> * cur_input = ptr_inputs[cur_ptr_idx++];
  
  int carved_index = cur_input->input;
  
  if (carved_index >= carved_ptrs.size()) { return 0; }

  PTR * carved_ptr = carved_ptrs[carved_index];

  char * ret_ptr = ((char *) carved_ptr->addr)
    + cur_input->pointer_offset;

  if (cur_input->pointer_offset == 0) {
    int num_already_replayed = replayed_index.size();
    int replay_index = 0;
    while (replay_index < num_already_replayed) {
      if ((*(replayed_index[replay_index])) ==  carved_index) {
        return ret_ptr;
      }
      replay_index++;
    }

    const char * pointee_type = carved_ptr->pointee_type;

    int * class_size_ptr = class_size[pointee_type];

    if (class_size_ptr != NULL) {
      cur_class_index = class_names.get_idx(pointee_type);
      cur_class_size = *class_size_ptr;
    } else if (default_class_name != NULL) {
      cur_class_index = class_names.get_idx(default_class_name);
      cur_class_size = *class_size[default_class_name];
    }

    cur_pointer_size
      = carved_ptr->alloc_size;
    replayed_index.push_back(carved_index);
  }

  return ret_ptr;
}

int Replay_ptr_alloc_size() {
  return cur_pointer_size;
}

int Replay_ptr_class_index() {
  return cur_class_index;
}

int Replay_ptr_class_size() {
  return cur_class_size;
}

static int func_ptr_index = 0;
void * Replay_func_ptr() {
  if (func_ptr_index >= func_ptr_inputs.size()) {
    return 0;
  }

  void * func_ptr = *func_ptr_inputs[func_ptr_index++];
  return func_ptr;
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.push_back(ptr);
  funcnames.push_back(name);
}

void __receive_carved_ptr(int size, char * pointee_type) {
  void * ptr = malloc(size);
  carved_ptrs.push_back(PTR(ptr, pointee_type, size));
}

void __receive_ptr_shape(int ptr_index, int offset) {
  char * mock_name = (char *) malloc(sizeof(char) * 2);
  mock_name[0] = 'a';
  mock_name[1] = 0;
  ptr_inputs.push_back(VAR<int> (ptr_index, mock_name, offset, INPUT_TYPE::POINTER));
}

void __receive_func_ptr(char * funcname) {
  int idx = 0;
  int size = funcnames.size();
  for (idx = 0; idx < size; idx++) {
    if (funcname == (*funcnames[idx])) {
      break;
    }
  }

  if (idx == size) {
    func_ptr_inputs.push_back(NULL);
  } else {
    func_ptr_inputs.push_back(*func_ptrs.get(idx));
  }
}

char * __update_class_ptr(char * ptr, int idx, int size) {
  return ptr + (idx * size);
}

void __keep_class_name(char * name, int size) {
  class_names.push_back(name);
  class_size.insert(name, size);
}

void __replay_fini() {
  fclose(input_fp);
  int idx = 0;
  int num_carved_ptrs = carved_ptrs.size();
  for (idx = 0; idx < num_carved_ptrs; idx++) {
    free(carved_ptrs[idx]->addr);
  }
}