CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2 -Iinclude -Itests
HEADERS  := $(wildcard include/*.h)
TESTSRC  := $(wildcard tests/*.cpp)
ifeq ($(OS),Windows_NT)
  WEBLIBS := -lws2_32
endif

all: routerush routerush_web run_tests benchmark

routerush: src/main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) src/main.cpp -o routerush

routerush_web: src/server.cpp $(HEADERS) web/index.html
	$(CXX) $(CXXFLAGS) src/server.cpp -o routerush_web -pthread $(WEBLIBS)

run_tests: $(TESTSRC) $(wildcard tests/*.h) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TESTSRC) -o run_tests

benchmark: bench/benchmark.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) bench/benchmark.cpp -o benchmark

test: run_tests
	./run_tests

run: routerush
	./routerush data

menu: routerush
	./routerush data --menu

web: routerush_web
	./routerush_web data

bench: benchmark
	./benchmark

clean:
	rm -f routerush routerush_web run_tests benchmark *.exe data/out_*.csv _t_*
