#ifndef __CROWN_CARVE_HEADER
#define __CROWN_CARVE_HEADER

#include <iostream>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
  PTR(void * _addr, int _size)
    : addr(_addr), pointee_type(0), alloc_size(_size)  {}

  PTR(void * _addr, char * pointee_type, int _size)
    : addr(_addr), pointee_type(pointee_type), alloc_size(_size)  {}
  
  void * addr;
  char * pointee_type;
  int alloc_size;
};

class IVAR {
public:
  IVAR(char * name, enum INPUT_TYPE type) : name(name), type(type) {}
  ~IVAR() { free(name); }
  enum INPUT_TYPE type;
  char * name;
};

template<class input_type>
class VAR : public IVAR {
public:

  VAR(input_type _input, char * _name, enum INPUT_TYPE _type)
    : input(_input), IVAR(_name, _type) {}

  VAR(input_type pointer_index, char * _name
    , int _pointer_offset, enum INPUT_TYPE _type)
    : input(pointer_index), IVAR(_name, _type), pointer_offset(_pointer_offset)
    {}

  input_type input;
  int pointer_offset;
};

template<class elem_type>
class vector {
public:
  vector() {
    capacity = 128;
    num_elem = 0;
    data = (elem_type *) malloc(sizeof(elem_type) * capacity);
  }

  void push_back(elem_type elem) {
    data[num_elem] = elem;
    num_elem++;
    if (num_elem >= capacity) {
      capacity *= 2;
      
      data = (elem_type *) realloc(data, sizeof(elem_type) * capacity);
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

  int get_idx(elem_type elem) {
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
    free(data);
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
        node->right->insert_right(node);
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

  data_node * operator[](int idx) {
    if (idx >= num_nodes) { return 0; }
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

  FUNC_CONTEXT(int _carved_idx, int _func_call_idx)
    : carving_index(_carved_idx), func_call_idx(_func_call_idx)
      , inputs(), carved_ptrs() {
    carved_ptr_begin_idx = 0;
  }

  void update_carved_ptr_begin_idx() {
    carved_ptr_begin_idx = carved_ptrs.size();
  }

  vector<IVAR *> inputs;
  vector<PTR *> carved_ptrs;
  int carved_ptr_begin_idx;
  int carving_index;
  int func_call_idx;
};

#endif