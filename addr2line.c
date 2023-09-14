/*  addr2line.c

The code does not attempt to report details
when an unexpected DW_DLV_NO_ENTRY is received
from libdwarf.  It just silently moves on.
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "dwarf.h"
#include "libdwarf.h"

#define DW_PR_DUx "llx"
#define DW_PR_DUu "llu"

#define MAX_ADDR_LEN 20
#define BATCHMODE_HEURISTIC 100
#define DWARF5_VERSION  5
#define TRUE 1
#define FALSE 0

#define HIGHADDR (Dwarf_Unsigned)0xffffffffffffffff

static const char *objfile_name = "<none>";

typedef struct flags {
    Dwarf_Bool addresses;
    Dwarf_Bool batchmode;
    Dwarf_Bool force_batchmode;
    Dwarf_Bool force_nobatchmode;
} flagsT;

typedef struct lookup_table {
    Dwarf_Line *table;
    Dwarf_Line_Context *ctxts;
    int cnt;
    Dwarf_Addr low;
    Dwarf_Addr high;
} lookup_tableT;

static void
fail_exit(const char *msg) {
    printf("addr2line failure reading %s: %s\n",
        objfile_name,
        msg);
    exit(1);
}

static void
err_handler(Dwarf_Error err, Dwarf_Ptr errarg)
{
    printf("libdwarf error reading %s: %lu %s\n",
        objfile_name,
        (unsigned long)dwarf_errno(err),
        dwarf_errmsg(err));
    if (errarg) {
        printf("Error: errarg is nonnull but it should be null\n");
    }
    printf("Giving up");
    exit(1);
}

static int
dwarf5_ranges(Dwarf_Die cu_die,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest);
static int dwarf4_ranges( Dwarf_Debug dbg,
    Dwarf_Die cu_die,
    Dwarf_Addr cu_lowpc,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest);

static Dwarf_Bool
pc_in_die(Dwarf_Debug dbg, Dwarf_Die die,int version, Dwarf_Addr pc)
{
    int ret;
    Dwarf_Addr cu_lowpc = HIGHADDR;
    Dwarf_Addr cu_highpc = 0;
    enum Dwarf_Form_Class highpc_cls;
    Dwarf_Addr lowest = HIGHADDR;
    Dwarf_Addr highest = 0;

    ret = dwarf_lowpc(die, &cu_lowpc, NULL);
    if (ret == DW_DLV_OK) {
        if (pc == cu_lowpc) {
            return TRUE;
        }
        ret = dwarf_highpc_b(die, &cu_highpc,
            NULL, &highpc_cls, NULL);
        if (ret == DW_DLV_OK) {
            if (highpc_cls == DW_FORM_CLASS_CONSTANT) {
                cu_highpc += cu_lowpc;
            }
            if (pc >= cu_lowpc && pc < cu_highpc) {
                return TRUE;
            }
        }
    }
    if (version >= DWARF5_VERSION) {
        ret = dwarf5_ranges(die,
            &lowest,&highest);
    } else {
        ret = dwarf4_ranges(dbg,die,cu_lowpc,
            &lowest,&highest);
    }
    if (pc >= lowest && pc < highest) {
        return TRUE;
    }
    return FALSE;
}

static void
print_line(Dwarf_Debug dbg,
    flagsT *flags,
    Dwarf_Line line,
    Dwarf_Addr pc)
{
    char *         linesrc = "??";
    Dwarf_Unsigned lineno = 0;

    if (flags->addresses) {
        printf("%#018" DW_PR_DUx "\n", pc);
    }
    if (line) {
        /*  These never return DW_DLV_NO_ENTRY */
        dwarf_linesrc(line, &linesrc, NULL);
        dwarf_lineno(line, &lineno, NULL);
    }
    printf("%s:%" DW_PR_DUu "\n", linesrc, lineno);
    if (line) {
        dwarf_dealloc(dbg, linesrc, DW_DLA_STRING);
    }
}

