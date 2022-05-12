ifeq (, $(shell which llvm-config))
$(error "No llvm-config in $$PATH")
endif

LLVMVER  = $(shell llvm-config --version 2>/dev/null | sed 's/git//' | sed 's/svn//' )
LLVM_MAJOR = $(shell llvm-config --version 2>/dev/null | sed 's/\..*//' )
LLVM_MINOR = $(shell llvm-config --version 2>/dev/null | sed 's/.*\.//' | sed 's/git//' | sed 's/svn//' | sed 's/ .*//' )
$(info Detected LLVM VERSION : $(LLVMVER))

ifneq ($(LLVM_MAJOR), 13)
$(info Warning : not LLVM 13 version)
endif

CC=clang
CXX=clang++
CFLAGS=`llvm-config --cflags` -fPIC
CXXFLAGS=`llvm-config --cxxflags` -fPIC
AR=ar

MAKEFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR:=$(dir $(MAKEFILE_PATH))

all: lib/pass1.so lib/carver.a lib/probe_names.txt

lib/pass1.so: src/pass1.cc
	$(CXX) $(CXXFLAGS) -ggdb -O0 -shared $(MAKEFILE_DIR)/$< -o $@

src/carver.o: src/carver.cc
	$(CC) $(CFLAGS) -I include/ -c $^ -o $@

lib/carver.a: src/carver.o
	$(AR) rsv $@ $^

lib/probe_names.txt: src/carver.o
	python3 bin/get_probe_name.py

clean:
	rm lib/pass1.so
	rm lib/carver.a
	rm src/carver.o
	rm lib/probe_names.txt
