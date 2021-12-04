# libdwarf-addr2line

This is a trivial example, the build will
take some fiddling with the simple Makefile.

The code assumes C99 stdbool.h  and
type 'bool' available for the code to compile.

### Usage

    addr2line [-a] [-e <objectpath>] [-b] [-n] [address] ...
    where
        -a --addresses  Turns on printing of address before
            the source line text
        -e --exe  <path> The the path to the object file to read
        -f --force-batch The CU address ranges will be looked
            up once at the start and the generated table used.
        -n --force-no-batch The addresses are looked up
            independently for each address present.
            In certain cases the no-batch will be overridden
            and batching used.
    if no addresses present the program reads STDIN, expecting
    a list of addresses there. 

Here is an example:
    q3 619: ./addr2line -a -e  addr2line 0x2470 0x33b0
    0x0000000000002470
    /home/davea/dwarf/libdwarf-addr2line/addr2line.c:479
    0x00000000000033b0
    /home/davea/dwarf/libdwarf-addr2line/addr2line.c:40


    ./addr2line -a -e  addr2line 0x2470 0x33b0
   
    echo 0x2470 >junk
    ./addr2line -a -e  addr2line <junk

