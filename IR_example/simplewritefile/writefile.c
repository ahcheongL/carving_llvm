#include<stdio.h>
#include<stdlib.h>


void foo(FILE * f) {
  fwrite("asdfsdf\n", 1, 9, f);

  int a = 243;
  fwrite(&a, 4, 1, f);
  fclose(f);

  fprintf(stderr, " wrote a file\n");

}


int main (int argc, char * argv []) {
  char * outdir = argv[1];
  char buf[240];
  snprintf(buf, 240, "%s/%s", outdir, "123");
  FILE * f = fopen(buf, "wb");
  foo(f);

  char * ptr = malloc(10);

  free(ptr);
}
