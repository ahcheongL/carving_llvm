#include<stdio.h>
#include<stdlib.h>

typedef struct _a {
  int f1;
  char f2;
} structa;

int foo (structa * s1) {
  fprintf(stderr, "foo: *s1 = {f1 = %d, f2 = %d}\n", s1->f1, s1->f2);
  return s1->f2;
}

int main(int argc, char * argv[]) {

  structa * ptr = malloc(sizeof(structa) * 2);
  ptr[0].f1 = 4; ptr[0].f2 = 5;
  ptr[1].f1 = 6; ptr[1].f2 = 7;
  foo(ptr);
  free(ptr);

  return 0;
}
