#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fstream>
#include <memory>
#include <map>
#include "carver.hpp"
#include <vector>

#ifndef __CROWN_CARVER_DEF
#define __CROWN_CARVER_DEF

static char * outdir_name = NULL;

static mem_info * alloced_addresses;
static int alloced_addresses_size;
static int num_alloced_addresses;

static int * num_func_calls;
static int num_func_calls_size;

static void ** carved_ptrs;
static int carved_ptrs_size;
static int num_carved_ptrs;

static int ** func_carved_filesize;

static int * callseq;
static int callseq_size;
static int callseq_index;

static std::vector<IVAR *> inputs;

void Carv_char(char input, char * name) {
  VAR<char> * inputv = new VAR<char>(input, name, INPUT_TYPE::CHAR);
  inputs.push_back((IVAR *) inputv);
}

void Carv_short(short input, char * name) {
  VAR<short> * inputv = new VAR<short>(input, name, INPUT_TYPE::SHORT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_int(int input, char * name) {
  VAR<int> * inputv = new VAR<int>(input, name, INPUT_TYPE::INT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_long(long input, char * name) {
  VAR<long> * inputv = new VAR<long>(input, name, INPUT_TYPE::LONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_longlong(long long input, char * name) {
  VAR<long long> * inputv
    = new VAR<long long>(input, name, INPUT_TYPE::LONGLONG);
  inputs.push_back((IVAR *) inputv);
}

void Carv_float(float input, char * name) {
  VAR<float> * inputv = new VAR<float>(input, name, INPUT_TYPE::FLOAT);
  inputs.push_back((IVAR *) inputv);
}

void Carv_double(double input, char * name) {
  VAR<double> * inputv = new VAR<double>(input, name, INPUT_TYPE::DOUBLE);
  inputs.push_back((IVAR *) inputv);
}


void __remove_mem_allocated_probe(void * ptr) {
  int i ;
  for (i = 0; i < num_alloced_addresses; i++) {
    void * addr = alloced_addresses[i].addr;
    size_t cur_size = alloced_addresses[i].size;
    if (addr == ptr) {
      alloced_addresses[i].addr = 0;
      break;
    }
  }
}

// int __CROWN_POINTER_CARVER(void * ptr) {
//   int size = 0;

//   //Find already carved ptr
//   int idx;

//   for (idx = 0; idx < num_carved_ptrs; idx++) {
//     if (ptr == carved_ptrs[idx]) {
//       //Already carved pointer
//       fwrite(&idx, 4, 1, __CROWN_OUTFILE);
//       return 0;
//     }
//   }

//   idx = -1;
//   fwrite(&idx, 4, 1, __CROWN_OUTFILE);

//   if (ptr != 0) {
//     for (idx = 0; idx < num_alloced_addresses; idx++) {
//       char * addr = (char *) alloced_addresses[idx].addr;
//       if (addr == 0) continue;
//       int cur_size = alloced_addresses[idx].size;
//       if (addr == ptr) {
//         size = cur_size;
//         break;
//       } else if ((addr < ptr) && ((addr + cur_size) > ptr)) {
//         size = 1;
//       }
//     }

//     carved_ptrs[num_carved_ptrs++] = ptr;
//     if (num_carved_ptrs >= carved_ptrs_size) {
//       carved_ptrs_size *= 2;
//       carved_ptrs = (void **) realloc(carved_ptrs
//         , sizeof(void *) * carved_ptrs_size);
//     }
//   }

//   fwrite(&size, 4, 1, __CROWN_OUTFILE);

//   return size;
// }

// char __CROWN_POINTER_CHECK_CARVER(void * ptr, int size) {
//   int idx;
//   for (idx = 0; idx < num_alloced_addresses; idx++) {
//     char * addr = (char *) alloced_addresses[idx].addr;
//     int cur_size = alloced_addresses[idx].size;
//     if ((addr <= ptr) && ((addr + cur_size) >= ((char *) ptr + size))) {
//       fputc(1, __CROWN_OUTFILE);
//       return 1;
//     }
//   }
//   fputc(0, __CROWN_OUTFILE);

//   fwrite(ptr, size, 1, __CROWN_OUTFILE);
//   return 0;
// }

void __mem_allocated_probe(void * ptr, int size) {
  int i;
  int zero_idx = -1;
  for (i = 0; i < num_alloced_addresses; i++) {
    void * addr = alloced_addresses[i].addr;
    if (addr == ptr) {
      alloced_addresses[i].size = size;
      break;
    } else if ((zero_idx == -1) && (addr == 0)) {
      zero_idx = i;
    }
  }

  if (i == num_alloced_addresses) {
    if (zero_idx != -1) {
      alloced_addresses[zero_idx].addr = ptr;
      alloced_addresses[zero_idx].size = size;
    } else {
      alloced_addresses[i].addr = ptr;
      alloced_addresses[i].size = size;
      num_alloced_addresses++;
      if (num_alloced_addresses == alloced_addresses_size) {
        alloced_addresses_size *= 2;
        alloced_addresses = (mem_info *) realloc(alloced_addresses,
          sizeof(mem_info) * alloced_addresses_size);
      }
    }
  }

  return;
}

void __carv_init() {
  alloced_addresses_size = 4096;
  alloced_addresses = (mem_info *) malloc(alloced_addresses_size
    * sizeof(mem_info));
  num_alloced_addresses = 0;
  num_func_calls_size = 256;
  num_func_calls = (int *) calloc(num_func_calls_size, sizeof(int));
  carved_ptrs_size = 256;
  carved_ptrs = (void **) malloc(carved_ptrs_size * sizeof(void *));
  num_carved_ptrs = 0;
  func_carved_filesize = (int **) calloc(num_func_calls_size, sizeof(int *));
  callseq_size = 16384;
  callseq = (int *) malloc(callseq_size * sizeof(int));
  callseq_index = 0;
  return;
}

int carved_index = 0;

//Insert at the begining of 
void Write_carved(char * func_name, int func_id) {

  //Write call sequence
  callseq[callseq_index++] = func_id;
  if (callseq_index >= callseq_size) {
    callseq_size *= 2;
    callseq = (int *) realloc(callseq, callseq_size * sizeof(int));
  }

  //Write # of function call 
  while (func_id >= num_func_calls_size) {
    int tmp = num_func_calls_size;
    num_func_calls_size *= 2;
    num_func_calls = (int*) realloc (num_func_calls
      , num_func_calls_size * sizeof(int));
    memset(num_func_calls + tmp, 0, tmp * sizeof(int));
    func_carved_filesize = (int **) realloc(func_carved_filesize
      , num_func_calls_size * sizeof(int *));
    memset(func_carved_filesize + tmp, 0, tmp * sizeof(int));
  }

  num_func_calls[func_id] += 1;
  
  //Open file
  std::string outfile_name = std::string(outdir_name) + "/" + func_name
    + "_" + std::to_string(carved_index++) + "_" 
    + std::to_string(num_func_calls[func_id]);

  std::ofstream outfile(outfile_name);  

  for (auto iter = inputs.begin(); iter != inputs.end(); iter++) {
    IVAR * elem = *iter;
    if (elem->type == INPUT_TYPE::CHAR) {
      outfile << elem->name << ":CHAR:"
              << ((VAR<char>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::SHORT) {
      outfile << elem->name << ":SHORT:"
              << ((VAR<short>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::INT) {
      outfile << elem->name << ":INT:"
              << ((VAR<int>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::LONG) {
      outfile << elem->name << ":LONG:"
              << ((VAR<long>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::LONGLONG) {
      outfile << elem->name << ":lONGLONG:"
              << ((VAR<long long>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::FLOAT) {
      outfile << elem->name << ":FLOAT:"
              << ((VAR<float>*) elem)->input << "\n";
    } else if (elem->type == INPUT_TYPE::DOUBLE) {
      outfile << elem->name << ":SHORT:"
              << ((VAR<double>*) elem)->input << "\n";
    }
  }

  int filesize = (int) outfile.tellp();
  outfile.close();

  for (auto iter = inputs.begin(); iter != inputs.end(); iter++) {
    IVAR * elem = *iter;
    delete elem;
  }

  inputs.clear();

  if ((filesize <= 64) || (filesize > 1048576)) {
    //remove(outfile_name.c_str());
  }

  int tmp = 128;
  int index = 0;
  while (filesize > tmp) {
    tmp *= 2;
    index += 1;
  }

  if (func_carved_filesize[func_id] == 0) {
    func_carved_filesize[func_id] = (int *) calloc(20, sizeof(int));
  }

  if (func_carved_filesize[func_id][index] >= 8) {
    remove(outfile_name.c_str());
  } else {
    func_carved_filesize[func_id][index] += 1;
  }  

  return;
}

void __argv_modifier(int * argcptr, char *** argvptr) {
  int argc = (*argcptr) - 1;
  *argcptr = argc;

  outdir_name = (*argvptr)[argc];

  (*argvptr)[argc] = 0;
  return;
}

void __carv_FINI() {
  char buffer[256];
  snprintf(buffer, 256, "%s/call_seq", outdir_name);
  FILE * __call_seq_file = fopen(buffer, "w");
  if (__call_seq_file == NULL) {
    return;
  }
  fwrite(callseq, sizeof(int), callseq_index, __call_seq_file);
  fclose(__call_seq_file);
}
#endif