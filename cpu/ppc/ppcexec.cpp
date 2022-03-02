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

#include <core/timermanager.h>
#include <loguru.hpp>
#include "ppcemu.h"
#include "ppcmmu.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <setjmp.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <unordered_map>

using namespace std;
using namespace dppc_interpreter;

MemCtrlBase* mem_ctrl_instance = 0;

bool power_on = 1;

SetPRS ppc_state;

bool rc_flag = 0;           // Record flag
bool oe_flag = 0;    // Overflow flag

bool grab_return;
bool grab_breakpoint;

uint32_t ppc_cur_instruction;    // Current instruction for the PPC
uint32_t ppc_effective_address;
uint32_t ppc_next_instruction_address;    // Used for branching, setting up the NIA

unsigned exec_flags;

/* copy of local variable bb_start_la. Need for correct
   calculation of CPU cycles after setjmp that clobbers
   non-volatile local variables. */
uint32_t    glob_bb_start_la;

/* variables related to virtual time */
uint64_t g_icycles;
int      icnt_factor;

/* global variables related to the timebase facility */
uint64_t tbr_wr_timestamp;  // stores vCPU virtual time of the last TBR write
uint64_t tbr_wr_value;      // last value written to the TBR
uint64_t tbr_freq_hz;       // TBR driving frequency in Hz
uint64_t timebase_counter;  // internal timebase counter
uint32_t decr;              // current value of PPC DEC register

#ifdef CPU_PROFILING

/* global variables for lightweight CPU profiling */
uint64_t num_executed_instrs;
uint64_t num_supervisor_instrs;
uint64_t num_int_loads;
uint64_t num_int_stores;
uint64_t exceptions_processed;

#include "utils/profiler.h"
#include <memory>

class CPUProfile : public BaseProfile {
public:
    CPUProfile() : BaseProfile("PPC_CPU") {};

