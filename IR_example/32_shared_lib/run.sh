#!/usr/bin/bash

gclang++ -shared foo.cc -fPIC -o libfoo.so
gclang++ main.cc -g -O0 -fPIC -c -o main.o
gclang++ main.o -L . -lfoo -o main
get-bc main

opt -enable-new-pm=0 -load ../../lib/carve_model_pass.so --carve < main.bc -o out.bc

llvm-dis out.bc

clang++ out.bc -L ../../lib -l:m_carver.a -L . -lfoo -Wl,-rpath=`pwd` -o out.carv

rm -rf carv_out
mkdir -p carv_out

../../pin/pin -t ../../pintool/obj-intel64/MemoryTrackTool.so --  ./out.carv ./carv_out

