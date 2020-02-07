/** @file   Logic for the disassembler.
    @author maximumspatium
 */

#define _CRT_SECURE_NO_WARNINGS /* shut up MSVC regarding the unsafe strcpy/strcat */

#include <iostream>
#include <string>
#include <cstring>
#include <functional> /* without this, MSVC doesn't understand std::function */
#include "ppcdisasm.h"

using namespace std;

template< typename... Args >
std::string my_sprintf(const char* format, Args... args)
{
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0)
        return {}; /* empty string in C++11 */

    char* buf = new char[length + 1];
    std::snprintf(buf, length + 1, format, args...);

    std::string str(buf);
    delete[] buf;
    return str;
}

const char* arith_im_mnem[9] = {
    "mulli", "subfic", "", "", "", "addic", "addic.", "addi", "addis"
};

const char* bx_mnem[4] = {
    "b", "bl", "ba", "bla"
};

const char* bclrx_mnem[2] = {
    "bclr", "bclrl"
};

const char* bcctrx_mnem[2] = {
    "bcctr", "bcctrl"
};

const char* br_cond[8] = { /* simplified branch conditions */
    "ge", "le", "ne", "ns", "lt", "gt", "eq", "so"
};

const char* bclrx_cond[8] = { /* simplified branch conditions */
    "gelr", "lelr", "nelr", "nslr", "ltlr", "gtlr", "eqlr", "solr"
};

const char* bcctrx_cond[8] = { /* simplified branch conditions */
    "gectr", "lectr", "nectr", "nsctr", "ltctr", "gtctr", "eqctr", "soctr"
};

const char* opc_idx_ldst[24] = { /* indexed load/store opcodes */
    "lwzx", "lwzux", "lbzx", "lbzux", "stwx", "stwux", "stbx", "stbux", "lhzx",
    "lhzux", "lhax", "lhaux", "sthx", "sthux", "", "", "lfsx", "lfsux", "lfdx",
    "lfdux", "stfsx", "stfsux", "stfdx", "stfdux"
};

const char* opc_logic[16] = { /* indexed load/store opcodes */
    "and", "andc", "", "nor", "", "", "", "", "eqv", "xor", "", "", "orc", "or",
    "nand", ""
};

const char* opc_subs[16] = { /* subtracts & friends */
    "subfc", "subf", "", "neg", "subfe", "", "subfze", "subfme", "doz", "", "",
    "abs", "", "", "", "nabs"
};

const char* opc_adds[9] = { /* additions */
    "addc", "", "", "", "adde", "", "addze", "addme", "add"
};

const char* opc_muldivs[16] = { /* multiply and division instructions */
    "mulhwu", "", "mulhw", "mul", "", "", "", "mullw", "", "", "div", "divs",
    "", "", "divwu", "divw"
};

const char* opc_int_ldst[16] = { /* integer load and store instructions */
    "lwz", "lwzu", "lbz", "lbzu", "stw", "stwu", "stb", "stbu", "lhz", "lhzu",
    "lha", "lhau", "sth", "sthu", "lmw", "stmw"
};

const char* proc_mgmt_str[32] = { /* processor managment + byte reversed load and store*/
    "", "dcbst", "dcbf", "", "stwcx.", "", "", "dcbtst",
    "dcbt", "eciwx", "", "", "", "ecowx", "dcbi", "",
    "lwbrx", "tlbsync", "sync", "", "stwbrx", "", "", "dcba",
    "lhbrx", "", "eieio", "", "sthbrx", "", "icbi", "dcbz"
};

const char* opc_flt_ldst[8] = { /* integer load and store instructions */
    "lfs", "lfsu", "lfd", "lfdu", "stfs", "stfsu", "sftd", "sftdu"
};

const char* opc_flt_ext_arith[32] = { /* integer load and store instructions */
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "fsel",
    "", "fmul", "", "", "fmsub", "fmadd", "fnmsub", "fnmadd",
};

/** various formatting helpers. */
void fmt_oneop(string& buf, const char* opc, int src)
{
    buf = my_sprintf("%-8sr%d", opc, src);
}

