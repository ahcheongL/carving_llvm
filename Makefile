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
	CXXFLAGS=`llvm-config --cxxflags` -fPIC -g -O2
endif

SMALL ?= 0
ifeq ($(SMALL), 1)
	CXXFLAGS += -DSMALL
endif

CXXFLAGS += -DLLVM_MAJOR=$(LLVM_MAJOR)

MAKEFILE_PATH=$(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR:=$(dir $(MAKEFILE_PATH))

all: carve_func_context carve_type_based carve_func_args \
	unit_test extend_driver fuzz_driver clementine_driver

carve_func_context: lib/carve_func_context_pass.so lib/fc_carver.a
carve_type_based: lib/carve_type_pass.so lib/tb_carver.a
carve_func_args: lib/carve_func_args_pass.so lib/fa_carver.a

unit_test: lib/unit_test_pass.so lib/unit_test_mock.a
extend_driver: lib/extend_driver_pass.so lib/extend_driver.a
fuzz_driver: lib/fuzz_driver_pass.so lib/fuzz_driver.a
clementine_driver: lib/clementine_driver_pass.so lib/cl_driver.a

tools: lib/extract_info_pass.so lib/read_gtest.so lib/get_call_seq.so lib/call_seq.a

lib/carve_func_context_pass.so: \
	src/carving/func_context/carve_func_context_pass.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include -shared $< src/utils/carve_pass_utils.o \
		src/utils/pass_utils.o -o $@

lib/carve_func_args_pass.so: \
	src/carving/func_args/carve_func_args_pass.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include -shared $< src/utils/carve_pass_utils.o \
		src/utils/pass_utils.o -o $@

lib/fc_carver.a: src/carving/func_context/fc_carver.cc src/utils/data_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/carving/func_context/fc_carver.o
	$(AR) rsv $@ src/carving/func_context/fc_carver.o src/utils/data_utils.o

lib/fa_carver.a: src/carving/func_args/fa_carver.cc src/utils/data_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/carving/func_args/fa_carver.o
	$(AR) rsv $@ src/carving/func_args/fa_carver.o src/utils/data_utils.o

lib/tb_carver.a: src/carving/type_based/tb_carver.cc src/utils/data_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/carving/type_based/tb_carver.o
	$(AR) rsv $@ src/carving/type_based/tb_carver.o src/utils/data_utils.o

lib/fuzz_driver_pass.so: src/drivers/fuzz_driver/fuzz_driver_pass.cc \
	src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/fuzz_driver.a: src/drivers/fuzz_driver/fuzz_driver_probes.cc
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/drivers/fuzz_driver/fuzz_driver.o
	$(AR) rsv $@ src/drivers/fuzz_driver/fuzz_driver.o

lib/cl_driver.a: src/drivers/clementine_driver/cl_driver.cc 
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/drivers/clementine_driver/cl_driver.o
	$(AR) rsv $@ src/drivers/clementine_driver/cl_driver.o

lib/clementine_driver_pass.so: \
	src/drivers/clementine_driver/clementine_driver_pass.cc \
	src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/simple_unit_driver_pass.so: \
	src/drivers/simple_replay/simple_unit_driver_pass.cc \
	src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/driver.a: src/drivers/driver.cc
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils \
		-c $< -o src/drivers/driver.o
	$(AR) rsv $@ src/drivers/driver.o

lib/extract_info_pass.so: src/tools/extract_info_pass.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/read_gtest.so: src/tools/read_gtest.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

src/utils/carve_pass_utils.o: src/utils/carve_pass_utils.cc
	$(CXX) $(CXXFLAGS) -I include/ -c $< -o $@

lib/get_call_seq.so: src/tools/get_call_seq.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/call_seq.a: src/tools/call_seq.o
	mkdir -p lib
	$(AR) rsv $@ $^

lib/carve_type_pass.so: src/carving/type_based/carve_type_pass.cc \
	src/utils/carve_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

src/utils/driver_pass_utils.o: src/utils/driver_pass_utils.cc
	$(CXX) $(CXXFLAGS) -I include/ -c $< -o $@

src/utils/pass_utils.o: src/utils/pass_utils.cc
	$(CXX) $(CXXFLAGS) -I include/ -c $< -o $@

lib/unit_test_pass.so: src/drivers/unit_test_mock/unit_test_mock_pass.cc \
	src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/unit_test_mock.a: src/drivers/unit_test_mock/unit_test_mock.o
	mkdir -p lib
	$(AR) rsv $@ $^

src/drivers/unit_test_mock/unit_test_mock.o: \
	src/drivers/unit_test_mock/unit_test_mock_probes.cc
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c $< -o $@

lib/extend_driver_pass.so: src/drivers/ossfuzz_extend/extend_driver_pass.cc \
	src/utils/driver_pass_utils.o src/utils/pass_utils.o
	mkdir -p lib
	$(CXX) $(CXXFLAGS) -I include/ -shared $^ -o $@

lib/extend_driver.a: src/drivers/ossfuzz_extend/extend_driver.o
	mkdir -p lib
	$(AR) rsv $@ $^

src/drivers/ossfuzz_extend/extend_driver.o: \
	src/drivers/ossfuzz_extend/extend_driver_probes.cc
	$(CXX) $(CXXFLAGS) -I include/ -I src/utils -c $< -o $@

src/utils/data_utils.o: src/utils/data_utils.cc
	$(CXX) $(CXXFLAGS) -I include/ -c $< -o $@

clean:
	rm -f lib/*.so lib/*.a src/carving/*.o drivers/*.o
	rm -f src/utils/*.o
	rm -f lib/*_probe_names.txt
	rm -rf src/drivers/*.o
	rm -rf src/drivers/*/*.o
