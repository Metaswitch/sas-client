sas-client
==========


This repository provides the Service Assurance Server client C++ library.

A makefile is provided to compile the code to an archive file (`libsas.a`), which can be statically linked by applications wishing to use this library.

To build the library, run `make` in the top level directory. A `clean` target is also supplied.

To include this library in your application, you must ensure that all the files in `include/` are in your applications include path, and you have `#include <sas.h>` in your code.

To link with this library you should ensure `libsas.a` is in your link path (using the `-L` option) and supply `-lsas -lpthread -lrt` to the linker.