void fmt_twoop(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sr%d, r%d", opc, dst, src);
}

void fmt_twoop_simm(string& buf, const char* opc, int dst, int imm)
{
    buf = my_sprintf("%-8sr%d, %s0x%X", opc, dst, (imm < 0) ? "-" : "", abs(imm));
}

void fmt_twoop_fromspr(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sr%d, spr%d", opc, dst, src);
}

void fmt_twoop_tospr(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sspr%d, r%d", opc, dst, src);
}

void fmt_twoop_flt(string& buf, const char* opc, int dst, int src1)
{
    buf = my_sprintf("%-8sfr%d, fr%d", opc, dst, src1);
}

void fmt_threeop(string& buf, const char* opc, int dst, int src1, int src2)
{
    buf = my_sprintf("%-8sr%d, r%d, r%d", opc, dst, src1, src2);
}

void fmt_threeop_crb(string& buf, const char* opc, int dst, int src1, int src2)
{
    buf = my_sprintf("%-8scrb%d, crb%d, crb%d", opc, dst, src1, src2);
}

void fmt_threeop_uimm(string& buf, const char* opc, int dst, int src1, int imm)
{
    buf = my_sprintf("%-8sr%d, r%d, 0x%04X", opc, dst, src1, imm);
}

void fmt_threeop_simm(string& buf, const char* opc, int dst, int src1, int imm)
{
    buf = my_sprintf("%-8sr%d, r%d, %s0x%X", opc, dst, src1,
        (imm < 0) ? "-" : "", abs(imm));
}

void fmt_threeop_flt(string& buf, const char* opc, int dst, int src1, int src2)
{
    buf = my_sprintf("%-8sfr%d, fr%d, fr%d", opc, dst, src1, src2);
}

void fmt_fourop_flt(string& buf, const char* opc, int dst, int src1, int src2, int src3)
{
    buf = my_sprintf("%-8sfr%d, fr%d, fr%d, fr%d", opc, dst, src1, src2, src3);
}

void fmt_rotateop(string& buf, const char* opc, int dst, int src1, int sh, int mb, int me, bool imm)
{
    if (imm)
        buf = my_sprintf("%-8sr%d, r%d, sh%d, mb%d, me%d", opc, dst, src1, sh, mb, me);
    else
        buf = my_sprintf("%-8sr%d, r%d, r%d, mb%d, me%d", opc, dst, src1, sh, mb, me);
}


void opc_illegal(PPCDisasmContext* ctx)
{
    ctx->instr_str = my_sprintf("%-8s0x%08X", "dc.l", ctx->instr_code);
}

void opc_twi(PPCDisasmContext* ctx)
{
    auto to = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    if (ctx->simplified) {
        switch (to) {
        case 1:
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%08X", "twlgti", ra, imm);
            break;
        case 6:
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%08X", "twllei", ra, imm);
            break;
        case 8:
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%08X", "twgti", ra, imm);
            break;
        case 16:
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%08X", "twlti", ra, imm);
            break;
        case 20:
            ctx->instr_str = my_sprintf("%-8sr%d, 0x%08X", "twlei", ra, imm);
            break;
        }
    }
    else {
        ctx->instr_str = my_sprintf("%-8s%d, r%d, 0x%08X", "twi", to, ra, imm);
    }
}

void opc_group4(PPCDisasmContext* ctx)
{
    printf("Altivec group 4 not supported yet\n");
}