static Dwarf_Bool
lookup_pc_cu(Dwarf_Debug dbg,
    flagsT *flags,
    Dwarf_Addr pc,
    Dwarf_Die cu_die)
{
    int ret;
    Dwarf_Unsigned version;
    Dwarf_Small table_count;
    Dwarf_Line_Context ctxt;
    Dwarf_Bool is_found = FALSE;

    ret = dwarf_srclines_b(cu_die, &version, &table_count,
        &ctxt, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        return FALSE;
    }
    if (table_count == 1) {
        Dwarf_Line *linebuf = 0;
        Dwarf_Signed linecount = 0;
        Dwarf_Addr prev_lineaddr = 0;

        dwarf_srclines_from_linecontext(ctxt, &linebuf,
            &linecount, NULL);
        Dwarf_Line prev_line = 0;
        for (int i = 0; i < linecount; i++) {
            Dwarf_Line line = linebuf[i];
            Dwarf_Addr lineaddr = 0;

            dwarf_lineaddr(line, &lineaddr, NULL);
            if (pc == lineaddr) {
                /* Print the last line entry containing current pc. */
                Dwarf_Line last_pc_line = line;

                for (int j = i + 1; j < linecount; j++) {
                    Dwarf_Line j_line = linebuf[j];
                    dwarf_lineaddr(j_line, &lineaddr, NULL);

                    if (pc == lineaddr) {
                        last_pc_line = j_line;
                    }
                }
                is_found = TRUE;
                print_line(dbg, flags, last_pc_line, pc);
                break;
            } else if (prev_line && pc > prev_lineaddr &&
                pc < lineaddr) {
                is_found = TRUE;
                print_line(dbg, flags, prev_line, pc);
                break;
            }
            Dwarf_Bool is_lne;
            dwarf_lineendsequence(line, &is_lne, NULL);
            if (is_lne) {
                prev_line = 0;
            } else {
                prev_lineaddr = lineaddr;
                prev_line = line;
            }
        }
    }
    dwarf_srclines_dealloc_b(ctxt);
    return is_found;
}

static Dwarf_Bool
lookup_pc(Dwarf_Debug dbg,
    flagsT *flags,
    Dwarf_Addr pc)
{
    Dwarf_Bool is_info = TRUE;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;
    Dwarf_Half dwversion = 0;
    Dwarf_Half offset_size = 0;
    int ret;

    for (int cu_i = 0;; cu_i++) {
        ret = dwarf_next_cu_header_d(dbg, is_info, NULL,
            NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, &next_cu_header, &header_cu_type, NULL);
        if (ret == DW_DLV_NO_ENTRY) {
            break;
        }
        Dwarf_Die cu_die = 0;
        ret = dwarf_siblingof_b(dbg, 0, is_info, &cu_die, NULL);
        if (ret == DW_DLV_OK) {
            dwarf_get_version_of_die(cu_die,&dwversion,&offset_size);
            if (pc_in_die(dbg, cu_die,dwversion, pc)) {
                Dwarf_Bool lookup_ret = lookup_pc_cu(dbg, flags,
                    pc, cu_die);
                dwarf_dealloc_die(cu_die);
                while (dwarf_next_cu_header_d(dbg, is_info,
                    NULL, NULL, NULL, NULL, NULL, NULL,
                    NULL, NULL, &next_cu_header,
                    &header_cu_type, NULL)
                    != DW_DLV_NO_ENTRY) {}
                return lookup_ret;
            } else {
                dwarf_dealloc_die(cu_die);
                cu_die = 0;
            }
        }
    }
    return FALSE;
}

