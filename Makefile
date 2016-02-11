.PHONY: all
all: build

.PHONY: build
build: libsas.a

libsas.a: sas.o sas_compress.o lz4.o
	ar cr libsas.a $^

C_FLAGS := -O2 -Iinclude -std=c99 -Wall -Werror -ggdb3
CPP_FLAGS := -O2 -Iinclude -std=c++0x -Wall -Werror -ggdb3

sas.o: source/sas.cpp source/sas_eventq.h source/sas_internal.h include/sas.h include/config.h include/lz4.h
	g++ ${CPP_FLAGS} -c $<
sas_compress.o: source/sas_compress.cpp source/sas_eventq.h source/sas_internal.h include/sas.h include/config.h include/lz4.h
	g++ ${CPP_FLAGS} -c $<
lz4.o: source/lz4.c include/lz4.h
	gcc ${C_FLAGS} -c $<

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
	g++ source/ut/main.cpp -o sas_test -I include -std=c++0x -L. -lsas -lrt -Wall -Werror -ggdb3 -lpthread
sas_compress_test: libsas.a source/ut/sastestutil.h source/ut/main_compress.cpp
	g++ source/ut/main_compress.cpp -o sas_compress_test -I include -std=c++0x -L. -lsas -lrt -lz -Wall -Werror -ggdb3 -lpthread

