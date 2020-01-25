#include <iostream>
#include <string>
#include <functional>  //Mandated by Visual Studio
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

const char* bx_mnem[4] = {
    "b", "bl", "ba", "bla"
};

const char* br_cond[8] = { /* simplified branch conditions */
    "ge", "le", "ne", "ns", "lt", "gt", "eq", "so"
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


/** various formatting helpers. */
void fmt_oneop(string& buf, const char* opc, int src)
{
    buf = my_sprintf("%-8sr%d", opc, src);
}

void fmt_twoop(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sr%d, r%d", opc, dst, src);
}

void fmt_twoop_imm(string& buf, const char* opc, int dst, int imm)
{
    buf = my_sprintf("%-8sr%d, 0x%04X", opc, dst, imm);
}

void fmt_twoop_fromspr(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sspr%d, r%d", opc, dst, src);
}

void fmt_twoop_tospr(string& buf, const char* opc, int dst, int src)
{
    buf = my_sprintf("%-8sr%d, spr%d", opc, dst, src);
}

void fmt_threeop(string& buf, const char* opc, int dst, int src1, int src2)
{
    buf = my_sprintf("%-8sr%d, r%d, r%d", opc, dst, src1, src2);
}

void fmt_threeop_imm(string& buf, const char* opc, int dst, int src1, int imm)
{
    buf = my_sprintf("%-8sr%d, r%d, 0x%04X", opc, dst, src1, imm);
}

void fmt_fourop(string& buf, const char* opc, int dst, int src1, int src2, int src3)
{
    buf = my_sprintf("%-8sr%d, r%d, r%d, r%d", opc, dst, src1, src2, src3);
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
    //return "DEADBEEF";
}

void opc_group4(PPCDisasmContext* ctx)
{
    printf("Altivec group 4 not supported yet\n");
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

    if (ctx->instr_code & 1)
        fmt_rotateop(ctx->instr_str, "rlwnm.", rs, ra, rb, mb, me, false);
    else
        fmt_rotateop(ctx->instr_str, "rlwnm", rs, ra, rb, mb, me, false);
}

void opc_mulli(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "mulli", rd, ra, imm);
}

void opc_subfic(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "subfic", rd, ra, imm);
}

void power_dozi(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "dozi", rd, ra, imm);
}

void opc_cmpli(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto crfd = (ctx->instr_code >> 23) & 0x07;
    auto imm = ctx->instr_code & 0xFFFF;

    if (ctx->instr_code & 0x200000)
        opc_illegal(ctx);
    else
        fmt_threeop_imm(ctx->instr_str, "cmpli", crfd, ra, imm);
}

void opc_cmpi(PPCDisasmContext* ctx)
{
    auto crfd = (ctx->instr_code >> 23) & 0x07;
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    if (ctx->instr_code & 0x200000)
        opc_illegal(ctx);
    else
        fmt_threeop_imm(ctx->instr_str, "cmpi", crfd, ra, imm);
}

void opc_addic(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "addic", rd, ra, imm);
}

void opc_addicdot(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "addic.", rd, ra, imm);
}

void opc_addi(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    if (ra == 0 && ctx->simplified)
        fmt_twoop_imm(ctx->instr_str, "li", rd, imm);
    else
        fmt_threeop_imm(ctx->instr_str, "addi", rd, ra, imm);
}

void opc_addis(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    if (ra == 0 && ctx->simplified)
        fmt_twoop_imm(ctx->instr_str, "lis", rd, imm);
    else
        fmt_threeop_imm(ctx->instr_str, "addis", rd, ra, imm);
}

void opc_lwz(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto offset = ctx->instr_code & 0xFFFF;

    if (ra == 0)
        fmt_twoop_imm(ctx->instr_str, "lzw", rd, offset);
    else
        fmt_threeop_imm(ctx->instr_str, "lzw", rd, ra, offset);
}