void opc_ar_im(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    if (ctx->simplified) {
        if (((ctx->instr_code >> 26) == 0xE) & !ra) {
            fmt_twoop_simm(ctx->instr_str, "li", rd, imm);
            return;
        }
        else if (((ctx->instr_code >> 26) == 0xF) & !ra) {
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

    fmt_threeop_simm(ctx->instr_str, arith_im_mnem[(ctx->instr_code >> 26) - 7],
        rd, ra, imm);
}

void power_dozi(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_simm(ctx->instr_str, "dozi", rd, ra, imm);
}

void opc_rlwimi(PPCDisasmContext* ctx)
{
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto sh = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->instr_code & 1)
        fmt_rotateop(ctx->instr_str, "rlwimi.", rs, ra, sh, mb, me, true);
    else
        fmt_rotateop(ctx->instr_str, "rlwimi", rs, ra, sh, mb, me, true);
}

void opc_rlwinm(PPCDisasmContext* ctx)
{
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto sh = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->instr_code & 1)
        fmt_rotateop(ctx->instr_str, "rlwinm.", rs, ra, sh, mb, me, true);
    else
        fmt_rotateop(ctx->instr_str, "rlwinm", rs, ra, sh, mb, me, true);
}

void opc_rlwnm(PPCDisasmContext* ctx)
{
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto mb = (ctx->instr_code >> 6) & 0x1F;
    auto me = (ctx->instr_code >> 1) & 0x1F;

    if (ctx->simplified) {
        if ((me == 31) & (mb == 0)) {
            if (ctx->instr_code & 1) {
                fmt_rotateop(ctx->instr_str, "rotlw.", rs, ra, rb, mb, me, false);
                return;
            }
            else {
                fmt_rotateop(ctx->instr_str, "rotlw", rs, ra, rb, mb, me, false);
                return;
            }
        }
    }
    if (ctx->instr_code & 1)
        fmt_rotateop(ctx->instr_str, "rlwnm.", rs, ra, rb, mb, me, false);
    else
        fmt_rotateop(ctx->instr_str, "rlwnm", rs, ra, rb, mb, me, false);
}

void opc_cmp_i_li(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto crfd = (ctx->instr_code >> 23) & 0x07;
    auto imm = ctx->instr_code & 0xFFFF;

    if (ctx->instr_code & 0x200000) {
        opc_illegal(ctx);
    }
    else {
        if ((ctx->instr_code >> 26) & 0x1)
            fmt_threeop_uimm(ctx->instr_str, "cmpi", crfd, ra, imm);
        else
            fmt_threeop_simm(ctx->instr_str, "cmpli", crfd, ra, imm);
    }
}

void generic_bcx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi, uint32_t dst)
{
    char opcode[10] = "bc";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (ctx->instr_code & 2) {
        strcat(opcode, "a"); /* add suffix "a" if the AA bit is set */
    }
    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi, dst);
}

void generic_bcctrx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi)
{
    char opcode[10] = "bcctr";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }

    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi);
}

void generic_bclrx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi)
{
    char opcode[10] = "bclr";

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }

    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi);
}

