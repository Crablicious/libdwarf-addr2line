#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <libelf.h>

#define MAX_ADDR_LEN 20

static void err_handler(Dwarf_Error err, Dwarf_Ptr errarg)
{
    errx(EXIT_FAILURE, "libdwarf error: %d %s0", dwarf_errno(err), dwarf_errmsg(err));
}

static bool pc_in_die(Dwarf_Die die, Dwarf_Addr pc)
{
    int ret;
    Dwarf_Addr lowpc, highpc;
    enum Dwarf_Form_Class highpc_cls;
    ret = dwarf_lowpc(die, &lowpc, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        return false;
    } else {
        if (pc < lowpc) {
            return false;
        }
    }
    ret = dwarf_highpc_b(die, &highpc, NULL, &highpc_cls, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        return false;
    } else {
        if (highpc_cls == DW_FORM_CLASS_CONSTANT) {
            highpc += lowpc;
        }
        return pc < highpc;
    }
}

static void print_line(Dwarf_Debug dbg, Dwarf_Line line)
{
    char *linesrc;
    Dwarf_Unsigned lineno;
    dwarf_linesrc(line, &linesrc, NULL);
    dwarf_lineno(line, &lineno, NULL);
    printf("%s:%" DW_PR_DUu "\n", linesrc, lineno);
    dwarf_dealloc(dbg, linesrc, DW_DLA_STRING);
}

static bool lookup_pc_cu(Dwarf_Debug dbg, Dwarf_Addr pc, Dwarf_Die cu_die)
{
    int ret;
    Dwarf_Unsigned version;
    Dwarf_Small table_count;
    Dwarf_Line_Context ctxt;
    ret = dwarf_srclines_b(cu_die, &version, &table_count, &ctxt, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        return false;
    }
    bool is_found = false;
    if (table_count == 1) {
        Dwarf_Line *linebuf = 0;
        Dwarf_Signed linecount = 0;
        Dwarf_Error err;
        ret = dwarf_srclines_from_linecontext(ctxt, &linebuf, &linecount, &err);
        if (ret == DW_DLV_ERROR) {
            dwarf_srclines_dealloc_b(ctxt);
            err_handler(err, NULL);
        }
        for (int i = 0; i < linecount; i++) {
            Dwarf_Line line = linebuf[i];
            Dwarf_Bool is_end_seq;
            dwarf_lineendsequence(line, &is_end_seq, NULL);
            if (is_end_seq) {
                continue;
            }
            Dwarf_Addr lineaddr;
            dwarf_lineaddr(line, &lineaddr, NULL);
            if (pc == lineaddr) {
                is_found = true;
                print_line(dbg, line);
                break;
            } else if (pc < lineaddr) {
                if (i == 0) {
                    break;
                } else {
                    is_found = true;
                    print_line(dbg, linebuf[i - 1]);
                }
            }
        }
    }
    dwarf_srclines_dealloc_b(ctxt);
    return is_found;
}

static bool lookup_pc(Dwarf_Debug dbg, Dwarf_Addr pc)
{
    Dwarf_Bool is_info = true;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;
    int ret;
    for (;;) {
        ret = dwarf_next_cu_header_d(dbg, is_info, NULL, NULL, NULL, NULL, NULL, NULL,
                                     NULL, NULL, &next_cu_header, &header_cu_type, NULL);
        if (ret == DW_DLV_NO_ENTRY) {
            break;
        }
        Dwarf_Die cu_die = 0;
        ret = dwarf_siblingof_b(dbg, 0, is_info, &cu_die, NULL);
        if (ret == DW_DLV_OK) {
            if (pc_in_die(cu_die, pc)) {
                bool lookup_ret = lookup_pc_cu(dbg, pc, cu_die);
                dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
                while (dwarf_next_cu_header_d(dbg, is_info, NULL, NULL, NULL, NULL, NULL, NULL,
                                              NULL, NULL, &next_cu_header, &header_cu_type, NULL)
                       != DW_DLV_NO_ENTRY) {}
                return lookup_ret;
            }
            dwarf_dealloc(dbg, cu_die, DW_DLA_DIE);
            cu_die = 0;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    char *objfile = "a.out";
    int c;
    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option longopts[] =
            {
             {"addresses", no_argument, 0, 'a'},
             {"exe", required_argument, 0, 'e'},
             {0, 0, 0, 0}
            };
        c = getopt_long(argc, argv, "ae:", longopts, &option_index);
        if (c == -1) {
            break;
        }
        /* TODO: Add address flag and some more flags. */
        switch (c) {
            case 'a':
                break;
            case 'e':
                objfile = optarg;
                break;
            case '?':
                break;
            default:
                printf("?? getopt returned character code 0%o ??\n", c);
                break;
        }
    }

    int ret;
    Dwarf_Debug dbg;
    ret = dwarf_init_path(objfile, NULL, 0, DW_DLC_READ, DW_GROUPNUMBER_ANY, err_handler, NULL, &dbg, 0, 0, 0, NULL);
    if (ret == DW_DLV_NO_ENTRY) {
        errx(EXIT_FAILURE, "%s not found", objfile);
    }

    if (optind < argc) {
        while (optind < argc) {
            char *endptr;
            Dwarf_Addr pc = strtoull(argv[optind], &endptr, 16);
            if (endptr != argv[optind++]) {
                ret = lookup_pc(dbg, pc);
                if (! ret) {
                    printf("??\n");
                }
            }
        }
    } else {
        char input_buf[MAX_ADDR_LEN];
        while (fgets(input_buf, MAX_ADDR_LEN, stdin)) {
            char *endptr;
            Dwarf_Addr pc = strtoull(input_buf, &endptr, 16);
            if (endptr != input_buf) {
                ret = lookup_pc(dbg, pc);
                if (! ret) {
                    printf("??\n");
                }
            }
        }
    }
    dwarf_finish(dbg, NULL);
    return 0;
}