#if 0
static Dwarf_Bool
get_pc_range_die(Dwarf_Die die,
    Dwarf_Addr *low_out,
    Dwarf_Addr *high_out)
{
    int ret;
    Dwarf_Addr lowpc, highpc;
    Dwarf_Bool is_low_set = FALSE;
    Dwarf_Bool is_high_set = FALSE;
    enum Dwarf_Form_Class highpc_cls;

    ret = dwarf_lowpc(die, &lowpc, NULL);
    if (ret == DW_DLV_OK) {
        is_low_set = TRUE;
        *low_out = lowpc;
    }
    if (is_low_set) {
        ret = dwarf_highpc_b(die, &highpc, NULL, &highpc_cls, NULL);
        if (ret == DW_DLV_OK) {
            if (highpc_cls == DW_FORM_CLASS_CONSTANT) {
                *high_out = lowpc + highpc;
            } else {
                *high_out = highpc;
            }
            is_high_set = TRUE;
        }
    }
    return is_low_set && is_high_set;
}
#endif

static int
get_rnglist_offset(Dwarf_Attribute attr, Dwarf_Unsigned* offset) {
    Dwarf_Half attrform = 0;
    dwarf_whatform(attr, &attrform, NULL);
    if (attrform == DW_FORM_rnglistx) {
        return dwarf_formudata(attr, offset, NULL);
    } else {
        return dwarf_global_formref(attr, offset, NULL);
    }
}

static int
dwarf5_ranges(Dwarf_Die cu_die,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest)
{
    Dwarf_Unsigned offset = 0;
    Dwarf_Attribute attr = 0;
    Dwarf_Half attrform = 0;
    Dwarf_Unsigned i = 0;
    int res = 0;

    res = dwarf_attr(cu_die, DW_AT_ranges, &attr, NULL);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (get_rnglist_offset(attr, &offset) == DW_DLV_OK) {
        Dwarf_Unsigned rlesetoffset = 0;
        Dwarf_Unsigned rnglists_count = 0;
        Dwarf_Rnglists_Head head = 0;

        dwarf_whatform(attr,&attrform,NULL);
        /* offset is in .debug_rnglists */
        res = dwarf_rnglists_get_rle_head(attr, attrform,offset,
            &head,
            &rnglists_count,&rlesetoffset,NULL);
        if (res != DW_DLV_OK) {
            /* ASSERT: is DW_DLV_NO_ENTRY */
            dwarf_dealloc_attribute(attr);
            return res;
        }
        for ( ; i < rnglists_count; ++i) {
            unsigned entrylen = 0;
            unsigned rle_val = 0;
            Dwarf_Unsigned raw1 = 0;
            Dwarf_Unsigned raw2 = 0;
            Dwarf_Bool unavail = 0;
            Dwarf_Unsigned cooked1 = 0;
            Dwarf_Unsigned cooked2 = 0;

            res = dwarf_get_rnglists_entry_fields_a(head,
                i,&entrylen,&rle_val,&raw1,&raw2,
                &unavail,&cooked1,&cooked2,NULL);
            if (res != DW_DLV_OK) {
                /* ASSERT: is DW_DLV_NO_ENTRY */
                continue;
            }
            if (unavail) {
                continue;
            }
            switch(rle_val) {
            case DW_RLE_end_of_list:
            case DW_RLE_base_address:
            case DW_RLE_base_addressx:
                /* These are accounted for already */
                break;
            case DW_RLE_offset_pair:
            case DW_RLE_startx_endx:
            case DW_RLE_start_end:
            case DW_RLE_startx_length:
            case DW_RLE_start_length:
                if (cooked1 < *lowest) {
                    *lowest = cooked1;
                }
                if (cooked2 > *highest) {
                    *highest = cooked2;
                }
            default:
                /* Something is wrong. */
                break;

            }
        }
        dwarf_dealloc_rnglists_head(head);
    }
    dwarf_dealloc_attribute(attr);
    return DW_DLV_OK;
}

