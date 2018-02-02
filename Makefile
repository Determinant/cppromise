.PHONY: all
all: test14 test17
test14: test.cpp promise.hpp
	g++ -o $@ test.cpp -std=c++17
test17: test.cpp promise.hpp
	g++ -o $@ test.cpp -std=c++14
