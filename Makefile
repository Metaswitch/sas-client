.PHONY: all
all: build

.PHONY: build
build: libsas.a

libsas.a: sas.o sas_compress.o
	ar cr libsas.a sas.o sas_compress.o

sas.o: source/sas.cpp source/sas_eventq.h source/sas_internal.h include/sas.h include/config.h
	g++ -Iinclude -std=c++0x -c source/sas.cpp -Wall -Werror -ggdb3
sas_compress.o: source/sas_compress.cpp source/sas_eventq.h source/sas_internal.h include/sas.h include/config.h
	g++ -Iinclude -std=c++0x -c source/sas_compress.cpp -Wall -Werror -ggdb3

include/config.h: configure
	./configure

.PHONY: clean
clean:
	rm -rf *.o *.a include/config.h sas_test sas_compress_test

.PHONY: test test_compress
test: sas_test
	./sas_test
test_compress: sas_compress_test
	./sas_compress_test

sas_test: libsas.a source/ut/sastestutil.h source/ut/main.cpp
	g++ source/ut/main.cpp -o sas_test -I include -std=c++0x -L. -lsas -lrt -Wall -Werror -ggdb3
sas_compress_test: libsas.a source/ut/sastestutil.h source/ut/main_compress.cpp
	g++ source/ut/main_compress.cpp -o sas_compress_test -I include -std=c++0x -L. -lsas -lrt -lz -Wall -Werror -ggdb3

