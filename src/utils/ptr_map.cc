
#include "utils/ptr_map.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

//////////////////////
// ptr_map::rbtree_node
///////////////////////

ptr_map::rbtree_node::rbtree_node(void *key, char *type_name, int alloc_size)
    : key_(key), type_name_(type_name), alloc_size_(alloc_size) {}

ptr_map::rbtree_node::~rbtree_node() {
  delete left_;
  delete right_;
}

ptr_map::rbtree_node *ptr_map::rbtree_node::get_uncle() {
  if (parent_ == nullptr) {
    return nullptr;
  }

  if (parent_->parent_ == nullptr) {
    return nullptr;
  }

  if (parent_->parent_->left_ == parent_) {
    return parent_->parent_->right_;
  } else {
    return parent_->parent_->left_;
  }
}

ptr_map::rbtree_node *ptr_map::rbtree_node::get_grandparent() {
  if (parent_ == nullptr) {
    return nullptr;
  }

  return parent_->parent_;
}

//////////////////////
// ptr_map
//////////////////////

ptr_map::ptr_map() {
  for (int i = 0; i < MAX_CACHE_ENTRY; i++) {
    cache[i].availability_ = false;
  }

  memset(roots, 0, sizeof(rbtree_node *) * ROOT_ENTRY);
}

ptr_map::~ptr_map() {
  for (int i = 0; i < ROOT_ENTRY; i++) {
    delete roots[i];
  }
}

void ptr_map::insert(void *key, char *type_name, int alloc_size) {
  rbtree_node *new_node = new rbtree_node(key, type_name, alloc_size);

  unsigned int root_hash = ROOT_HASH(key);

  // TODO check memory boundary

  rbtree_node *root = roots[root_hash];

  if (root == nullptr) {
    root = new_node;
    root->color_ = BLACK;
    roots[root_hash] = root;
    return;
  }

  rbtree_node *n = root;
  while (1) {
    if (key == n->key_) {
      delete new_node;

      // Overlapping memory regions... how?
      // Maybe update?
      n->key_ = key;
      n->type_name_ = type_name;
      n->alloc_size_ = alloc_size;
      return;
    } else if (key < n->key_) {
      if (n->left_ == nullptr) {
        n->left_ = new_node;
        new_node->parent_ = n;
        break;
      } else {
        n = n->left_;
      }
    } else {
      if (n->right_ == nullptr) {
        n->right_ = new_node;
        new_node->parent_ = n;
        break;
      } else {
        n = n->right_;
      }
    }
  }

  // assert(new_node->parent_ != nullptr);

  insert_case2(new_node);

  unsigned int cache_hash = CACHE_HASH(key);
  cache[cache_hash].node_ = new_node;
  cache[cache_hash].availability_ = true;
  return;
}

void ptr_map::insert_case1(rbtree_node *n) {
  if (n->parent_ == nullptr) {
    n->color_ = BLACK;
  } else {
    insert_case2(n);
  }
}

void ptr_map::insert_case2(rbtree_node *n) {
  if (n->parent_->color_ == BLACK) {
    return;
  } else {
    insert_case3(n);
  }
}

void ptr_map::insert_case3(rbtree_node *n) {
  rbtree_node *uncle = n->get_uncle();

  if ((uncle != nullptr) && (uncle->color_ == RED)) {
    n->parent_->color_ = BLACK;
    uncle->color_ = BLACK;
    rbtree_node *g = n->get_grandparent();
    g->color_ = RED;
    insert_case1(g);
  } else {
    insert_case4(n);
  }
}

void ptr_map::insert_case4(rbtree_node *n) {
  rbtree_node *grandp = n->get_grandparent();
  rbtree_node *parent = n->parent_;

  if ((n == n->parent_->right_) && (parent == grandp->left_)) {
    rotate_left(parent);
    n = n->left_;
  } else if ((n == parent->left_) && (parent == grandp->right_)) {
    rotate_right(parent);
    n = n->right_;
  }

  // case 5

  parent = n->parent_;
  grandp = n->get_grandparent();

  parent->color_ = BLACK;
  grandp->color_ = RED;

  if (n == parent->left_) {
    rotate_right(grandp);
  } else {
    rotate_left(grandp);
  }
}

void ptr_map::rotate_left(rbtree_node *n) {
  rbtree_node *child = n->right_;
  rbtree_node *parent = n->parent_;

  if (child->left_ != nullptr) {
    child->left_->parent_ = n;
  }

  n->right_ = child->left_;
  n->parent_ = child;
  child->left_ = n;
  child->parent_ = parent;

  if (parent != nullptr) {
    if (parent->left_ == n) {
      parent->left_ = child;
    } else {
      parent->right_ = child;
    }
  } else {
    unsigned int root_hash = ROOT_HASH(n->key_);
    roots[root_hash] = child;
  }
}

