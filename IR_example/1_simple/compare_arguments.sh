rm -rf carving replay

args="1 2 3 4 5"

mkdir carving
cd carving
gclang ../main.c -O0 -o main
get-bc main
../../../bin/carve_pass.py main.bc func_args
mkdir carve_inputs 
./main.carv $args carve_inputs
cd ..
echo -e "\n"

mkdir replay
cd replay
gclang --coverage ../main.c -g -O0 -c -o main.o
gclang --coverage main.o -g -O0 -o main
get-bc main
../../../bin/simple_unit_driver_pass.py main.bc foo
../../../bin/simple_unit_driver_pass.py main.bc goo
cd ..

func="foo"

echo -e "\nOriginal register dump ($func)"

gdb -batch -ex "b $func" -ex "run" -ex "i r rdi rsi" -ex "quit" --args ./carving/main $args | grep 'rdi\|rsi'
echo -e "\nReplay register dump"

gdb -batch -ex "b $func" -ex "run" -ex "i r rdi rsi" -ex "quit" --args ./replay/main.foo.driver ./carving/carve_inputs/foo_1_0 | grep 'rdi\|rsi'