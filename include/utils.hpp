#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cstring>
#include <fstream>
#include <memory>
#include <assert.h>

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
  UNKNOWN_PTR,
};

class PTR {
public:

  PTR() : addr(0), pointee_type(0), alloc_size(0) {}

  PTR(void * _addr, int _size)
    : addr(_addr), pointee_type(0), alloc_size(_size)  {}

  PTR(void * _addr, const char * pointee_type, int _size)
    : addr(_addr), pointee_type(pointee_type), alloc_size(_size)  {}
  
  void * addr;
  const char * pointee_type;
  int alloc_size;
};

class PTR2 {
public:

  PTR2() : addr(0), pointee_type_id(0), alloc_size(0) {}

  PTR2(void * _addr, int _size)
    : addr(_addr), pointee_type_id(0), alloc_size(_size)  {}

  PTR2(void * _addr, int pointee_type, int _size)
    : addr(_addr), pointee_type_id(pointee_type), alloc_size(_size)  {}
  
  void * addr;
  int pointee_type_id;
  int alloc_size;
};

class IVAR {
public:
  IVAR() : type(INPUT_TYPE::CHAR), name(0) {}
  IVAR(char * name, enum INPUT_TYPE type) : name(name), type(type) {}
  ~IVAR() { free(name); }
  enum INPUT_TYPE type;
  char * name;
};

template<class input_type>
class VAR : public IVAR {
public:

  VAR() : input(0), pointer_offset(0), IVAR() {}

  VAR(input_type _input, char * _name, enum INPUT_TYPE _type)
    : input(_input), IVAR(_name, _type) {}

  VAR(input_type pointer_index, char * _name
    , int _pointer_offset, enum INPUT_TYPE _type)
    : input(pointer_index), IVAR(_name, _type), pointer_offset(_pointer_offset)
    {}
  
  //copy constructor
  VAR(const VAR<input_type> & other)
    : input(other.input), IVAR(strdup(other.name), other.type)
      , pointer_offset(other.pointer_offset) {}
  
  //move constructor
  VAR(VAR<input_type> && other)
    : input(other.input), IVAR(other.name, other.type)
      , pointer_offset(other.pointer_offset) {}
  
  VAR& operator=(const VAR<input_type> & other) {
    input = other.input;
    name = strdup(other.name);
    type = other.type;
    pointer_offset = other.pointer_offset;
    return *this;
  }

