
all: map_test

map_test: map.cc ../include/utils.hpp
	clang++ map.cc -I ../include/ -fsanitize=address -O0 -ggdb -o map_test

clean:
	rm -f map_test