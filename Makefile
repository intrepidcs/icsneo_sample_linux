CXX=g++
CC=gcc
CFLAGS=-g -c -O2
LDFLAGS=
AR=ar

all: sample 

sample: icsneo_sample.o
	$(CXX) $(LDFLAGS) icsneo_sample.o -o icsneo_sample -lpthread -licsneoapi

icsneo_sample.o: icsneo_sample.cpp
	$(CXX) $(CFLAGS) icsneo_sample.cpp

clean:
	rm -rf *.o icsneo_sample