    void populate_variables(std::vector<ProfileVar>& vars) {
        vars.clear();

        vars.push_back({.name = "Executed Instructions Total",
                        .format = ProfileVarFmt::DEC,
                        .value = num_executed_instrs});

        vars.push_back({.name = "Executed Supervisor Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_supervisor_instrs});

        vars.push_back({.name = "Integer Load Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_int_loads});

        vars.push_back({.name = "Integer Store Instructions",
                        .format = ProfileVarFmt::DEC,
                        .value = num_int_stores});

        vars.push_back({.name = "Exceptions processed",
                        .format = ProfileVarFmt::DEC,
                        .value = exceptions_processed});
    };

    void reset() {
        num_executed_instrs = 0;
        num_supervisor_instrs = 0;
        num_int_loads = 0;
        num_int_stores = 0;
        exceptions_processed = 0;
    };
};

#endif

/** Opcode lookup tables. */

/** Primary opcode (bits 0...5) lookup table. */
static PPCOpcode OpcodeGrabber[] = {
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_twi,       ppc_illegalop, ppc_illegalop,
    ppc_illegalop, ppc_mulli,     ppc_subfic,    power_dozi,    ppc_cmpli,     ppc_cmpi,
    ppc_addic,     ppc_addicdot,  ppc_addi,      ppc_addis,     ppc_opcode16,  ppc_sc,
    ppc_opcode18,  ppc_opcode19,  ppc_rlwimi,    ppc_rlwinm,    power_rlmi,    ppc_rlwnm,
    ppc_ori,       ppc_oris,      ppc_xori,      ppc_xoris,     ppc_andidot,   ppc_andisdot,
    ppc_illegalop, ppc_opcode31,  ppc_lwz,       ppc_lwzu,      ppc_lbz,       ppc_lbzu,
    ppc_stw,       ppc_stwu,      ppc_stb,       ppc_stbu,      ppc_lhz,       ppc_lhzu,
    ppc_lha,       ppc_lhau,      ppc_sth,       ppc_sthu,      ppc_lmw,       ppc_stmw,
    ppc_lfs,       ppc_lfsu,      ppc_lfd,       ppc_lfdu,      ppc_stfs,      ppc_stfsu,
    ppc_stfd,      ppc_stfdu,     ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_opcode59,
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_opcode63};

/** Lookup tables for branch instructions. */
static PPCOpcode SubOpcode16Grabber[] = {
    dppc_interpreter::ppc_bc,
    dppc_interpreter::ppc_bcl,
    dppc_interpreter::ppc_bca,
    dppc_interpreter::ppc_bcla};

static PPCOpcode SubOpcode18Grabber[] = {
    dppc_interpreter::ppc_b,
    dppc_interpreter::ppc_bl,
    dppc_interpreter::ppc_ba,
    dppc_interpreter::ppc_bla};

/** Instructions decoding tables for integer,
    single floating-point, and double-floating point ops respectively */

PPCOpcode SubOpcode31Grabber[1024] = { ppc_illegalop };
PPCOpcode SubOpcode59Grabber[32]   = { ppc_illegalop };
PPCOpcode SubOpcode63Grabber[1024] = { ppc_illegalop };

/** Exception helpers. */

void ppc_illegalop() {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
}

void ppc_fpu_off() {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::FPU_OFF);
}

void ppc_ext_int() {
    if (ppc_state.msr & 0x8000) {
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
    }
}

/** Opcode decoding functions. */

void ppc_opcode16() {
    SubOpcode16Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode18() {
    SubOpcode18Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode19() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FF;

#ifdef EXHAUSTIVE_DEBUG
    uint32_t regrab = (uint32_t)subop_grab;
    LOG_F(INFO, "Executing Opcode 19 table subopcode entry \n", regrab);
#endif    // EXHAUSTIVE_DEBUG

    if (subop_grab == 32) {
        ppc_bclr();
    } else if (subop_grab == 33) {
        ppc_bclrl();
    } else if (subop_grab == 1056) {
        ppc_bcctr();
    } else if (subop_grab == 1057) {
        ppc_bcctrl();

    } else {
        switch (subop_grab) {
        case 0:
            ppc_mcrf();
            break;
        case 66:
            ppc_crnor();
            break;
        case 100:
            ppc_rfi();
            break;
        case 258:
            ppc_crandc();
            break;
        case 300:
            ppc_isync();
            break;
        case 386:
            ppc_crxor();
            break;
        case 450:
            ppc_crnand();
            break;
        case 514:
            ppc_crand();
            break;
        case 578:
            ppc_creqv();
            break;
        case 834:
            ppc_crorc();
            break;
        case 898:
            ppc_cror();
            break;
        default:
            ppc_illegalop();
        }
    }
}

void ppc_opcode31() {
    uint16_t subop_grab = (ppc_cur_instruction & 0x7FF) >> 1;

    rc_flag = ppc_cur_instruction & 0x1;
    oe_flag = ppc_cur_instruction & 0x400;

#ifdef EXHAUSTIVE_DEBUG
    LOG_F(INFO, "Executing Opcode 31 table subopcode entry %n \n", (uint32_t)subop_grab);
#endif    // EXHAUSTIVE_DEBUG

    SubOpcode31Grabber[subop_grab]();
}

void ppc_opcode59() {
    uint16_t subop_grab = (ppc_cur_instruction >> 1) & 0x1F;
    rc_flag             = ppc_cur_instruction & 1;
#ifdef EXHAUSTIVE_DEBUG
    LOG_F(INFO, "Executing Opcode 59 table subopcode entry %n \n", (uint32_t)subop_grab);
#endif    // EXHAUSTIVE_DEBUG
    SubOpcode59Grabber[subop_grab]();
}

void ppc_opcode63() {
    uint16_t subop_grab = (ppc_cur_instruction >> 1) & 0x3FF;
    rc_flag             = ppc_cur_instruction & 1;
#ifdef EXHAUSTIVE_DEBUG
    LOG_F(INFO, "Executing Opcode 63 table subopcode entry %n \n", (uint32_t)subop_grab);
#endif    // EXHAUSTIVE_DEBUG
    SubOpcode63Grabber[subop_grab]();
}

/* Dispatch using main opcode */
void ppc_main_opcode()
{
#ifdef CPU_PROFILING
    num_executed_instrs++;
#endif
    OpcodeGrabber[(ppc_cur_instruction >> 26) & 0x3F]();
}

uint64_t get_virt_time_ns()
{
    return g_icycles << icnt_factor;
}

uint64_t process_events()
{
    uint64_t slice_ns = TimerManager::get_instance()->process_timers(get_virt_time_ns());
    if (slice_ns == 0) {
        // execute 10.000 cycles
        // if there are no pending timers
        return g_icycles + 10000;
    }
    return g_icycles + ((slice_ns + (1 << icnt_factor)) >> icnt_factor);
}

void force_cycle_counter_reload()
{
    // tell the interpreter loop to reload cycle counter
    exec_flags |= EXEF_TIMER;
}

/** Execute PPC code as long as power is on. */
// inner interpreter loop
static void ppc_exec_inner()
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;

    max_cycles = 0;

    while (power_on) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        eb_end     = (eb_start + PAGE_SIZE) & PAGE_MASK;
        exec_flags = 0;

        page_start = eb_start & PAGE_MASK;
        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while (ppc_state.pc < eb_end) {
            ppc_main_opcode();
            if (g_icycles++ >= max_cycles) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                if (!power_on)
                    break;
                // reload cycle counter if requested
                if (exec_flags & EXEF_TIMER) {
                    max_cycles = process_events();
                    if (!(exec_flags & ~EXEF_TIMER)) {
                        ppc_state.pc += 4;
                        pc_real += 4;
                        ppc_set_cur_instruction(pc_real);
                        exec_flags = 0;
                        continue;
                    }
                }
                // define next execution block
                eb_start = ppc_next_instruction_address;
                eb_end   = (eb_start + PAGE_SIZE) & PAGE_MASK;
                if ((eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    pc_real = mmu_translate_imem(eb_start);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                ppc_set_cur_instruction(pc_real);
            }
        }
    }
}

// outer interpreter loop
void ppc_exec()
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    while (power_on) {
        ppc_exec_inner();
    }
}

/** Execute one PPC instruction. */
void ppc_exec_single()
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
        exec_flags = 0;
        return;
    }

