.PHONY: all
all: build

.PHONY: build
build: libsas.a

libsas.a: sas.o
	ar cr libsas.a sas.o

sas.o: source/sas.cpp include/sas.h include/eventq.h include/config.h
	g++ -Iinclude -std=c++0x -c source/sas.cpp -Wall -Werror

include/config.h: configure
	./configure

.PHONY: clean
clean:
	rm -rf *.o *.a include/config.h sas_test

.PHONY: test
test: sas_test
	./sas_test

sas_test: libsas.a source/ut/sastestutil.h source/ut/main.cpp
	g++ source/ut/main.cpp -o sas_test -I include -std=c++0x -L. -lsas -lrt -Wall -Werror

