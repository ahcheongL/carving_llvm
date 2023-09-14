#include <assert.h>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace std;

class classinfo {
 public:
  int class_index;
  int size;

  classinfo(int class_index, int size) {
    this->class_index = class_index;
    this->size = size;
  }

  classinfo() {
    this->class_index = 0;
    this->size = 0;
  }

  classinfo(int) {
    class_index = 0;
    size = 0;
  }

  classinfo(const classinfo &other) {
    class_index = other.class_index;
    size = other.size;
  }

  classinfo(classinfo &&other) {
    class_index = other.class_index;
    size = other.size;
    other.class_index = 0;
    other.size = 0;
  }

  classinfo &operator=(const classinfo &other) {
    class_index = other.class_index;
    size = other.size;
    return *this;
  }

  classinfo &operator=(classinfo &&other) {
    class_index = other.class_index;
    size = other.size;
    other.class_index = 0;
    other.size = 0;
    return *this;
  }
};

enum INPUT_TYPE {
  CHAR,
  SHORT,
  INT,
  LONG,
  LONGLONG,
  FLOAT,
  DOUBLE,
  LONGDOUBLE,
  PTR,
  NULLPTR,
  FUNCPTR,
  VTABLE_PTR,
  UNKNOWN_PTR
};

class POINTER {
 public:
  POINTER() : addr(0), pointee_type(0), alloc_size(0) {}

  POINTER(void *_addr, int _size)
      : addr(_addr), pointee_type(0), alloc_size(_size) {}

  POINTER(void *_addr, const char *pointee_type, int _size)
      : addr(_addr), pointee_type(pointee_type), alloc_size(_size) {}

  void *addr;
  const char *pointee_type;
  int alloc_size;
};

class IVAR {
 public:
  IVAR() : type(INPUT_TYPE::CHAR), name(0) {}
  IVAR(char *name, enum INPUT_TYPE type) : name(name), type(type) {}
  ~IVAR() { free(name); }
  enum INPUT_TYPE type;
  char *name;
};

template <class input_type>
class VAR : public IVAR {
 public:
  VAR() : input(0), pointer_offset(0), IVAR() {}

  VAR(input_type _input, char *_name, enum INPUT_TYPE _type)
      : input(_input), IVAR(_name, _type) {}

  VAR(input_type pointer_index, char *_name, int _pointer_offset,
      enum INPUT_TYPE _type)
      : input(pointer_index),
        IVAR(_name, _type),
        pointer_offset(_pointer_offset) {}

  // copy constructor
  VAR(const VAR<input_type> &other)
      : input(other.input),
        IVAR(strdup(other.name), other.type),
        pointer_offset(other.pointer_offset) {}

  // move constructor
  VAR(VAR<input_type> &&other)
      : input(other.input),
        IVAR(other.name, other.type),
        pointer_offset(other.pointer_offset) {}

  VAR &operator=(const VAR<input_type> &other) {
    input = other.input;
    name = strdup(other.name);
    type = other.type;
    pointer_offset = other.pointer_offset;
    return *this;
  }

  VAR &operator=(VAR<input_type> &&other) {
    input = other.input;
    name = other.name;
    type = other.type;
    pointer_offset = other.pointer_offset;
    other.name = 0;
    return *this;
  }

  input_type input;
  int pointer_offset;
};

