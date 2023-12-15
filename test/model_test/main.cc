#include<stdio.h>
#include<stdlib.h>


void goo(int a) {

  return;
}


void foo(int * ptr) {

  *ptr = 10;
  ptr[0] = 20;

  int a = ptr[2];
  
  goo(a);

  return;
}



int main() {


  int * a = (int *) malloc(sizeof(int) * 4);

  if (a == nullptr) { return -1; }

  a[0] = 20;
  a[1] = 34;
  a[2] = 60;
  a[3] = 71;

  foo(a);

  free(a);

  return 0;
}
