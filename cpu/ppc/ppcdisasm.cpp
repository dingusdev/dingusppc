/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** @file   Logic for the disassembler.
    @author maximumspatium
 */

#define _CRT_SECURE_NO_WARNINGS /* shut up MSVC regarding the unsafe strcpy/strcat */

#include "ppcdisasm.h"
#include <cstring>
#include <functional> /* without this, MSVC doesn't understand std::function */
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

template <typename... Args>
std::string my_sprintf(const char* format, Args... args) {
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0)
        return {}; /* empty string in C++11 */

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;
    return str;
}

const char* arith_im_mnem[9] = {"mulli", "subfic", "", "", "", "addic", "addic.", "addi", "addis"};

const char* bx_mnem[4] = {"b", "bl", "ba", "bla"};

const char* bclrx_mnem[2] = {"blr", "blrl"};

const char* bcctrx_mnem[2] = {"bctr", "bctrl"};

const char* br_cond[8] = {/* simplified branch conditions */
                          "ge",
                          "le",
                          "ne",
                          "ns",
                          "lt",
                          "gt",
                          "eq",
                          "so"};

const char* bclrx_cond[8] = {/* simplified branch conditions */
                             "gelr",
                             "lelr",
                             "nelr",
                             "nslr",
                             "ltlr",
                             "gtlr",
                             "eqlr",
                             "solr"};

const char* bcctrx_cond[8] = {/* simplified branch conditions */
                              "gectr",
                              "lectr",
                              "nectr",
                              "nsctr",
                              "ltctr",
                              "gtctr",
                              "eqctr",
                              "soctr"};

const char* opc_idx_ldst[32] = {/* indexed load/store opcodes */
                                "lwzx",   "lwzux", "lbzx",   "lbzux", "stwx",  "stwux", "stbx",
                                "stbux",  "lhzx",  "lhzux",  "lhax",  "lhaux", "sthx",  "sthux",
                                "",       "",      "lfsx",   "lfsux", "lfdx",  "lfdux", "stfsx",
                                "stfsux", "stfdx", "stfdux", "",      "",      "",      "",
                                "",       "",      "stfiwx", ""};

const char* opc_bim_str[6] = {/* boolean immediate instructions */
                              "ori",
                              "oris",
                              "xori",
                              "xoris",
                              "andi.",
                              "andis."};

const char* opc_logic[16] = {/* logic instructions */
                             "and",
                             "andc",
                             "",
                             "nor",
                             "",
                             "",
                             "",
                             "",
                             "eqv",
                             "xor",
                             "",
                             "",
                             "orc",
                             "or",
                             "nand",
                             ""};

const char* opc_subs[16] = {/* subtracts & friends */
                            "subfc",
                            "subf",
                            "",
                            "neg",
                            "subfe",
                            "",
                            "subfze",
                            "subfme",
                            "doz",
                            "",
                            "",
                            "abs",
                            "",
                            "",
                            "",
                            "nabs"};

const char* opc_adds[9] = {/* additions */
                           "addc",
                           "",
                           "",
                           "",
                           "adde",
                           "",
                           "addze",
                           "addme",
                           "add"};

const char* opc_muldivs[16] = {/* multiply and division instructions */
                               "mulhwu",
                               "",
                               "mulhw",
                               "mul",
                               "",
                               "",
                               "",
                               "mullw",
                               "",
                               "",
                               "div",
                               "divs",
                               "",
                               "",
                               "divwu",
                               "divw"};

const char* opc_shft_reg[32]{
    /* Regular shift instructions */
    "slw",  "",      "",     "",      "slq", "sliq", "sllq", "slliq", "", "",    "",
    "",     "",      "",     "",      "",    "srw",  "",     "",      "", "srq", "sriq",
    "srlq", "srliq", "sraw", "srawi", "",    "",     "sraq", "sraiq", "", ""};


const char* opc_shft_ext[32]{
    /* Extended shift instructions (601 only) */
    "",     "", "", "", "sle", "", "sleq", "", "", "", "", "", "",     "", "", "",
    "rrib", "", "", "", "sre", "", "sreq", "", "", "", "", "", "srea", "", "", ""};

const char* opc_int_ldst[16] = {/* integer load and store instructions */
                                "lwz",
                                "lwzu",
                                "lbz",
                                "lbzu",
                                "stw",
                                "stwu",
                                "stb",
                                "stbu",
                                "lhz",
                                "lhzu",
                                "lha",
                                "lhau",
                                "sth",
                                "sthu",
                                "lmw",
                                "stmw"};

/* processor management + byte reversed load and store */
const char* proc_mgmt_str[32] = {
    "", "dcbst", "dcbf",  "",     "stwcx.", "",      "",        "dcbtst", "dcbt", "eciwx",  "",
    "", "",      "ecowx", "dcbi", "",       "lwbrx", "tlbsync", "sync",   "",     "stwbrx", "",
    "", "dcba",  "lhbrx", "",     "eieio",  "",      "sthbrx",  "",       "icbi", "dcbz"};

const char* opc_flt_ldst[8] = {/* integer load and store instructions */
                               "lfs",
                               "lfsu",
                               "lfd",
                               "lfdu",
                               "stfs",
                               "stfsu",
                               "stfd",
                               "stfdu"};

const char* opc_flt_ext_arith[32] = {
    /* integer load and store instructions */
    "", "", "", "", "", "", "", "",     "", "",     "", "", "",      "",      "",       "",
    "", "", "", "", "", "", "", "fsel", "", "fmul", "", "", "fmsub", "fmadd", "fnmsub", "fnmadd",
};

const char* trap_cond[32] = {/*Trap conditions*/
                             "",     "twlgt", "twllt", "", "tweq", "twlge", "twlle", "",
                             "twgt", "",      "",      "", "twge", "",      "",      "",
                             "twlt", "",      "",      "", "twle", "",      "",      "",
                             "twne", "",      "",      "", "",     "",      "",      ""};

const char* spr_index0[32] = {"mq", "xer",  "",      "",     "rtcu", "rtcl", "",    "",
                              "lr", "ctr",  "",      "",     "",     "",     "",    "",
                              "",   "",     "dsisr", "dar",  "",     "",     "dec", "",
                              "",   "sdr1", "srr0",  "srr1", "",     "",     "",    ""};

const char* spr_index8[32] = {
    "",      "",      "", "", "",    "",      "",      "",      "",      "",      "",
    "",      "",      "", "", "",    "sprg0", "sprg1", "sprg2", "sprg3", "sprg4", "sprg5",
    "sprg6", "sprg7", "", "", "ear", "",      "tbl",   "tbu",   "",      "pvr"};