extern "C" {

map<char *, classinfo> __replay_class_info;

// Function pointer names
static map<void *, char *> __replay_func_ptrs;

IVAR **__replay_inputs = NULL;
unsigned int __replay_inputs_size = 0;
unsigned int __replay_inputs_capacity = 0;

vector<POINTER> __replay_carved_ptrs;

// adhoc to make a set
map<int, char> __replay_replayed_ptr;

char *__carved_obj_dir = NULL;

void __driver_inputf_open(char *inputfilename);

void __driver_input_argv_modifier(int *argcptr, char ***argvptr) {
  int argc = *argcptr - 1;

  if (argc == 0) {
    return;
  }

  *argcptr = argc;

  __carved_obj_dir = (*argvptr)[argc];

  (*argvptr)[argc] = 0;
}

void __driver_inputf_open(char *inputfilename) {
  if (inputfilename == NULL) {
    fprintf(stderr, "Replay error : inputfilename is NULL\n");
    // std::abort();
    return;
  }

  fprintf(stderr, "Trying to read %s\n", inputfilename);

  FILE *input_fp = fopen(inputfilename, "r");
  if (input_fp == NULL) {
    // fprintf(stderr, "Can't read input file\n");
    std::abort();
  }

  __replay_inputs_size = 0;
  __replay_inputs_capacity = 1024;
  __replay_inputs = (IVAR **)malloc(sizeof(IVAR *) * __replay_inputs_capacity);

  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  bool is_carved_ptr = true;
  while ((read = getline(&line, &len, input_fp)) != -1) {
    if (is_carved_ptr) {
      if (line[0] == '#') {
        is_carved_ptr = false;
      } else {
        char *addr_str = strchr(line, ':');
        if (addr_str == NULL) {
          // fprintf(stderr, "Invalid input file\n");
          // std::abort();
        }
        char *size_str = strchr(addr_str + 1, ':');
        if (size_str == NULL) {
          // fprintf(stderr, "Invalid input file\n");
          // std::abort();
        }
        char *type_str = strchr(size_str + 1, ':');
        if (type_str == NULL) {
          // fprintf(stderr, "Invalid input file\n");
          // std::abort();
        }

        *type_str = 0;
        len = strlen(type_str + 1);
        type_str[len] = 0;

        int ptr_size = atoi(size_str + 1);

        void *new_ptr = malloc(ptr_size);

        __replay_carved_ptrs.push_back(
            POINTER(new_ptr, strdup(type_str + 1), ptr_size));
      }
    } else {
      char *type_str = strchr(line, ':');
      if (type_str == NULL) {
        fprintf(stderr, "Invalid input file\n");
        std::abort();
      }
      char *value_str = strchr(type_str + 1, ':');
      if (value_str == NULL) {
        fprintf(stderr, "Invalid input file\n");
        std::abort();
      }
      char *index_str = strchr(value_str + 1, ':');

      *value_str = 0;

      type_str += 1;
      if (!strncmp(type_str, "CHAR", 4)) {
        char value = atoi(value_str + 1);
        VAR<char> *inputv = new VAR<char>(value, 0, INPUT_TYPE::CHAR);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "SHORT", 5)) {
        short value = atoi(value_str + 1);
        VAR<short> *inputv = new VAR<short>(value, 0, INPUT_TYPE::SHORT);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "INT", 3)) {
        int value = atoi(value_str + 1);
        VAR<int> *inputv = new VAR<int>(value, 0, INPUT_TYPE::INT);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "LONG", 4)) {
        long value = atol(value_str + 1);
        VAR<long> *inputv = new VAR<long>(value, 0, INPUT_TYPE::LONG);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "LONGLONG", 8)) {
        long long value = atoll(value_str + 1);
        VAR<long long> *inputv =
            new VAR<long long>(value, 0, INPUT_TYPE::LONGLONG);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "FLOAT", 5)) {
        float value = atof(value_str + 1);
        VAR<float> *inputv = new VAR<float>(value, 0, INPUT_TYPE::FLOAT);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "DOUBLE", 6)) {
        double value = atof(value_str + 1);
        VAR<double> *inputv = new VAR<double>(value, 0, INPUT_TYPE::DOUBLE);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "NULLPTR", 7)) {
        VAR<void *> *inputv = new VAR<void *>(0, 0, INPUT_TYPE::NULLPTR);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "FUNCPTR", 7)) {
        char *func_name = value_str + 1;
        len = strlen(func_name);
        func_name[len - 1] = 0;

        bool found_func = false;

        for (auto iter : __replay_func_ptrs) {
          if (!strcmp(iter.second, func_name)) {
            VAR<void *> *inputv =
                new VAR<void *>(iter.first, 0, INPUT_TYPE::FUNCPTR);
            __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
            found_func = true;
            break;
          }
        }

        if (!found_func) {
          VAR<void *> *inputv = new VAR<void *>(0, 0, INPUT_TYPE::FUNCPTR);
          __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
        }
      } else if (!strncmp(type_str, "PTR", 3)) {
        if (index_str == NULL) {
          // fprintf(stderr, "Invalid input file\n");
          // std::abort();
        }

        int ptr_index = atoi(value_str + 1);
        int ptr_offset = atoi(index_str + 1);
        VAR<int> *inputv =
            new VAR<int>(ptr_index, 0, ptr_offset, INPUT_TYPE::PTR);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else if (!strncmp(type_str, "UNKNOWN_PTR", 11)) {
        VAR<void *> *inputv = new VAR<void *>(0, 0, INPUT_TYPE::UNKNOWN_PTR);
        __replay_inputs[__replay_inputs_size++] = ((IVAR *)inputv);
      } else {
        // fprintf(stderr, "Invalid input file\n");
        // std::abort();
      }

      if (__replay_inputs_size >= __replay_inputs_capacity) {
        __replay_inputs_capacity *= 2;
        __replay_inputs = (IVAR **)realloc(
            __replay_inputs, sizeof(IVAR *) * __replay_inputs_capacity);
      }
    }
  }

  if (line) {
    free(line);
  }
  fclose(input_fp);

  fprintf(stderr, "Read %u inputs\n", __replay_inputs_size);
  return;
}

