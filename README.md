# libdwarf-addr2line

This is a trivial example, the build will
take some fiddling with the simple Makefile.

The code assumes C99 types like 'bool'
available.

### Usage

    addr2line [-a] [-e <objectpath>] [-b] [-n]
    where
        -a --addresses 
        -e --exe  <path>
        -f --force-batch
        -n --force-no-batch

Here is an example:
    q3 619: ./addr2line -a -e  addr2line 0x2470 0x33b0
    0x0000000000002470
    /home/davea/dwarf/libdwarf-addr2line/addr2line.c:479
    0x00000000000033b0
    /home/davea/dwarf/libdwarf-addr2line/addr2line.c:40




    ./addr2line -a -e <objectname> lineaddress ...   

    ./addr2line -a -e  addr2line 0x2470 0x33b0
   

