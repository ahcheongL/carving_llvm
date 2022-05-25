#include "utils.hpp"

static vector<IVAR *> inputs;
static vector<PTR> carved_ptrs;

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
        //TODO
        VAR<char *> * inputv = new VAR<char *> (NULL, line, INPUT_TYPE::FUNCPTR);
        inputs.push_back((IVAR *) inputv);
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