static int
dwarf4_ranges( Dwarf_Debug dbg,
    Dwarf_Die cu_die,
    Dwarf_Addr cu_lowpc,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest)
{
    Dwarf_Unsigned offset;
    Dwarf_Attribute attr = 0;
    int res = 0;

    res = dwarf_attr(cu_die, DW_AT_ranges, &attr, NULL);
    if (res != DW_DLV_OK) {
        return res;
    }
    if (dwarf_global_formref(attr, &offset, NULL) == DW_DLV_OK) {
        Dwarf_Signed count = 0;
        Dwarf_Ranges *ranges = 0;
        Dwarf_Addr baseaddr = 0;
        if (cu_lowpc != HIGHADDR) {
            baseaddr = cu_lowpc;
        }
        res = dwarf_get_ranges_b(dbg, offset, cu_die,
            NULL, &ranges, &count, NULL, NULL);
        for (int i = 0; i < count; i++) {
            Dwarf_Ranges *cur = ranges + i;

            if (cur->dwr_type == DW_RANGES_ENTRY) {
                Dwarf_Addr rng_lowpc, rng_highpc;
                rng_lowpc = baseaddr + cur->dwr_addr1;
                rng_highpc = baseaddr + cur->dwr_addr2;
                if (rng_lowpc < *lowest) {
                    *lowest = rng_lowpc;
                }
                if (rng_highpc > *highest) {
                    *highest = rng_highpc;
                }
            } else if (cur->dwr_type ==
                DW_RANGES_ADDRESS_SELECTION) {
                baseaddr = cur->dwr_addr2;
            } else {  // DW_RANGES_END
                baseaddr = cu_lowpc;
            }
        }
        dwarf_dealloc_ranges(dbg, ranges, count);
    }
    dwarf_dealloc_attribute(attr);
    return DW_DLV_OK;
}

static Dwarf_Bool
get_pc_range(Dwarf_Debug dbg,
    Dwarf_Addr *lowest,
    Dwarf_Addr *highest,
    int *cu_cnt)
{
    Dwarf_Bool is_info = TRUE;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;
    Dwarf_Half dwversion = 0;
    Dwarf_Half offset_size = 0;

    *lowest = HIGHADDR;
    *highest = 0;
    int ret, cu_i;
    for (cu_i = 0;; cu_i++) {
        ret = dwarf_next_cu_header_d(dbg, is_info, NULL,
            NULL, NULL, NULL, NULL, NULL,
            NULL, NULL, &next_cu_header, &header_cu_type, NULL);
        if (ret == DW_DLV_NO_ENTRY) {
            break;
        }
        Dwarf_Die cu_die = 0;
        ret = dwarf_siblingof_b(dbg, 0, is_info, &cu_die, NULL);
        if (ret == DW_DLV_OK) {
            Dwarf_Addr cu_lowpc = HIGHADDR, cu_highpc;
            enum Dwarf_Form_Class highpc_cls;

            dwarf_get_version_of_die(cu_die,&dwversion,&offset_size);
            ret = dwarf_lowpc(cu_die, &cu_lowpc, NULL);
            if (ret == DW_DLV_OK) {
                if (cu_lowpc < *lowest) {
                    *lowest = cu_lowpc;
                }
                ret = dwarf_highpc_b(cu_die, &cu_highpc,
                    NULL, &highpc_cls, NULL);
                if (ret == DW_DLV_OK) {
                    if (highpc_cls == DW_FORM_CLASS_CONSTANT) {
                        cu_highpc += cu_lowpc;
                    }
                    if (cu_highpc > *highest) {
                        *highest = cu_highpc;
                    }
                }
            }
            if (dwversion >= DWARF5_VERSION) {
                ret = dwarf5_ranges(cu_die,
                    lowest,highest);
            } else {
                ret = dwarf4_ranges(dbg,cu_die,cu_lowpc,
                    lowest,highest);
            }
            if (ret == DW_DLV_ERROR) {
                return 0;
            }
            dwarf_dealloc_die(cu_die);
            cu_die = 0;
        }
    }
    *cu_cnt = cu_i;
    return (*lowest != HIGHADDR && *highest != 0);
}