void opc_lwzu(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rd = (ctx->instr_code >> 21) & 0x1F;
    auto offset = ctx->instr_code & 0xFFFF;

    if ((ra == 0) || (ra == rd))
        opc_illegal(ctx);
    else
        fmt_threeop_imm(ctx->instr_str, "lzwu", rd, ra, offset);
}

void generic_bcx(PPCDisasmContext* ctx, uint32_t bo, uint32_t bi, uint32_t dst)
{
    char opcode[10] = "bc";

    if (ctx->instr_code & 1) {
        strcat_s(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (ctx->instr_code & 2) {
        strcat_s(opcode, "a"); /* add suffix "a" if the AA bit is set */
    }
    ctx->instr_str = my_sprintf("%-8s%d, %d, 0x%08X", opcode, bo, bi, dst);
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
        strcat_s(opcode, "d");
        strcat_s(opcode, (bo & 2) ? "z" : "nz");
        if (!(bo & 0x10)) {
            strcat_s(opcode, (bo & 8) ? "t" : "f");
            if (cr) {
                strcat_s(operands, "4*cr0+");
                operands[4] = cr + '0';
            }
            strcat_s(operands, br_cond[4 + (bi & 3)]);
        }
    }
    else { /* CTR ignored */
        strcat_s(opcode, br_cond[((bo >> 1) & 4) | (bi & 3)]);
        if (cr) {
            strcat_s(operands, "cr0");
            operands[2] = cr + '0';
        }
    }

    if (ctx->instr_code & 1) {
        strcat_s(opcode, "l"); /* add suffix "l" if the LK bit is set */
    }
    if (ctx->instr_code & 2) {
        strcat_s(opcode, "a"); /* add suffix "a" if the AA bit is set */
    }
    if (bo & 1) { /* incorporate prediction bit if set */
        strcat_s(opcode, (ctx->instr_code & 0x8000) ? "-" : "+");
    }

    ctx->instr_str = my_sprintf("%-8s%s, 0x%08X", opcode, operands, dst);
}

void opc_bx(PPCDisasmContext* ctx)
{
    uint32_t dst = ((ctx->instr_code & 2) ? 0 : ctx->instr_addr)
        + SIGNEXT(ctx->instr_code & 0x3FFFFFC, 25);

    ctx->instr_str = my_sprintf("%-8s0x%08X", bx_mnem[ctx->instr_code & 3], dst);
}

void opc_andidot(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "andi.", ra, rs, imm);
}

void opc_andisdot(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "andi.", ra, rs, imm);
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
    fmt_threeop_imm(ctx->instr_str, "ori", ra, rs, imm);
}

void opc_oris(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "oris", ra, rs, imm);
}

void opc_xori(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "xori", ra, rs, imm);
}

void opc_xoris(PPCDisasmContext* ctx)
{
    auto ra = (ctx->instr_code >> 16) & 0x1F;
    auto rs = (ctx->instr_code >> 21) & 0x1F;
    auto imm = ctx->instr_code & 0xFFFF;

    fmt_threeop_imm(ctx->instr_str, "xoris", ra, rs, imm);
}

