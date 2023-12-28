#!/usr/bin/bash


gclang++ fuzz.cc -o fuzz.out -g -O0 -I ../../../include ../../../src/utils/ptr_map.o

clang++ fuzz.cc -o fuzz.asan -fsanitize=address -g -O0 -I ../../../include ../../../src/utils/ptr_map.o
clang++ fuzz.cc -o fuzz.udb -fsanitize=undefined -g -O0 -I ../../../include ../../../src/utils/ptr_map.o

get-bc fuzz.out
