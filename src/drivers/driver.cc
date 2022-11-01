#include "utils.hpp"

map<char *, classinfo> __replay_class_info;

//Function pointer names
static map<void *, char *> __replay_func_ptrs;

vector<IVAR *> __replay_inputs;
vector<POINTER> __replay_carved_ptrs;

//adhoc to make a set
map<int, char> __replay_replayed_ptr;

void __driver_inputf_open(char * inputfilename);

void __driver_input_modifier(int * argcptr, char *** argvptr) {

  int argc = *argcptr - 1;
  *argcptr = argc;

  char * inputf_name = (*argvptr)[argc];

  __driver_inputf_open(inputf_name);

  (*argvptr)[argc] = 0;
}

void __driver_inputf_open(char * inputfilename) {
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

        __replay_carved_ptrs.push_back(POINTER(new_ptr, strdup(type_str + 1), ptr_size));
      }
    } else {
      char * type_str = strchr(line, ':');
      if (type_str == NULL) { 
        fprintf(stderr, "Invalid input file\n");
        std::abort();
      }
      char * value_str = strchr(type_str + 1, ':');
      if (value_str == NULL) { 
        fprintf(stderr, "Invalid input file\n");
        std::abort();
      }
      char * index_str = strchr(value_str + 1, ':');

      *value_str = 0;

      type_str += 1;
      if (!strncmp(type_str, "CHAR", 4)) {
        char value = atoi(value_str + 1);
        VAR<char> * inputv = new VAR<char>(value, 0, INPUT_TYPE::CHAR);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "SHORT", 5)) {
        short value = atoi(value_str + 1);
        VAR<short> * inputv = new VAR<short>(value, 0, INPUT_TYPE::SHORT);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "INT", 3)) {
        int value = atoi(value_str + 1);
        VAR<int> * inputv = new VAR<int>(value, 0, INPUT_TYPE::INT);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "LONG", 4)) {
        long value = atol(value_str + 1);
        VAR<long> * inputv = new VAR<long>(value, 0, INPUT_TYPE::LONG);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "LONGLONG", 8)) {
        long long value = atoll(value_str + 1);
        VAR<long long> * inputv = new VAR<long long>(value, 0, INPUT_TYPE::LONGLONG);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "FLOAT", 5)) {
        float value = atof(value_str + 1);
        VAR<float> * inputv = new VAR<float>(value, 0, INPUT_TYPE::FLOAT);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "DOUBLE", 6)) {
        double value = atof(value_str + 1);
        VAR<double> * inputv = new VAR<double>(value, 0, INPUT_TYPE::DOUBLE);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "NULLPTR", 7)) {
        VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::NULLPTR);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "FUNCPTR", 7)) {
        char * func_name = value_str + 1;
        len = strlen(func_name);
        func_name[len - 1] = 0;
        int idx = 0;
        int num_funcs = __replay_func_ptrs.size();
        for (idx = 0; idx < num_funcs; idx++) {
          auto data = __replay_func_ptrs.get_by_idx(idx);
          if (!strcmp(func_name, data->elem)) {
            VAR<void *> * inputv = new VAR<void *>(data->key, 0, INPUT_TYPE::FUNCPTR);
            __replay_inputs.push_back((IVAR *) inputv);
            break;
          }
        }

        if (idx == num_funcs) {
          //fprintf(stderr, "Replay error : Can't get function name : %s\n", func_name);
          VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::FUNCPTR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
      } else if (!strncmp(type_str, "PTR", 3)) {
        if (index_str == NULL) {
          //fprintf(stderr, "Invalid input file\n");
          //std::abort();
        }

        int ptr_index = atoi(value_str + 1);
        int ptr_offset = atoi(index_str + 1);
        VAR<int> * inputv = new VAR<int>(ptr_index, 0, ptr_offset, INPUT_TYPE::PTR);
        __replay_inputs.push_back((IVAR *) inputv);
      } else if (!strncmp(type_str, "UNKNOWN_PTR", 11)) {
        VAR<void *> * inputv = new VAR<void *>(0, 0, INPUT_TYPE::UNKNOWN_PTR);
        __replay_inputs.push_back((IVAR *) inputv);
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
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::CHAR)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  
  return ((VAR<char> *) elem_ptr)->input;
}

short Replay_short() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::SHORT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<short> *) elem_ptr)->input;
}

int Replay_int() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::INT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<int> *) elem_ptr)->input;
}

long Replay_longtype() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::LONG)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;

  return ((VAR<long> *) elem_ptr)->input;
}

long long Replay_longlong() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::LONGLONG)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<long long> *) elem_ptr)->input;
}

float Replay_float() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::FLOAT)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<float> *) elem_ptr)->input;
}

double Replay_double() {
  auto elem = __replay_inputs[cur_input_idx++];

  if ((elem == NULL) || ((*elem)->type != INPUT_TYPE::DOUBLE)) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    return 0;
  }

  IVAR * elem_ptr = *elem;
  return ((VAR<double> *) elem_ptr)->input;
}

int __replay_cur_alloc_size = 0;
int __replay_cur_class_index = -1;
int __replay_cur_pointee_size = -1;

void * Replay_pointer (int default_idx, int default_pointee_size, char * pointee_type_name) {

  auto elem = __replay_inputs[cur_input_idx++];
  if (elem == NULL) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    //std::abort();
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  IVAR * elem_ptr = *elem;

  if (elem_ptr->type == INPUT_TYPE::NULLPTR) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  if (elem_ptr->type == INPUT_TYPE::UNKNOWN_PTR) {
    //alloc 1 object
#define ALLOC_1_OBJ
#ifdef ALLOC_1_OBJ
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return malloc(default_pointee_size);
#else
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
#endif
  }

  if (elem_ptr->type != INPUT_TYPE::PTR) {
    //fprintf(stderr, "Replay error : Invalid input type\n");
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  VAR<int> * elem_v = (VAR<int> *) elem_ptr;
  int ptr_index = elem_v->input;
  int ptr_offset = elem_v->pointer_offset;
  
  POINTER * carved_ptr = __replay_carved_ptrs[ptr_index];

  if (ptr_offset != 0) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *) carved_ptr->addr + ptr_offset;
  }

  char * search = __replay_replayed_ptr.find(ptr_index);
  if (search != NULL) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *) carved_ptr->addr + ptr_offset;
  }
  __replay_replayed_ptr.insert(ptr_index, 0);

  __replay_cur_alloc_size = carved_ptr->alloc_size;

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
  int num___replay_class_info = __replay_class_info.size();
  int idx = 0;
  for (idx = 0; idx < num___replay_class_info; idx++) {
    auto __replay_class_info_elem = __replay_class_info.get_by_idx(idx);
    if (!strcmp(type_name, __replay_class_info_elem->key)) {
      __replay_cur_pointee_size = __replay_class_info_elem->elem.size;
      __replay_cur_class_index = __replay_class_info_elem->elem.class_index;
      break;
    }
  }

  return (char *) carved_ptr->addr + ptr_offset;
}

void * Replay_func_ptr() {
  auto elem = __replay_inputs[cur_input_idx++];

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
  __replay_class_info.insert(class_name, tmp);
}

void __record_func_ptr(void * ptr, char * name) {
  __replay_func_ptrs.insert(ptr, name);
}

char * __update_class_ptr(char * ptr, int idx, int size) {
  return ptr + (idx * size);
}

void __replay_fini() {
  //TODO free
}