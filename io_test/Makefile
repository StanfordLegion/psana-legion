.PHONY: all
all: io_test

CXXFLAGS += -fopenmp

io_test: io_test.o
	$(CXX) -o $@ $< -fopenmp

.PHONY: clean
clean:
	rm -f io_test io_test.o
