#include <cinttypes>
#include <thirdparty/loguru/loguru.hpp>
#include "ppcemu.h"
#include "ppcmmu.h"
#include "ppcdefs.h"
#include "jittables.h"
#include "nuinterpreter.h"

#if defined(USE_DTC)

#define CAST(name)          &&lab_##name

struct InterpInstr {
    void*       emu_fn;
    InstrInfo   info;
};

#define REG_D(opcode)     ((opcode >> 21) & 0x1F)
#define REG_A(opcode)     ((opcode >> 16) & 0x1F)
#define REG_B(opcode)     ((opcode >> 11) & 0x1F)
#define REG_S             REG_D
#define BR_BO             REG_D
#define BR_BI             REG_A
#define SIMM(opcode)      ((int32_t)((int16_t)(opcode & 0xFFFF)))
#define UIMM(opcode)      ((uint32_t)((uint16_t)(opcode & 0xFFFF)))
#define REG_TO(opcode)    ((opcode >> 21) & 0x1F)
#define REG_CRFD(opcode)  (((opcode >> 23) & 7) << 2)
#define REG_CRFS(opcode)  (((opcode >> 18) & 7) << 2)
#define REG_SR(opcode)    (opcode >> 16) & 15
#define LI(opcode)        (opcode >> 2) & 0xFFFFFF

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = 0xFFFFFFFFUL << (31 - rot_me);
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

CachedInstr code_block[256];

