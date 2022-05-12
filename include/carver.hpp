#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

enum inputtypes {
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

class IVAR {};

template<class input_type>
class VAR : public IVAR {
public:

  VAR(input_type _input, std::string _name) : input(_input), name(_name) {}

  input_type input;
  std::string name;
};

typedef struct _mem_info {
  void * addr;
  int size;
} mem_info;

void __CROWN_CARVER_INIT();

void __CROWN_MALLOC_PROBE(void * ptr, int size);
int __CROWN_POINTER_CARVER(void * ptr);

int __CROWN_CARVE_END(char *, int);
void __CROWN_REMOVE_CARVER(void * ptr);

void __CROWN_argv_modifier(int * argcptr, char *** argvptr);

char __CROWN_POINTER_CHECK_CARVER(void *, int);

void __CROWN_FINI();
#endif