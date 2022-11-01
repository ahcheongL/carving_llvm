#include "utils.hpp"

extern vector<IVAR *> __replay_inputs;
extern vector<POINTER> __replay_carved_ptrs;

static vector<void *> func_ptr_index;

void __driver_inputfb_open(char * inputfname) {
  FILE * input_fp = fopen(inputfname, "rb");
  if (input_fp == NULL) {
    fprintf(stderr, "Can't read input file\n");
    std::abort();
  }

  int num_ptrs = 0;
  if (!fread(&num_ptrs, sizeof(int), 1, input_fp)) {
    return;
  }

  int index;
  for (index = 0; index < num_ptrs; index++) {
    fseek(input_fp, sizeof(void *), SEEK_CUR);
    int size = 0;
    if (!fread(&size, sizeof(int), 1, input_fp)) {
      return;
    }
    void * new_addr = malloc(size);
    POINTER ptr {new_addr, size};
    __replay_carved_ptrs.push_back(ptr);
  }

  int num_inputs = 0;
  if (!fread(&num_inputs, sizeof(int), 1, input_fp)) {
    return;
  }

  int num_func = func_ptr_index.size();

  for (index = 0; index < num_inputs; index++) {
    char type_id = 0;
    if (!fread(&type_id, sizeof(char), 1, input_fp)) {
      return;
    }

    type_id = type_id % (INPUT_TYPE::UNKNOWN_PTR + 1);

    switch(type_id) {
      case INPUT_TYPE::CHAR:
        {
          char val[sizeof(char) + 1];
          if (!fread(val, sizeof(char), 1, input_fp)) {
            return;
          }
          VAR<char> * inputv = new VAR<char>(val[0], 0, INPUT_TYPE::CHAR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::SHORT:
        {
          char val[sizeof(short) + 1];
          if (!fread(val, sizeof(short), 1, input_fp)) {
            return;
          }
          VAR<short> * inputv = new VAR<short>(*(short *)val, 0, INPUT_TYPE::SHORT);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::INT:
        {
          char val[sizeof(int) + 1];
          if (!fread(val, sizeof(int), 1, input_fp)) {
            return;
          }
          VAR<int> * inputv = new VAR<int>(*(int *)val, 0, INPUT_TYPE::INT);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::LONG:
        {
          char val[sizeof(long) + 1];
          if (!fread(val, sizeof(long), 1, input_fp)) {
            return;
          }
          VAR<long> * inputv = new VAR<long>(*(long *)val, 0, INPUT_TYPE::LONG);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::FLOAT:
        {
          char val[sizeof(float) + 1];
          if (!fread(val, sizeof(float), 1, input_fp)) {
            return;
          }
          VAR<float> * inputv = new VAR<float>(*(float *)val, 0, INPUT_TYPE::FLOAT);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::DOUBLE:
        {
          char val[sizeof(double) + 1];
          if (!fread(val, sizeof(double), 1, input_fp)) {
            return;
          }
          VAR<double> * inputv = new VAR<double>(*(double *)val, 0, INPUT_TYPE::DOUBLE);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::PTR:
        {
          char val[sizeof(int) + 1];
          if (!fread(val, sizeof(int), 1, input_fp)) {
            return;
          }
          char offset_val[sizeof(int) + 1];
          if (!fread(offset_val, sizeof(int), 1, input_fp)) {
            return;
          }

          VAR<int> * inputv = new VAR<int>(*(int *)val, 0, *(int *) offset_val, INPUT_TYPE::PTR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::NULLPTR:
        {
          VAR<void *> * inputv = new VAR<void *>(NULL, 0, INPUT_TYPE::NULLPTR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::FUNCPTR:
        {
          char val[sizeof(int) + 1];
          if (!fread(val, sizeof(int), 1, input_fp)) {
            return;
          }

          int func_index = (*(int *)val) % num_func;
          void * func_ptr = *func_ptr_index.get(func_index);
          VAR<void *> * inputv = new VAR<void *>(func_ptr, 0, INPUT_TYPE::FUNCPTR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      case INPUT_TYPE::UNKNOWN_PTR:
        {
          VAR<void *> * inputv = new VAR<void *>(NULL, 0, INPUT_TYPE::NULLPTR);
          __replay_inputs.push_back((IVAR *) inputv);
        }
        break;
      default:
        break;  
    }
  }

  fclose(input_fp);
  return;
}

void __record_func_ptr_index(void * ptr) {
  func_ptr_index.push_back(ptr);
}