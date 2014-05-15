PROGRAM=coupon2vec.full
OBJECTS=
LDLIBS=-lm -lz
CFLAGS= -Wall -Ofast -march=native -funroll-loops -ftree-vectorize
CC=clang --std=c99

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)

clean:
	rm -rf $(PROGRAM)
