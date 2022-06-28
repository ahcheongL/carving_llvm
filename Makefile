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
all: lib/shape_fixed_driver_pass.so lib/shape_fixed_driver.a lib/shape_fixed_driver_probe_names.txt
all: lib/simple_unit_driver_pass.so lib/simple_unit_driver.a lib/simple_unit_driver_probe_names.txt

lib/carver_pass.so: src/carver_pass.cc include/pass.hpp src/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/pass_utils.o -o $@

src/carver.o: src/carver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/carver.cc -o $@

lib/carver.a: src/carver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/shape_fixed_driver_pass.so: src/shape_fixed_driver_pass.cc include/pass.hpp src/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/pass_utils.o -o $@

src/shape_fixed_driver.o: src/shape_fixed_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/shape_fixed_driver_probes.cc -o $@

lib/shape_fixed_driver.a: src/shape_fixed_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/simple_unit_driver_pass.so: src/simple_unit_driver_pass.cc include/pass.hpp src/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/pass_utils.o -o $@

src/simple_unit_driver.o: src/simple_unit_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/simple_unit_driver_probes.cc -o $@

lib/simple_unit_driver.a: src/simple_unit_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carver_probe_names.txt: src/carver.o src/carver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/carver.o src/carver_probes.txt $@

lib/shape_fixed_driver_probe_names.txt: src/shape_fixed_driver.o src/shape_fixed_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/shape_fixed_driver.o src/shape_fixed_driver_probes.txt $@

lib/simple_unit_driver_probe_names.txt: src/simple_unit_driver.o src/simple_unit_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/simple_unit_driver.o src/simple_unit_driver_probes.txt $@

src/pass_utils.o: src/pass_utils.cc include/pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/pass_utils.cc -o $@

clean:
	rm -f lib/carver_pass.so
	rm -f lib/carver.a
	rm -f src/carver.o
	rm -f lib/carver_probe_names.txt
	rm -f lib/shape_fixed_driver_pass.so
	rm -f lib/shape_fixed_driver.a
	rm -f src/shape_fixed_driver.o
	rm -f lib/shape_fixed_driver_probe_names.txt
	rm -f lib/simple_unit_driver_pass.so
	rm -f lib/simple_unit_driver.a
	rm -f src/simple_unit_driver.o
	rm -f lib/simple_unit_driver_probe_names.txt
	rm -f src/pass_utils.o
