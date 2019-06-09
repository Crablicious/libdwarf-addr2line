IDIR=/usr/include/libdwarf
LDIR=/usr/lib/
CC=gcc
CFLAGS=-O4 -g -I$(IDIR) -L$(LDIR)

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

clean:
	rm -f $(ODIR)/*.o *~
