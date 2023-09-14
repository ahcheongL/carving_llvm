#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

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
  POINTER();

  POINTER(void *_addr, int _size);

  POINTER(void *_addr, const char *pointee_type, int _size);

  void *addr;
  const char *pointee_type;
  int alloc_size;
};

class IVAR {
 public:
  IVAR();
  IVAR(char *name, enum INPUT_TYPE type);
  ~IVAR();
  enum INPUT_TYPE type;
  char *name;
};

template <class input_type>
class VAR : public IVAR {
 public:
  VAR();

  VAR(input_type _input, char *_name, enum INPUT_TYPE _type);

  VAR(input_type pointer_index, char *_name, int _pointer_offset,
      enum INPUT_TYPE _type);

  // copy constructor
  VAR(const VAR<input_type> &other);

  // move constructor
  VAR(VAR<input_type> &&other);

  VAR &operator=(const VAR<input_type> &other);

  VAR &operator=(VAR<input_type> &&other);

  input_type input;
  int pointer_offset;
};

template <class elem_type>
class vector {
 public:
  vector();

  vector(const vector<elem_type> &other);

  vector(vector<elem_type> &&other);

  vector &operator=(const vector<elem_type> &other);

  vector &operator=(vector<elem_type> &&other);

  void increase_capacity();

  void push_back(elem_type elem);

  void insert(int idx, elem_type elem);

  void pop_back();

  elem_type *operator[](int idx);

  elem_type *get(int idx);

  int get_idx(const elem_type elem);

  int size();

  void remove(int idx);

  elem_type *back();

  void clear();

  ~vector();

 private:
  elem_type *data;
  int capacity;
  int num_elem;
};

template <class Key, class Elem>
class map {
 public:
  class data_node {
   public:
    data_node();

    data_node(Key _key, Elem _elem, data_node *_left, data_node *_right,
              data_node *_parent);

    data_node(const data_node &other);

    data_node(data_node &&other);

    data_node &operator=(const data_node &other);

    data_node &operator=(data_node &&other);

    void insert_right(data_node *node);

    data_node *find_node(Key _key, data_node **parent_ptr);

    data_node *find_small_closest_node(Key _key);

    data_node *left;
    data_node *right;
    data_node *parent;
    Key key;
    Elem elem;
  };

  map();

  ~map();

  Elem *find(Key key);

  data_node *find_small_closest(Key key);

  data_node *get_by_idx(int idx);

  void insert(Key key, Elem elem);

  void move_or_remove_last(data_node *node);

  void remove(Key key);

  int size();

  void clear();

  data_node *nodes;
  data_node *root;
  int capacity;
  int num_nodes;
};

class FUNC_CONTEXT {
 public:
  FUNC_CONTEXT();

  FUNC_CONTEXT(int _carved_idx, int _func_call_idx, int _func_id);

  FUNC_CONTEXT(const FUNC_CONTEXT &other);

  FUNC_CONTEXT(FUNC_CONTEXT &&other);

  FUNC_CONTEXT &operator=(const FUNC_CONTEXT &other);

  FUNC_CONTEXT &operator=(FUNC_CONTEXT &&other);

  void update_carved_ptr_begin_idx();

  vector<IVAR *> inputs;
  vector<POINTER> carved_ptrs;
  int carved_ptr_begin_idx;
  int carving_index;
  int func_call_idx;
  int func_id;
  bool is_carved;
};

class typeinfo {
 public:
  char *type_name;
  int size;

  typeinfo(char *type_name, int size);

  typeinfo();

  typeinfo(int);

  typeinfo(const typeinfo &other);

  typeinfo(typeinfo &&other);

  typeinfo &operator=(const typeinfo &other);

  typeinfo &operator=(typeinfo &&other);
};

class classinfo {
 public:
  int class_index;
  int size;

  classinfo(int class_index, int size);

  classinfo();

  classinfo(int);

  classinfo(const classinfo &other);

  classinfo(classinfo &&other);

  classinfo &operator=(const classinfo &other);

  classinfo &operator=(classinfo &&other);
};

#endif