const char* spr_index16[32] = {
    "",       "",       "",       "",       "",       "",       "",       "",
    "",       "",       "",       "",       "",       "",       "",       "",
    "ibat0u", "ibat0l", "ibat1u", "ibat1l", "ibat2u", "ibat2l", "ibat3u", "ibat3l",
    "dbat0u", "dbat0l", "dbat1u", "dbat1l", "dbat2u", "dbat2l", "dbat3u", "dbat3l",
};

const char* spr_index29[32] = {
    "",     "",       "",      "",      "",     "",    "",      "",     "ummcr0", "upmc1", "upmc2",
    "usia", "ummcr1", "upmc3", "upmc4", "",     "",    "",      "",     "",       "",      "",
    "",     "",       "mmcr0", "pmc1",  "pmc2", "sia", "mmcr1", "pmc3", "pmc4",   "sda"};

const char* spr_index30[32] = {
    "",      "",     "",      "",      "",      "",     "",    "", "", "", "", "", "", "", "", "",
    "dmiss", "dcmp", "hash1", "hash2", "imiss", "icmp", "rpa", "", "", "", "", "", "", "", "", ""};

const char* spr_index31[32] = {
    "", "", "", "",     "", "",     "",      "",      "",      "",    "",
    "", "", "", "",     "", "hid0", "hid1",  "iabr",  "",      "",    "dabr",
    "", "", "", "l2cr", "", "ictc", "thrm1", "thrm2", "thrm3", "pir",
};

/** various formatting helpers. */
void fmt_oneop(string& buf, const char* opc, int src) {
    buf = my_sprintf("%-8sr%d", opc, src);
}

void fmt_twoop(string& buf, const char* opc, int dst, int src) {
    buf = my_sprintf("%-8sr%d, r%d", opc, dst, src);
}

void fmt_twoop_simm(string& buf, const char* opc, int dst, int imm) {
    buf = my_sprintf("%-8sr%d, %s0x%X", opc, dst, (imm < 0) ? "-" : "", abs(imm));
}

void fmt_twoop_fromspr(string& buf, const char* opc, int dst, int src) {
    buf = my_sprintf("%-8sr%d, spr%d", opc, dst, src);
}

void fmt_twoop_tospr(string& buf, const char* opc, int dst, int src) {
    buf = my_sprintf("%-8sspr%d, r%d", opc, dst, src);
}

void fmt_twoop_flt(string& buf, const char* opc, int dst, int src1) {
    buf = my_sprintf("%-8sf%d, f%d", opc, dst, src1);
}

void fmt_threeop(string& buf, const char* opc, int dst, int src1, int src2) {
    buf = my_sprintf("%-8sr%d, r%d, r%d", opc, dst, src1, src2);
}

void fmt_threeop_crb(string& buf, const char* opc, int dst, int src1, int src2) {
    buf = my_sprintf("%-8scrb%d, crb%d, crb%d", opc, dst, src1, src2);
}

void fmt_threeop_uimm(string& buf, const char* opc, int dst, int src1, int imm) {
    buf = my_sprintf("%-8sr%d, r%d, 0x%X", opc, dst, src1, imm);
}

void fmt_threeop_simm(string& buf, const char* opc, int dst, int src1, int imm) {
    buf = my_sprintf("%-8sr%d, r%d, %s0x%X", opc, dst, src1, (imm < 0) ? "-" : "", abs(imm));
}

void fmt_threeop_flt(string& buf, const char* opc, int dst, int src1, int src2) {
    buf = my_sprintf("%-8sf%d, f%d, f%d", opc, dst, src1, src2);
}

void fmt_fourop_flt(string& buf, const char* opc, int dst, int src1, int src2, int src3) {
    buf = my_sprintf("%-8sf%d, f%d, f%d, f%d", opc, dst, src1, src2, src3);
}

void fmt_rotateop(string& buf, const char* opc, int dst, int src, int sh, int mb, int me, bool imm) {
    if (imm)
        buf = my_sprintf("%-8sr%d, r%d, %d, %d, %d", opc, dst, src, sh, mb, me);
    else
        buf = my_sprintf("%-8sr%d, r%d, r%d, %d, %d", opc, dst, src, sh, mb, me);
}

/* Opcodes */

void opc_illegal(PPCDisasmContext* ctx) {
    ctx->instr_str = my_sprintf("%-8s0x%08X", "dc.l", ctx->instr_code);
}

void opc_twi(PPCDisasmContext* ctx) {
    char opcode[10] = "";

    auto to     = (ctx->instr_code >> 21) & 0x1F;
    auto ra     = (ctx->instr_code >> 16) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    if (ctx->simplified) {
        strcpy(opcode, trap_cond[to]);

        if (strlen(opcode) > 0) {
            strcat(opcode, "i");
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%X", opcode, ra, imm);
            return;
        }
    }
    ctx->instr_str = my_sprintf("%-8s%d, r%d, 0x%X", "twi", to, ra, imm);
}

void opc_group4(PPCDisasmContext* ctx) {
    printf("Altivec group 4 not supported yet\n");
}

void opc_ar_im(PPCDisasmContext* ctx) {
    auto ra     = (ctx->instr_code >> 16) & 0x1F;
    auto rd     = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    if (ctx->simplified) {
        if (((ctx->instr_code >> 26) == 0xE) && !ra) {
            fmt_twoop_simm(ctx->instr_str, "li", rd, imm);
            return;
        } else if (((ctx->instr_code >> 26) == 0xF) && !ra) {
            fmt_twoop_simm(ctx->instr_str, "lis", rd, imm);
            return;
        }

        if (imm > 0x7FFF) {
            switch ((ctx->instr_code >> 26)) {
            case 0xC:
                fmt_threeop_simm(ctx->instr_str, "subic", rd, ra, imm);
                return;
            case 0xD:
                fmt_threeop_simm(ctx->instr_str, "subic.", rd, ra, imm);
                return;
            case 0xE:
                fmt_threeop_simm(ctx->instr_str, "subi", rd, ra, imm);
                return;
            case 0xF:
                fmt_threeop_simm(ctx->instr_str, "subis", rd, ra, imm);
                return;
            }
        }
    }

    fmt_threeop_simm(ctx->instr_str, arith_im_mnem[(ctx->instr_code >> 26) - 7], rd, ra, imm);
}