static int filename_cmp(const void *l, const void *r) {
  return strcmp((const char *)l, (const char *)r);
}

char *__select_replay_file(char *name, unsigned int id) {
  if (__carved_obj_dir == NULL) {
    // std::abort();
    return 0;
  }

  char *type_dir_name =
      (char *)malloc(strlen(__carved_obj_dir) + strlen(name) + 2);
  sprintf(type_dir_name, "%s/%s", __carved_obj_dir, name);

  DIR *dir = opendir(type_dir_name);
  if (dir == NULL) {
    // std::abort();
    return 0;
  }

  struct dirent *ent;
  int num_files = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_type == DT_REG) {
      num_files++;
    }
  }

  if (num_files == 0) {
    fprintf(stderr, "Replay error : No files in directory : %s\n",
            type_dir_name);
    // std::abort();
    return 0;
  }

  char **file_names = (char **)malloc(sizeof(char *) * num_files);

  rewinddir(dir);

  int file_idx = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_type == DT_REG) {
      file_names[file_idx++] = ent->d_name;
    }
  }

  qsort(file_names, num_files, sizeof(char *), filename_cmp);

  int sel_file_idx = id % num_files;

  char *file_name = (char *)malloc(strlen(type_dir_name) +
                                   strlen(file_names[sel_file_idx]) + 2);

  sprintf(file_name, "%s/%s", type_dir_name, file_names[sel_file_idx]);
  closedir(dir);

  free(type_dir_name);
  free(file_names);

  return file_name;
}

