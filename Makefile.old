CXX=g++
CC=gcc

CXXSRC=$(wildcard *.cpp)
CSRC=$(wildcard *.c)
SRC=$(CXXSRC) $(CSRC)

CXXFLAGS=-ansi -pedantic -Wall -O2 -DNDEBUG
CFLAGS=$(CXXFLAGS)

CXXOBJECTS=$(CXXSRC:%.cpp=%.o)
COBJECTS=$(CSRC:%.c=%.o)
OBJECTS=$(CXXOBJECTS) $(COBJECTS)

LIBS=-lm -lcrypto -lvorbis

extract: $(OBJECTS)
	g++ -o extract $(OBJECTS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(OBJECTS) extract