void power_dozi(PPCDisasmContext* ctx) {
    auto ra  = (ctx->instr_code >> 16) & 0x1F;
    auto rd  = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_simm(ctx->instr_str, "dozi", rd, ra, imm);
}

void fmt_rot_imm(PPCDisasmContext* ctx, const char* opc, int ra, int rs, int n) {
    char opcode[10];

    strcpy(opcode, opc);
    if (ctx->instr_code & 1)
        strcat(opcode, ".");

    ctx->instr_str = my_sprintf("%-8sr%d, r%d, %d", opcode, ra, rs, n);
}

void fmt_rot_2imm(PPCDisasmContext* ctx, const char* opc, int ra, int rs, int n, int b) {
    char opcode[10];

    strcpy(opcode, opc);
    if (ctx->instr_code & 1)
        strcat(opcode, ".");

    ctx->instr_str = my_sprintf("%-8sr%d, r%d, %d, %d", opcode, ra, rs, n, b);
}

void opc_rlwimi(PPCDisasmContext* ctx) {
    char opcode[10];
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto sh = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->simplified) {
        if ((32 - sh) == mb) {
            fmt_rot_2imm(ctx, "inslwi", ra, rs, me + 1 - mb, mb);
            return;
        } else if (sh == 32 - (me + 1)) {
            fmt_rot_2imm(ctx, "insrwi", ra, rs, (me - mb + 1), mb);
            return;
        }
    }

    strcpy(opcode, "rlwimi");
    if (ctx->instr_code & 1)
        strcat(opcode, ".");

    fmt_rotateop(ctx->instr_str, opcode, ra, rs, sh, mb, me, true);
}

void opc_rlwinm(PPCDisasmContext* ctx) {
    char opcode[10];
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto sh = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->simplified) {
        if ((mb == 0) && (me == 31)) {
            if (sh < 16) {
                fmt_rot_imm(ctx, "rotlwi", ra, rs, sh);
            } else {
                fmt_rot_imm(ctx, "rotrwi", ra, rs, 32 - sh);
            }
            return;
        } else if (me == 31) {
            if ((32 - sh) == mb) {
                fmt_rot_imm(ctx, "srwi", ra, rs, mb);
            } else if (sh == 0) {
                fmt_rot_imm(ctx, "clrlwi", ra, rs, mb);
            } else {
                fmt_rot_2imm(ctx, "extrwi", ra, rs, (32 - mb), (sh - (32 - mb)));
            }
            return;
        } else if (mb == 0) {
            if ((31 - me) == sh) {
                fmt_rot_imm(ctx, "slwi", ra, rs, sh);
            } else if (sh == 0) {
                fmt_rot_imm(ctx, "clrrwi", ra, rs, (31 - me));
            } else {
                fmt_rot_2imm(ctx, "extlwi", ra, rs, (me + 1), sh);
            }
            return;
        } else if (mb) {
            if ((31 - me) == sh) {
                fmt_rot_2imm(ctx, "clrlslwi", ra, rs, (mb + sh), sh);
                return;
            }
        }
    }

    strcpy(opcode, "rlwinm");
    if (ctx->instr_code & 1)
        strcat(opcode, ".");

    fmt_rotateop(ctx->instr_str, opcode, ra, rs, sh, mb, me, true);
}

void opc_rlmi(PPCDisasmContext* ctx) {
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->instr_code & 1)
        fmt_rotateop(ctx->instr_str, "rlmi.", ra, rs, rb, mb, me, false);
    else
        fmt_rotateop(ctx->instr_str, "rlmi", ra, rs, rb, mb, me, false);
}

void opc_rlwnm(PPCDisasmContext* ctx) {
    char opcode[10];
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->simplified) {
        if ((me == 31) && (mb == 0)) {
            strcpy(opcode, "rotlw");

            if (ctx->instr_code & 1)
                strcat(opcode, ".");

            fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
            return;
        }
    }
    strcpy(opcode, "rlwnm");
    if (ctx->instr_code & 1)
        strcat(opcode, ".");

    fmt_rotateop(ctx->instr_str, "rlwnm", ra, rs, rb, mb, me, false);
}

void opc_cmp_i_li(PPCDisasmContext* ctx) {
    auto ls   = (ctx->instr_code >> 21) & 0x1;
    auto ra   = (ctx->instr_code >> 16) & 0x1F;
    auto crfd = (ctx->instr_code >> 23) & 0x07;
    int imm   = ctx->instr_code & 0xFFFF;


    if (ctx->simplified) {
        if (!ls) {
            if ((ctx->instr_code >> 26) & 0x1)
                ctx->instr_str = my_sprintf("%-8scr%d, r%d, 0x%X", "cmpwi", crfd, ra, imm);
            else
                ctx->instr_str = my_sprintf(
                    "%-8scr%d, r%d, %s0x%X", "cmplwi", crfd, ra, (imm < 0) ? "-" : "", abs(imm));

            return;
        }
    }

    if ((ctx->instr_code >> 26) & 0x1)
        ctx->instr_str = my_sprintf("%-8scr%d, %d, r%d, 0x%X", "cmpi", crfd, ls, ra, imm);
    else
        ctx->instr_str = my_sprintf(
            "%-8scr%d, %d, r%d, %s0x%X", "cmpli", crfd, ls, ra, (imm < 0) ? "-" : "", abs(imm));
}

void opc_bool_im(PPCDisasmContext* ctx) {
    char opcode[10];

    auto ra    = (ctx->instr_code >> 16) & 0x1F;
    auto rs    = (ctx->instr_code >> 21) & 0x1F;
    auto index = ((ctx->instr_code >> 26) & 0x1F) - 24;
    auto imm   = ctx->instr_code & 0xFFFF;

    if (ctx->simplified) {
        if (index == 0) {
            if (imm == 0 && !ra && !rs && !imm) { /* unofficial, produced by IDA */
                ctx->instr_str = my_sprintf("%-8s", "nop");
                return;
            }
        }
    }

    strcpy(opcode, opc_bim_str[index]);
    fmt_threeop_uimm(ctx->instr_str, opcode, ra, rs, imm);
}

void generic_bcx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi, uint32_t dst) {
    char opcode[10] = "bc";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (ctx->instr_code & 2) {
        strcat(opcode, "a"); /* add suffix "a" if the AA bit is set */
    }
    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi, dst);
}

void generic_bcctrx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi) {
    char opcode[10] = "bcctr";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }

    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi);
}

void generic_bclrx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi) {
    char opcode[10] = "bclr";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }

    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi);
}

