#include<stdlib.h>
#include<stdio.h>

static FILE * call_seq_file;

void open_call_seq_file() {
  call_seq_file = fopen("call_seq.txt", "w");
  fprintf(call_seq_file, "main\n");
}

void close_call_seq_file() {
  fclose(call_seq_file);
}

void write_call_seq_file(const char *s) {
  fprintf(call_seq_file, "%s\n", s);
}
