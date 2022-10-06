ifeq (, $(shell which llvm-config))
$(error "No llvm-config in $$PATH")
endif

LLVMVER  = $(shell llvm-config --version 2>/dev/null | sed 's/git//' | sed 's/svn//' )
LLVM_MAJOR = $(shell llvm-config --version 2>/dev/null | sed 's/\..*//' )
LLVM_MINOR = $(shell llvm-config --version 2>/dev/null | sed 's/.*\.//' | sed 's/git//' | sed 's/svn//' | sed 's/ .*//' )
$(info Detected LLVM VERSION : $(LLVMVER))

CC=clang
CXX=clang++
CFLAGS=`llvm-config --cflags` -fPIC -O2
AR=ar

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CXXFLAGS=`llvm-config --cxxflags` -fPIC -ggdb -O0 -DDEBUG
else
	CXXFLAGS=`llvm-config --cxxflags` -fPIC -O2
endif

SMALL ?= 0
ifeq ($(SMALL), 1)
	CXXFLAGS += -DSMALL
endif

CXXFLAGS += -DLLVM_MAJOR=$(LLVM_MAJOR)

MAKEFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR:=$(dir $(MAKEFILE_PATH))

all: lib/carve_func_args_pass.so lib/carver.a lib/carver_probe_names.txt
all: lib/simple_unit_driver_pass.so lib/driver.a lib/driver_probe_names.txt

unit_test: lib/unit_test_pass.so lib/unit_test_mock.a lib/unit_test_probe_names.txt all
carve_type: lib/carve_type_pass.so all
extend_driver: lib/extend_driver_pass.so lib/extend_driver.a lib/extend_driver_probe_names.txt all

tools: lib/extract_info_pass.so lib/read_gtest.so lib/get_call_seq.so lib/call_seq.a

lib/carve_func_args_pass.so: src/carving/carve_func_args_pass.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

src/carving/carver.o: src/carving/carver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c src/carving/carver.cc -o $@

lib/carver.a: src/carving/carver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/shape_fixed_driver_pass.so: src/drivers/shape_fixed_driver_pass.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

src/drivers/shape_fixed_driver.o: src/drivers/shape_fixed_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c src/drivers/shape_fixed_driver_probes.cc -o $@

lib/shape_fixed_driver.a: src/drivers/shape_fixed_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/simple_unit_driver_pass.so: src/drivers/simple_unit_driver_pass.cc src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/driver_pass_utils.o src/utils/pass_utils.o -o $@

src/drivers/driver.o: src/drivers/driver.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c src/drivers/driver.cc -o $@

lib/driver.a: src/drivers/driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carver_probe_names.txt: src/carving/carver.o src/carving/carver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/carving/carver.o src/carving/carver_probes.txt $@

lib/shape_fixed_driver_probe_names.txt: src/drivers/shape_fixed_driver.o src/drivers/shape_fixed_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/shape_fixed_driver.o src/drivers/shape_fixed_driver_probes.txt $@

lib/driver_probe_names.txt: src/drivers/driver.o src/drivers/driver_probe_names.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/driver.o src/drivers/driver_probe_names.txt $@

lib/extract_info_pass.so: src/tools/extract_info_pass.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

lib/read_gtest.so: src/tools/read_gtest.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

src/utils/carve_pass_utils.o: src/utils/carve_pass_utils.cc include/carve_pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/utils/carve_pass_utils.cc -o $@

lib/get_call_seq.so: src/tools/get_call_seq.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

lib/call_seq.a: src/tools/call_seq.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carve_type_pass.so: src/carving/carve_type_pass.cc include/carve_pass.hpp src/utils/carve_pass_utils.o src/utils/pass_utils.o include/utils.hpp
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/carve_pass_utils.o src/utils/pass_utils.o -o $@

src/utils/driver_pass_utils.o: src/utils/driver_pass_utils.cc include/driver_pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/utils/driver_pass_utils.cc -o $@

src/utils/pass_utils.o: src/utils/pass_utils.cc include/pass.hpp
	$(CXX) $(CXXFLAGS) -I include/ -c $(MAKEFILE_DIR)/src/utils/pass_utils.cc -o $@


lib/unit_test_pass.so: src/drivers/unit_test_mock_pass.cc src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/driver_pass_utils.o src/utils/pass_utils.o -o $@

lib/unit_test_mock.a: src/drivers/unit_test_mock.o
	mkdir -p lib
	$(AR) rsv $@ $^

src/drivers/unit_test_mock.o: src/drivers/unit_test_mock_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c src/drivers/unit_test_mock_probes.cc -o $@

lib/unit_test_probe_names.txt: src/drivers/unit_test_mock.o src/drivers/unit_test_mock_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/unit_test_mock.o src/drivers/unit_test_mock_probes.txt $@

lib/extend_driver_pass.so: src/drivers/extend_driver_pass.cc src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $< src/utils/driver_pass_utils.o src/utils/pass_utils.o -o $@

lib/extend_driver.a: src/drivers/extend_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

src/drivers/extend_driver.o: src/drivers/extend_driver_probes.cc include/utils.hpp
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c src/drivers/extend_driver_probes.cc -o $@

lib/extend_driver_probe_names.txt: src/drivers/extend_driver.o src/drivers/extend_driver_probes.txt
	mkdir -p lib
	python3 bin/get_probe_names.py src/drivers/extend_driver.o src/drivers/extend_driver_probes.txt $@


clean:
	rm -f lib/*.so lib/*.a src/carving/*.o drivers/*.o
	rm -f src/utils/*.o
	rm -f lib/carver_probe_names.txt
	rm -f lib/shape_fixed_driver_probe_names.txt
	rm -f lib/driver_probe_names.txt
	rm -f lib/unit_test_probe_names.txt