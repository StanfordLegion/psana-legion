.PHONY: all
all: io_test

CXXFLAGS += -O3 -fopenmp -Wall

io_test: io_test.o
	$(CXX) -o $@ $< -fopenmp

.PHONY: clean
clean:
	rm -f io_test io_test.o