static void
populate_lookup_table_die(lookup_tableT *lookup_table,
    int cu_i,
    Dwarf_Die cu_die)
{
    int ret;
    Dwarf_Unsigned version;
    Dwarf_Small table_count;

    ret = dwarf_srclines_b(cu_die, &version, &table_count,
        &lookup_table->ctxts[cu_i], NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        return;
    }
    if (table_count == 1) {
        Dwarf_Line *linebuf = 0;
        Dwarf_Signed linecount = 0;

        ret = dwarf_srclines_from_linecontext(
            lookup_table->ctxts[cu_i], &linebuf, &linecount, NULL);
        if (ret == DW_DLV_NO_ENTRY) {
            dwarf_srclines_dealloc_b(lookup_table->ctxts[cu_i]);
            return;
        }
        Dwarf_Addr prev_lineaddr;
        Dwarf_Line prev_line = 0;
        for (int i = 0; i < linecount; i++) {
            Dwarf_Line line = linebuf[i];
            Dwarf_Addr lineaddr = 0;
            ret = dwarf_lineaddr(line, &lineaddr, NULL);
            if (ret == DW_DLV_NO_ENTRY) {
                continue;
            }
            if (prev_line) {
                for (Dwarf_Addr addr = prev_lineaddr;
                    addr < lineaddr; addr++) {
                    lookup_table->table[addr - lookup_table->low] =
                        linebuf[i - 1];
                }
            }
            Dwarf_Bool is_lne = 0;
            ret = dwarf_lineendsequence(line, &is_lne, NULL);
            if (ret == DW_DLV_NO_ENTRY) {
                continue;
            }
            if (is_lne) {
                prev_line = 0;
            } else {
                prev_lineaddr = lineaddr;
                prev_line = line;
            }
        }
    }
}

static void
populate_lookup_table(Dwarf_Debug dbg,
    lookup_tableT *lookup_table)
{
    Dwarf_Bool is_info = TRUE;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;
    int ret;
    for (int cu_i = 0;; cu_i++) {
        ret = dwarf_next_cu_header_d(dbg, is_info, NULL, NULL,
            NULL, NULL, NULL, NULL,
            NULL, NULL, &next_cu_header, &header_cu_type, NULL);
        if (ret == DW_DLV_NO_ENTRY) {
            break;
        }
        Dwarf_Die cu_die = 0;
        ret = dwarf_siblingof_b(dbg, 0, is_info, &cu_die, NULL);
        if (ret == DW_DLV_OK) {
            populate_lookup_table_die(lookup_table,
                cu_i, cu_die);
            dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
            cu_die = 0;
        }
    }
}

static int
create_lookup_table(Dwarf_Debug dbg,
    lookup_tableT *lookup_table)
{
    Dwarf_Addr low, high;
    int cu_cnt;

    if (!get_pc_range(dbg, &low, &high, &cu_cnt)) {
        goto exit;
    }
    lookup_table->table = malloc((high - low) * sizeof(Dwarf_Line));
    if (! lookup_table->table) {
        goto exit;
    }
    lookup_table->ctxts = malloc(cu_cnt * sizeof(Dwarf_Line_Context));
    if (! lookup_table->ctxts) {
        goto free_table_exit;
    }
    lookup_table->cnt = cu_cnt;
    lookup_table->low = low;
    lookup_table->high = high;
    populate_lookup_table(dbg, lookup_table);
    return 0;

    free_table_exit:
    free(lookup_table->table);

    exit:
    lookup_table->table = NULL;
    lookup_table->ctxts = NULL;
    return 1;
}

static void
delete_lookup_table(lookup_tableT *lookup_table)
{
    free(lookup_table->table);
    lookup_table->table = NULL;
    for (int i = 0; i < lookup_table->cnt; i++) {
        dwarf_srclines_dealloc_b(lookup_table->ctxts[i]);
    }
    free(lookup_table->ctxts);
    lookup_table->ctxts = NULL;
}

static char *
get_pc_buf(int argc, char **argv, char *buf,
    Dwarf_Bool do_read_stdin)
{
    if (do_read_stdin) {
        return fgets(buf, MAX_ADDR_LEN, stdin);
    } else {
        if (optind < argc) {
            return argv[optind++];
        } else {
            return NULL;
        }
    }
}