void ptr_map::rotate_right(rbtree_node *n) {
  rbtree_node *child = n->left_;
  rbtree_node *parent = n->parent_;

  if (child->right_ != nullptr) {
    child->right_->parent_ = n;
  }

  n->left_ = child->right_;
  n->parent_ = child;
  child->right_ = n;
  child->parent_ = parent;

  if (parent != nullptr) {
    if (parent->right_ == n) {
      parent->right_ = child;
    } else {
      parent->left_ = child;
    }
  } else {
    unsigned int root_hash = ROOT_HASH(n->key_);
    roots[root_hash] = child;
  }
}

ptr_map::rbtree_node *ptr_map::find(void *key) {
  unsigned int cache_hash = CACHE_HASH(key);
  unsigned long key_v = (unsigned long)key;

  if (cache[cache_hash].availability_ == true) {
    unsigned long cache_key_v = (unsigned long)cache[cache_hash].node_->key_;

    // Include the end point
    if (cache_key_v <= key_v &&
        (cache_key_v + cache[cache_hash].node_->alloc_size_) >= key_v) {
      return cache[cache_hash].node_;
    }
  }

  unsigned int root_hash = ROOT_HASH(key_v);

  rbtree_node *n = roots[root_hash];
  if (n == nullptr) {
    return nullptr;
  }

  while (n != nullptr) {
    unsigned long n_key = (unsigned long)n->key_;

    // Should we include the end point here?
    if ((n_key <= key_v) && ((n_key + n->alloc_size_) > key_v)) {
      cache_hash = CACHE_HASH(n_key);
      cache[cache_hash].node_ = n;
      cache[cache_hash].availability_ = true;
      return n;
    } else if (key_v < n_key) {
      n = n->left_;
    } else {
      n = n->right_;
    }
  }

  return nullptr;
}

void ptr_map::remove(void *key) {
  unsigned long key_v = (unsigned long)key;

  unsigned int root_hash = ROOT_HASH(key_v);
  unsigned int cache_hash = CACHE_HASH(key_v);

  rbtree_node *node = nullptr;

  if (cache[cache_hash].availability_ == true) {
    if (cache[cache_hash].node_->key_ == key) {
      node = cache[cache_hash].node_;
      cache[cache_hash].availability_ = false;
    }
  }

  if (node == nullptr) {
    rbtree_node *n = roots[root_hash];
    if (n == nullptr) {
      return;
    }

    while (n != nullptr) {
      unsigned long n_key = (unsigned long)n->key_;

      if (n->key_ == key) {
        node = n;
        break;
      } else if (key_v < n_key) {
        n = n->left_;
      } else {
        n = n->right_;
      }
    }
  }

  if (node == nullptr) {
    return;
  }

  rbtree_node *node_to_delete = node;

  if (node->left_ != nullptr && node->right_ != nullptr) {
    rbtree_node *pred = node->left_;
    while (pred->right_ != nullptr) {
      pred = pred->right_;
    }

    node->key_ = pred->key_;
    node->type_name_ = pred->type_name_;
    node->alloc_size_ = pred->alloc_size_;

    if ((cache[cache_hash].availability_ == true) &&
        (cache[cache_hash].node_ == pred)) {
      cache[cache_hash].node_ = node;
    }

    // Remove pred instead.
    node_to_delete = pred;
  }

  //   assert(node->right_ == nullptr || node->left_ == nullptr);

  rbtree_node *child = node_to_delete->right_ == nullptr
                           ? node_to_delete->left_
                           : node_to_delete->right_;

  // assert(node_to_delete->right_ == nullptr || node_to_delete->left_ ==
  // nullptr);

  if (node_to_delete->color_ == rbtree_node_color::BLACK) {
    node_to_delete->color_ =
        child == nullptr ? rbtree_node_color::BLACK : child->color_;
    delete_case1(node_to_delete);
  }

  if (node_to_delete->parent_ == nullptr) {
    roots[root_hash] = child;

    if (child != nullptr) {
      child->color_ = rbtree_node_color::BLACK;
    }
  } else {
    if (node_to_delete == node_to_delete->parent_->left_) {
      node_to_delete->parent_->left_ = child;
    } else {
      node_to_delete->parent_->right_ = child;
    }
  }

  if (child != nullptr) {
    child->parent_ = node_to_delete->parent_;
  }

  node_to_delete->left_ = nullptr;
  node_to_delete->right_ = nullptr;
  delete node_to_delete;
  return;
}

void ptr_map::delete_case1(rbtree_node *n) {
  if (n->parent_ == nullptr) {
    return;
  }

  delete_case2(n);
}