  VAR& operator=(VAR<input_type> && other) {
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

template<class elem_type>
class vector {
public:
  vector() {
    capacity = 128;
    num_elem = 0;
    data = new elem_type[capacity];
  }

  vector(const vector<elem_type> & other)
    : capacity(other.capacity), num_elem(other.num_elem)
      , data(new elem_type[other.capacity]) {
    for (int i = 0; i < num_elem; i++) {
      data[i] = other.data[i];
    }
  }

  vector(vector<elem_type> && other)
    : capacity(other.capacity), num_elem(other.num_elem)
      , data(other.data) {
    other.data = 0;
  }

  vector& operator=(const vector<elem_type> & other) {
    if (this != &other) {
      delete[] data;
      capacity = other.capacity;
      num_elem = other.num_elem;
      data = new elem_type[capacity];
      for (int i = 0; i < num_elem; i++) {
        data[i] = other.data[i];
      }
    }
    return *this;
  }

  vector& operator=(vector<elem_type> && other) {
    if (this != &other) {
      delete[] data;
      capacity = other.capacity;
      num_elem = other.num_elem;
      data = other.data;
      other.data = 0;
    }
    return *this;
  }

  void push_back(elem_type elem) {
    data[num_elem] = elem;
    num_elem++;
    if (num_elem >= capacity) {
      capacity *= 2;

      elem_type * tmp = new elem_type[capacity];
      int idx;
      for (idx = 0; idx < num_elem; idx++) {
        tmp[idx] = data[idx];
      }

      delete [] data;
      data = tmp;
    }
  }

  void pop_back() {
    if (num_elem == 0) return;
    num_elem--;
    return;
  }

  elem_type * operator[](int idx) {
    if (idx >= num_elem) { return 0; }
    return &(data[idx]);
  }

  elem_type * get(int idx) {
    if (idx >= num_elem) { return 0; }
    return &(data[idx]);
  }

  int get_idx(const elem_type elem) {
    int idx = 0;
    for (idx = 0 ; idx < num_elem; idx++) {
      if (elem == data[idx]) {
        return idx;
      }
    }
    return -1;
  }

  int size() {
    return num_elem;
  }

  void remove(int idx) {
    if (idx >= num_elem) { return ;}

    while (idx < num_elem - 1) {
      data[idx] = data[idx + 1];
      idx ++;
    }
    
    num_elem --;
  }

  elem_type * back() {
    if (num_elem == 0) return NULL;
    return &(data[num_elem - 1]);
  }

  void clear() {
    num_elem = 0;
  }

  ~vector() {
    delete [] data;
  }

private:
  elem_type * data;
  int capacity;
  int num_elem;
};

template<class Key, class Elem>
class map {
public:

  class data_node {
  public:
    data_node(Key _key, Elem _elem) {
      key = _key; elem = _elem; left = NULL; right= NULL;
    }

    data_node * insert_inner(Key _key, Elem _elem) {
      if (_key == key) {
        elem = _elem;
        return NULL;
      } else if (_key < key) {
        if (left == NULL) {
          left = new data_node(_key, _elem);
          return left;
        } else {
          data_node * inserted = left->insert_inner(_key, _elem);
          return inserted;
        }
      } else {
        if (right == NULL) {
          right = new data_node(_key, _elem);
          return right;
        } else {
          data_node * inserted = right->insert_inner(_key, _elem);
          return inserted;
        }
      }
    }

    Elem * find_inner(Key _key) {
      if (key == _key) { return &elem; }
      else if (_key < key) {
        if (left == NULL) { return NULL; }
        return left->find_inner(_key);
      } else {
        if (right == NULL) { return NULL; }
        return right->find_inner(_key);
      }
    }

    void insert_right(data_node * node) {
      if (right == NULL) {
        right = node;
      } else {
        data_node * ptr = right;
        while (ptr->right != NULL) {
          ptr = ptr->right;
        }
        ptr->right = node;
      }
    }

    data_node * remove_key(data_node ** parent, Key _key) {
      if (key == _key) {
        if (left == NULL) {
          *parent = right;
        } else if (right == NULL) {
          *parent = left;
        } else {
          left->insert_right(right);
          *parent = left;
        }

        return this;
      } else if (_key < key) {
        if (left == NULL) { return NULL; }
        return left->remove_key(&left, _key);
      } else {
        if (right == NULL) { return NULL; }
        return right->remove_key(&right, _key);
      }
    }

    data_node *left;
    data_node *right;
    Key key; 
    Elem elem;
  };

  map() {
    root = NULL;
    capacity = 128;
    num_nodes = 0;
    nodes = (data_node **) malloc(sizeof(data_node *) * capacity);
  }

  ~map() {
    int idx = 0;
    for (idx = 0; idx < num_nodes; idx++) {
      delete nodes[idx];
    }
    free(nodes);
  }

  Elem* find (Key key) {
    if (root == NULL) { return NULL; }
    return root->find_inner(key);
  }

  Elem * operator[](Key key) {
    if (root == NULL) { return NULL; }
    return root->find_inner(key);
  }

  data_node * get_by_idx(int idx) {
    if (idx >= num_nodes) { return NULL; }
    return nodes[idx];
  }

  void insert(Key key, Elem elem) {
    if (root == NULL) {
      root = new data_node(key, elem);
      nodes[num_nodes++] = root;
    } else {
      data_node * inserted = root->insert_inner(key, elem);
      if (inserted != NULL) {
        nodes[num_nodes++] = inserted;
        if (num_nodes >= capacity) {
          capacity *= 2;
          nodes = (data_node **) realloc(nodes, sizeof(data_node *) * capacity);
        }
      }
    }

    return;
  }

  void remove(Key key) {
    if (root == NULL) {
      return;
    } else {
      //update nodes array
      data_node * removed = NULL;

      if (root->key == key) {
        removed = root;

        if (root->left != NULL) {
          data_node * tmp = root->left;
          if (root->right != NULL) {
            root->left->insert_right(root->right);
          }
          root = tmp;          
        } else {
          if (root->right == NULL) {
            root = NULL;
          } else {
            data_node * tmp = root->right;
            root = tmp;
          }
        }
      } else if (key < root->key) {
        if (root->left == NULL) { return; }
        removed = root->left->remove_key(&(root->left), key);
      } else {
        if (root->right == NULL) { return; }
        removed = root->right->remove_key(&(root->right), key);
      }
      if (removed != NULL) {
        int idx = 0;
        for (idx = 0; idx < num_nodes; idx++) {
          if (nodes[idx] == removed) {
            memmove(&(nodes[idx]), &(nodes[idx+1])
              , sizeof(data_node *) * (num_nodes - idx - 1));
            num_nodes --;
            break;
          }
        }

        delete removed;
      }
    }
    
    return;
  }

  int size() {
    return num_nodes;
  }

private:
  data_node * root;
  data_node ** nodes;
  int capacity;
  int num_nodes;
};

class FUNC_CONTEXT {
public:

  FUNC_CONTEXT() : carved_ptr_begin_idx(0), carving_index(0)
    , func_call_idx(0), func_id(0), is_carved(false) {}

  FUNC_CONTEXT(int _carved_idx, int _func_call_idx, int _func_id)
    : carving_index(_carved_idx), func_call_idx(_func_call_idx)
      , carved_ptr_begin_idx(0)
      , inputs(), carved_ptrs(), func_id(_func_id), is_carved(true) {}

  FUNC_CONTEXT(const FUNC_CONTEXT & other) :
    carving_index(other.carving_index), func_call_idx(other.func_call_idx)
    , carved_ptr_begin_idx(other.carved_ptr_begin_idx)
    , inputs(other.inputs), carved_ptrs(other.carved_ptrs)
    , func_id(other.func_id), is_carved(other.is_carved) {}
  
  FUNC_CONTEXT(FUNC_CONTEXT && other) :
    carving_index(other.carving_index), func_call_idx(other.func_call_idx)
    , carved_ptr_begin_idx(other.carved_ptr_begin_idx)
    , inputs(other.inputs), carved_ptrs(other.carved_ptrs)
    , func_id(other.func_id), is_carved(other.is_carved) {}

  FUNC_CONTEXT& operator=(const FUNC_CONTEXT & other) {
    carving_index = other.carving_index;
    func_call_idx = other.func_call_idx;
    carved_ptr_begin_idx = other.carved_ptr_begin_idx;
    inputs = other.inputs;
    carved_ptrs = other.carved_ptrs;
    func_id = other.func_id;
    is_carved = other.is_carved;
    return *this;
  }

  FUNC_CONTEXT& operator=(FUNC_CONTEXT && other) {
    carving_index = other.carving_index;
    func_call_idx = other.func_call_idx;
    carved_ptr_begin_idx = other.carved_ptr_begin_idx;
    inputs = other.inputs;
    carved_ptrs = other.carved_ptrs;
    func_id = other.func_id;
    is_carved = other.is_carved;
    return *this;
  }

  void update_carved_ptr_begin_idx() {
    carved_ptr_begin_idx = carved_ptrs.size();
  }

  vector<IVAR *> inputs;
  vector<PTR> carved_ptrs;
  int carved_ptr_begin_idx;
  int carving_index;
  int func_call_idx;
  int func_id;
  bool is_carved;
};

#endif