char *__fetch_file(char *name, unsigned int id) {
  if (__carved_obj_dir == NULL) {
    fprintf(stderr, "Replay error : __carved_obj_dir is NULL\n");
    // std::abort();
    return 0;
  }

  char *type_dir_name =
      (char *)malloc(strlen(__carved_obj_dir) + strlen(name) + 14);
  sprintf(type_dir_name, "%s/carved_file_%s", __carved_obj_dir, name);

  DIR *dir = opendir(type_dir_name);
  if (dir == NULL) {
    fprintf(stderr, "Replay error : Can't open directory : %s\n",
            type_dir_name);
    // std::abort();
    return 0;
  }

  struct dirent *ent;
  int num_files = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_type == DT_REG) {
      num_files++;
    }
  }

  if (num_files == 0) {
    fprintf(stderr, "Replay error : No files in directory : %s\n",
            type_dir_name);
    // std::abort();
    return 0;
  }

  char **file_names = (char **)malloc(sizeof(char *) * num_files);

  rewinddir(dir);

  int file_idx = 0;
  while ((ent = readdir(dir)) != NULL) {
    if (ent->d_type == DT_REG) {
      file_names[file_idx++] = ent->d_name;
    }
  }

  qsort(file_names, num_files, sizeof(char *), filename_cmp);

  int sel_file_idx = id % num_files;

  char *file_name = (char *)malloc(strlen(type_dir_name) +
                                   strlen(file_names[sel_file_idx]) + 2);

  sprintf(file_name, "%s/%s", type_dir_name, file_names[sel_file_idx]);
  closedir(dir);

  // copy file
  FILE *to_fp = fopen(name, "w");
  FILE *from_fp = fopen(file_name, "r");
  char buf[1024];
  size_t nread;
  while ((nread = fread(buf, 1, sizeof(buf), from_fp)) > 0) {
    fwrite(buf, 1, nread, to_fp);
  }

  fclose(to_fp);
  fclose(from_fp);

  free(type_dir_name);
  free(file_names);

  return file_name;
}

static int cur_input_idx = 0;

void __driver_initialize() {
  for (int idx = 0; idx < __replay_inputs_size; idx++) {
    delete __replay_inputs[idx];
  }

  __replay_inputs_size = 0;
  __replay_carved_ptrs.clear();
  __replay_replayed_ptr.clear();
  cur_input_idx = 0;
  return;
}

char Replay_char() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if ((elem->type != INPUT_TYPE::CHAR)) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<char> *)elem)->input;
}

short Replay_short() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::SHORT) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<short> *)elem)->input;
}

int Replay_int() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::INT) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<int> *)elem)->input;
}

long Replay_longtype() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::LONG) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<long> *)elem)->input;
}

long long Replay_longlong() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::LONGLONG) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<long long> *)elem)->input;
}

float Replay_float() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }
  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::FLOAT) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<float> *)elem)->input;
}

double Replay_double() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }
  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type != INPUT_TYPE::DOUBLE) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<double> *)elem)->input;
}

int __replay_cur_alloc_size = 0;
int __replay_cur_class_index = -1;
int __replay_cur_pointee_size = -1;
void *__replay_cur_zero_address = 0;

void *Replay_pointer(int default_idx, int default_pointee_size,
                     char *pointee_type_name) {
  if (__replay_inputs_size <= cur_input_idx) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  IVAR *elem = __replay_inputs[cur_input_idx++];

  if (elem->type == INPUT_TYPE::NULLPTR) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  if (elem->type == INPUT_TYPE::UNKNOWN_PTR) {
    // alloc 1 object
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

  if (elem->type != INPUT_TYPE::PTR) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return 0;
  }

  VAR<int> *elem_v = (VAR<int> *)elem;
  int ptr_index = elem_v->input;
  int ptr_offset = elem_v->pointer_offset;

  POINTER carved_ptr = __replay_carved_ptrs[ptr_index];

  auto search = __replay_replayed_ptr.find(ptr_index);
  if (search != __replay_replayed_ptr.end()) {
    __replay_cur_alloc_size = 0;
    __replay_cur_pointee_size = -1;
    return (char *)carved_ptr.addr + ptr_offset;
  }
  __replay_replayed_ptr.insert(make_pair(ptr_index, 0));

  __replay_cur_alloc_size = carved_ptr.alloc_size;
  __replay_cur_zero_address = carved_ptr.addr;

  const char *type_name = carved_ptr.pointee_type;

  __replay_cur_pointee_size = default_pointee_size;
  __replay_cur_class_index = default_idx;

  if (!strcmp(type_name, pointee_type_name)) {
    return (char *)carved_ptr.addr + ptr_offset;
  }

  // carved ptr has different type
  for (auto iter : __replay_class_info) {
    if (!strcmp(iter.first, type_name)) {
      __replay_cur_pointee_size = iter.second.size;
      __replay_cur_class_index = iter.second.class_index;
      break;
    }
  }

  return (char *)carved_ptr.addr + ptr_offset;
}