void ptr_map::delete_case2(rbtree_node *n) {
  rbtree_node *s =
      n->parent_->left_ == n ? n->parent_->right_ : n->parent_->left_;

  assert(s != nullptr);

  if (s->color_ == rbtree_node_color::RED) {
    n->parent_->color_ = rbtree_node_color::RED;
    s->color_ = rbtree_node_color::BLACK;
    if (n == n->parent_->left_) {
      rotate_left(n->parent_);
    } else {
      rotate_right(n->parent_);
    }
  }

  delete_case3(n);
}

void ptr_map::delete_case3(rbtree_node *n) {
  rbtree_node *s =
      n->parent_->left_ == n ? n->parent_->right_ : n->parent_->left_;

  assert(s != nullptr);

  if (n->parent_->color_ == rbtree_node_color::BLACK &&
      s->color_ == rbtree_node_color::BLACK &&
      (s->left_ == nullptr || s->left_->color_ == rbtree_node_color::BLACK) &&
      (s->right_ == nullptr || s->right_->color_ == rbtree_node_color::BLACK)) {
    s->color_ = rbtree_node_color::RED;
    delete_case1(n->parent_);
  } else {
    delete_case4(n);
  }
}

void ptr_map::delete_case4(rbtree_node *n) {
  rbtree_node *s =
      n->parent_->left_ == n ? n->parent_->right_ : n->parent_->left_;

  assert(s != nullptr);

  if (n->parent_->color_ == rbtree_node_color::RED &&
      s->color_ == rbtree_node_color::BLACK &&
      (s->left_ == nullptr || s->left_->color_ == rbtree_node_color::BLACK) &&
      (s->right_ == nullptr || s->right_->color_ == rbtree_node_color::BLACK)) {
    s->color_ = rbtree_node_color::RED;
    n->parent_->color_ = rbtree_node_color::BLACK;
  } else {
    delete_case5(n);
  }
}

void ptr_map::delete_case5(rbtree_node *n) {
  rbtree_node *s =
      n->parent_->left_ == n ? n->parent_->right_ : n->parent_->left_;

  assert(s != nullptr);

  if (s->color_ == rbtree_node_color::BLACK) {
    if (n == n->parent_->left_ &&
        (s->right_ == nullptr ||
         s->right_->color_ == rbtree_node_color::BLACK) &&
        (s->left_ != nullptr && s->left_->color_ == rbtree_node_color::RED)) {
      s->color_ = rbtree_node_color::RED;
      s->left_->color_ = rbtree_node_color::BLACK;
      rotate_right(s);
    } else if (n == n->parent_->right_ &&
               (s->left_ == nullptr ||
                s->left_->color_ == rbtree_node_color::BLACK) &&
               (s->right_ != nullptr &&
                s->right_->color_ == rbtree_node_color::RED)) {
      s->color_ = rbtree_node_color::RED;
      s->right_->color_ = rbtree_node_color::BLACK;
      rotate_left(s);
    }
  }

  delete_case6(n);
}

void ptr_map::delete_case6(rbtree_node *n) {
  rbtree_node *s =
      n->parent_->left_ == n ? n->parent_->right_ : n->parent_->left_;

  assert(s != nullptr);

  s->color_ = n->parent_->color_;
  n->parent_->color_ = rbtree_node_color::BLACK;

  if (n == n->parent_->left_) {
    assert(s->right_->color_ == rbtree_node_color::RED);
    s->right_->color_ = rbtree_node_color::BLACK;
    rotate_left(n->parent_);
  } else {
    assert(s->left_->color_ == rbtree_node_color::RED);
    s->left_->color_ = rbtree_node_color::BLACK;
    rotate_right(n->parent_);
  }
}

void ptr_map::print_tree_sub(rbtree_node *node, unsigned int depth) {
  if (node == nullptr) {
    return;
  }

  for (int i = 0; i < depth; i++) {
    std::cerr << "  ";
  }

  std::cerr << node << ":"
            << "color : " << node->color_ << ", key : " << node->key_
            << ", left : " << node->left_ << ", right : " << node->right_
            << std::endl;

  print_tree_sub(node->left_, depth + 1);
  print_tree_sub(node->right_, depth + 1);
}

void ptr_map::print_tree(rbtree_node *node, unsigned int root_hash) {
  std::cerr << "Print tree : root : " << node << ", hash : " << root_hash
            << std::endl;

  if (node == nullptr) {
    return;
  }

  print_tree_sub(node, 0);

  std::cerr << std::endl;
}

void ptr_map::print_cache() {
  int num_enabled_entry = 0;

  for (int i = 0; i < MAX_CACHE_ENTRY; i++) {
    if (cache[i].availability_ == true) {
      std::cerr << "Cache : " << i << ", key : " << cache[i].node_->key_
                << ", node : " << cache[i].node_ << std::endl;
      num_enabled_entry++;
    }
  }

  std::cerr << "Num enabled entry : " << num_enabled_entry << std::endl;
  return;
}