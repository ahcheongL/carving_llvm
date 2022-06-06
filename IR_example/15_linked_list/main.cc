#include<iostream>
#include<cstdlib>

class Node {
public:
  Node(int _elem) : elem(_elem) { next = NULL; }

  int elem;
  class Node * next;
};

void foo(class Node *) {
  return;
}

int main(int argc, char * argv[]) {
  class Node * root = new Node(14);
  root->next = new Node(20);
  root->next->next = new Node(15);
  root->next->next->next = root;

  foo(root);
  return 0;
}
