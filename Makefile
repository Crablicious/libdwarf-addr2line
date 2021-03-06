IDIR=/usr/include/libdwarf
LDIR=/usr/lib/
CC=gcc
CFLAGS=-O4 -g -I$(IDIR) -L$(LDIR)
LDFLAGS=

ODIR=obj
LIBS=$(LDIR)/libdwarf.a $(LDIR)/libelf.a $(LDIR)/libz.a

_OBJ = addr2line.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

addr2line: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

.PHONY: clean run

run: addr2line
	./addr2line -a -e a.out 0x1510 0x1511

docker:
	docker build -f Dockerfile -t static-addr2line .
	docker create --name static-addr2line-cont static-addr2line
	docker cp static-addr2line-cont:/addr2line/addr2line docker-addr2line

verify:
	./addr2line -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > batch.log
	./addr2line -b -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > nobatch.log
#	addr2line -b -a -e /home/adwe/code/build/gdb/gdb/gdb <gdb_pcs.txt > addr2line.log
	diff nobatch.log batch.log

# One diff happens since batch-mode does not look at DW_AT_ranges and
# fully bases it on linetable. Otherwise fine.

clean:
	rm -f $(ODIR)/*.o *~

$(shell mkdir -p $(ODIR))
