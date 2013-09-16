all: build

build: saslib.a

saslib.a: sas.o
	ar cr saslib.a sas.o

sas.o: source/sas.cpp include/sas.h include/log.h include/logger.h include/eventq.h 
	gcc -Iinclude -std=c++0x -c source/sas.cpp
clean:
	rm -rf *.o *.a
