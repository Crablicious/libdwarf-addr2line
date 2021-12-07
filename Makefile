IDIR=/usr/include/libdwarf
IDIR=/home/davea/dwarf/code/src/lib/libdwarf
LDIR=/usr/lib/
LDIR=/var/tmp/bld/src/lib/libdwarf/.libs
CC=gcc
CFLAGS=-O2 -g -I$(IDIR) -L$(LDIR)
LDFLAGS=
LIBNAME=libdwarf.a

LIBS=$(LDIR)/$(LIBNAME) -lz

addr2line: addr2line.c
	$(CC) -o $@ $<  $(CFLAGS) $(LDFLAGS) $(LIBS)

.PHONY: clean run

check: addr2line
	./addr2line -a -e test/test   0x1040
	./addr2line -a -e test/dwarf5test  0x1041 0x1046


# One diff happens since batch-mode does not look at DW_AT_ranges and
# fully bases it on linetable. Otherwise fine.

clean:
	rm -f *~
	rm -f addr2line
	rm -f addr2line.o
