#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

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
  VOID,
  VOIDPOINTER,
  FUNCPOINTER,
  FUNCTION,
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

  input_type input;
};

typedef struct _mem_info {
  void * addr;
  int size;
} mem_info;

void __carv_init();

void __mem_allocated_probe(void * ptr, int size);
void __remove_mem_allocated_probe(void * ptr);

int __CROWN_POINTER_CARVER(void * ptr);
void Write_carved(char *, int);

void __argv_modifier(int * argcptr, char *** argvptr);
void __carv_FINI();
#endif