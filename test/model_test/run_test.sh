
rm -rf carv_out out* *.bc main

gclang++ main.cc -O0 -g -o main
get-bc main

opt -enable-new-pm=0 -load ../../lib/carve_model_pass.so --carve --target=targets.txt  < main.bc -o out.bc

llvm-dis out.bc

clang++ -O0 -g out.bc -o out.carv -L ../../lib -l:m_carver.a 

mkdir -p carv_out

../../pin/pin -t ../../pintool/obj-intel64/MemoryTrackTool.so -- ./out.carv carv_out
