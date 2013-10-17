all: build

build: libsas.a

libsas.a: sas.o
	ar cr libsas.a sas.o

sas.o: source/sas.cpp include/sas.h include/eventq.h include/config.h
	g++ -Iinclude -std=c++0x -c source/sas.cpp

include/config.h: configure
	./configure

clean:
	rm -rf *.o *.a include/config.h


