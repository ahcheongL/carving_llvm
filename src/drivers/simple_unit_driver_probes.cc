#include "utils.hpp"

static map<char *, classinfo> class_info;

static map<void *, struct typeinfo> alloced_ptrs;

//Function pointer names
static map<void *, char *> func_ptrs;

static vector<IVAR *> inputs;
static vector<PTR> carved_ptrs;

static map<int, char> replayed_ptr;

void __driver_inputf_open(char ** argv) {
  char * inputfilename = argv[1];
  FILE * input_fp = fopen(inputfilename, "r");
  if (input_fp == NULL) {
    //fprintf(stderr, "Can't read input file\n");
    std::abort();
  }

  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  bool is_carved_ptr = true;
  while ((read = getline(&line, &len, input_fp)) != -1) {
    if (is_carved_ptr) {
      if (line[0] == '#') {
        is_carved_ptr = false;
      } else {
        char * addr_str = strchr(line, ':');
        if (addr_str == NULL) { 
          //fprintf(stderr, "Invalid input file\n");
          //std::abort();
        }
        char * size_str = strchr(addr_str + 1, ':');
        if (size_str == NULL) { 
          //fprintf(stderr, "Invalid input file\n");
          //std::abort();
        }
        char * type_str = strchr(size_str + 1, ':');
        if (type_str == NULL) { 
          //fprintf(stderr, "Invalid input file\n");
          //std::abort();
        }

        *type_str = 0;
        len = strlen(type_str + 1);
        type_str[len] = 0;

        int ptr_size = atoi(size_str + 1);

        void * new_ptr = malloc(ptr_size);

        carved_ptrs.push_back(PTR(new_ptr, strdup(type_str + 1), ptr_size));
      }
    } else {
      char * type_str = strchr(line, ':');
      if (type_str == NULL) { 
        //fprintf(stderr, "Invalid input file\n");
        //std::abort();
      }
      char * value_str = strchr(type_str + 1, ':');
      if (value_str == NULL) { 
        //fprintf(stderr, "Invalid input file\n");
        //std::abort();
      }
      char * index_str = strchr(value_str + 1, ':');

      *value_str = 0;

      type_str += 1;
      if (!strncmp(type_str, "CHAR", 4)) {
        char value = atoi(value_str + 1);
        VAR<char> * inputv = new VAR<char>(value, 0, INPUT_TYPE::CHAR);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "SHORT", 5)) {
        short value = atoi(value_str + 1);
        VAR<short> * inputv = new VAR<short>(value, 0, INPUT_TYPE::SHORT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "INT", 3)) {
        int value = atoi(value_str + 1);
        VAR<int> * inputv = new VAR<int>(value, 0, INPUT_TYPE::INT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "LONG", 4)) {
        long value = atol(value_str + 1);
        VAR<long> * inputv = new VAR<long>(value, 0, INPUT_TYPE::LONG);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "LONGLONG", 8)) {
        long long value = atoll(value_str + 1);
        VAR<long long> * inputv = new VAR<long long>(value, 0, INPUT_TYPE::LONGLONG);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "FLOAT", 5)) {
        float value = atof(value_str + 1);
        VAR<float> * inputv = new VAR<float>(value, 0, INPUT_TYPE::FLOAT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "DOUBLE", 6)) {
        double value = atof(value_str + 1);
        VAR<double> * inputv = new VAR<double>(value, 0, INPUT_TYPE::DOUBLE);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "NULL", 4)) {
        VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::NULLPTR);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "FUNCPTR", 7)) {
        char * func_name = value_str + 1;
        len = strlen(func_name);
        func_name[len - 1] = 0;
        int idx = 0;
        int num_funcs = func_ptrs.size();
        for (idx = 0; idx < num_funcs; idx++) {
          auto data = func_ptrs.get_by_idx(idx);
          if (!strcmp(func_name, data->elem)) {
            VAR<void *> * inputv = new VAR<void *>(data->key, 0, INPUT_TYPE::FUNCPTR);
            inputs.push_back((IVAR *) inputv);
            break;
          }
        }

        if (idx == num_funcs) {
          //fprintf(stderr, "Replay error : Can't get function name : %s\n", func_name);
          VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::FUNCPTR);
          inputs.push_back((IVAR *) inputv);
        }
      } else if (!strncmp(type_str, "PTR", 3)) {
        if (index_str == NULL) {
          //fprintf(stderr, "Invalid input file\n");
          //std::abort();
        }

        int ptr_index = atoi(value_str + 1);
        int ptr_offset = atoi(index_str + 1);
        VAR<int> * inputv = new VAR<int>(ptr_index, 0, ptr_offset, INPUT_TYPE::POINTER);
        inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "UNKNOWN_PTR", 11)) {
        VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::UNKNOWN_PTR);
        inputs.push_back((IVAR *) inputv);
      } else {
        //fprintf(stderr, "Invalid input file\n");
        //std::abort();
      }
    }
  }

  if (line) { free(line); }
  fclose(input_fp);
  return;
}