    mmu_translate_imem(ppc_state.pc);
    ppc_main_opcode();
    if (exec_flags) {
        if (exec_flags & EXEF_TIMER) {
            ppc_state.pc += 4;
        } else {
            ppc_state.pc = ppc_next_instruction_address;
        }
        exec_flags = 0;
    } else {
        ppc_state.pc += 4;
    }
    g_icycles++;
}

/** Execute PPC code until goal_addr is reached. */

// inner interpreter loop
static void ppc_exec_until_inner(const uint32_t goal_addr)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;

    max_cycles = 0;

    while (ppc_state.pc != goal_addr) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        eb_end     = (eb_start + PAGE_SIZE) & PAGE_MASK;
        exec_flags = 0;

        page_start = eb_start & PAGE_MASK;
        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while ((ppc_state.pc != goal_addr) && (ppc_state.pc < eb_end)) {
            ppc_main_opcode();
            if (g_icycles++ >= max_cycles) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // reload cycle counter if requested
                if (exec_flags & EXEF_TIMER) {
                    max_cycles = process_events();
                    if (!(exec_flags & ~EXEF_TIMER)) {
                        ppc_state.pc += 4;
                        pc_real += 4;
                        ppc_set_cur_instruction(pc_real);
                        exec_flags = 0;
                        continue;
                    }
                }
                // define next execution block
                eb_start = ppc_next_instruction_address;
                eb_end   = (eb_start + PAGE_SIZE) & PAGE_MASK;
                if ((eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    pc_real = mmu_translate_imem(eb_start);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                ppc_set_cur_instruction(pc_real);
            }
        }
    }
}

// outer interpreter loop
void ppc_exec_until(volatile uint32_t goal_addr)
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    while (ppc_state.pc != goal_addr) {
        ppc_exec_until_inner(goal_addr);
    }
}

/** Execute PPC code until control is reached the specified region. */

// inner interpreter loop
static void ppc_exec_dbg_inner(const uint32_t start_addr, const uint32_t size)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;

    max_cycles = 0;

    while (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        eb_end     = (eb_start + PAGE_SIZE) & PAGE_MASK;
        exec_flags = 0;

        page_start = eb_start & PAGE_MASK;
        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while ((ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)
                && (ppc_state.pc < eb_end)) {
            ppc_main_opcode();
            if (g_icycles++ >= max_cycles) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // reload cycle counter if requested
                if (exec_flags & EXEF_TIMER) {
                    max_cycles = process_events();
                    if (!(exec_flags & ~EXEF_TIMER)) {
                        ppc_state.pc += 4;
                        pc_real += 4;
                        ppc_set_cur_instruction(pc_real);
                        exec_flags = 0;
                        continue;
                    }
                }
                // define next execution block
                eb_start = ppc_next_instruction_address;
                eb_end   = (eb_start + PAGE_SIZE) & PAGE_MASK;
                if ((eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    pc_real = mmu_translate_imem(eb_start);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                ppc_set_cur_instruction(pc_real);
            }
        }
    }
}