void *Replay_func_ptr() {
  if (__replay_inputs_size <= cur_input_idx) {
    return 0;
  }
  IVAR *elem = __replay_inputs[cur_input_idx++];

  if ((elem->type != INPUT_TYPE::FUNCPTR) &&
      (elem->type != INPUT_TYPE::NULLPTR)) {
    // fprintf(stderr, "Replay error : Invalid input type\n");
    // std::abort();
    return 0;
  }

  return ((VAR<void *> *)elem)->input;
}

void __keep_class_info(char *class_name, int size, int index) {
  classinfo tmp{index, size};
  __replay_class_info.insert(make_pair(class_name, tmp));
}

void __record_func_ptr(void *ptr, char *name) {
  __replay_func_ptrs.insert(make_pair(ptr, name));
}

char *__update_class_ptr(char *ptr, int idx, int size) {
  return ptr + (idx * size);
}

// coverage
static map<char *, map<std::string, map<std::string, bool>>>
    __replay_coverage_info;

void __record_bb_cov(char *file_name, char *func_name, char *bb_name) {
  if (__replay_coverage_info.find(file_name) == __replay_coverage_info.end()) {
    __replay_coverage_info.insert(
        make_pair(file_name, map<std::string, map<std::string, bool>>()));
  }

  map<std::string, map<std::string, bool>> &file_info =
      __replay_coverage_info[file_name];
  if (file_info.find(func_name) == file_info.end()) {
    file_info.insert(make_pair(func_name, map<std::string, bool>()));
  }

  map<std::string, bool> &func_info = file_info[func_name];
  if (func_info.find(bb_name) == func_info.end()) {
    func_info.insert(make_pair(bb_name, true));
  }

  return;
}

void __cov_fini() {
  for (auto iter : __replay_coverage_info) {
    const std::string cov_file_name = std::string(iter.first) + ".cov";

    map<std::string, map<std::string, bool>> &file_info = iter.second;

    std::ifstream cov_file_in(cov_file_name, std::ios::in);

    if (cov_file_in.is_open()) {
      std::string type;
      std::string name;
      bool is_covered = false;
      std::string cur_func = "";
      std::string line;
      while (getline(cov_file_in, line)) {
        auto pos1 = line.find(" ");

        if (pos1 == std::string::npos) {
          break;
        }

        auto pos2 = line.find_last_of(" ");

        if (pos2 == std::string::npos) {
          break;
        }

        if (pos1 == pos2) {
          break;
        }

        type = line.substr(0, pos1);
        name = line.substr(pos1 + 1, pos2 - pos1 - 1);
        is_covered = line.substr(pos2 + 1) == "1";
        if (type == "F") {
          if (file_info.find(name) == file_info.end()) {
            file_info.insert(make_pair(name, map<std::string, bool>()));
          }

          cur_func = name;
        } else {
          if (file_info[cur_func].find(name) == file_info[cur_func].end()) {
            file_info[cur_func].insert(make_pair(name, is_covered));
          } else if (is_covered) {
            file_info[cur_func][name] = true;
          }
        }
      }
    }
    cov_file_in.close();

    std::ofstream cov_file_out(cov_file_name, std::ios::out);

    for (auto iter2 : file_info) {
      bool is_func_covered = false;
      const map<std::string, bool> &func_info = iter2.second;

      for (auto iter3 : func_info) {
        if (iter3.second) {
          is_func_covered = true;
          break;
        }
      }

      cov_file_out << "F " << iter2.first << " " << is_func_covered << "\n";

      for (auto iter3 : func_info) {
        cov_file_out << "B " << iter3.first << " " << iter3.second << "\n";
      }
    }

    cov_file_out.close();
  }
  return;
}
}