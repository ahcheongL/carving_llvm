#include "utils.hpp"
#include "dirent.h"

extern map<void *, char *> __replay_func_ptrs;
extern vector<IVAR *> __replay_inputs;
extern vector<PTR> __replay_carved_ptrs;

char read_carv_file(char * data, int size) {

  // int limit = size;
  // if (limit > 500) {
  //   limit = 500;
  // }

  // int carv_index = 0;
  
  // for (int i = 0; i < limit; i++) {
  //   carv_index += data[i];
  // }

  int carv_index = *(int *) data;
  
  char * carv_dir = getenv("CARV_DIR");

  if (carv_dir == NULL) {
    return 0;
  }

  DIR *d;
  struct dirent *dir;
  d = opendir(carv_dir);
  if (!d) return 0;

  vector<dirent *> files;

  while ((dir = readdir(d)) != NULL) {
    if (dir->d_type != DT_REG) { continue; }
    bool inserted = false;

    for (int i = 0; i < files.size(); i++) {
      if (strcmp(dir->d_name, (*files.get(i))->d_name) < 0) {
        files.insert(i, dir);
        inserted = true;
        break;
      }
    }

    if (!inserted) {
      files.push_back(dir);
    }
  }

  int num_files = files.size();

  carv_index = carv_index % (num_files + 10);
  if (carv_index >= num_files) {
    return 0;
  }

  char * carv_file_name = (*files.get(carv_index))->d_name;

  char * full_name = (char *) malloc(strlen(carv_dir) + strlen(carv_file_name) + 2);
  strcpy(full_name, carv_dir);
  strcat(full_name, "/");
  strcat(full_name, carv_file_name);

  FILE * input_fp = fopen(full_name, "r");
  if (input_fp == NULL) {
    //fprintf(stderr, "Can't read input file\n");
    return 0;
  }

  files.clear();
  closedir(d);

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

        __replay_carved_ptrs.push_back(PTR(new_ptr, strdup(type_str + 1), ptr_size));
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
      } else if (!strncmp(type_str, "NULL", 4)) {
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
        VAR<int> * inputv = new VAR<int>(ptr_index, 0, ptr_offset, INPUT_TYPE::POINTER);
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
  return 1;
}