// outer interpreter loop
void ppc_exec_dbg(volatile uint32_t start_addr, volatile uint32_t size)
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    while (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size) {
        ppc_exec_dbg_inner(start_addr, size);
    }
}

void initialize_ppc_opcode_tables() {
    SubOpcode31Grabber[0]  = ppc_cmp;
    SubOpcode31Grabber[4]  = ppc_tw;
    SubOpcode31Grabber[32] = ppc_cmpl;

    SubOpcode31Grabber[8] = SubOpcode31Grabber[520]   = ppc_subfc;
    SubOpcode31Grabber[40] = SubOpcode31Grabber[552]  = ppc_subf;
    SubOpcode31Grabber[104] = SubOpcode31Grabber[616] = ppc_neg;
    SubOpcode31Grabber[136] = SubOpcode31Grabber[648] = ppc_subfe;
    SubOpcode31Grabber[200] = SubOpcode31Grabber[712] = ppc_subfze;
    SubOpcode31Grabber[232] = SubOpcode31Grabber[744] = ppc_subfme;

    SubOpcode31Grabber[10] = SubOpcode31Grabber[522] = ppc_addc;
    SubOpcode31Grabber[138] = SubOpcode31Grabber[650] = ppc_adde;
    SubOpcode31Grabber[202] = SubOpcode31Grabber[714] = ppc_addze;
    SubOpcode31Grabber[234] = SubOpcode31Grabber[746] = ppc_addme;
    SubOpcode31Grabber[266] = SubOpcode31Grabber[778] = ppc_add;

    SubOpcode31Grabber[11]  = ppc_mulhwu;
    SubOpcode31Grabber[75]  = ppc_mulhw;
    SubOpcode31Grabber[235] = SubOpcode31Grabber[747] = ppc_mullw;
    SubOpcode31Grabber[459] = SubOpcode31Grabber[971] = ppc_divwu;
    SubOpcode31Grabber[491] = SubOpcode31Grabber[1003] = ppc_divw;

    SubOpcode31Grabber[20]  = ppc_lwarx;
    SubOpcode31Grabber[23]  = ppc_lwzx;
    SubOpcode31Grabber[55]  = ppc_lwzux;
    SubOpcode31Grabber[87]  = ppc_lbzx;
    SubOpcode31Grabber[119] = ppc_lbzux;
    SubOpcode31Grabber[279] = ppc_lhzx;
    SubOpcode31Grabber[311] = ppc_lhzux;
    SubOpcode31Grabber[343] = ppc_lhax;
    SubOpcode31Grabber[375] = ppc_lhaux;
    SubOpcode31Grabber[533] = ppc_lswx;
    SubOpcode31Grabber[534] = ppc_lwbrx;
    SubOpcode31Grabber[535] = ppc_lfsx;
    SubOpcode31Grabber[567] = ppc_lfsux;
    SubOpcode31Grabber[597] = ppc_lswi;
    SubOpcode31Grabber[599] = ppc_lfdx;
    SubOpcode31Grabber[631] = ppc_lfdux;
    SubOpcode31Grabber[790] = ppc_lhbrx;

    SubOpcode31Grabber[150] = ppc_stwcx;
    SubOpcode31Grabber[151] = ppc_stwx;
    SubOpcode31Grabber[183] = ppc_stwux;
    SubOpcode31Grabber[215] = ppc_stbx;
    SubOpcode31Grabber[247] = ppc_stbux;
    SubOpcode31Grabber[407] = ppc_sthx;
    SubOpcode31Grabber[439] = ppc_sthux;
    SubOpcode31Grabber[661] = ppc_stswx;
    SubOpcode31Grabber[662] = ppc_stwbrx;
    SubOpcode31Grabber[663] = ppc_stfsx;
    SubOpcode31Grabber[695] = ppc_stfsux;
    SubOpcode31Grabber[725] = ppc_stswi;
    SubOpcode31Grabber[727] = ppc_stfdx;
    SubOpcode31Grabber[759] = ppc_stfdux;
    SubOpcode31Grabber[918] = ppc_sthbrx;
    SubOpcode31Grabber[983] = ppc_stfiwx;

    SubOpcode31Grabber[310] = ppc_eciwx;
    SubOpcode31Grabber[438] = ppc_ecowx;

    SubOpcode31Grabber[24]  = ppc_slw;
    SubOpcode31Grabber[28]  = ppc_and;
    SubOpcode31Grabber[60]  = ppc_andc;
    SubOpcode31Grabber[124] = ppc_nor;
    SubOpcode31Grabber[284] = ppc_eqv;
    SubOpcode31Grabber[316] = ppc_xor;
    SubOpcode31Grabber[412] = ppc_orc;
    SubOpcode31Grabber[444] = ppc_or;
    SubOpcode31Grabber[476] = ppc_nand;
    SubOpcode31Grabber[536] = ppc_srw;
    SubOpcode31Grabber[792] = ppc_sraw;
    SubOpcode31Grabber[824] = ppc_srawi;
    SubOpcode31Grabber[922] = ppc_extsh;
    SubOpcode31Grabber[954] = ppc_extsb;

    SubOpcode31Grabber[26] = ppc_cntlzw;

    SubOpcode31Grabber[19]  = ppc_mfcr;
    SubOpcode31Grabber[83]  = ppc_mfmsr;
    SubOpcode31Grabber[144] = ppc_mtcrf;
    SubOpcode31Grabber[146] = ppc_mtmsr;
    SubOpcode31Grabber[210] = ppc_mtsr;
    SubOpcode31Grabber[242] = ppc_mtsrin;
    SubOpcode31Grabber[339] = ppc_mfspr;
    SubOpcode31Grabber[371] = ppc_mftb;
    SubOpcode31Grabber[467] = ppc_mtspr;
    SubOpcode31Grabber[512] = ppc_mcrxr;
    SubOpcode31Grabber[595] = ppc_mfsr;
    SubOpcode31Grabber[659] = ppc_mfsrin;

    SubOpcode31Grabber[54]   = ppc_dcbst;
    SubOpcode31Grabber[86]   = ppc_dcbf;
    SubOpcode31Grabber[246]  = ppc_dcbtst;
    SubOpcode31Grabber[278]  = ppc_dcbt;
    SubOpcode31Grabber[598]  = ppc_sync;
    SubOpcode31Grabber[470]  = ppc_dcbi;
    SubOpcode31Grabber[1014] = ppc_dcbz;

    SubOpcode31Grabber[29]                             = power_maskg;
    SubOpcode31Grabber[107] = SubOpcode31Grabber[619]  = power_mul;
    SubOpcode31Grabber[152]                            = power_slq;
    SubOpcode31Grabber[153]                            = power_sle;
    SubOpcode31Grabber[184]                            = power_sliq;
    SubOpcode31Grabber[216]                            = power_sllq;
    SubOpcode31Grabber[217]                            = power_sleq;
    SubOpcode31Grabber[248]                            = power_slliq;
    SubOpcode31Grabber[264] = SubOpcode31Grabber[776]  = power_doz;
    SubOpcode31Grabber[277]                            = power_lscbx;
    SubOpcode31Grabber[331] = SubOpcode31Grabber[843]  = power_div;
    SubOpcode31Grabber[360] = SubOpcode31Grabber[872]  = power_abs;
    SubOpcode31Grabber[363] = SubOpcode31Grabber[875]  = power_divs;
    SubOpcode31Grabber[488] = SubOpcode31Grabber[1000] = power_nabs;
    SubOpcode31Grabber[531]                            = power_clcs;
    SubOpcode31Grabber[537]                            = power_rrib;
    SubOpcode31Grabber[541]                            = power_maskir;
    SubOpcode31Grabber[664]                            = power_srq;
    SubOpcode31Grabber[665]                            = power_sre;
    SubOpcode31Grabber[696]                            = power_sriq;
    SubOpcode31Grabber[728]                            = power_srlq;
    SubOpcode31Grabber[729]                            = power_sreq;
    SubOpcode31Grabber[760]                            = power_srliq;
    SubOpcode31Grabber[920]                            = power_sraq;
    SubOpcode31Grabber[921]                            = power_srea;
    SubOpcode31Grabber[952]                            = power_sraiq;

    SubOpcode31Grabber[306]  = ppc_tlbie;
    SubOpcode31Grabber[370]  = ppc_tlbia;
    SubOpcode31Grabber[566]  = ppc_tlbsync;
    SubOpcode31Grabber[854]  = ppc_eieio;
    SubOpcode31Grabber[982]  = ppc_icbi;
    SubOpcode31Grabber[978]  = ppc_tlbld;
    SubOpcode31Grabber[1010] = ppc_tlbli;

    SubOpcode59Grabber[18] = ppc_fdivs;
    SubOpcode59Grabber[20] = ppc_fsubs;
    SubOpcode59Grabber[21] = ppc_fadds;
    SubOpcode59Grabber[22] = ppc_fsqrts;
    SubOpcode59Grabber[24] = ppc_fres;
    SubOpcode59Grabber[25] = ppc_fmuls;
    SubOpcode59Grabber[28] = ppc_fmsubs;
    SubOpcode59Grabber[29] = ppc_fmadds;
    SubOpcode59Grabber[30] = ppc_fnmsubs;
    SubOpcode59Grabber[31] = ppc_fnmadds;

    SubOpcode63Grabber[0]   = ppc_fcmpu;
    SubOpcode63Grabber[12]  = ppc_frsp;
    SubOpcode63Grabber[14]  = ppc_fctiw;
    SubOpcode63Grabber[15]  = ppc_fctiwz;
    SubOpcode63Grabber[18]  = ppc_fdiv;
    SubOpcode63Grabber[20]  = ppc_fsub;
    SubOpcode63Grabber[21]  = ppc_fadd;
    SubOpcode63Grabber[22]  = ppc_fsqrt;
    SubOpcode63Grabber[26]  = ppc_frsqrte;
    SubOpcode63Grabber[32]  = ppc_fcmpo;
    SubOpcode63Grabber[38]  = ppc_mtfsb1;
    SubOpcode63Grabber[40]  = ppc_fneg;
    SubOpcode63Grabber[64]  = ppc_mcrfs;
    SubOpcode63Grabber[70]  = ppc_mtfsb0;
    SubOpcode63Grabber[72]  = ppc_fmr;
    SubOpcode63Grabber[134] = ppc_mtfsfi;
    SubOpcode63Grabber[136] = ppc_fnabs;
    SubOpcode63Grabber[264] = ppc_fabs;
    SubOpcode63Grabber[583] = ppc_mffs;
    SubOpcode63Grabber[711] = ppc_mtfsf;

    for (int i = 0; i < 1024; i += 32) {
        SubOpcode63Grabber[i + 23] = ppc_fsel;
        SubOpcode63Grabber[i + 25] = ppc_fmul;
        SubOpcode63Grabber[i + 28] = ppc_fmsub;
        SubOpcode63Grabber[i + 29] = ppc_fmadd;
        SubOpcode63Grabber[i + 30] = ppc_fnmsub;
        SubOpcode63Grabber[i + 31] = ppc_fnmadd;
    }
}

