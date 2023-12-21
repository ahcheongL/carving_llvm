#!/usr/bin/bash

clang++ main.cc -I ../../include -g -O0 ../../src/utils/ptr_map.o \
     ../../src/utils/data_utils.o -o main
./main

# gprof main gmon.out > analysis.txt