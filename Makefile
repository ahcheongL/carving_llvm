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
CFLAGS=`llvm-config --cflags` -fPIC -ggdb -O0
CXXFLAGS=`llvm-config --cxxflags` -fPIC -ggdb -O0
AR=ar

MAKEFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR:=$(dir $(MAKEFILE_PATH))

all: lib/carver_pass.so lib/carver.a lib/carver_probe_names.txt
all: lib/driver_pass.so lib/driver.a lib/driver_probe_names.txt

lib/carver_pass.so: src/carver_pass.cc include/pass.hpp src/pass_utils.o
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/pass_utils.o -o $@

src/carver.o: src/carver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/carver.cc -o $@

lib/carver.a: src/carver.o
	$(AR) rsv $@ $^

lib/driver_pass.so: src/driver_pass.cc include/pass.hpp src/pass_utils.o
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/pass_utils.o -o $@

src/driver.o: src/driver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/driver.cc -o $@

lib/driver.a: src/driver.o
	$(AR) rsv $@ $^

lib/carver_probe_names.txt: src/carver.o src/carver_probes.txt
	python3 bin/get_carver_probe_name.py

lib/driver_probe_names.txt: src/driver.o src/driver_probes.txt
	python3 bin/get_driver_probe_name.py

src/pass_utils.o: src/pass_utils.cc include/pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/pass_utils.cc -o $@

clean:
	rm lib/carver_pass.so
	rm lib/carver.a
	rm src/carver.o
	rm lib/carver_probe_names.txt
	rm lib/driver_pass.so
	rm lib/driver.a
	rm src/driver.o
	rm lib/driver_probe_names.txt
