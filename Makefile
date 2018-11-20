.PHONY: all clean
all: test14 test17 test14_stack_free test17_stack_free
clean:
	rm test14 test17 test14_stack_free test17_stack_free
test14: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++14 -Wall -Wextra -Wpedantic -O2
test17: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++17 -Wall -Wextra -Wpedantic -O2
test14_stack_free: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++14 -Wall -Wextra -Wpedantic -O2 -DCPPROMISE_USE_STACK_FREE
test17_stack_free: test.cpp promise.hpp
	$(CXX) -o $@ test.cpp -std=c++17 -Wall -Wextra -Wpedantic -O2 -DCPPROMISE_USE_STACK_FREE