void NuInterpExec(uint32_t next_pc) {
    /* WARNING: entries in this table MUST be in the same order as constants in PPCInstr! */
    InterpInstr interp_tab[] = {
        {nullptr,             {InstrOps::opNone,      CFlowType::CFL_NONE,          0}},
        {CAST(addi),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(adde),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addze),         {InstrOps::opDA,        CFlowType::CFL_NONE,          1}},
        {CAST(andidot),       {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lwz),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lwzu),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lbz),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lhz),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(rlwinm),        {InstrOps::opRot,       CFlowType::CFL_NONE,          1}},
        {CAST(srawidot),      {InstrOps::opSASh,      CFlowType::CFL_NONE,          1}},
        {CAST(bc),            {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bdnz),          {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bdz),           {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bclr),          {InstrOps::opBrLink,    CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(mtspr),         {InstrOps::opSSpr,      CFlowType::CFL_NONE,          2}},
        {CAST(bexit),         {InstrOps::opNone,      CFlowType::CFL_UNCOND_BRANCH, 0}},
        {CAST(twi),           {InstrOps::opTOASimm,   CFlowType::CFL_NONE,          1}},
        {CAST(mulli),         {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(cmpli),         {InstrOps::opCrfDASimm, CFlowType::CFL_NONE,          1}},
        {CAST(cmpi),          {InstrOps::opCrfDAUimm, CFlowType::CFL_NONE,          1}},
        {CAST(addic),         {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(addicdot),      {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(sc),            {InstrOps::opNone,      CFlowType::CFL_NONE,          1}},
        {CAST(rlwimix),       {InstrOps::opRot,       CFlowType::CFL_NONE,          1}},
        {CAST(rlwnmx),        {InstrOps::opRot,       CFlowType::CFL_NONE,          1}},
        {CAST(ori),           {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(oris),          {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(xori),          {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(xoris),         {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(andisdot),      {InstrOps::opSAUimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lbzu),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stw),           {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stwu),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stb),           {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stbu),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lhzu),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lha),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lhau),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(sth),           {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(sthu),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lmw),           {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stmw),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lfs),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lfsu),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lfd),           {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(lfdu),          {InstrOps::opDASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stfs),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stfsu),         {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stfd),          {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(stfdu),         {InstrOps::opSASimm,    CFlowType::CFL_NONE,          1}},
        {CAST(bcl),           {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bca),           {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bcla),          {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(b),             {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bl),            {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(ba),            {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(bla),           {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,   0}},
        {CAST(crnor),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(rfi),           {InstrOps::opNone,      CFlowType::CFL_COND_BRANCH,   1}},
        {CAST(crandc),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(isync),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(crxor),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(crnand),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(crand),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(creqv),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(crorc),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(cror),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(bcctr),         {InstrOps::opBrRel,     CFlowType::CFL_NONE,          1}},
        {CAST(bcctrl),        {InstrOps::opBrRel,     CFlowType::CFL_NONE,          1}},
        {CAST(cmp),           {InstrOps::opCrfDAB,    CFlowType::CFL_NONE,          1}},
        {CAST(tw),            {InstrOps::opTOAB,      CFlowType::CFL_NONE,          1}},
        {CAST(subfc),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(subfcdot),      {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addc),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addcdot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mulhwu),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mulhwudot),     {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mfcr),          {InstrOps::opD,         CFlowType::CFL_NONE,          1}},
        {CAST(lwarx),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lwzx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(slw),           {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(slwdot),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(cntlzw),        {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},
        {CAST(cntlzwdot),     {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},
        {CAST(ppc_and),       {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(anddot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(cmpl),          {InstrOps::opCrfDAB,    CFlowType::CFL_NONE,          1}},
        {CAST(subf),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(subfdot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(dcbst),         {InstrOps::opAB,        CFlowType::CFL_NONE,          1}},
        {CAST(lwzux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(andc),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(andcdot),       {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mulhw),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mulhwdot),      {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        /*
        
        {CAST(lbzux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lbzx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lfdux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lfdx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lfsux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lfsx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lhaux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lhax),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lhbrx),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lhzux),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lhzx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lswi),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lswx),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(lwbrx),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},

        {CAST(add),           {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(adddot),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addc),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addcdot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addedot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addme),         {InstrOps::opDA,        CFlowType::CFL_NONE,          1}},
        {CAST(addmedot),      {InstrOps::opDA,        CFlowType::CFL_NONE,          1}},
        {CAST(addze),         {InstrOps::opDA,        CFlowType::CFL_NONE,          1}},
        {CAST(addzedot),      {InstrOps::opDA,        CFlowType::CFL_NONE,          1}},
        {CAST(and),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(anddot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(divw),          {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(divwdot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(divwu),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(divwudot),      {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(eieio),         {InstrOps::opNone,       CFlowType::CFL_NONE,          1}},

        {CAST(cmpl),          {InstrOps::opCrfDAB,    CFlowType::CFL_NONE,          1}},
        {CAST(eqv),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(eqvdot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(extsb),         {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},
        {CAST(extsbdot),      {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},
        {CAST(extsh),         {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},
        {CAST(extshdot),      {InstrOps::opSA,        CFlowType::CFL_NONE,          1}},

        {CAST(fmr),           {InstrOps::opDB,        CFlowType::CFL_NONE,          1}},
        {CAST(icbi),          {InstrOps::opAB,        CFlowType::CFL_NONE,          1}},
        {CAST(isync),         {InstrOps::opNone,      CFlowType::CFL_NONE,          1}},

        {CAST(mtcrf),         {InstrOps::opNone,      CFlowType::CFL_NONE,          1}},
        {CAST(mulhwu),        {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(mullw),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(nand),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(nanddot),       {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(neg),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(negdot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(nor),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(nordot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(or),            {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(ordot),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(orc),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(orcdot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(slw),           {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(slwdot),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(sraw),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(srawdot),       {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(srawi),         {InstrOps::opSASh,      CFlowType::CFL_NONE,          1}},
        {CAST(stbux),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stbx),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stfdux),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stfdx),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stfiwx),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stfsux),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stfsx),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(sthbrx),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(sthux),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(sthx),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stswi),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stswx),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stwbrx),        {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stwcx),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stwux),         {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},
        {CAST(stwx),          {InstrOps::opSAB,       CFlowType::CFL_NONE,          1}},

        {CAST(addedot),       {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addeo),         {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},
        {CAST(addeodot),      {InstrOps::opDAB,       CFlowType::CFL_NONE,          1}},

        */
    };

    CachedInstr* c_instr;
    uint32_t opcode, main_opcode;
    unsigned instr_index;
    uint8_t* pc_real;
    bool     done;

    pc_real = quickinstruction_translate(next_pc);

    c_instr = &code_block[0];

    done = false;

    while (!done) {
        opcode      = ppc_cur_instruction;
        main_opcode = opcode >> 26;

        instr_index = main_index_tab[main_opcode];
        if (!instr_index) {
            switch (main_opcode) {
            case 16:
                instr_index = subgrp16_index_tab[opcode & 3];
                break;
            case 18:
                instr_index = subgrp18_index_tab[opcode & 3];
                break;
            case 19:
                instr_index = subgrp19_index_tab[opcode & 0x7FF];
                break;
            case 31:
                instr_index = subgrp31_index_tab[opcode & 0x7FF];
                break;
            case 59:
                instr_index = subgrp59_index_tab[opcode & 0x3F];
                break;
            case 63:
                instr_index = subgrp63_index_tab[opcode & 0x7FF];
                break;
            default:
                instr_index = 0;
            }

            if (!instr_index) {
                LOG_F(INFO, "Illegal opcode 0x%08X - Program exception!", opcode);
                return;
            }
        }

        const InterpInstr* p_instr = &interp_tab[instr_index];

        c_instr->call_me = p_instr->emu_fn;

        /* pre-decode operands, immediate values etc. */
        switch (p_instr->info.ops_fmt) {
        case InstrOps::opDA:
        case InstrOps::opSA:
            c_instr->d1   = REG_D(opcode);
            c_instr->d2   = REG_A(opcode);
            break;
        case InstrOps::opDAB:
        case InstrOps::opSAB:
            c_instr->d1   = REG_D(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->d3   = REG_B(opcode);
            break;
        case InstrOps::opAB:
            c_instr->d2 = REG_A(opcode);
            c_instr->d3 = REG_B(opcode);
            break;
        case InstrOps::opTOAB:
            c_instr->d1 = REG_TO(opcode);
            c_instr->d2 = REG_A(opcode);
            c_instr->d3 = REG_B(opcode);
            break;
        case InstrOps::opCrfDAB:
            c_instr->d1 = REG_CRFD(opcode);
            c_instr->d2 = REG_A(opcode);
            c_instr->d3 = REG_B(opcode);
            break;
        case InstrOps::opDASimm:
        case InstrOps::opSASimm:
            c_instr->d1   = REG_D(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->simm = SIMM(opcode);
            break;
        case InstrOps::opSAUimm:
            c_instr->d1   = REG_S(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->uimm = UIMM(opcode);
            break;
        case InstrOps::opSASh:
            c_instr->d1   = REG_S(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->d3   = (opcode >> 11) & 0x1F;  // shift
            c_instr->uimm = (1 << c_instr->d3) - 1; // mask
            break;
        case InstrOps::opRot:
            c_instr->d1   = REG_S(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->d3   = (opcode >> 11) & 0x1F;  // shift
            c_instr->d4   = opcode & 1; // Rc bit
            c_instr->uimm = rot_mask((opcode >> 6) & 31, (opcode >> 1) & 31); //mask
            break;
        case InstrOps::opSSpr:
            c_instr->d1   = REG_S(opcode);
            c_instr->uimm = (REG_B(opcode) << 5) | REG_A(opcode);
            break;
        case InstrOps::opD:
            c_instr->d1 = REG_D(opcode);
            break;
        case InstrOps::opTOASimm:
            c_instr->d1   = REG_TO(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->simm = SIMM(opcode);
            break;
        case InstrOps::opTOB:
            c_instr->d1 = REG_TO(opcode);
            c_instr->d2 = REG_A(opcode);
            c_instr->d3 = REG_B(opcode);
            break;
        case InstrOps::opCrfDASimm:
            c_instr->d1   = REG_CRFD(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->simm = SIMM(opcode);
            break;
        case InstrOps::opCrfDAUimm:
            c_instr->d1   = REG_CRFD(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->simm = UIMM(opcode);
            break;
        case InstrOps::opDSR:
            c_instr->d1 = REG_D(opcode);
            c_instr->d2 = REG_SR(opcode);
        case InstrOps::opDB:
            c_instr->d1 = REG_D(opcode);
            c_instr->d2 = REG_B(opcode);
        case InstrOps::opNone:
            break;
        case InstrOps::opBrRel:
            c_instr->bt = SIMM(opcode) >> 2;
            switch (BR_BO(opcode) & 0x1E) {
            case 12:
            case 14:
                c_instr->call_me = interp_tab[PPCInstr::bc].emu_fn;
                c_instr->uimm = 0x80000000UL >> BR_BI(opcode);
                break;
            case 16:
                c_instr->call_me = interp_tab[PPCInstr::bdnz].emu_fn;
                break;
            case 18:
                c_instr->call_me = interp_tab[PPCInstr::bdz].emu_fn;
                break;
            default:
                LOG_F(ERROR, "Unsupported opcode 0x%08X - Program exception!", opcode);
                return;
            }
            break;
        case InstrOps::opBrLink:
            switch (BR_BO(opcode) & 0x1E) {
            case 20: // blr
                c_instr->call_me = interp_tab[PPCInstr::bexit].emu_fn;
                c_instr->uimm    = next_pc;
                done = true;
                continue;
            default:
                LOG_F(ERROR, "Unsupported opcode 0x%08X - Program exception!", opcode);
                return;
            }
            break;
        default:
            LOG_F(ERROR, "Unknown opcode format %d!", p_instr->info.ops_fmt);
            return;
        }

        c_instr++;

        next_pc += 4;
        pc_real += 4;
        ppc_set_cur_instruction(pc_real);
    } // while

    //LOG_F(INFO, "PreDecode completed!");

    CachedInstr* code = &code_block[0];

    goto *(code->call_me); // begin code execution

    #define GEN_OP(name,body)   lab_##name: { body; }
    #define NEXT                goto *((++code)->call_me)

    #include "interpops.h" // auto-generate labeled emulation blocks
}

#endif