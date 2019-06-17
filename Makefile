IDIR=/usr/include/libdwarf
LDIR=/usr/lib/
CC=gcc
CFLAGS=-O0 -g -I$(IDIR) -L$(LDIR)

ODIR=obj

LIBS=/usr/lib/libdwarf.a /usr/lib/libelf.a /usr/lib/libz.a

_OBJ = addr2line.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

addr2line: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean run

run: addr2line
	./addr2line -a -e a.out 0x1510 0x1511

verify:
	./addr2line -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > batch.log
	./addr2line -b -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > nobatch.log
#	addr2line -b -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > addr2line.log
	diff nobatch.log batch.log

# One diff happens since batch-mode does not look at DW_AT_ranges and
# fully bases it on linetable. Otherwise fine.

clean:
	rm -f $(ODIR)/*.o *~
