CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude -Itests
HEADERS  := $(wildcard include/*.h)
TESTSRC  := $(wildcard tests/*.cpp)

all: routerush run_tests benchmark

routerush: src/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) src/main.cpp -o routerush

run_tests: $(TESTSRC) $(wildcard tests/*.h) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TESTSRC) -o run_tests

benchmark: bench/benchmark.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) bench/benchmark.cpp -o benchmark

test: run_tests
	./run_tests

run: routerush
	./routerush data

bench: benchmark
	./benchmark

clean:
	rm -f routerush run_tests benchmark *.exe data/out_*.csv _t_*.csv
