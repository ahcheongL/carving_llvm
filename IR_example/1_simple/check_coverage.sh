rm -rf carving cov-original cov-replay

mkdir carving
cd carving
gclang ../main.c -O0 -o main
get-bc main
../../../bin/carve_pass.py main.bc func_args
mkdir carve_inputs 
./main.carv 1 2 3 4 5 carve_inputs
cd ..
echo -e "\n"

mkdir cov-original
cd cov-original
gclang --coverage ../main.c -g -c -O0 -o main.o
gclang --coverage main.o -g -O0 -o main
./main 1 2 3 4 5
llvm-cov gcov main.gcno
cd ..

mkdir cov-replay
cd cov-replay
gclang --coverage ../main.c -g -O0 -c -o main.o
gclang --coverage main.o -g -O0 -o main
get-bc main
../../../bin/simple_unit_driver_pass.py main.bc foo
./main.foo.driver ../carving/carve_inputs/foo_1_0
llvm-cov gcov main.gcno
cd ..