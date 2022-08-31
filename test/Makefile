
all: map_test boostmap

map_test: map.cc ../include/utils.hpp
	clang++ map.cc -I ../include/ -I ../src/utils -fsanitize=address -O0 -ggdb -o map_test

boostmap:
	clang++ boostmap.cc -I ../include/ -I ../src/utils -fsanitize=address -O0 -ggdb -o boost_map

clean:
	rm -f map_test boostmap