void opc_group19(PPCDisasmContext* ctx)
{
    int  ext_opc = (ctx->instr_code >> 1) & 0x3FF; /* extract extended opcode */

    if (ext_opc == 50)
        ctx->instr_str = my_sprintf("%-8s, "rfi");
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
            strcpy_s(opcode, opc_subs[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat_s(opcode, "o");
            if (rc_set)
                strcat_s(opcode, ".");
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
            strcpy_s(opcode, opc_adds[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat_s(opcode, "o");
            if (rc_set)
                strcat_s(opcode, ".");
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
            strcpy_s(opcode, opc_muldivs[index]);
            if (ext_opc & 0x200) /* check OE bit */
                strcat_s(opcode, "o");
            if (rc_set)
                strcat_s(opcode, ".");
            if ((!index || index == 2) && (ext_opc & 0x200))
                opc_illegal(ctx);
            else
                fmt_threeop(ctx->instr_str, opcode, rs, ra, rb);
        }
        return;

    case 0x1C: /* logical instructions */
        if (index == 13 && rs == rb && ctx->simplified) {
            fmt_twoop(ctx->instr_str, rc_set ? "mr." : "mr", ra, rs);
        }
        else {
            strcpy_s(opcode, opc_logic[index]);
            if (!strlen(opcode)) {
                opc_illegal(ctx);
            }
            else {
                if (rc_set)
                    strcat_s(opcode, ".");
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
        break;
    }

    auto ref_spr = (((ctx->instr_code >> 11) & 31) << 5) | ((ctx->instr_code >> 16) & 31);

    switch (ext_opc) {
    case 4:
        if (rc_set)
            opc_illegal(ctx);
        else
            ctx->instr_str = my_sprintf("%-8s%d, r%d, r%d", "tw", rs, ra, rb);
        break;
    case 19: /* mfcr */
        fmt_oneop(ctx->instr_str, "mfcr", rs);
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
        fmt_twoop_fromspr(ctx->instr_str, "mfspr", rs, ref_spr);
        break;
    case 467: /* mtspr */
        fmt_twoop_tospr(ctx->instr_str, "mtspr", ref_spr, rs);
        break;
    default:
        opc_illegal(ctx);
    }
}

/** main dispatch table. */
static std::function<void(PPCDisasmContext*)> OpcodeDispatchTable[64] = {
    opc_illegal,   opc_illegal,   opc_illegal,   opc_twi,
    opc_group4,    opc_illegal,   opc_illegal,   opc_mulli,
    opc_subfic,    power_dozi,    opc_cmpli,     opc_cmpi,
    opc_addic,     opc_addicdot,  opc_addi,      opc_addis,
    opc_bcx,       opc_illegal,   opc_bx,        opc_group19,
    opc_rlwimi,    opc_rlwinm,    opc_illegal,   opc_rlwnm,
    opc_ori,       opc_oris,      opc_xori,      opc_xoris,
    opc_andidot,   opc_andisdot,  opc_illegal,   opc_group31,
    opc_lwz,       opc_lwzu,      opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal,
    opc_illegal,   opc_illegal,   opc_illegal,   opc_illegal

    /*
    {15, &opc_addis},     {16, &opc_bcx},  {17, &opc_sc},
    {18, &opc_bx},        {19, &opc_opcode19},  {20, &opc_rlwimi},
    {21, &opc_rlwinm},    {22, &power_rlmi},    {23, &opc_rlwnm},
    {24, &opc_ori},       {25, &opc_oris},      {26, &opc_xori},
    {27, &opc_xoris},     {28, &opc_andidot},   {29, &opc_andisdot},
    {30, &opc_illegal},   {31, &opc_group31},   {32, &opc_lwz},
    {33, &opc_lwzu},      {34, &opc_lbz},       {35, &opc_lbzu},
    {36, &opc_stw},       {37, &opc_stwu},      {38, &opc_stb},
    {39, &opc_stbu},      {40, &opc_lhz},       {41, &opc_lhzu},
    {42, &opc_lha},       {43, &opc_lhau},      {44, &opc_sth},
    {45, &opc_sthu},      {46, &opc_lmw},       {47, &opc_stmw},
    {48, &opc_lfs},       {49, &opc_lfsu},      {50, &opc_lfd},
    {51, &opc_lfdu},      {52, &opc_stfs},      {53, &opc_stfsu},
    {54, &opc_stfd},      {55, &opc_stfdu},     {56, &opc_psq_l},
    {57, &opc_psq_lu},    {58, &opc_illegal},   {59, &opc_illegal},
    {60, &opc_psq_st},    {61, &opc_psq_stu},   {62, &opc_illegal},
    {63, &opc_opcode63}
    */
};

string disassemble_single(PPCDisasmContext* ctx)
{
    OpcodeDispatchTable[ctx->instr_code >> 26](ctx);

    ctx->instr_addr += 4;

    return ctx->instr_str;
}