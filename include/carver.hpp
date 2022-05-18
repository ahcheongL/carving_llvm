#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fstream>
#include <memory>
#include <map>
#include <vector>

enum INPUT_TYPE {
  CHAR,
  UCHAR,
  SHORT,
  USHORT,
  INT,
  UINT,
  LONG,
  ULONG,
  LONGLONG,
  ULONGLONG,
  FLOAT,
  DOUBLE,
  LONGDOUBLE,
  POINTER,
  NULLPTR,
  VOID,
  VOIDPTR,
  FUNCPTR,
  FUNCTION,
};

class PTR {
public:
  PTR(void * _addr, int _size) { addr = _addr; alloc_size = _size; }
  enum INPUT_TYPE pointee_type; //TODO
  void * addr;
  int alloc_size;
};

class IVAR {
public:
  enum INPUT_TYPE type;
  std::string name;
};

template<class input_type>
class VAR : public IVAR {
public:

  VAR(input_type _input, std::string _name, enum INPUT_TYPE _type)
    : input(_input) { name = _name; type = _type; }

  VAR(input_type pointer_index, std::string _name
    , int _pointer_offset, enum INPUT_TYPE _type)
    : input(pointer_index) {
    name = _name;
    type = _type;
    pointer_offset = _pointer_offset;
  }

  input_type input;
  int pointer_offset;
};
#endif