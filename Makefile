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
CXXFLAGS=`llvm-config --cxxflags` -fPIC -ggdb -O0
AR=ar

MAKEFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR:=$(dir $(MAKEFILE_PATH))

all: lib/carver_pass.so lib/carver.a lib/carver_probe_names.txt
all: lib/driver_pass.so lib/driver.a lib/driver_probe_names.txt

lib/carver_pass.so: src/carver_pass.cc
	$(CXX) $(CXXFLAGS) -shared $(MAKEFILE_DIR)/$< -o $@

src/carver.o: src/carver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/carver.cc -o $@

lib/carver.a: src/carver.o
	$(AR) rsv $@ $^

src/driver.o: src/driver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/driver.cc -o $@

lib/driver.a: src/driver.o
	$(AR) rsv $@ $^

lib/driver_pass.so: src/driver_pass.cc
	$(CXX) $(CXXFLAGS) -shared $(MAKEFILE_DIR)/$< -o $@

lib/carver_probe_names.txt: src/carver.o src/carver_probes.txt
	python3 bin/get_carver_probe_name.py

lib/driver_probe_names.txt: src/driver.o src/driver_probes.txt
	python3 bin/get_driver_probe_name.py

clean:
	rm lib/carver_pass.so
	rm lib/carver.a
	rm src/carver.o
	rm lib/carver_probe_names.txt
	rm lib/driver_probe_names.txt