void opc_bcx(PPCDisasmContext* ctx) {
    uint32_t bo, bi, dst, cr;
    char opcode[10]   = "b";
    char operands[12] = "";

    bo  = (ctx->instr_code >> 21) & 0x1F;
    bi  = (ctx->instr_code >> 16) & 0x1F;
    cr  = bi >> 2;
    dst = ((ctx->instr_code & 2) ? 0 : ctx->instr_addr) + SIGNEXT(ctx->instr_code & 0xFFFC, 15);

    if (!ctx->simplified || ((bo & 0x10) && bi) || (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
        generic_bcx(ctx, bo, bi, dst);
        return;
    }

    if ((bo & 0x14) == 0x14) {
        ctx->instr_str = my_sprintf("%-8s0x%08X", bx_mnem[0], dst);
        return;
    }

    if (!(bo & 4)) {
        strcat(opcode, "d");
        strcat(opcode, (bo & 2) ? "z" : "nz");
        if (!(bo & 0x10)) {
            strcat(opcode, (bo & 8) ? "t" : "f");
            if (cr) {
                strcat(operands, "4*cr0+");
                operands[4] = cr + '0';
            }
            strcat(operands, br_cond[4 + (bi & 3)]);
            strcat(operands, ", ");
        }
    } else { /* CTR ignored */
        strcat(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
        if (cr) {
            strcat(operands, "cr0, ");
            operands[2] = cr + '0';
        }
    }

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (ctx->instr_code & 2) {
        strcat(opcode, "a"); /* add suffix "a" if the AA bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat(opcode, (ctx->instr_code & 0x8000) ? "-" : "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s0x%08X", opcode, operands, dst);
}

void opc_bcctrx(PPCDisasmContext* ctx) {
    uint32_t bo, bi, cr;
    char opcode[10]  = "b";
    char operands[4] = "";

    bo = (ctx->instr_code >> 21) & 0x1F;
    bi = (ctx->instr_code >> 16) & 0x1F;
    cr = bi >> 2;

    if (!(bo & 4)) { /* bcctr with BO[2] = 0 is invalid */
        opc_illegal(ctx);
    }

    if (!ctx->simplified || ((bo & 0x10) && bi) || (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
        generic_bcctrx(ctx, bo, bi);
        return;
    }

    if ((bo & 0x14) == 0x14) {
        ctx->instr_str = my_sprintf("%-8s", bcctrx_mnem[ctx->instr_code & 1]);
        return;
    }

    strcat(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
    strcat(opcode, "ctr");
    if (cr) {
        strcat(operands, "cr0");
        operands[2] = cr + '0';
    }

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat(opcode, "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s", opcode, operands);
}

void opc_bclrx(PPCDisasmContext* ctx) {
    uint32_t bo, bi, cr;
    char opcode[10]   = "b";
    char operands[12] = "";

    bo = (ctx->instr_code >> 21) & 0x1F;
    bi = (ctx->instr_code >> 16) & 0x1F;
    cr = bi >> 2;

    if (!ctx->simplified || ((bo & 0x10) && bi) || (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
        generic_bclrx(ctx, bo, bi);
        return;
    }

    if ((bo & 0x14) == 0x14) {
        ctx->instr_str = my_sprintf("%-8s", bclrx_mnem[ctx->instr_code & 1]);
        return;
    }

    if (!(bo & 4)) {
        strcat(opcode, "d");
        strcat(opcode, (bo & 2) ? "z" : "nz");
        if (!(bo & 0x10)) {
            strcat(opcode, (bo & 8) ? "t" : "f");
            if (cr) {
                strcat(operands, "4*cr0+");
                operands[4] = cr + '0';
            }
            strcat(operands, br_cond[4 + (bi & 3)]);
        }
    } else { /* CTR ignored */
        strcat(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
        if (cr) {
            strcat(operands, "cr0");
            operands[2] = cr + '0';
        }
    }

    strcat(opcode, "lr");

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat(opcode, "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s", opcode, operands);
}

void opc_bx(PPCDisasmContext* ctx) {
    uint32_t dst = ((ctx->instr_code & 2) ? 0 : ctx->instr_addr) +
        SIGNEXT(ctx->instr_code & 0x3FFFFFC, 25);

    ctx->instr_str = my_sprintf("%-8s0x%08X", bx_mnem[ctx->instr_code & 3], dst);
}

void opc_sc(PPCDisasmContext* ctx) {
    ctx->instr_str = my_sprintf("%-8s", "sc");
}

void opc_group19(PPCDisasmContext* ctx) {
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int ext_opc       = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    char operand1[12] = "";
    char operand2[12] = "";
    char operand3[12] = "";

    if (rs > 3) {
        strcat(operand1, "4*cr0+");
        operand1[4] = (rs >> 2) + '0';
    }
    strcat(operand1, br_cond[((rs % 4) + 4)]);


    if (ra > 3) {
        strcat(operand2, "4*cr0+");
        operand2[4] = (ra >> 2) + '0';
    }
    strcat(operand2, br_cond[((ra % 4) + 4)]);


    if (rb > 3) {
        strcat(operand3, "4*cr0+");
        operand3[4] = (rb >> 2) + '0';
    }
    strcat(operand3, br_cond[((rb % 4) + 4)]);

    switch (ext_opc) {
    case 0:
        ctx->instr_str = my_sprintf("%-8scr%d, cr%d", "mcrf", (rs >> 2), (ra >> 2));
        return;
    case 16:
        opc_bclrx(ctx);
        return;
    case 33:
        if (ctx->simplified && (ra == rb)) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crnor", operand1, operand2, operand3);
        } else {
            fmt_threeop_crb(ctx->instr_str, "crnor", rs, ra, rb);
        }
        return;
    case 50:
        ctx->instr_str = my_sprintf("%-8s", "rfi");
        return;
    case 129:
        if (ctx->simplified) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crandc", operand1, operand2, operand3);
        } else {
            fmt_threeop_crb(ctx->instr_str, "crandc", rs, ra, rb);
        }
        return;
    case 150:
        ctx->instr_str = my_sprintf("%-8s", "isync");
        return;
    case 193:
        if (ctx->simplified && (rs == ra) && (rs == rb)) {
            ctx->instr_str = my_sprintf("%-8scrb%d", "crclr", rs);
            return;
        } else if (ctx->simplified && ((rs != ra) || (rs != rb))) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crxor", operand1, operand2, operand3);
            return;
        }
        fmt_threeop_crb(ctx->instr_str, "crxor", rs, ra, rb);
        return;
    case 225:
        if (ctx->simplified) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crnand", operand1, operand2, operand3);
        } else {
            fmt_threeop_crb(ctx->instr_str, "crnand", rs, ra, rb);
        }
        return;
    case 257:
        if (ctx->simplified) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crand", operand1, operand2, operand3);
        } else {
            fmt_threeop_crb(ctx->instr_str, "crand", rs, ra, rb);
        }
        return;
    case 289:
        if (ctx->simplified && (rs == ra) && (rs == rb)) {
            ctx->instr_str = my_sprintf("%-8scrb%d", "crset", rs);
            return;
        } else if (ctx->simplified && ((rs != ra) || (rs != rb))) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "creqv", operand1, operand2, operand3);
            return;
        }
        fmt_threeop_crb(ctx->instr_str, "creqv", rs, ra, rb);
        return;
    case 417:
        if (ctx->simplified) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "crorc", operand1, operand2, operand3);
        } else {
            fmt_threeop_crb(ctx->instr_str, "crorc", rs, ra, rb);
        }
        return;
    case 449:
        if (ctx->simplified && (ra == rb)) {
            ctx->instr_str = my_sprintf("%-8scrb%d, crb%d", "crmove", rs, ra);
            return;
        } else if (ctx->simplified && (ra != rb)) {
            ctx->instr_str = my_sprintf("%-8s%s, %s, %s", "cror", operand1, operand2, operand3);
            return;
        }
        fmt_threeop_crb(ctx->instr_str, "cror", rs, ra, rb);
        return;
    case 528:
        opc_bcctrx(ctx);
        return;
    }
}


