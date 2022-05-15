#include<stdio.h>
#include<stdlib.h>


int foo (int a , int b) {
  return a + b;
}

int goo (int * ptr) {
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