char *usagestrings[] = {
"addr2line [ -h] [-a] [-e <objectpath>] [-b] [-n] [address] ...",
"where",
"-a --addresses  Turns on printing of address before",
"    the source line text",
"-e --exe  <path> The the path to the object file to read.",
"    Path defaults to \"a.out\"",
"-b --force-batch The CU address ranges will be looked",
"    up once at the start and the generated table used.",
"-n --force-no-batch The addresses are looked up",
"    independently for each address present.",
"    In certain cases the no-batch will be overridden",
"    and batching used.",
"-h --help  Prints a help message and stops.",
0
};

static void
usage(void)
{
    char **u = usagestrings;

    for( ; *u ; u++) {
        printf("%s\n",*u);
    }
    exit(0);
}

static void
populate_options(int argc, char *argv[], char **objfile,
    flagsT *flags)
{
    int c;
    while (1) {
        int option_index = 0;
        static struct option longopts[] =
            {
            {"addresses", no_argument, 0, 'a'},
            {"exe", required_argument, 0, 'e'},
            {"help", no_argument, 0, 'h'},
            {"force-batch", no_argument, 0, 'b'},
            {"force-no-batch", no_argument, 0, 'n'},
            {0, 0, 0, 0}
            };
        c = getopt_long(argc, argv, "ae:hbn", longopts,
            &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'a':
            flags->addresses = TRUE;
            break;
        case 'e':
            *objfile = optarg;
            break;
        case 'b':
            flags->force_batchmode = TRUE;
            break;
        case 'h':
            usage();
            break;
        case 'n':
            flags->force_nobatchmode = TRUE;
            break;
        case '?':
            break;
        default:
            printf("?? getopt returned character code 0%o ??\n", c);
            break;
        }
    }
}

int
main(int argc, char *argv[])
{
    int ret;
    Dwarf_Debug dbg;
    char *objfile = "a.out";
    Dwarf_Bool do_read_stdin = FALSE;
    char buf[MAX_ADDR_LEN];
    char *pc_buf = 0;
    char *endptr = 0;
    lookup_tableT lookup_table;
    Dwarf_Ptr errarg = 0;

    flagsT flags = {0};
    populate_options(argc, argv, &objfile, &flags);
    objfile_name = objfile;
    ret = dwarf_init_path(objfile, NULL, 0,
        DW_GROUPNUMBER_ANY, err_handler, errarg, &dbg, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        fail_exit("Unable to open file");
    }

    do_read_stdin = (optind >= argc);
    if (! flags.force_nobatchmode &&
        (flags.force_batchmode || do_read_stdin ||
        (argc + BATCHMODE_HEURISTIC) > optind)) {
        flags.batchmode = TRUE;
    }
    if (flags.batchmode) {
        ret = create_lookup_table(dbg, &lookup_table);
        if (ret != DW_DLV_OK) {
            fail_exit("Unable to create lookup table");
        }
    }

    while ((pc_buf = get_pc_buf(argc, argv, buf, do_read_stdin))) {
        Dwarf_Addr pc = strtoull(pc_buf, &endptr, 16);
        Dwarf_Bool is_found = FALSE;

        if (endptr != pc_buf) {
            if (flags.batchmode && lookup_table.table) {
                if (pc >= lookup_table.low &&
                    pc < lookup_table.high) {
                    Dwarf_Line line =
                        lookup_table.table[pc - lookup_table.low];
                    if (line) {
                        print_line(dbg, &flags, line, pc);
                        is_found = TRUE;
                    }
                }
            } else {
                is_found = lookup_pc(dbg, &flags, pc);
            }
        }
        if (! is_found) {
            print_line(dbg, &flags, NULL, pc);
        }
    }
    if (flags.batchmode && lookup_table.table) {
        delete_lookup_table(&lookup_table);
    }
    dwarf_finish(dbg);
    return 0;
}
