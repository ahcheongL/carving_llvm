#include "utils.hpp"

#define MAX_PTR_SIZE 1024

static vector<int> class_size;

static vector<PTR2> alloced_ptrs;

static vector<void *> func_ptrs;

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

static int cur_alloc_size = 0;

void * Replay_pointer_with_given_size(int pointee_size) {
  int coin = Replay_int();
  int coin2 = coin % (MAX_PTR_SIZE * 3);

  if (coin2 > (MAX_PTR_SIZE * 2)) {
    cur_alloc_size = 0;
    return NULL;
  } else if (coin2 > MAX_PTR_SIZE) {
    int num_alloced_ptr = alloced_ptrs.size();
    if (num_alloced_ptr == 0) {
      cur_alloc_size = 0;
      return NULL;
    } else {
      int idx = coin % num_alloced_ptr;
      cur_alloc_size = alloced_ptrs[idx]->alloc_size;
      return alloced_ptrs[idx]->addr;
    }
  } else {
    cur_alloc_size = coin2 * pointee_size;
    char * new_ptr = (char *)malloc(cur_alloc_size);
    alloced_ptrs.push_back(PTR2((void*) new_ptr
      , class_size.size() //char * type id
      , cur_alloc_size));
    return new_ptr;
  }
}

static int cur_class_index = 0;
static int cur_pointee_size = 0;

void * Replay_pointer_check_pointee_type(int default_idx, int default_pointee_size) {
  int coin = Replay_int();
  int coin2 = coin % (MAX_PTR_SIZE * 3);

  if (coin2 > (MAX_PTR_SIZE * 2)) {
    cur_alloc_size = 0;
    return NULL;
  } else if (coin2 > MAX_PTR_SIZE) {
    int num_alloced_ptr = alloced_ptrs.size();
    if (num_alloced_ptr == 0) {
      cur_alloc_size = 0;
      return NULL;
    } else {
      int idx = coin % num_alloced_ptr;
      PTR2 * selected_ptr = alloced_ptrs[idx];
      cur_alloc_size = selected_ptr->alloc_size;
      cur_class_index = selected_ptr->pointee_type_id;
      if (cur_class_index == class_size.size()) {
        cur_pointee_size = 1;
      } else {
        cur_pointee_size = *class_size[cur_class_index];
      }
      return selected_ptr->addr;
    }
  } else {
    int num_class_types = class_size.size();

    int idx = Replay_int();
    idx = idx % (num_class_types* 10);

    if (idx > num_class_types) {
      //use default type id
      cur_alloc_size = coin2 * default_pointee_size;
      char * new_ptr = (char *)malloc(cur_alloc_size);
      cur_pointee_size = default_pointee_size;
      cur_class_index = default_idx;
      alloced_ptrs.push_back(PTR2((void*) new_ptr
        , default_idx, cur_alloc_size));
      return new_ptr;
    } else {
      cur_class_index = idx;
      if (idx == num_class_types) {
        cur_pointee_size = 1;
      } else {
        cur_pointee_size = *class_size[idx];
      }
      cur_alloc_size = coin2 * cur_pointee_size;
      char * new_ptr = (char *)malloc(cur_alloc_size);
      alloced_ptrs.push_back(PTR2((void*) new_ptr, idx, cur_alloc_size));
      return new_ptr;
    }
  }
}

int Replay_ptr_alloc_size() {
  return cur_alloc_size;
}

int Replay_ptr_class_index() {
  return cur_class_index;
}

int Replay_ptr_pointee_size() {
  return cur_pointee_size;
}

void * Replay_func_ptr() {

  int num_func_ptrs = func_ptrs.size() * 2;

  if (num_func_ptrs == 0) { return NULL; }

  int coin = Replay_int();

  int coin2 = coin % num_func_ptrs;

  if (coin2 >= num_func_ptrs) {
    return 0;
  }

  void * func_ptr = *func_ptrs[coin2];
  return func_ptr;
}

void __keep_class_size(int size) {
  class_size.push_back(size);
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.push_back(ptr);
}

char * __update_class_ptr(char * ptr, int idx, int size) {
  return ptr + (idx * size);
}

void __replay_fini() {
  fclose(input_fp);
}