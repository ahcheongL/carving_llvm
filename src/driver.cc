#include "utils.hpp"

static vector<IVAR *> inputs;
static vector<PTR> carved_ptrs;
static vector<int> replayed_index;

//assert(func_ptr.size() == funcnames.size())
static vector<void *> func_ptrs;
static vector<char *> funcnames;

void __driver_inputf_reader(char ** argv) {
  char * inputfilename = argv[1];
  FILE * fp = fopen(inputfilename, "r");
  if (fp == NULL) { fprintf(stderr, "Can't read input file\n"); std::abort(); }

  ssize_t readbyte;
  size_t len = 256;
  char * line = NULL;
  bool is_carved_ptr = false;
  while ((readbyte = getline(&line, &len, fp)) != -1) {
    line[readbyte - 1] = 0;  //remove '\n'
    if (!is_carved_ptr) {
      if (line[0] == '#') {
        is_carved_ptr = true;
      } else {
        char * num_bytes_str = strchr(line, ':') + 1;
        num_bytes_str = strchr(num_bytes_str, ':') + 1;
        int num_bytes = atoi(num_bytes_str);
        carved_ptrs.push_back(PTR(malloc(num_bytes), num_bytes));
      }
    } else {
      char * split = strchr(line, ':');
      char * type = split + 1;
      *split = 0;
      split = strchr(type, ':');
      char * value = split + 1;
      *split = 0;
      char * offset = NULL;
      split = strchr(value, ':');
      if (split != NULL) {
        offset = split + 1;
        *split = 0;
      }

      if (!strcmp(type, "CHAR")) {
        VAR<char> * inputv = new VAR<char> (atoi(value), line, INPUT_TYPE::CHAR);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "SHORT")) {
        VAR<short> * inputv = new VAR<short> (atoi(value), line, INPUT_TYPE::SHORT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "INT")) {
        VAR<int> * inputv = new VAR<int> (atoi(value), line, INPUT_TYPE::INT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "LONG")) {
        VAR<long> * inputv = new VAR<long> (atoi(value), line, INPUT_TYPE::LONG);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "LONGLONG")) {
        VAR<long long> * inputv = new VAR<long long> (atoi(value), line, INPUT_TYPE::LONGLONG);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "FLOAT")) {
        VAR<float> * inputv = new VAR<float> (atoi(value), line, INPUT_TYPE::FLOAT);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "DOUBLE")) {
        VAR<double> * inputv = new VAR<double> (atoi(value), line, INPUT_TYPE::DOUBLE);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "NULL")) {
        VAR<void *> * inputv = new VAR<void *> (NULL, line, INPUT_TYPE::NULLPTR);
        inputs.push_back((IVAR *) inputv);
      } else if (!strcmp(type, "FUNCPTR")) {
        int num_func = func_ptrs.size();
        int idx;
        size_t value_len = strlen(value);
        
        for (idx = 0; idx < num_func; idx++) {
          if (strncmp(value, *(funcnames[idx]), value_len) == 0) {
            VAR<void *> * inputv = new VAR<void *> (*(func_ptrs[idx]), line, INPUT_TYPE::FUNCPTR);
            inputs.push_back((IVAR *) inputv);
            break;
          }
        }

        if (idx == num_func) {
          VAR<void *> * inputv = new VAR<void *> (NULL, line, INPUT_TYPE::FUNCPTR);
          inputs.push_back((IVAR *) inputv);
        }        
      } else if (!strcmp(type, "PTR")) {
        VAR<int> * inputv = new VAR<int> (atoi(value), line, atoi(offset)
          , INPUT_TYPE::POINTER);
        inputs.push_back((IVAR *) inputv);
      }
      line = NULL;
    }
  }
  
  return;
}

static int cur_input_idx = 0;

char Replay_char() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::CHAR) { return 0; }
  VAR<char> * cur_input = (VAR<char> *) cur_input_tmp;
  return cur_input->input;
}

short Replay_short() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::SHORT) { return 0; }
  VAR<short> * cur_input = (VAR<short> *) cur_input_tmp;
  return cur_input->input;
}

int Replay_int() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::INT) { return 0; }
  VAR<int> * cur_input = (VAR<int> *) cur_input_tmp;
  return cur_input->input;
}

long Replay_longtype() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::LONG) { return 0; }
  VAR<long> * cur_input = (VAR<long> *) cur_input_tmp;
  return cur_input->input;
}

long long Replay_longlong() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::LONGLONG) { return 0; }
  VAR<long long> * cur_input = (VAR<long long> *) cur_input_tmp;
  return cur_input->input;
}

float Replay_float() {

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::FLOAT) { return 0; }
  VAR<float> * cur_input = (VAR<float> *) cur_input_tmp;
  return cur_input->input;
}

double Replay_double() {
  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::DOUBLE) { return 0; }
  VAR<double> * cur_input = (VAR<double> *) cur_input_tmp;
  return cur_input->input;
}

static int cur_ptr_alloc_size = 0;

void * Replay_pointer() {
  cur_ptr_alloc_size = 0;

  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::POINTER) { return 0; }
  VAR<int> * cur_input = (VAR<int> *) cur_input_tmp;
  
  int carved_index = cur_input->input;
  
  if (carved_index >= carved_ptrs.size()) { return 0; }

  char * ret_ptr = ((char *) carved_ptrs[carved_index]->addr)
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

    cur_ptr_alloc_size
      = carved_ptrs[carved_index]->alloc_size;
    replayed_index.push_back(std::move(carved_index));
  }

  return ret_ptr;
}

int Replay_ptr_alloc_size() {
  return cur_ptr_alloc_size;
}

void * Replay_func_ptr() {
  if (cur_input_idx >= inputs.size()) {
    return 0;
  }

  IVAR * cur_input_tmp = *(inputs[cur_input_idx++]);
  if (cur_input_tmp->type != INPUT_TYPE::FUNCPTR) { return 0; }
  VAR<void *> * cur_input = (VAR<void *> *) cur_input_tmp;
  return cur_input->input;
}

void __record_func_ptr(void * ptr, char * name) {
  func_ptrs.push_back(std::move(ptr));
  funcnames.push_back(std::move(name));
}
