
rm -rf carv_out out* *.bc main

gclang++ main.cc -O0 -g -o main
get-bc main

opt -enable-new-pm=0 -load ../../lib/carve_model_pass.so --carve -crash < main.bc -o out.bc

clang++ -O0 -g out.bc -o out.carv -L ../../lib -l:m_carver.a 

mkdir -p carv_out

./out.carv carv_out
