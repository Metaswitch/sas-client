all: build

build: libsas.a

libsas.a: sas.o
	ar cr libsas.a sas.o

sas.o: source/sas.cpp include/sas.h include/eventq.h
	g++ -Iinclude -std=c++0x -c source/sas.cpp

clean:
	rm -rf *.o *.a
