### Copyright (C) 2021 David Anderson
### This Makefile is hereby placed into the Public Domain
### for use by anyone and use in any way.


### standard places
IDIR=/usr/include/libdwarf
LDIR=/usr/lib/
### comment these next two out or alter them for your environment
IDIR=/home/davea/dwarf/code/src/lib/libdwarf
LDIR=/var/tmp/bld/src/lib/libdwarf/.libs

CC=gcc
CFLAGS=-O2 -g -I$(IDIR) -L$(LDIR)
### Using the longer set of compiler options
CFLAGS=-O2 -g -I$(IDIR) -L$(LDIR) -Wall -Wextra -Wpointer-arith -Wmissing-declarations -Wcomment -Wformat -Wpedantic -Wuninitialized -Wshadow -Werror -Wno-long-long
LDFLAGS=
LIBNAME=libdwarf.a

LIBS=$(LDIR)/$(LIBNAME) -lz

addr2line: addr2line.c
	$(CC) -o $@ $<  $(CFLAGS) $(LDFLAGS) $(LIBS)

.PHONY: clean run

check: addr2line
	sh test/runtest.sh 


# One diff happens since batch-mode does not look at DW_AT_ranges and
# fully bases it on linetable. Otherwise fine.

clean:
	rm -f *~
	rm -f addr2line
	rm -f addr2line.o
	rm -f test/junk*
