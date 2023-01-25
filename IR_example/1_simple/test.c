#include<stdio.h>
#include<stdlib.h>


int goo (int * ptr);

int foo (int a , int b) {
  fprintf(stderr, "foo input : %d, %d\n", a, b);
  int arr[5];
  arr[4] = 30;
  return a + b + goo(arr);
}

int goo (int * ptr) {
  fprintf(stderr, "goo[4] : %d\n", ptr[4]);
  return ptr[4];
}


int main(int argc, char * argv[]) {
 int a = 3;
 int b = argc;

 int c = foo(a, b);

 int * d = (int *) malloc(sizeof(int) * 10);
 d[4] = 4023;

 int e = goo(d);

 free(d);
 

 return 0;

}