void opc_bcx(PPCDisasmContext* ctx)
{
    uint32_t bo, bi, dst, cr;
    char opcode[10] = "b";
    char operands[10] = "";

    bo = (ctx->instr_code >> 21) & 0x1F;
    bi = (ctx->instr_code >> 16) & 0x1F;
    cr = bi >> 2;
    dst = ((ctx->instr_code & 2) ? 0 : ctx->instr_addr) +
        SIGNEXT(ctx->instr_code & 0xFFFC, 15);

    if (!ctx->simplified || ((bo & 0x10) && bi) ||
        (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
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
    }
    else { /* CTR ignored */
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

void opc_bcctrx(PPCDisasmContext* ctx)
{
    uint32_t bo, bi, cr;
    char opcode[10] = "b";
    char operands[10] = "";

    bo = (ctx->instr_code >> 21) & 0x1F;
    bi = (ctx->instr_code >> 16) & 0x1F;
    cr = bi >> 2;

    if (!ctx->simplified || ((bo & 0x10) && bi) ||
        (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
        generic_bcctrx(ctx, bo, bi);
        return;
    }

    if ((bo & 0x14) == 0x14) {
        ctx->instr_str = my_sprintf("%-8s0x%08X", bcctrx_mnem[0]);
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
    }
    else { /* CTR ignored */
        strcat(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
        if (cr) {
            strcat(operands, "cr0, ");
            operands[2] = cr + '0';
        }
    }

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat(opcode, (ctx->instr_code & 0x8000) ? "-" : "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s0x%08X", opcode, operands);
}

void opc_bclrx(PPCDisasmContext* ctx)
{
    uint32_t bo, bi, cr;
    char opcode[10] = "b";
    char operands[10] = "";

    bo = (ctx->instr_code >> 21) & 0x1F;
    bi = (ctx->instr_code >> 16) & 0x1F;
    cr = bi >> 2;

    if (!ctx->simplified || ((bo & 0x10) && bi) ||
        (((bo & 0x14) == 0x14) && (bo & 0xB) && bi)) {
        generic_bclrx(ctx, bo, bi);
        return;
    }

    if ((bo & 0x14) == 0x14) {
        ctx->instr_str = my_sprintf("%-8s0x%08X", bclrx_mnem[0]);
        return;
    }

    if (!(bo & 4)) {
        strcat(opcode, "d");
        strcat(opcode, (bo & 2) ? "z" : "nz");
        if (!(bo & 0x10)) {
            strcat(opcode, (bo & 8) ? "tlr" : "flr");
            if (cr) {
                strcat(operands, "4*cr0+");
                operands[4] = cr + '0';
            }
            strcat(operands, br_cond[4 + (bi & 3)]);
            strcat(operands, ", ");
        }
    }
    else { /* CTR ignored */
        strcat(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
        if (cr) {
            strcat(operands, "cr0, ");
            operands[2] = cr + '0';
        }
    }

    if (ctx->instr_code & 1) {
        strcat(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat(opcode, (ctx->instr_code & 0x8000) ? "-" : "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s0x%08X", opcode, operands);
}

void opc_bx(PPCDisasmContext* ctx)
{
    uint32_t dst = ((ctx->instr_code & 2) ? 0 : ctx->instr_addr)
        + SIGNEXT(ctx->instr_code & 0x3FFFFFC, 25);

    ctx->instr_str = my_sprintf("%-8s0x%08X", bx_mnem[ctx->instr_code & 3], dst);
}

void opc_ori(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    if (!ra && !rs && !imm && ctx->simplified) {
        ctx->instr_str = "nop";
        return;
    }
    if (imm == 0 && ctx->simplified) { /* inofficial, produced by IDA */
        fmt_twoop(ctx->instr_str, "mr", ra, rs);
        return;
    }
    fmt_threeop_uimm(ctx->instr_str, "ori", ra, rs, imm);
}

void opc_oris(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_uimm(ctx->instr_str, "oris", ra, rs, imm);
}

void opc_xori(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_uimm(ctx->instr_str, "xori", ra, rs, imm);
}

void opc_xoris(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_uimm(ctx->instr_str, "xoris", ra, rs, imm);
}

void opc_andidot(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_uimm(ctx->instr_str, "andi.", ra, rs, imm);
}

void opc_andisdot(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_uimm(ctx->instr_str, "andis.", ra, rs, imm);
}

void opc_sc(PPCDisasmContext* ctx)
{
    ctx->instr_str = my_sprintf("%-8s", "sc");
}

void opc_group19(PPCDisasmContext* ctx)
{
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */

    switch (ext_opc) {
    case 0:
        ctx->instr_str = my_sprintf("%-8scrf%d, crf%d", "mcrf", (rs >> 2), (ra >> 2));
        return;
    case 16:
        opc_bclrx(ctx);
        return;
    case 33:
        fmt_threeop_crb(ctx->instr_str, "crnor", rs, ra, rb);
        return;
    case 50:
        ctx->instr_str = my_sprintf("%-8sr%d", "rfi");
        return;
    case 129:
        fmt_threeop_crb(ctx->instr_str, "crandc", rs, ra, rb);
        return;
    case 150:
        ctx->instr_str = my_sprintf("%-8sr%d", "isync");
        return;
    case 193:
        if (ctx->simplified && (rs == ra) && (rs == rb)) {
            ctx->instr_str = my_sprintf("%-8scrb%d", "crclr", rs);
        }
        else {
            fmt_threeop_crb(ctx->instr_str, "crxor", rs, ra, rb);
        }
        return;
    case 225:
        fmt_threeop_crb(ctx->instr_str, "crnand", rs, ra, rb);
        return;
    case 257:
        fmt_threeop_crb(ctx->instr_str, "crand", rs, ra, rb);
        return;
    case 289:
        fmt_threeop_crb(ctx->instr_str, "creqv", rs, ra, rb);
        return;
    case 417:
        fmt_threeop_crb(ctx->instr_str, "crorc", rs, ra, rb);
        return;
    case 449:
        fmt_threeop_crb(ctx->instr_str, "cror", rs, ra, rb);
        return;
    case 528:
        opc_bcctrx(ctx);
        return;
    }
}


void opc_group31(PPCDisasmContext* ctx)
{
    char opcode[10] = "";

    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int  ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    int  index = ext_opc >> 5;
    bool rc_set = ctx->instr_code & 1;

    switch (ext_opc & 0x1F) {
    case 8: /* subtracts & friends */
        index &= 0xF; /* strip OE bit */
        if (!strlen(opc_subs[index])) {
            opc_illegal(ctx);
        }
        else {
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
            }
            else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 10: /* additions */
        index &= 0xF; /* strip OE bit */
        if (index > 8 || !strlen(opc_adds[index])) {
            opc_illegal(ctx);
        }
        else {
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
            }
            else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 11: /* integer multiplications and divisions */
        index &= 0xF; /* strip OE bit */
        if (!strlen(opc_muldivs[index])) {
            opc_illegal(ctx);
        }
        else {
            strcpy(opcode, opc_muldivs[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat(opcode, "o");
            if (rc_set)
                strcat(opcode, ".");
            if ((!index || index == 2) && (ext_opc & 0x200))
                opc_illegal(ctx);
            else
                fmt_threeop_simm(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 0x12: /* tlb instructions */

        if (index == 11) {
            ctx->instr_str = my_sprintf("%-8s", "tlbia");
        }
        else if (index == 18) {
            ctx->instr_str = my_sprintf("%-8sr%s", "tlbie", rb);
        }
        return;

    case 0x18: /* Shifting instructions */
        if (index == 0)
            fmt_threeop(ctx->instr_str, "slw", rs, ra, rb);
        else if (index == 16)
            fmt_threeop(ctx->instr_str, "srw", rs, ra, rb);
        else if (index == 24)
            fmt_threeop(ctx->instr_str, "sraw", rs, ra, rb);
        else if (index == 25)
            fmt_threeop_uimm(ctx->instr_str, "srawi", rs, ra, rb);
        return;

    case 0x1A: /* Byte sign extend instructions */
        if (index == 28)
            fmt_twoop(ctx->instr_str, "extsh", rs, ra);
        else if (index == 29)
            fmt_twoop(ctx->instr_str, "extsb", rs, ra);
        return;

    case 0x1C: /* logical instructions */
        if (index == 13 && rs == rb && ctx->simplified) {
            fmt_twoop(ctx->instr_str, rc_set ? "mr." : "mr", ra, rs);
        }
        else {
            strcpy(opcode, opc_logic[index]);
            if (!strlen(opcode)) {
                opc_illegal(ctx);
            }
            else {
                if (rc_set)
                    strcat(opcode, ".");
                fmt_threeop(ctx->instr_str, opcode, ra, rs, rb);
            }
        }
        return;


    case 0x17: /* indexed load/store instructions */
        if (index > 23 || rc_set || strlen(opc_idx_ldst[index]) == 0) {
            opc_illegal(ctx);
            return;
        }
        if (index < 16)
            fmt_threeop(ctx->instr_str, opc_idx_ldst[index], rs, ra, rb);
        else
            ctx->instr_str = my_sprintf("%-8sfp%d, r%d, r%d",
                opc_idx_ldst[index], rs, ra, rb);
        return;

    case 0x16: /* processor mgmt + byte reversed load and store instructions */
        strcpy(opcode, proc_mgmt_str[index]);

        if ((index == 4) | (index == 9) | (index == 13) | (index == 16) \
            | (index == 20) | (index == 24) | (index == 28)) {
            fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
            return;
            break;
        }
        else if ((index == 18)) {
            ctx->instr_str = my_sprintf("%-8s", opcode);
            return;
        }
        else {
            fmt_twoop(ctx->instr_str, opcode, ra, rb);
        }

        return;
        break;

    }

    auto ref_spr = (((ctx->instr_code >> 11) & 31) << 5) | ((ctx->instr_code >> 16) & 31);

    switch (ext_opc) {
    case 0: /* cmp */
        if (rc_set)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, r%d, r%d", "cmp", rs, ra, rb);
        break;
    case 4:
        if (rc_set)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, r%d, r%d", "tw", rs, ra, rb);
        break;
    case 19: /* mfcr */
        fmt_oneop(ctx->instr_str, "mfcr", rs);
        break;
    case 26: /* cntlzw */
        strcpy(opcode, "cntlzw");

        if (rc_set)
            strcat(opcode, ".");

        fmt_twoop(ctx->instr_str, opcode, rs, ra);
        break;
    case 32: /* cmpl */
        if (rc_set)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, r%d, r%d", "cmpl", rs, ra, rb);
        break;
    case 83: /* mfmsr */
        ctx->instr_str = my_sprintf("%-8sr%d", "mfmsr",
            (ctx->instr_code >> 21) & 0x1F);
        break;
    case 144: /* mtcrf */
        if (ctx->instr_code & 0x100801)
            opc_illegal(ctx);
        else {
            ctx->instr_str = my_sprintf("%-8s0x%02X, r%d", "mtcrf",
                (ctx->instr_code >> 12) & 0xFF, rs);
        }
        break;
    case 146: /* mtmsr */
        fmt_oneop(ctx->instr_str, "mtmsr", rs);
        break;
    case 339: /* mfspr */
        if (ctx->simplified) {
            switch (ref_spr) {
            case 1:
                ctx->instr_str = my_sprintf("%-8sr%d", "mfxer", rs);
                return;
            case 8:
                ctx->instr_str = my_sprintf("%-8sr%d", "mflr", rs);
                return;
            case 9:
                ctx->instr_str = my_sprintf("%-8sr%d", "mfctr", rs);
                return;
            }
        }
        fmt_twoop_fromspr(ctx->instr_str, "mfspr", rs, ref_spr);
        break;
    case 371: /* mftb */
        fmt_twoop_tospr(ctx->instr_str, "mftb", ref_spr, rs);
        break;
    case 467: /* mtspr */
        if (ctx->simplified) {
            switch (ref_spr) {
            case 1:
                ctx->instr_str = my_sprintf("%-8sr%d", "mtxer", rs);
                return;
            case 8:
                ctx->instr_str = my_sprintf("%-8sr%d", "mtlr", rs);
                return;
            case 9:
                ctx->instr_str = my_sprintf("%-8sr%d", "mtctr", rs);
                return;
            }
        }
        fmt_twoop_tospr(ctx->instr_str, "mtspr", ref_spr, rs);
        break;
    case 512: /* mcrxr */
        ctx->instr_str = my_sprintf("%-8scrf%d", "mcrxr", (rs >> 2));
        break;
    case 533: /* lswx */
        fmt_threeop_simm(ctx->instr_str, "lswx", rs, ra, rb);
        break;
    case 597: /* lswi */
        fmt_threeop_simm(ctx->instr_str, "lswi", rs, ra, rb);
        break;
    default:
        opc_illegal(ctx);
    }
}

void opc_group59(PPCDisasmContext* ctx)
{
    char opcode[10] = "";

    auto rc = (ctx->instr_code >> 6) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int  ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    int  index = ext_opc >> 5;
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

        if ((rc != 0) | (ra != 0))
            opc_illegal(ctx);
        else
            fmt_twoop_flt(ctx->instr_str, "fsqrts", rs, rb);

        return;

    case 24: /* fres */
        strcpy(opcode, "fres");

        if (rc_set)
            strcat(opcode, ".");

        if ((rc != 0) | (ra != 0))
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

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 29: /* fmadds */

        strcpy(opcode, opc_flt_ext_arith[29]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 30: /* fnmsubs */

        strcpy(opcode, opc_flt_ext_arith[30]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 31: /* fnmadds */

        strcpy(opcode, opc_flt_ext_arith[31]);
        strcat(opcode, "s");

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;
    }
}

void opc_group63(PPCDisasmContext* ctx)
{
    char opcode[10] = "";

    auto rc = (ctx->instr_code >> 6) & 0x1F;
    auto rb = (ctx->instr_code >> 11) & 0x1F;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;

    int  ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */
    int  index = ext_opc >> 5;
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

        if ((rc != 0) | (ra != 0))
            opc_illegal(ctx);
        else
            fmt_threeop_flt(ctx->instr_str, opcode, rs, ra, rb);

        return;

    case 23: /* fsel */
        strcpy(opcode, "fsel");

        if (rc_set)
            strcat_s(opcode, ".");

        if ((rc != 0) | (ra != 0))
            opc_illegal(ctx);
        else
            fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

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

        if ((rc != 0) | (ra != 0))
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, fr%d, fr%d", opcode, rs, rb);

        return;

    case 28: /* fmsub */
        strcpy(opcode, opc_flt_ext_arith[28]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 29: /* fmadd */
        strcpy(opcode, opc_flt_ext_arith[29]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 30: /* fnmsub */
        strcpy(opcode, opc_flt_ext_arith[30]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;

    case 31: /* fnmadd */
        strcpy(opcode, opc_flt_ext_arith[31]);

        if (rc_set)
            strcat(opcode, ".");

        fmt_fourop_flt(ctx->instr_str, opcode, rs, ra, rb, rc);

        return;
    }

    auto fm = (ctx->instr_code >> 17) & 0xFF;

    switch (ext_opc) {
    case 0: /* fcmpu */
        if (rs & 3)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8scrf%d, r%d, r%d,", "fcmpu", (rs >> 2), ra, rb);
        break;
    case 12: /* frsp */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, r%d, r%d", "frsp", rs, rb);
        break;
    case 14: /* fctiw */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, r%d, r%d", "fctiw", rs, rb);
        break;
    case 15: /* fctiwz */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, r%d, r%d", "fctiwz", rs, rb);
        break;
    case 32: /* fcmpo */
        if (rs & 3)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8scrf%d, r%d, r%d,", "fcmpo", (rs >> 2), ra, rb);
        break;
    case 38: /* mtfsb1 */
        strcpy(opcode, "mtfsb1");

        if (rc_set)
            strcat(opcode, ".");

        ctx->instr_str = my_sprintf("%-8scrb%d", opcode, rs);
        break;
    case 40: /* fneg */
        strcpy(opcode, "fneg");

        if (rc_set)
            strcat(opcode, ".");

        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, r%d", opcode, rs, rb);
        break;
    case 64:
        strcpy(opcode, "mcrfs");

        ctx->instr_str = my_sprintf("%-8scrf%d, crf%d", opcode, (rs >> 2), (ra >> 2));
        break;
    case 70: /* mtfsb0 */
        strcpy(opcode, "mtfsb0");

        if (rc_set)
            strcat(opcode, ".");

        ctx->instr_str = my_sprintf("%-8scrb%d", opcode, rs);
        break;
    case 72: /* fmr */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sr%d, r%d", "fmr", rs, rb);
        break;
    case 134: /* mtfsfi */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8scrf%d, r%d", "mtfsfi", (rs >> 2), (rb >> 1));
        break;
    case 136: /* fnabs */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, fr%d, fr%d", "fnabs", rs, rb);
        break;
    case 264: /* fabs */
        if (ra != 0)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, fr%d, fr%d", "fabs", rs, rb);
        break;
    case 583: /* mffs */
        strcpy(opcode, "mffs");

        if (rc_set)
            strcat(opcode, ".");

        if ((ra != 0) | (rb != 0))
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8sfr%d", opcode, rs);
        break;
    case 711: /* mtfsf */
        ctx->instr_str = my_sprintf("%-8sfm%d, r%d", "mtfsf", fm, rb);
        break;
    default:
        opc_illegal(ctx);
    }
}

void opc_intldst(PPCDisasmContext* ctx)
{
    int32_t opcode = (ctx->instr_code >> 26) - 32;
    int32_t ra = (ctx->instr_code >> 16) & 0x1F;
    int32_t rd = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    /* ra = 0 is forbidden for loads and stores with update */
    /* ra = rd is forbidden for loads with update */
    if (((opcode < 14) && (opcode & 5) == 1 && ra == rd) || ((opcode & 1) && !ra))
    {
        opc_illegal(ctx);
        return;
    }

    if (ra) {
        ctx->instr_str = my_sprintf("%-8sr%d, %s0x%X(r%d)", opc_int_ldst[opcode],
            rd, ((imm < 0) ? "-" : ""), abs(imm), ra);
    }
    else {
        ctx->instr_str = my_sprintf("%-8sr%d, %s0x%X", opc_int_ldst[opcode],
            rd, ((imm < 0) ? "-" : ""), abs(imm));
    }
}

void opc_fltldst(PPCDisasmContext* ctx)
{
    int32_t opcode = (ctx->instr_code >> 26) - 48;
    int32_t ra = (ctx->instr_code >> 16) & 0x1F;
    int32_t rd = (ctx->instr_code >> 21) & 0x1F;
    int32_t imm = SIGNEXT(ctx->instr_code & 0xFFFF, 15);

    /* ra = 0 is forbidden for loads and stores with update */
    /* ra = rd is forbidden for loads with update */
    if ((((opcode == 1) || (opcode == 3)) && ra == rd) || ((opcode & 1) && !ra))
    {
        opc_illegal(ctx);
        return;
    }

    if (ra) {
        ctx->instr_str = my_sprintf("%-8sfr%d, %s0x%X(r%d)", opc_flt_ldst[opcode],
            rd, ((imm < 0) ? "-" : ""), abs(imm), ra);
    }
    else {
        ctx->instr_str = my_sprintf("%-8sfr%d, %s0x%X", opc_flt_ldst[opcode],
            rd, ((imm < 0) ? "-" : ""), abs(imm));
    }
}

/** main dispatch table. */
static std::function<void(PPCDisasmContext*)> OpcodeDispatchTable[64] = {
    opc_illegal,   opc_illegal,   opc_illegal,   opc_twi,
    opc_group4,    opc_illegal,   opc_illegal,   opc_ar_im,
    opc_ar_im,     power_dozi,    opc_cmp_i_li,  opc_cmp_i_li,
    opc_ar_im,     opc_ar_im,     opc_ar_im,     opc_ar_im,
    opc_bcx,       opc_sc,        opc_bx,        opc_group19,
    opc_rlwimi,    opc_rlwinm,    opc_illegal,   opc_rlwnm,
    opc_ori,       opc_oris,      opc_xori,      opc_xoris,
    opc_andidot,   opc_andisdot,  opc_illegal,   opc_group31,
    opc_intldst,   opc_intldst,   opc_intldst,   opc_intldst,
    opc_intldst,   opc_intldst,   opc_intldst,   opc_intldst,
    opc_intldst,   opc_intldst,   opc_intldst,   opc_intldst,
    opc_intldst,   opc_intldst,   opc_intldst,   opc_intldst,
    opc_fltldst,   opc_fltldst,   opc_fltldst,   opc_fltldst,
    opc_fltldst,   opc_fltldst,   opc_fltldst,   opc_fltldst,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_group59,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_group63
};

string disassemble_single(PPCDisasmContext* ctx)
{
    OpcodeDispatchTable[ctx->instr_code >> 26](ctx);

    ctx->instr_addr += 4;

    return ctx->instr_str;
}