void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version) {
    int i;

    mem_ctrl_instance = mem_ctrl;

    //test_timebase_update();

    initialize_ppc_opcode_tables();

    // initialize emulator timers
    TimerManager::get_instance()->set_time_now_cb(&get_virt_time_ns);
    TimerManager::get_instance()->set_notify_changes_cb(&force_cycle_counter_reload);

    // initialize time base facility
    g_icycles   = 0;
    icnt_factor = 4;
    tbr_wr_timestamp = 0;
    tbr_wr_value = 0;
    tbr_freq_hz = 16705000; // FIXME: this should be set properly during machine initialization

    exec_flags = 0;

    timebase_counter = 0;
    decr = 0;

    /* zero all GPRs as prescribed for MPC601 */
    /* For later PPC CPUs, GPR content is undefined */
    for (i = 0; i < 32; i++) {
        ppc_state.gpr[i] = 0;
    }

    /* zero all FPRs as prescribed for MPC601 */
    /* For later PPC CPUs, GPR content is undefined */
    for (i = 0; i < 32; i++) {
        ppc_state.fpr[i].int64_r = 0;
    }

    /* zero all segment registers as prescribed for MPC601 */
    /* For later PPC CPUs, SR content is undefined */
    for (i = 0; i < 16; i++) {
        ppc_state.sr[i] = 0;
    }

    ppc_state.cr    = 0;
    ppc_state.fpscr = 0;

    ppc_state.pc = 0;

    ppc_state.tbr[0] = 0;
    ppc_state.tbr[1] = 0;

    /* zero all SPRs */
    for (i = 0; i < 1024; i++) {
        ppc_state.spr[i] = 0;
    }

    ppc_state.spr[SPR::PVR] = cpu_version;

    if ((cpu_version & 0xFFFF0000) == 0x00010000) {
        /* MPC601 sets MSR[ME] bit during hard reset / Power-On */
        ppc_state.msr = 0x1040;
    } else {
        ppc_state.msr           = 0x40;
        ppc_state.spr[SPR::DEC] = 0xFFFFFFFFUL;
    }

    ppc_mmu_init(cpu_version);

    /* redirect code execution to reset vector */
    ppc_state.pc = 0xFFF00100;

#ifdef CPU_PROFILING
    gProfilerObj->register_profile("PPC_CPU",
        std::unique_ptr<BaseProfile>(new CPUProfile()));
#endif
}

