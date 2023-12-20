#!/usr/bin/bash

gclang++ main.cc -g -O0 -o main
get-bc main

opt -enable-new-pm=0 -load ../../lib/carve_model_pass.so --carve < main.bc -o out.bc

clang++ out.bc -L ../../lib -l:m_carver.a -o out.carv

rm -rf carv_out
mkdir -p carv_out

./out.carv carv_out

