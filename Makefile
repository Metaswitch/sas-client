all: build

build:  
	gcc -Iinclude -std=c++0x -c source/sas.cpp
	ar cr saslib.a sas.o

clean:
	rm -rf *.o *.a
