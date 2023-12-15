#include <stdio.h>
#include <stdlib.h>

void goo(int a) { return; }

void foo(int **ptr) {
  **ptr = 10;
  ptr[0][0] = 20;

  int a = ptr[1][3];

  goo(a);

  return;
}

int main() {
  int **dptr = (int **)malloc(sizeof(int *) * 2);
  dptr[0] = (int *)malloc(sizeof(int) * 4);
  dptr[1] = (int *)malloc(sizeof(int) * 4);

  if (dptr[1] == nullptr) {
    return -1;
  }

  int *a = dptr[1];

  a[0] = 20;
  a[1] = 34;
  a[2] = 60;
  a[3] = 71;

  foo(dptr);
  return 0;
}
