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

all: lib/carve_func_args_pass.so lib/carver.a lib/carver_probe_names.txt
all: lib/shape_fixed_driver_pass.so lib/shape_fixed_driver.a lib/shape_fixed_driver_probe_names.txt
all: lib/simple_unit_driver_pass.so lib/simple_unit_driver.a lib/simple_unit_driver_probe_names.txt
all: lib/extract_info_pass.so lib/read_gtest.so lib/get_call_seq.so lib/call_seq.a
all: lib/carve_type_pass.so

lib/carve_func_args_pass.so: src/carving/carve_func_args_pass.cc include/pass.hpp src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

src/carving/carver.o: src/carving/carver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/carving/carver.cc -o $@

lib/carver.a: src/carving/carver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/shape_fixed_driver_pass.so: src/drivers/shape_fixed_driver_pass.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

src/drivers/shape_fixed_driver.o: src/drivers/shape_fixed_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/drivers/shape_fixed_driver_probes.cc -o $@

lib/shape_fixed_driver.a: src/drivers/shape_fixed_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/simple_unit_driver_pass.so: src/drivers/simple_unit_driver_pass.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

src/drivers/simple_unit_driver.o: src/drivers/simple_unit_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c src/drivers/simple_unit_driver_probes.cc -o $@

lib/simple_unit_driver.a: src/drivers/simple_unit_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carver_probe_names.txt: src/carving/carver.o src/carving/carver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/carving/carver.o src/carving/carver_probes.txt $@

lib/shape_fixed_driver_probe_names.txt: src/drivers/shape_fixed_driver.o src/drivers/shape_fixed_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/shape_fixed_driver.o src/drivers/shape_fixed_driver_probes.txt $@

lib/simple_unit_driver_probe_names.txt: src/drivers/simple_unit_driver.o src/drivers/simple_unit_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/simple_unit_driver.o src/drivers/simple_unit_driver_probes.txt $@

lib/extract_info_pass.so: src/tools/extract_info_pass.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

lib/read_gtest.so: src/tools/read_gtest.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

src/utils/pass_utils.o: src/utils/pass_utils.cc include/pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/utils/pass_utils.cc -o $@

lib/get_call_seq.so: src/tools/get_call_seq.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

lib/call_seq.a: src/tools/call_seq.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carve_type_pass.so: src/carving/carve_type_pass.cc include/pass.hpp src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/pass_utils.o -o $@

clean:
	rm -f lib/carve_func_args_pass.so
	rm -f lib/carver.a
	rm -f src/carving/carver.o
	rm -f lib/carver_probe_names.txt
	rm -f lib/shape_fixed_driver_pass.so
	rm -f lib/shape_fixed_driver.a
	rm -f src/drivers/shape_fixed_driver.o
	rm -f lib/shape_fixed_driver_probe_names.txt
	rm -f lib/simple_unit_driver_pass.so
	rm -f lib/simple_unit_driver.a
	rm -f src/drivers/simple_unit_driver.o
	rm -f lib/simple_unit_driver_probe_names.txt
	rm -f lib/extract_info_pass.so
	rm -f lib/read_gtest.so
	rm -f lib/get_call_seq.so
	rm -f lib/call_seq.a
	rm -f src/utils/pass_utils.o
	rm -f lib/carve_type_pass.so
