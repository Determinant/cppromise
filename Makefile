.PHONY: all
all: test14 test17
test14: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++14 -Wall -Wextra -Wpedantic -O2
test17: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++17 -Wall -Wextra -Wpedantic -O2
