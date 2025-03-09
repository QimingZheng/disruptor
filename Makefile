CC=g++
CCFLAGS=--std=c++20 -pthread -I

all: disruptor

disruptor: disruptor.h disruptor.cc
	${CC} ${CCFLAGS} $^ -o $@

clean:
	rm disruptor