void opc_group31(PPCDisasmContext* ctx) {
    char opcode[10] = "";

    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    int index   = ext_opc >> 5;
    bool rc_set = ctx->instr_code & 1;

    switch (ext_opc & 0x1F) {
    case 8:           /* subtracts & friends */
        index &= 0xF; /* strip OE bit */
        if (!strlen(opc_subs[index])) {
            opc_illegal(ctx);
        } else {
            strcpy(opcode, opc_subs[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat(opcode, "o");
            if (rc_set)
                strcat(opcode, ".");
            if (index == 3 || index == 6 || index == 7 || index == 11 ||
                index == 15) { /* ugly check for two-operands instructions */
                if (rb != 0)
                    opc_illegal(ctx);
                else
                    fmt_twoop(ctx->instr_str, opcode, rs, ra);
            } else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 10:          /* additions */
        index &= 0xF; /* strip OE bit */
        if (index > 8 || !strlen(opc_adds[index])) {
            opc_illegal(ctx);
        } else {
            strcpy(opcode, opc_adds[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat(opcode, "o");
            if (rc_set)
                strcat(opcode, ".");
            if (index == 6 || index == 7) {
                if (rb != 0)
                    opc_illegal(ctx);
                else
                    fmt_twoop(ctx->instr_str, opcode, rs, ra);
            } else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 11:          /* integer multiplications and divisions */
        index &= 0xF; /* strip OE bit */
        if (!strlen(opc_muldivs[index])) {
            opc_illegal(ctx);
        } else {
            strcpy(opcode, opc_muldivs[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat(opcode, "o");
            if (rc_set)
                strcat(opcode, ".");
            if ((!index || index == 2) && (ext_opc & 0x200))
                opc_illegal(ctx);
            else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 0x12: /* tlb & segment register instructions */

        if (index == 4) { /* mtmsr */
            if ((ra != 0) || (rb != 0) || rc_set)
                opc_illegal(ctx);
            else
                ctx->instr_str = my_sprintf("%-8sr%d", "mtmsr", rs);
        } else if (index == 6) { /* mtsr */
            if (ra & 16)
                opc_illegal(ctx);
            else
                ctx->instr_str = my_sprintf("%-8s%d, r%d", "mtsr", ra, rs);
        } else if (index == 7) { /* mtsrin */
            ctx->instr_str = my_sprintf("%-8sr%d, r%d", "mtsrin", rs, rb);
        } else if (index == 9) { /* tlbie */
            ctx->instr_str = my_sprintf("%-8sr%d", "tlbie", rb);
        } else if (index == 11) { /* tlbia */
            ctx->instr_str = my_sprintf("%-8s", "tlbia");
        } else if (index == 30) { /* tlbld - 603 only */
            if (!rs && !ra)
                opc_illegal(ctx);
            else
                ctx->instr_str = my_sprintf("%-8sr%s", "tlbld", rb);
        } else if (index == 30) { /* tlbli - 603 only */
            if (!rs && !ra)
                opc_illegal(ctx);
            else
                ctx->instr_str = my_sprintf("%-8sr%s", "tlbli", rb);
        }
        return;

    case 0x18: /* Shifting instructions */
        strcpy(opcode, opc_shft_reg[index]);

        if (rc_set)
            strcat(opcode, ".");


        if ((index == 0) || (index == 4) || (index == 6) || (index == 16) || (index == 20) ||
            (index == 22) || (index == 24) || (index == 28)) {
            fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
        } else if ((index == 5) || (index == 7) || (index == 21) || (index == 23) || (index == 25) || (index == 29)) {
            fmt_threeop_simm(ctx->instr_str, opcode, ra, rs, rb);
        } else {
            opc_illegal(ctx);
        }

        return;

    case 0x19: /* (Extended) Shifting instructions - 601 only*/
        strcpy(opcode, opc_shft_ext[index]);

        if (rc_set)
            strcat(opcode, ".");

        if ((index == 4) || (index == 6) || (index == 16) || (index == 20) || (index == 22) ||
            (index == 28)) {
            fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
        } else {
            opc_illegal(ctx);
        }

        return;

    case 0x1A: /* Byte sign extend instructions (and cntlzw) */

        if (index == 0)
            strcpy(opcode, "cntlzw");
        else if (index == 28)
            strcpy(opcode, "extsh");
        else if (index == 29)
            strcpy(opcode, "extsb");

        if (rc_set)
            strcat(opcode, ".");

        fmt_twoop(ctx->instr_str, opcode, rs, ra);

        return;

    case 0x1C: /* logical instructions */
        if (index == 13 && rs == rb && ctx->simplified) {
            fmt_twoop(ctx->instr_str, rc_set ? "mr." : "mr", ra, rs);
        } else {
            strcpy(opcode, opc_logic[index]);
            if (!strlen(opcode)) {
                opc_illegal(ctx);
            } else {
                if (rc_set)
                    strcat(opcode, ".");
                fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
            }
        }
        return;


    case 0x17:             /* indexed load/store instructions */
        if (index == 30) { /* stfiwx sneaks in here */
            if (rc_set) {
                opc_illegal(ctx);
                return;
            } else {
                if (ra == 0)
                    ctx->instr_str = my_sprintf("%-8sf%d, 0, r%d", opc_idx_ldst[index], rs, rb);
                else {
                    ctx->instr_str = my_sprintf("%-8sf%d, r%d, r%d", opc_idx_ldst[index], rs, ra, rb);
                }
                return;
            }
        }
        if (index > 23 || rc_set || strlen(opc_idx_ldst[index]) == 0) {
            opc_illegal(ctx);
            return;
        }
        if (index < 16) {
            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", opc_idx_ldst[index], rs, rb);
            else
                fmt_threeop(ctx->instr_str, opc_idx_ldst[index], rs, ra, rb);

            return;
        } else {
            ctx->instr_str = my_sprintf("%-8sf%d, r%d, r%d", opc_idx_ldst[index], rs, ra, rb);
        }
        return;

    case 0x16: /* processor mgmt + byte reversed load and store instructions */
        strcpy(opcode, proc_mgmt_str[index]);

        if (index == 4) { /* stwcx.*/
            if (!rc_set) {
                opc_illegal(ctx);
                return;
            } else {
                if (ra == 0)
                    ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", opcode, rs, rb);
                else
                    fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
                return;
            }
        }
        /* eciwx, ecowx, lhbrx, lwbrx, stwbrx, sthbrx */
        else if ((index == 9) || (index == 13) || (index == 16) || (index == 20) || (index == 24) || (index == 28)) {
            if (rc_set) {
                opc_illegal(ctx);
                return;
            } else {
                if (ra == 0)
                    ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", opcode, rs, rb);
                else
                    fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
                return;
            }
        } else if ((index == 18) || (index == 26)) { /* sync, eieio */
            ctx->instr_str = my_sprintf("%-8s", opcode);
            return;
        }
        /* dcba, dcbf, dcbi, dcbst, dcbt, dcbz, icbi */
        else if (
            (index == 1) || (index == 2) || (index == 7) || (index == 8) || (index == 14) ||
            (index == 23) || (index == 30) || (index == 31)) {
            if (rc_set || (rs != 0)) {
                opc_illegal(ctx);
                return;
            } else {
                if (ra == 0)
                    ctx->instr_str = my_sprintf("%-8s0, r%d", opcode, rb);
                else
                    fmt_twoop(ctx->instr_str, opcode, ra, rb);
                return;
            }
        } else if (index == 17) { /* tlbsync */
            ctx->instr_str = my_sprintf("%-8s", opcode);
            return;
        } else {
            opc_illegal(ctx);
        }

        return;
        break;
    }

    auto ref_spr  = (((ctx->instr_code >> 11) & 31) << 5) | ((ctx->instr_code >> 16) & 31);
    auto spr_high = (ctx->instr_code >> 11) & 31;
    auto spr_low  = (ctx->instr_code >> 16) & 31;

    switch (ext_opc) {
    case 0: /* cmp */
        if (rc_set) {
            opc_illegal(ctx);
            return;
        }


        if (ctx->simplified) {
            if (!(rs & 1)) {
                if ((rs >> 2) == 0) {
                    ctx->instr_str = my_sprintf("%-8sr%d, r%d", "cmpw", ra, rb);
                    return;
                } else {
                    ctx->instr_str = my_sprintf("%-8scr%d, r%d, r%d", "cmpw", (rs >> 2), ra, rb);
                    return;
                }
            }
        }

        ctx->instr_str = my_sprintf("%-8scr%d, r%d, r%d", "cmp", (rs >> 2), ra, rb);
        break;
    case 4: /* tw */
        if (rc_set) {
            opc_illegal(ctx);
        } else {
            if (ctx->simplified) {
                strcpy(opcode, trap_cond[rs]);

                if (strlen(opcode) != 0) {
                    ctx->instr_str = my_sprintf("%-8sr%d, r%d", opcode, ra, rb);
                    break;
                }
            }

            ctx->instr_str = my_sprintf("%-8s%d, r%d, r%d", "tw", rs, ra, rb);
        }
        break;
    case 19: /* mfcr */
        fmt_oneop(ctx->instr_str, "mfcr", rs);
        break;
    case 20: /* lwarx */
        if (rc_set) {
            opc_illegal(ctx);
        } else {
            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", "lwarx", rs, rb);
            else
                fmt_threeop(ctx->instr_str, "lwarx", rs, ra, rb);
        }
        break;
    case 29: /* maskg */
        strcpy(opcode, "maskg");

        if (rc_set)
            strcat(opcode, ".");

        fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        break;
    case 32: /* cmpl */
        if (rc_set) {
            opc_illegal(ctx);
            return;
        }

        if (ctx->simplified) {
            if (!(rs & 1)) {
                if ((rs >> 2) == 0)
                    ctx->instr_str = my_sprintf("%-8sr%d, r%d", "cmplw", ra, rb);
                else
                    ctx->instr_str = my_sprintf("%-8scr%d, r%d, r%d", "cmplw", (rs >> 2), ra, rb);

                return;
            }
        }

        else {
            ctx->instr_str = my_sprintf(
                "%-8scr%d, %d, r%d, r%d", "cmpl", (rs >> 2), (rs & 1), ra, rb);
        }
        break;
    case 83: /* mfmsr */
        ctx->instr_str = my_sprintf("%-8sr%d", "mfmsr", (ctx->instr_code >> 21) & 0x1F);
        break;
    case 144: /* mtcrf */
        if (ctx->instr_code & 0x100801)
            opc_illegal(ctx);
        else {
            if (((ctx->instr_code >> 12) & 0xFF) == 0xFF)
                ctx->instr_str = my_sprintf("%-8sr%d", "mtcr", rs);
            else
                ctx->instr_str = my_sprintf(
                    "%-8s0x%02X, r%d", "mtcrf", (ctx->instr_code >> 12) & 0xFF, rs);
        }
        break;
    case 277: /* lscbx */
        strcpy(opcode, "lscbx");

        if (rc_set)
            strcat(opcode, ".");

        fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        break;
    case 339: /* mfspr */
        if (ctx->simplified) {
            switch (spr_high) {
            case 0:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index0[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 8:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index8[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 16:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index16[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 29:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index29[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 30:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index30[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 31:
                strcpy(opcode, "mf");
                strcat(opcode, spr_index31[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            }
        }
        fmt_twoop_fromspr(ctx->instr_str, "mfspr", rs, ref_spr);
        break;
    case 371: /* mftb */
        if (ctx->simplified) {
            if (ref_spr == 268) {
                fmt_oneop(ctx->instr_str, "mftbl", rs);
            } else if (ref_spr == 269) {
                fmt_oneop(ctx->instr_str, "mftbu", rs);
            }
            return;
        }
        ctx->instr_str = my_sprintf("%-8sr%d, %d", "mftb", rs, ref_spr);
        break;
    case 467: /* mtspr */
        if (ctx->simplified) {
            switch (spr_high) {
            case 0:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index0[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 8:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index8[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 16:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index16[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 29:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index29[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 30:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index30[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            case 31:
                strcpy(opcode, "mt");
                strcat(opcode, spr_index31[spr_low]);
                ctx->instr_str = my_sprintf("%-8sr%d", opcode, rs);
                return;
            }
        }
        fmt_twoop_tospr(ctx->instr_str, "mtspr", ref_spr, rs);
        break;
    case 512: /* mcrxr */
        ctx->instr_str = my_sprintf("%-8scr%d", "mcrxr", (rs >> 2));
        break;
    case 531: /* clcs */
        strcpy(opcode, "clcs");

        if (rc_set)
            strcat(opcode, ".");

        fmt_twoop(ctx->instr_str, opcode, rs, ra);
        break;
    case 533: /* lswx */
        if (rc_set) {
            opc_illegal(ctx);
        } else {
            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", "lswx", rs, rb);
            else
                fmt_threeop(ctx->instr_str, "lswx", rs, ra, rb);
        }
        break;
    case 541: /* maskir */
        strcpy(opcode, "maskir");

        if (rc_set)
            strcat(opcode, ".");

        fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
        return;
    case 595: /* mfsr */
        if (ra & 16)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, %d", "mfsr", rs, ra);
        break;
    case 597: /* lswi */
        if (rc_set) {
            opc_illegal(ctx);
        } else {
            if (rb == 0)
                rb = 32;

            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, %x", "lswi", rs, rb);
            else
                fmt_threeop_simm(ctx->instr_str, "lswi", rs, ra, rb);
        }
        break;
    case 659: /* mfsrin */
        ctx->instr_str = my_sprintf("%-8sr%d, r%d", "mtsrin", rs, rb);
        break;
    case 661: /* stswx */
        if (rc_set) {
            opc_illegal(ctx);
            return;
        } else {
            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, r%d", "stswx", rs, rb);
            else
                fmt_threeop(ctx->instr_str, "stswx", rs, ra, rb);
            return;
        }
        break;
    case 725: /* stswi */
        if (rc_set) {
            opc_illegal(ctx);
            return;
        } else {
            if (rb == 0)
                rb = 32;

            if (ra == 0)
                ctx->instr_str = my_sprintf("%-8sr%d, 0, %d", "stswi", rs, rb);
            else
                fmt_threeop_simm(ctx->instr_str, "stswi", rs, ra, rb);
            return;
        }
        break;
    default:
        opc_illegal(ctx);
    }
}

void opc_group59(PPCDisasmContext* ctx) {
    char opcode[10] = "";

    auto rc = (ctx->instr_code >> 6) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    bool rc_set = ctx->instr_code & 1;

    switch (ext_opc & 0x1F) {
    case 18: /* floating point division */
        strcpy(opcode, "fdivs");

        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 20: /* floating point subtract */
        strcpy(opcode, "fsubs");

        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 21: /* floating point addition */
        strcpy(opcode, "fadds");

        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 22: /* floating point square root */
        strcpy(opcode, "fsqrts");

        if (rc_set)
            strcat(opcode, ".");

        if ((rc != 0) || (ra != 0))
            opc_illegal(ctx);
        else
            fmt_twoop_flt(ctx->instr_str, "fsqrts", rs, rb);

        return;

    case 24: /* fres */
        strcpy(opcode, "fres");

        if (rc_set)
            strcat(opcode, ".");

        if ((rc != 0) || (ra != 0))
            opc_illegal(ctx);
        else
            fmt_twoop_flt(ctx->instr_str, opcode, rs, rb);

        return;

    case 25: /* fmuls */

        strcpy(opcode, opc_flt_ext_arith[25]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        if (rb != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rc);

        return;

    case 28: /* fmsubs */

        strcpy(opcode, opc_flt_ext_arith[28]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 29: /* fmadds */

        strcpy(opcode, opc_flt_ext_arith[29]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 30: /* fnmsubs */

        strcpy(opcode, opc_flt_ext_arith[30]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 31: /* fnmadds */

        strcpy(opcode, opc_flt_ext_arith[31]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;
    }
}

void opc_group63(PPCDisasmContext* ctx) {
    char opcode[10] = "";

    auto rc = (ctx->instr_code >> 6) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    bool rc_set = ctx->instr_code & 1;

    switch (ext_opc & 0x1F) {
    case 18: /* floating point division */
        strcpy(opcode, "fdiv");

        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 20: /* floating point subtract */
        strcpy(opcode, "fsub");
        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 21: /* floating point addition */
        strcpy(opcode, "fadd");

        if (rc_set)
            strcat(opcode, ".");

        if (rc != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 22: /* floating point square root */
        strcpy(opcode, "fsqrt");

        if (rc_set)
            strcat(opcode, ".");

        if ((rc != 0) || (ra != 0))
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 23: /* fsel */
        strcpy(opcode, opc_flt_ext_arith[23]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 25: /* fmul */

        strcpy(opcode, opc_flt_ext_arith[25]);

        if (rc_set)
            strcat(opcode, ".");

        if (rb != 0)
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rc);

        return;

    case 26: /* frsqrte */
        strcpy(opcode, "frsqrte");

        if (rc_set)
            strcat(opcode, ".");

        if ((rc != 0) || (ra != 0))
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);

        return;

    case 28: /* fmsub */
        strcpy(opcode, opc_flt_ext_arith[28]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 29: /* fmadd */
        strcpy(opcode, opc_flt_ext_arith[29]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 30: /* fnmsub */
        strcpy(opcode, opc_flt_ext_arith[30]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;

    case 31: /* fnmadd */
        strcpy(opcode, opc_flt_ext_arith[31]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rc, rb);

        return;
    }

    auto fm = (ctx->instr_code >> 17) & 0xFF;

    switch (ext_opc) {
    case 0: /* fcmpu */
        if (rs & 3)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8scr%d, f%d, f%d", "fcmpu", (rs >> 2), ra, rb);
        break;
    case 12: /* frsp */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "frsp");
            if (rc_set)
                strcat(opcode, ".");
            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 14: /* fctiw */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "fctiw");
            if (rc_set)
                strcat(opcode, ".");
            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 15: /* fctiwz */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "fctiwz");
            if (rc_set)
                strcat(opcode, ".");
            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 32: /* fcmpo */
        if (rs & 3)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8scr%d, f%d, f%d", "fcmpo", (rs >> 2), ra, rb);
        break;
    case 38: /* mtfsb1 */
        strcpy(opcode, "mtfsb1");

        if (rc_set)
            strcat(opcode, ".");

        ctx->instr_str = my_sprintf("%-8s%d", opcode, rs);
        break;
    case 40: /* fneg */
        strcpy(opcode, "fneg");

        if (rc_set)
            strcat(opcode, ".");

        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        break;
    case 64:
        strcpy(opcode, "mcrfs");

        ctx->instr_str = my_sprintf("%-8scr%d, cr%d", opcode, (rs >> 2), (ra >> 2));
        break;
    case 70: /* mtfsb0 */
        strcpy(opcode, "mtfsb0");

        if (rc_set)
            strcat(opcode, ".");

        ctx->instr_str = my_sprintf("%-8s%d", opcode, rs);
        break;
    case 72: /* fmr */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "fmr");

            if (rc_set)
                strcat(opcode, ".");

            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 134: /* mtfsfi */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "mtfsfi");
            if (rc_set)
                strcat(opcode, ".");

            ctx->instr_str = my_sprintf("%-8scr%d, %d", opcode, (rs >> 2), (rb >> 1));
        }
        break;
    case 136: /* fnabs */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "fnabs");
            if (rc_set)
                strcat(opcode, ".");

            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 264: /* fabs */
        if (ra != 0)
            opc_illegal(ctx);
        else {
            strcpy(opcode, "fabs");
            if (rc_set)
                strcat(opcode, ".");

            ctx->instr_str = my_sprintf("%-8sf%d, f%d", opcode, rs, rb);
        }
        break;
    case 583: /* mffs */
        strcpy(opcode, "mffs");

        if (rc_set)
            strcat(opcode, ".");

        if ((ra != 0) || (rb != 0))
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sf%d", opcode, rs);
        break;
    case 711: /* mtfsf */
        strcpy(opcode, "mtfsf");

        if (rc_set)
            strcat(opcode, ".");

        ctx->instr_str = my_sprintf("%-8s%d, f%d", opcode, fm, rb);
        break;
    default:
        opc_illegal(ctx);
    }
}

void opc_intldst(PPCDisasmContext* ctx) {
    int32_t opcode = (ctx->instr_code >> 26) - 32;
    int32_t ra     = (ctx->instr_code >> 16) & 0x1F;
    int32_t rd     = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm    = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    /* ra = 0 is forbidden for loads and stores with update */
    /* ra = rd is forbidden for loads with update */
    if (((opcode < 14) && (opcode & 5) == 1 && ra == rd) || ((opcode & 1) && !ra)) {
        opc_illegal(ctx);
        return;
    }

    if (ra) {
        ctx->instr_str = my_sprintf(
            "%-8sr%d, %s0x%X(r%d)", opc_int_ldst[opcode], rd, ((imm < 0) ? "-" : ""), abs(imm), ra);
    } else {
        ctx->instr_str = my_sprintf(
            "%-8sr%d, %s0x%X", opc_int_ldst[opcode], rd, ((imm < 0) ? "-" : ""), abs(imm));
    }
}

void opc_fltldst(PPCDisasmContext* ctx) {
    int32_t opcode = (ctx->instr_code >> 26) - 48;
    int32_t ra     = (ctx->instr_code >> 16) & 0x1F;
    int32_t rd     = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm    = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    /* ra = 0 is forbidden for loads and stores with update */
    /* ra = rd is forbidden for loads with update */
    if ((((opcode == 1) || (opcode == 3)) && ra == rd) || ((opcode & 1) && !ra)) {
        opc_illegal(ctx);
        return;
    }

    if (ra) {
        ctx->instr_str = my_sprintf(
            "%-8sf%d, %s0x%X(r%d)", opc_flt_ldst[opcode], rd, ((imm < 0) ? "-" : ""), abs(imm), ra);
    } else {
        ctx->instr_str = my_sprintf(
            "%-8sf%d, %s0x%X", opc_flt_ldst[opcode], rd, ((imm < 0) ? "-" : ""), abs(imm));
    }
}

/** main dispatch table. */
static std::function<void(PPCDisasmContext*)> OpcodeDispatchTable[64] = {
    opc_illegal, opc_illegal, opc_illegal, opc_twi,      opc_group4,   opc_illegal, opc_illegal,
    opc_ar_im,   opc_ar_im,   power_dozi,  opc_cmp_i_li, opc_cmp_i_li, opc_ar_im,   opc_ar_im,
    opc_ar_im,   opc_ar_im,   opc_bcx,     opc_sc,       opc_bx,       opc_group19, opc_rlwimi,
    opc_rlwinm,  opc_rlmi,    opc_rlwnm,   opc_bool_im,  opc_bool_im,  opc_bool_im, opc_bool_im,
    opc_bool_im, opc_bool_im, opc_illegal, opc_group31,  opc_intldst,  opc_intldst, opc_intldst,
    opc_intldst, opc_intldst, opc_intldst, opc_intldst,  opc_intldst,  opc_intldst, opc_intldst,
    opc_intldst, opc_intldst, opc_intldst, opc_intldst,  opc_intldst,  opc_intldst, opc_fltldst,
    opc_fltldst, opc_fltldst, opc_fltldst, opc_fltldst,  opc_fltldst,  opc_fltldst, opc_fltldst,
    opc_illegal, opc_illegal, opc_illegal, opc_group59,  opc_illegal,  opc_illegal, opc_illegal,
    opc_group63};

string disassemble_single(PPCDisasmContext* ctx) {
    if (ctx->instr_addr & 3) {
        throw std::invalid_argument(string("PPC instruction address must be a multiply of 4!"));
    }

    OpcodeDispatchTable[ctx->instr_code >> 26](ctx);

    ctx->instr_addr += 4;

    return ctx->instr_str;
}
