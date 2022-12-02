#include<stdio.h>
#include<stdlib.h>


int foo (int ** f1) {
  for (int i=0; i<4; i++){

  }
  fprintf(stderr, )
  return (*f1)[2];
}

int main(int argc, char * argv[]) {

  int ** ptr = malloc(sizeof(int*) * 3);
  int idx;
  for (idx = 0; idx < 4; idx++) { ptr[idx] = malloc(sizeof(int) * 4); }

  int d = foo(ptr);

  for (idx = 0; idx < 4; idx++) { free(ptr[idx]); }
  free(ptr);

  return d;
}