static int cur_input_idx = 0;

char Replay_char() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::CHAR)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  
  return ((VAR<char> *) elem_ptr)->input;
}

short Replay_short() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::SHORT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<short> *) elem_ptr)->input;
}

int Replay_int() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::INT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<int> *) elem_ptr)->input;
}

long Replay_longtype() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::LONG)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;

  return ((VAR<long> *) elem_ptr)->input;
}

long long Replay_longlong() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::LONGLONG)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<long long> *) elem_ptr)->input;
}

float Replay_float() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::FLOAT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<float> *) elem_ptr)->input;
}

double Replay_double() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::DOUBLE)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<double> *) elem_ptr)->input;
}

static int cur_alloc_size = 0;
int __replay_cur_class_index = -1;
int __replay_cur_pointee_size = -1;

void * Replay_pointer (int default_idx, int default_pointee_size, char * pointee_type_name) {

  auto elem = inputs[cur_input_idx++];
  if (elem == NULL) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  IVAR * elem_ptr = *elem;

  if (elem_ptr->type == INPUT_TYPE::NULLPTR) {
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  if (elem_ptr->type == INPUT_TYPE::UNKNOWN_PTR) {
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  if (elem_ptr->type != INPUT_TYPE::POINTER) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
    //std::abort();
  }

  VAR<int> * elem_v = (VAR<int> *) elem_ptr;
  int ptr_index = elem_v->input;
  int ptr_offset = elem_v->pointer_offset;
  
  PTR * carved_ptr = carved_ptrs[ptr_index];

  if (ptr_offset != 0) {
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *) carved_ptr->addr + ptr_offset;
  }

  char * search = replayed_ptr.find(ptr_index);
  if (search != NULL) {
    cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *) carved_ptr->addr + ptr_offset;
  }
  replayed_ptr.insert(ptr_index, 0);

  cur_alloc_size = carved_ptr->alloc_size;

  if (pointee_type_name == NULL) {
    return (char *) carved_ptr->addr + ptr_offset;
  }

  const char * type_name = carved_ptr->pointee_type;

  __replay_cur_pointee_size = default_pointee_size;
  __replay_cur_class_index = default_idx;

  if (!strcmp(type_name, pointee_type_name)) {
    return (char *) carved_ptr->addr + ptr_offset;
  }

  // carved ptr has different type
  int num_class_info = class_info.size();
  int idx = 0;
  for (idx = 0; idx < num_class_info; idx++) {
    auto class_info_elem = class_info.get_by_idx(idx);
    if (!strcmp(type_name, class_info_elem->key)) {
      __replay_cur_pointee_size = class_info_elem->elem.size;
      __replay_cur_class_index = class_info_elem->elem.class_index;
      break;
    }
  }

  return (char *) carved_ptr->addr + ptr_offset;
}

int Replay_ptr_alloc_size() {
  return cur_alloc_size;
}

void * Replay_func_ptr() {
  auto elem = inputs[cur_input_idx++];

  if ((elem == NULL)
    || (((*elem)->type != INPUT_TYPE::FUNCPTR)
        &&  ((*elem)->type != INPUT_TYPE::NULLPTR))) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<void *> *) elem_ptr)->input;
}

void __keep_class_info(char * class_name, int size, int index) {
  classinfo tmp {index, size};
  class_info.insert(class_name, tmp);
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.insert(ptr, name);
}

char * __update_class_ptr(char * ptr, int idx, int size) {
  return ptr + (idx * size);
}

void __replay_fini() {
  //TODO free
}