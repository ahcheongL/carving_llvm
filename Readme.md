# Carving by instrumenting LLVM IR code

## Prerequisite

1. Clang/LLVM, developed on version 13.0.1, not tested on other versions.
    * It is recommend to use apt package downloader, https://apt.llvm.org/
    * `sudo apt install llvm-13-dev clang-13 libclang-13-dev lld-13 libc++abi-13-dev`
    * It assumes `llvm-config, clang, clang++, ...` are on `PATH`

2. [gllvm](https://github.com/SRI-CSL/gllvm), needed to get whole program bitcode

3. Python 3.6+, make

## Build
  `make`

## 1. Target subject build

1. Build target program using `make` or `cmake` as usual, but set `CC=gclang` and `CXX=gclang++` to let gllvm to compile the target program. It varies how to set compiler to use for different programs, but most popular open source programs support building with non-default compiler.
    * It is recommend to use `--disable-shared` flags.
        * Codes that is build in shared libraries won't be instrumented for carving, so you won't able to get carved objects.
    * It is recommend to turn on debug options, but it is not necessary.
    * Example : ``CC=gclang CXX=gclang++ CFLAGS="-O0 -g" CXXFLAGS="-O0 -g" ./configure --prefix=`pwd`\gclang_install --disable-shared``
2. `get-bc <target executable>` You can get bitcode of the executable file.
3. (optional) `llvm-dis <target.bc>` will make human-readable LLVM IR code of the target program.

## 2. Carving Instrumentation

1. `./bin/carve_pass.py <target.bc> func_args <compile flags>`
    * `<compile flags>` are usually shared libraries that are linked to the original target executable.
    * You can get list of shared linked shared libraries by running `ldd <target executable>`, if libpthread.so is linked, you need to put `-lpthread` as compile flags
    * You can control functions to carve by writing `targets.txt` file.

2. It will generate an executable named as like `target.carv`.

## 3. Run carving

1. `mkdir carve_inputs`
2. `<target.carv> <args> carve_inputs`; Run the new carving executable as similar as the original executable, but add a directory to store all carved states.
    * Currently it suffers heavy overhead due to naive implementation, I'm fixing it now...

## 4. Replay

1. `./bin/simple_unit_driver_pass.py <target.bc> <target function> <compile flags>`
    * It will generate a new executable named as like `target.targetfunction.driver`
2. `<target.targetfunction.driver> <carved unit state file>`
    * Use gdb to check parameter and global variables are correctly set.

3. If you want to get coverage data, use `simple_unit_driver_pass_coverage.py` instead of `simple_unit_driver_pass.py`. You don't have to build the original bitcode file again with `--coverage` option.
    * gllvm has a bug that failure on measuring coverage when you compile and link at the same time. Seperate the compile and link commands.
        * Example : `gclang main.c --coverage -c -o main.o; gclang main.o --coverage -o main`
