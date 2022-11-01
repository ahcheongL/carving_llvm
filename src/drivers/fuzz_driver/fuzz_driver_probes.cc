#include "utils.hpp"

extern vector<POINTER> __replay_carved_ptrs;
extern map<char *, classinfo> __replay_class_info;
extern map<int, char> __replay_replayed_ptr;

static vector<void *> func_ptr_index;

static FILE * input_fp = NULL;

#define MAX_NUM_PTRS 1000

void __driver_inputfb_open(char * inputfname) {
  input_fp = fopen(inputfname, "rb");
  if (input_fp == NULL) {
    fprintf(stderr, "Can't read input file\n");
    std::abort();
  }

  int num_ptrs = 0;
  if (!fread(&num_ptrs, sizeof(int), 1, input_fp)) {
    return;
  }

  num_ptrs = num_ptrs % MAX_NUM_PTRS;

  int index;
  for (index = 0; index < num_ptrs; index++) {
    int size = 0;
    if (!fread(&size, sizeof(int), 1, input_fp)) {
      return;
    }
    void * new_addr = malloc(size);
    POINTER ptr {new_addr, size};
    __replay_carved_ptrs.push_back(ptr);
  }

  return;
}

void __record_func_ptr_index(void * ptr) {
  func_ptr_index.push_back(ptr);
}

char Replay_char2() {
  char val;
  if (!fread(&val, sizeof(char), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

short Replay_short2() {
  short val;
  if (!fread(&val, sizeof(short), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

int Replay_int2() {
  int val;
  if (!fread(&val, sizeof(int), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

long Replay_long2() {
  long val;
  if (!fread(&val, sizeof(long), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

float Replay_float2() {
  float val;
  if (!fread(&val, sizeof(float), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

double Replay_double2() {
  double val;
  if (!fread(&val, sizeof(double), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

long long Replay_longlong2() {
  long long val;
  if (!fread(&val, sizeof(long long), 1, input_fp)) {
    return 0;
  }
  
  return val;
}

extern int __replay_cur_alloc_size;
extern int __replay_cur_class_index;
extern int __replay_cur_pointee_size;

void * Replay_pointer2(int default_idx, int default_pointee_size, char * pointee_type_name) {
  int ptr_idx;
  if (!fread(&ptr_idx, sizeof(int), 1, input_fp)) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return NULL;
  }

  if (ptr_idx == -1) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return NULL;
  }

  int num_carved_ptrs = __replay_carved_ptrs.size();
  ptr_idx = ptr_idx % (num_carved_ptrs * 2);

  if (ptr_idx >= num_carved_ptrs) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return NULL;
  }

  int offset;
  if (!fread(&offset, sizeof(int), 1, input_fp)) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return NULL;
  }

  POINTER * carved_ptr = __replay_carved_ptrs[ptr_idx];

  if (offset != 0) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *) carved_ptr->addr + offset;
  }

  char * search = __replay_replayed_ptr.find(ptr_idx);
  if (search != NULL) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return carved_ptr->addr;
  }
  __replay_replayed_ptr.insert(ptr_idx, 0);

  __replay_cur_alloc_size = carved_ptr->alloc_size;
  __replay_cur_pointee_size = default_pointee_size;
  __replay_cur_class_index = default_idx;

  return carved_ptr->addr;
}

void * Replay_func_ptr2() {
  char val[sizeof(int) + 1];
  if (!fread(val, sizeof(int), 1, input_fp)) {
    return NULL;
  }

  int func_index = (*(int *)val) % func_ptr_index.size();
  void * func_ptr = *func_ptr_index.get(func_index);

  return func_ptr;
}