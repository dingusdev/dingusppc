#include <cinttypes>
#include <thirdparty/loguru/loguru.hpp>
#include "ppcemu.h"
#include "ppcmmu.h"
#include "ppcdefs.h"
#include "jittables.h"
#include "nuinterpreter.h"

#if !defined(USE_DTC)

CachedInstr* interp_tpc;
bool         interp_running;

#define GEN_OP(name,body)   void impl_##name(const CachedInstr* code) {body}
#define NEXT                interp_tpc++

#include "interpops.h"

struct InterpInstr {
    ImplSubr    emu_fn;     // pointer to the instruction emulation routine
    InstrInfo   info;
};

/* WARNING: entries in this table MUST be in the same order as constants in PPCInstr! */
InterpInstr interp_tab[] = {
    {nullptr,       {InstrOps::opNone,      CFlowType::CFL_NONE,           0}},
    {impl_addi,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_adde,     {InstrOps::opDAB,       CFlowType::CFL_NONE,           1}},
    {impl_addze,    {InstrOps::opDA,        CFlowType::CFL_NONE,           1}},
    {impl_andidot,  {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_lwz,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lwzu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lbz,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lhz,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_rlwinm,   {InstrOps::opRot,       CFlowType::CFL_NONE,           1}},
    {impl_srawidot, {InstrOps::opSASh,      CFlowType::CFL_NONE,           1}},
    {impl_bc,       {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,    0}},
    {impl_bdnz,     {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,    0}},
    {impl_bdz,      {InstrOps::opBrRel,     CFlowType::CFL_COND_BRANCH,    0}},
    {impl_bclr,     {InstrOps::opBrLink,    CFlowType::CFL_COND_BRANCH,    0}},
    {impl_mtspr,    {InstrOps::opSSpr,      CFlowType::CFL_NONE,           2}},
    {impl_bexit,    {InstrOps::opNone,      CFlowType::CFL_UNCOND_BRANCH,  0}},
    {impl_twi,      {InstrOps::opTOASimm,   CFlowType::CFL_NONE,           1}},
    {impl_mulli,    {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_cmpli,    {InstrOps::opCrfDASimm, CFlowType::CFL_NONE,           1}},
    {impl_cmpi,     {InstrOps::opCrfDAUimm, CFlowType::CFL_NONE,           1}},
    {impl_addic,    {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_addicdot, {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_sc,       {InstrOps::opSC,        CFlowType::CFL_NONE,           1}},
    {impl_rlwimix,  {InstrOps::opRot,       CFlowType::CFL_NONE,           1}},
    {impl_rlwnmx,   {InstrOps::opRot,       CFlowType::CFL_NONE,           1}},
    {impl_ori,      {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_oris,     {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_xori,     {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_xoris,    {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_andisdot, {InstrOps::opSAUimm,    CFlowType::CFL_NONE,           1}},
    {impl_lbzu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_stw,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_stwu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_stb,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_stbu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lhzu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lha,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_lhau,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_sth,      {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
    {impl_sthu,     {InstrOps::opDASimm,    CFlowType::CFL_NONE,           1}},
};

CachedInstr code_block[256];

#define REG_D(opcode)     ((opcode >> 21) & 0x1F)
#define REG_A(opcode)     ((opcode >> 16) & 0x1F)
#define REG_B(opcode)     ((opcode >> 11) & 0x1F)
#define REG_S             REG_D
#define BR_BO             REG_D
#define BR_BI             REG_A
#define SIMM(opcode)      ((int32_t)((int16_t)(opcode & 0xFFFF)))
#define UIMM(opcode)      ((uint32_t)((uint16_t)(opcode & 0xFFFF)))
#define REG_TO(opcode)    REG_D
#define REG_CRFD(opcode)  (((opcode >> 23) & 7) << 2)
#define REG_SR(opcode)    (opcode >> 16) & 15

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = 0xFFFFFFFFUL << (31 - rot_me);
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}


bool PreDecode(uint32_t next_pc, CachedInstr* c_instr)
{
    uint32_t opcode, main_opcode;
    unsigned instr_index;
    uint8_t* pc_real;
    bool     done;

    pc_real = quickinstruction_translate(next_pc);

    //CachedInstr* c_instr = &code[0];

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
                LOG_F(ERROR, "Illegal opcode 0x%08X - Program exception!", opcode);
                return false;
            }
        }

        const InterpInstr* p_instr = &interp_tab[instr_index];

        c_instr->call_me = p_instr->emu_fn;

        /* pre-decode operands, immediate values etc. */
        switch (p_instr->info.ops_fmt) {
        case InstrOps::opDA:
            c_instr->d1   = REG_D(opcode);
            c_instr->d2   = REG_A(opcode);
            break;
        case InstrOps::opDAB:
            c_instr->d1   = REG_D(opcode);
            c_instr->d2   = REG_A(opcode);
            c_instr->d3   = REG_B(opcode);
            break;
        case InstrOps::opDASimm:
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
        case InstrOps::opSC:
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
                return false;
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
                return false;
            }
            break;
        default:
            LOG_F(ERROR, "Unknown opcode format %d!", p_instr->info.ops_fmt);
            return false;
        }

        c_instr++;

        next_pc += 4;
        pc_real += 4;
        ppc_set_cur_instruction(pc_real);
    } // while

    return true;
}

void NuInterpExec(uint32_t start_addr) {
    interp_tpc = &code_block[0];

    if (!PreDecode(start_addr, interp_tpc)) {
        return;
    }

    interp_running = true;

    while(interp_running) {
        interp_tpc->call_me(interp_tpc);
    }
}

#endif // if !defined(USE_DTC)
