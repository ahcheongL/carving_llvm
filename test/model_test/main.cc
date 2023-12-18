#include <stdio.h>
#include <stdlib.h>

typedef struct _a {
  int f1;
  char f2;
  struct _a * next;
} structa;

void goo(int a) { return; }

void foo(int **ptr) {
  **ptr = 10;
  // ptr[0][0] = 20;

  int a = ptr[1][3];

  goo(a);

  return;
}

int foo2 (structa * s1) {
  fprintf(stderr, "%d, %d, %d, %d\n", s1->f1, s1->f2, s1->next->f1, s1->next->f2);
  return s1->f2;
}

char goo2 (structa s1){
  return s1.f2;
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


  structa * ptr = (structa *)malloc(sizeof(structa) * 2);
  ptr[0].f1 = 4; ptr[0].f2 = 5; ptr[0].next = ptr + 1;
  ptr[1].f1 = 6; ptr[1].f2 = 7; ptr[1].next = ptr;

  foo2(ptr);

  return 0;
}