void print_fprs() {
    for (int i = 0; i < 32; i++)
        cout << "FPR " << dec << i << " : " << ppc_state.fpr[i].dbl64_r << endl;
}

static map<string, int> SPRName2Num = {
    {"XER", SPR::XER}, {"LR", SPR::LR}, {"CTR", SPR::CTR}, {"DEC", SPR::DEC}, {"PVR", SPR::PVR}};

uint64_t reg_op(string& reg_name, uint64_t val, bool is_write) {
    string reg_name_u, reg_num_str;
    unsigned reg_num;
    map<string, int>::iterator spr;

    if (reg_name.length() < 2)
        goto bail_out;

    reg_name_u = reg_name;

    /* convert reg_name string to uppercase */
    std::for_each(reg_name_u.begin(), reg_name_u.end(), [](char& c) { c = ::toupper(c); });

    try {
        if (reg_name_u == "PC") {
            if (is_write)
                ppc_state.pc = val;
            return ppc_state.pc;
        }
        if (reg_name_u == "MSR") {
            if (is_write)
                ppc_state.msr = val;
            return ppc_state.msr;
        }
        if (reg_name_u == "CR") {
            if (is_write)
                ppc_state.cr = val;
            return ppc_state.cr;
        }
        if (reg_name_u == "FPSCR") {
            if (is_write)
                ppc_state.fpscr = val;
            return ppc_state.fpscr;
        }

        if (reg_name_u.substr(0, 1) == "R") {
            reg_num_str = reg_name_u.substr(1);
            reg_num     = stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.gpr[reg_num] = val;
                return ppc_state.gpr[reg_num];
            }
        }

        if (reg_name_u.substr(0, 1) == "FR") {
            reg_num_str = reg_name_u.substr(2);
            reg_num     = stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.fpr[reg_num].int64_r = val;
                return ppc_state.fpr[reg_num].int64_r;
            }
        }

        if (reg_name_u.substr(0, 3) == "SPR") {
            reg_num_str = reg_name_u.substr(3);
            reg_num     = stoul(reg_num_str, NULL, 0);
            if (reg_num < 1024) {
                if (is_write)
                    ppc_state.spr[reg_num] = val;
                return ppc_state.spr[reg_num];
            }
        }

        if (reg_name_u.substr(0, 2) == "SR") {
            reg_num_str = reg_name_u.substr(2);
            reg_num     = stoul(reg_num_str, NULL, 0);
            if (reg_num < 16) {
                if (is_write)
                    ppc_state.sr[reg_num] = val;
                return ppc_state.sr[reg_num];
            }
        }

        spr = SPRName2Num.find(reg_name_u);
        if (spr != SPRName2Num.end()) {
            if (is_write)
                ppc_state.spr[spr->second] = val;
            return ppc_state.spr[spr->second];
        }
    } catch (...) {
    }

bail_out:
    throw std::invalid_argument(string("Unknown register ") + reg_name);
}

uint64_t get_reg(string reg_name) {
    return reg_op(reg_name, 0, false);
}

void set_reg(string reg_name, uint64_t val) {
    reg_op(reg_name, val, true);
}
