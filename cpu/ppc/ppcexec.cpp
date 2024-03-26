/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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
#include <iostream>
#include <map>
#include <setjmp.h>
#include <stdexcept>
#include <stdio.h>
#include <string>

using namespace std;
using namespace dppc_interpreter;

MemCtrlBase* mem_ctrl_instance = 0;

bool is_601 = false;

bool power_on = false;
Po_Cause power_off_reason = po_enter_debugger;

SetPRS ppc_state;

bool grab_return;
bool grab_breakpoint;

uint32_t ppc_cur_instruction;    // Current instruction for the PPC
uint32_t ppc_effective_address;
uint32_t ppc_next_instruction_address;    // Used for branching, setting up the NIA

#ifdef EXEC_FLAGS_ATOMIC
std::atomic<unsigned> exec_flags{0}; // execution control flags
#else
unsigned exec_flags; // FIXME: read by main thread ppc_main_opcode; written by audio dbdma DMAChannel::update_irq .. add_immediate_timer
#endif
bool int_pin = false; // interrupt request pin state: true - asserted
bool dec_exception_pending = false;

/* copy of local variable bb_start_la. Need for correct
   calculation of CPU cycles after setjmp that clobbers
   non-volatile local variables. */
uint32_t    glob_bb_start_la;

/* variables related to virtual time */
uint64_t g_icycles;
int      icnt_factor;

/* global variables related to the timebase facility */
uint64_t tbr_wr_timestamp;  // stores vCPU virtual time of the last TBR write
uint64_t rtc_timestamp;     // stores vCPU virtual time of the last RTC write
uint64_t tbr_wr_value;      // last value written to the TBR
uint32_t tbr_freq_ghz;      // TBR/RTC driving frequency in GHz expressed as a 32 bit fraction less than 1.0 (999.999999 MHz maximum).
uint64_t tbr_period_ns;     // TBR/RTC period in ns expressed as a 64 bit value with 32 fractional bits (<1 Hz minimum).
uint64_t timebase_counter;  // internal timebase counter
uint64_t dec_wr_timestamp;  // stores vCPU virtual time of the last DEC write
uint32_t dec_wr_value;      // last value written to the DEC register
uint32_t rtc_lo;            // MPC601 RTC lower, counts nanoseconds
uint32_t rtc_hi;            // MPC601 RTC upper, counts seconds

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
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_twi,       
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_mulli,     
    ppc_subfic,    power_dozi,    ppc_cmpli,     ppc_cmpi,      
    ppc_addic<RC0>,    ppc_addic<RC1>, 
    ppc_addi<SHFT0>,   ppc_addi<SHFT1>,     
    ppc_opcode16,      ppc_sc,
    ppc_opcode18,      ppc_opcode19<NOT601>,
    ppc_rlwimi,    ppc_rlwinm,    power_rlmi,    ppc_rlwnm,     
    ppc_ori<SHFT0>,    ppc_ori<SHFT1>, 
    ppc_xori<SHFT0>,   ppc_xori<SHFT1>,     
    ppc_andirc<SHFT0>, ppc_andirc<SHFT1>,
    ppc_illegalop,     ppc_opcode31,  
    ppc_lz<uint32_t>,  ppc_lzu<uint32_t>, 
    ppc_lz<uint8_t>,   ppc_lzu<uint8_t>,      
    ppc_st<uint32_t>,  ppc_stu<uint32_t>,
    ppc_st<uint8_t>,   ppc_stu<uint8_t>,
    ppc_lz<uint16_t>,  ppc_lzu<uint16_t>, 
    ppc_lha,           ppc_lhau,
    ppc_st<uint16_t>,  ppc_stu<uint16_t>,
    ppc_lmw,       ppc_stmw,      
    ppc_lfs,       ppc_lfsu,      ppc_lfd,       ppc_lfdu,
    ppc_stfs,      ppc_stfsu,     ppc_stfd,      ppc_stfdu,     
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_opcode59,
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_opcode63
};

/** Lookup tables for branch instructions. */
const static PPCOpcode SubOpcode16Grabber[] = {
    dppc_interpreter::ppc_bc<LK0, AA0>,     // bc
    dppc_interpreter::ppc_bc<LK1, AA0>,     // bcl
    dppc_interpreter::ppc_bc<LK0, AA1>,     // bca
    dppc_interpreter::ppc_bc<LK1, AA1>};    // bcla

const static PPCOpcode SubOpcode18Grabber[] = {
    dppc_interpreter::ppc_b<LK0, AA0>,      // b
    dppc_interpreter::ppc_b<LK1, AA0>,      // bl
    dppc_interpreter::ppc_b<LK0, AA1>,      // ba
    dppc_interpreter::ppc_b<LK1, AA1>};     // bla

/** Instructions decoding tables for integer,
    single floating-point, and double-floating point ops respectively */

PPCOpcode SubOpcode31Grabber[2048];
PPCOpcode SubOpcode59Grabber[64];
PPCOpcode SubOpcode63Grabber[2048];

/** Exception helpers. */

void ppc_illegalop() {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
}

void ppc_fpu_off() {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::FPU_OFF);
}

void ppc_assert_int() {
    int_pin = true;
    if (ppc_state.msr & MSR::EE) {
        LOG_F(5, "CPU ExtIntHandler called");
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
    } else {
        LOG_F(5, "CPU IRQ ignored!");
    }
}

void ppc_release_int() {
    int_pin = false;
}

/** Opcode decoding functions. */

void ppc_opcode16() {
    SubOpcode16Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode18() {
    SubOpcode18Grabber[ppc_cur_instruction & 3]();
}

template<bool for601>
void ppc_opcode19() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FF;

    switch (subop_grab) {
    case 0:
        ppc_mcrf();
        break;
    case 32:
        ppc_bclr<false>();
        break;
    case 33:
        ppc_bclr<true>();
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
    case 1056:
        if (for601)
            ppc_bcctr<RC0, IS601>();
        else
            ppc_bcctr<RC0, NOT601>();
        break;
    case 1057:
        if (for601)
            ppc_bcctr<RC1, IS601>();
        else
            ppc_bcctr<RC1, NOT601>();
        break;
    default:
        ppc_illegalop();
    }
}

template void ppc_opcode19<NOT601>();
template void ppc_opcode19<IS601>();

void ppc_opcode31() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FFUL;
    SubOpcode31Grabber[subop_grab]();
}

void ppc_opcode59() {
    uint16_t subop_grab = ppc_cur_instruction & 0x3FUL;
    SubOpcode59Grabber[subop_grab]();
}

void ppc_opcode63() {
    uint16_t subop_grab = ppc_cur_instruction & 0x7FFUL;
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
    uint64_t slice_ns = TimerManager::get_instance()->process_timers();
    if (slice_ns == 0) {
        // execute 10.000 cycles
        // if there are no pending timers
        return g_icycles + 10000;
    }
    return g_icycles + ((slice_ns + (1ULL << icnt_factor)) >> icnt_factor);
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
        page_start = eb_start & PAGE_MASK;
        eb_end     = page_start + PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {
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
                if (!(exec_flags & EXEF_RFI) && (eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    eb_end = page_start + PAGE_SIZE - 1;
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
    g_icycles++;
    process_events();

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
}

/** Execute PPC code until goal_addr is reached. */

// inner interpreter loop
static void ppc_exec_until_inner(const uint32_t goal_addr)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;

    max_cycles = 0;

    do {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PAGE_MASK;
        eb_end     = page_start + PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {
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
                if (!(exec_flags & EXEF_RFI) && (eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    eb_end = page_start + PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                ppc_set_cur_instruction(pc_real);
            }

            if (ppc_state.pc == goal_addr)
                break;
        }
    } while (power_on && ppc_state.pc != goal_addr);
}

// outer interpreter loop
void ppc_exec_until(volatile uint32_t goal_addr)
{
    if (setjmp(exc_env)) {
        // process low-level exceptions
        //LOG_F(9, "PPC-EXEC: low_level exception raised!");
        ppc_state.pc = ppc_next_instruction_address;
    }

    do {
        ppc_exec_until_inner(goal_addr);
    } while (power_on && ppc_state.pc != goal_addr);
}

/** Execute PPC code until control is reached the specified region. */

// inner interpreter loop
static void ppc_exec_dbg_inner(const uint32_t start_addr, const uint32_t size)
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint8_t* pc_real;

    max_cycles = 0;

    while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PAGE_MASK;
        eb_end     = page_start + PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);

        // interpret execution block
        while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)
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
                if (!(exec_flags & EXEF_RFI) && (eb_start & PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    ppc_set_cur_instruction(pc_real);
                } else {
                    page_start = eb_start & PAGE_MASK;
                    eb_end = page_start + PAGE_SIZE - 1;
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

    while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)) {
        ppc_exec_dbg_inner(start_addr, size);
    }
}

void initialize_ppc_opcode_tables() {
    std::fill_n(SubOpcode31Grabber, 2048, ppc_illegalop);
    SubOpcode31Grabber[0]  = ppc_cmp;
    SubOpcode31Grabber[8]  = ppc_tw;
    SubOpcode31Grabber[64] = ppc_cmpl;

    SubOpcode31Grabber[16]    = ppc_subf<CARRY1, RC0, OV0>;
    SubOpcode31Grabber[17]    = ppc_subf<CARRY1, RC1, OV0>;
    SubOpcode31Grabber[80]    = ppc_subf<CARRY0, RC0, OV0>;
    SubOpcode31Grabber[81]    = ppc_subf<CARRY0, RC1, OV0>;
    SubOpcode31Grabber[208]   = ppc_neg<RC0, OV0>;
    SubOpcode31Grabber[209]   = ppc_neg<RC1, OV0>;
    SubOpcode31Grabber[272]   = ppc_subfe<RC0, OV0>;
    SubOpcode31Grabber[273]   = ppc_subfe<RC1, OV0>;
    SubOpcode31Grabber[400]   = ppc_subfze<RC0, OV0>;
    SubOpcode31Grabber[401]   = ppc_subfze<RC1, OV0>;
    SubOpcode31Grabber[464]   = ppc_subfme<RC0, OV0>;
    SubOpcode31Grabber[465]   = ppc_subfme<RC1, OV0>;

    SubOpcode31Grabber[1040]  = ppc_subf<CARRY1, RC0, OV1>;
    SubOpcode31Grabber[1041]  = ppc_subf<CARRY1, RC1, OV1>;
    SubOpcode31Grabber[1104]  = ppc_subf<CARRY0, RC0, OV1>;
    SubOpcode31Grabber[1105]  = ppc_subf<CARRY0, RC1, OV1>;
    SubOpcode31Grabber[1232]  = ppc_neg<RC0, OV1>;
    SubOpcode31Grabber[1233]  = ppc_neg<RC1, OV1>;
    SubOpcode31Grabber[1296]  = ppc_subfe<RC0, OV1>;
    SubOpcode31Grabber[1297]  = ppc_subfe<RC1, OV1>;
    SubOpcode31Grabber[1424]  = ppc_subfze<RC0, OV1>;
    SubOpcode31Grabber[1425]  = ppc_subfze<RC1, OV1>;
    SubOpcode31Grabber[1488]  = ppc_subfme<RC0, OV1>;
    SubOpcode31Grabber[1489]  = ppc_subfme<RC1, OV1>;

    SubOpcode31Grabber[20]   = ppc_add<CARRY1, RC0, OV0>;
    SubOpcode31Grabber[21]   = ppc_add<CARRY1, RC1, OV0>;
    SubOpcode31Grabber[276]  = ppc_adde<RC0, OV0>;
    SubOpcode31Grabber[277]  = ppc_adde<RC1, OV0>;
    SubOpcode31Grabber[404]  = ppc_addze<RC0, OV0>;
    SubOpcode31Grabber[405]  = ppc_addze<RC1, OV0>;
    SubOpcode31Grabber[468]  = ppc_addme<RC0, OV0>;
    SubOpcode31Grabber[469]  = ppc_addme<RC1, OV0>;
    SubOpcode31Grabber[532]  = ppc_add<CARRY0, RC0, OV0>;
    SubOpcode31Grabber[533]  = ppc_add<CARRY0, RC1, OV0>;

    SubOpcode31Grabber[1044] = ppc_add<CARRY1, RC0, OV1>;
    SubOpcode31Grabber[1045] = ppc_add<CARRY1, RC1, OV1>;
    SubOpcode31Grabber[1300] = ppc_adde<RC0, OV1>;
    SubOpcode31Grabber[1301] = ppc_adde<RC1, OV1>;
    SubOpcode31Grabber[1428] = ppc_addze<RC0, OV1>;
    SubOpcode31Grabber[1429] = ppc_addze<RC1, OV1>;
    SubOpcode31Grabber[1492] = ppc_addme<RC0, OV1>;
    SubOpcode31Grabber[1493] = ppc_addme<RC1, OV1>;
    SubOpcode31Grabber[1556] = ppc_add<CARRY0, RC0, OV1>;
    SubOpcode31Grabber[1557] = ppc_add<CARRY0, RC1, OV1>;

    SubOpcode31Grabber[22]   = ppc_mulhwu<RC0>;
    SubOpcode31Grabber[23]   = ppc_mulhwu<RC1>;
    SubOpcode31Grabber[150]  = ppc_mulhw<RC0>;
    SubOpcode31Grabber[151]  = ppc_mulhw<RC1>;
    SubOpcode31Grabber[470]  = ppc_mullw<RC0, OV0>;
    SubOpcode31Grabber[471]  = ppc_mullw<RC1, OV0>;
    SubOpcode31Grabber[918]  = ppc_divwu<RC0, OV0>;
    SubOpcode31Grabber[919]  = ppc_divwu<RC1, OV0>;
    SubOpcode31Grabber[982]  = ppc_divw<RC0, OV0>;
    SubOpcode31Grabber[983]  = ppc_divw<RC1, OV0>;

    SubOpcode31Grabber[1494] = ppc_mullw<RC0, OV1>;
    SubOpcode31Grabber[1495] = ppc_mullw<RC1, OV1>;
    SubOpcode31Grabber[1942] = ppc_divwu<RC0, OV1>;
    SubOpcode31Grabber[1943] = ppc_divwu<RC1, OV1>;
    SubOpcode31Grabber[2006] = ppc_divw<RC0, OV1>;
    SubOpcode31Grabber[2007] = ppc_divw<RC1, OV1>;

    SubOpcode31Grabber[40]   = ppc_lwarx;
    SubOpcode31Grabber[46]   = ppc_lzx<uint32_t>;
    SubOpcode31Grabber[110]  = ppc_lzux<uint32_t>;
    SubOpcode31Grabber[174]  = ppc_lzx<uint8_t>;
    SubOpcode31Grabber[238]  = ppc_lzux<uint8_t>;
    SubOpcode31Grabber[558]  = ppc_lzx<uint16_t>;
    SubOpcode31Grabber[622]  = ppc_lzux<uint16_t>;
    SubOpcode31Grabber[686]  = ppc_lhax;
    SubOpcode31Grabber[750]  = ppc_lhaux;
    SubOpcode31Grabber[1066] = ppc_lswx;
    SubOpcode31Grabber[1068] = ppc_lwbrx;
    SubOpcode31Grabber[1070] = ppc_lfsx;
    SubOpcode31Grabber[1134] = ppc_lfsux;
    SubOpcode31Grabber[1194] = ppc_lswi;
    SubOpcode31Grabber[1198] = ppc_lfdx;
    SubOpcode31Grabber[1262] = ppc_lfdux;
    SubOpcode31Grabber[1580] = ppc_lhbrx;

    SubOpcode31Grabber[301]  = ppc_stwcx;
    SubOpcode31Grabber[302]  = ppc_stx<uint32_t>;
    SubOpcode31Grabber[366]  = ppc_stux<uint32_t>;
    SubOpcode31Grabber[430]  = ppc_stx<uint8_t>;
    SubOpcode31Grabber[494]  = ppc_stux<uint8_t>;
    SubOpcode31Grabber[814]  = ppc_stx<uint16_t>;
    SubOpcode31Grabber[878]  = ppc_stux<uint16_t>;
    SubOpcode31Grabber[1322] = ppc_stswx;
    SubOpcode31Grabber[1324] = ppc_stwbrx;
    SubOpcode31Grabber[1326] = ppc_stfsx;
    SubOpcode31Grabber[1390] = ppc_stfsux;
    SubOpcode31Grabber[1450] = ppc_stswi;
    SubOpcode31Grabber[1454] = ppc_stfdx;
    SubOpcode31Grabber[1518] = ppc_stfdux;
    SubOpcode31Grabber[1836] = ppc_sthbrx;
    SubOpcode31Grabber[1966] = ppc_stfiwx;

    SubOpcode31Grabber[620] = ppc_eciwx;
    SubOpcode31Grabber[876] = ppc_ecowx;

    SubOpcode31Grabber[48]   = ppc_shift<SHFT1, RC0>;
    SubOpcode31Grabber[49]   = ppc_shift<SHFT1, RC1>;
    SubOpcode31Grabber[56]   = ppc_do_bool<bool_and, RC0>;
    SubOpcode31Grabber[57]   = ppc_do_bool<bool_and, RC1>;
    SubOpcode31Grabber[120]  = ppc_do_bool<bool_andc, RC0>;
    SubOpcode31Grabber[121]  = ppc_do_bool<bool_andc, RC1>;
    SubOpcode31Grabber[248]  = ppc_do_bool<bool_nor, RC0>;
    SubOpcode31Grabber[249]  = ppc_do_bool<bool_nor, RC1>;
    SubOpcode31Grabber[568]  = ppc_do_bool<bool_eqv, RC0>;
    SubOpcode31Grabber[569]  = ppc_do_bool<bool_eqv, RC1>;
    SubOpcode31Grabber[632]  = ppc_do_bool<bool_xor, RC0>;
    SubOpcode31Grabber[633]  = ppc_do_bool<bool_xor, RC1>;
    SubOpcode31Grabber[824]  = ppc_do_bool<bool_orc, RC0>;
    SubOpcode31Grabber[825]  = ppc_do_bool<bool_orc, RC1>;
    SubOpcode31Grabber[888]  = ppc_do_bool<bool_or, RC0>;
    SubOpcode31Grabber[889]  = ppc_do_bool<bool_or, RC1>;
    SubOpcode31Grabber[952]  = ppc_do_bool<bool_nand, RC0>;
    SubOpcode31Grabber[953]  = ppc_do_bool<bool_nand, RC1>;
    SubOpcode31Grabber[1072] = ppc_shift<SHFT0, RC0>;
    SubOpcode31Grabber[1073] = ppc_shift<SHFT0, RC1>;
    SubOpcode31Grabber[1584] = ppc_sraw<RC0>;
    SubOpcode31Grabber[1585] = ppc_sraw<RC1>;
    SubOpcode31Grabber[1648] = ppc_srawi<RC0>;
    SubOpcode31Grabber[1649] = ppc_srawi<RC1>;
    SubOpcode31Grabber[1844] = ppc_exts<int16_t, RC0>;
    SubOpcode31Grabber[1845] = ppc_exts<int16_t, RC1>;
    SubOpcode31Grabber[1908] = ppc_exts<int8_t, RC0>;
    SubOpcode31Grabber[1909] = ppc_exts<int8_t, RC1>;

    SubOpcode31Grabber[52] = ppc_cntlzw<RC0>;
    SubOpcode31Grabber[53] = ppc_cntlzw<RC1>;

    SubOpcode31Grabber[38]   = ppc_mfcr;
    SubOpcode31Grabber[166]  = ppc_mfmsr;
    SubOpcode31Grabber[288]  = ppc_mtcrf;
    SubOpcode31Grabber[292]  = ppc_mtmsr;
    SubOpcode31Grabber[420]  = ppc_mtsr;
    SubOpcode31Grabber[484]  = ppc_mtsrin;
    SubOpcode31Grabber[678]  = ppc_mfspr;
    SubOpcode31Grabber[742]  = ppc_mftb;
    SubOpcode31Grabber[934]  = ppc_mtspr;
    SubOpcode31Grabber[1024] = ppc_mcrxr;
    SubOpcode31Grabber[1190] = ppc_mfsr;
    SubOpcode31Grabber[1318] = ppc_mfsrin;

    SubOpcode31Grabber[108]  = ppc_dcbst;
    SubOpcode31Grabber[172]  = ppc_dcbf;
    SubOpcode31Grabber[492]  = ppc_dcbtst;
    SubOpcode31Grabber[556]  = ppc_dcbt;
    SubOpcode31Grabber[940]  = ppc_dcbi;
    SubOpcode31Grabber[1196] = ppc_sync;
    SubOpcode31Grabber[2028] = ppc_dcbz;

    SubOpcode31Grabber[58]   = power_maskg<RC0>;
    SubOpcode31Grabber[59]   = power_maskg<RC1>;
    SubOpcode31Grabber[214]  = power_mul<RC0, OV0>;
    SubOpcode31Grabber[215]  = power_mul<RC1, OV0>;
    SubOpcode31Grabber[304]  = power_slq<RC0>;
    SubOpcode31Grabber[305]  = power_slq<RC1>;
    SubOpcode31Grabber[306]  = power_sle<RC0>;
    SubOpcode31Grabber[306]  = power_sle<RC1>;
    SubOpcode31Grabber[368]  = power_sliq<RC0>;
    SubOpcode31Grabber[369]  = power_sliq<RC1>;
    SubOpcode31Grabber[432]  = power_sllq<RC0>;
    SubOpcode31Grabber[433]  = power_sllq<RC1>;
    SubOpcode31Grabber[434]  = power_sleq<RC0>;
    SubOpcode31Grabber[435]  = power_sleq<RC1>;
    SubOpcode31Grabber[496]  = power_slliq<RC0>;
    SubOpcode31Grabber[497]  = power_slliq<RC1>;
    SubOpcode31Grabber[528]  = power_doz<RC0, OV0>;
    SubOpcode31Grabber[529]  = power_doz<RC1, OV0>;
    SubOpcode31Grabber[554]  = power_lscbx<RC0>;
    SubOpcode31Grabber[555]  = power_lscbx<RC1>;
    SubOpcode31Grabber[662]  = power_div<RC0, OV0>;
    SubOpcode31Grabber[663]  = power_div<RC1, OV0>;
    SubOpcode31Grabber[720]  = power_abs<RC0, OV0>;
    SubOpcode31Grabber[721]  = power_abs<RC1, OV0>;
    SubOpcode31Grabber[726]  = power_divs<RC0, OV0>;
    SubOpcode31Grabber[727]  = power_divs<RC1, OV0>;
    SubOpcode31Grabber[976]  = power_nabs<RC0, OV0>;
    SubOpcode31Grabber[977]  = power_nabs<RC1, OV0>;
    SubOpcode31Grabber[1062] = power_clcs;
    SubOpcode31Grabber[1074] = power_rrib<RC0>;
    SubOpcode31Grabber[1075] = power_rrib<RC1>;
    SubOpcode31Grabber[1082] = power_maskir<RC0>;
    SubOpcode31Grabber[1083] = power_maskir<RC1>;
    SubOpcode31Grabber[1328] = power_srq<RC0>;
    SubOpcode31Grabber[1329] = power_srq<RC1>;
    SubOpcode31Grabber[1330] = power_sre<RC0>;
    SubOpcode31Grabber[1331] = power_sre<RC1>;
    SubOpcode31Grabber[1332] = power_sriq<RC0>;
    SubOpcode31Grabber[1333] = power_sriq<RC1>;
    SubOpcode31Grabber[1456] = power_srlq<RC0>;
    SubOpcode31Grabber[1457] = power_srlq<RC1>;
    SubOpcode31Grabber[1458] = power_sreq<RC0>;
    SubOpcode31Grabber[1459] = power_sreq<RC1>;
    SubOpcode31Grabber[1520] = power_srliq<RC0>;
    SubOpcode31Grabber[1521] = power_srliq<RC1>;
    SubOpcode31Grabber[1840] = power_sraq<RC0>;
    SubOpcode31Grabber[1841] = power_sraq<RC1>;
    SubOpcode31Grabber[1842] = power_srea<RC0>;
    SubOpcode31Grabber[1843] = power_srea<RC1>;
    SubOpcode31Grabber[1904] = power_sraiq<RC0>;
    SubOpcode31Grabber[1905] = power_sraiq<RC1>;

    SubOpcode31Grabber[1238] = power_mul<RC0, OV1>;
    SubOpcode31Grabber[1239] = power_mul<RC1, OV1>;
    SubOpcode31Grabber[1552] = power_doz<RC0, OV1>;
    SubOpcode31Grabber[1553] = power_doz<RC1, OV1>;
    SubOpcode31Grabber[1686] = power_div<RC0, OV1>;
    SubOpcode31Grabber[1687] = power_div<RC1, OV1>;
    SubOpcode31Grabber[1744] = power_abs<RC0, OV1>;
    SubOpcode31Grabber[1745] = power_abs<RC1, OV1>;
    SubOpcode31Grabber[1750] = power_divs<RC0, OV1>;
    SubOpcode31Grabber[1751] = power_divs<RC1, OV1>;
    SubOpcode31Grabber[2000] = power_nabs<RC0, OV1>;
    SubOpcode31Grabber[2001] = power_nabs<RC1, OV1>;

    SubOpcode31Grabber[612]  = ppc_tlbie;
    SubOpcode31Grabber[740]  = ppc_tlbia;
    SubOpcode31Grabber[1132] = ppc_tlbsync;
    SubOpcode31Grabber[1708] = ppc_eieio;
    SubOpcode31Grabber[1964] = ppc_icbi;
    SubOpcode31Grabber[1956] = ppc_tlbld;
    SubOpcode31Grabber[2020] = ppc_tlbli;

    std::fill_n(SubOpcode59Grabber, 64, ppc_illegalop);
    SubOpcode59Grabber[36] = ppc_fdivs<RC0>;
    SubOpcode59Grabber[37] = ppc_fdivs<RC1>;
    SubOpcode59Grabber[40] = ppc_fsubs<RC0>;
    SubOpcode59Grabber[41] = ppc_fsubs<RC1>;
    SubOpcode59Grabber[42] = ppc_fadds<RC0>;
    SubOpcode59Grabber[43] = ppc_fadds<RC1>;
    SubOpcode59Grabber[44] = ppc_fsqrts<RC0>;
    SubOpcode59Grabber[45] = ppc_fsqrts<RC1>;
    SubOpcode59Grabber[48] = ppc_fres<RC0>;
    SubOpcode59Grabber[49] = ppc_fres<RC1>;
    SubOpcode59Grabber[50] = ppc_fmuls<RC0>;
    SubOpcode59Grabber[51] = ppc_fmuls<RC1>;
    SubOpcode59Grabber[56] = ppc_fmsubs<RC0>;
    SubOpcode59Grabber[57] = ppc_fmsubs<RC1>;
    SubOpcode59Grabber[58] = ppc_fmadds<RC0>;
    SubOpcode59Grabber[59] = ppc_fmadds<RC1>;
    SubOpcode59Grabber[60] = ppc_fnmsubs<RC0>;
    SubOpcode59Grabber[61] = ppc_fnmsubs<RC1>;
    SubOpcode59Grabber[62] = ppc_fnmadds<RC0>;
    SubOpcode59Grabber[63] = ppc_fnmadds<RC1>;

    std::fill_n(SubOpcode63Grabber, 2048, ppc_illegalop);
    SubOpcode63Grabber[0]    = ppc_fcmpu;
    SubOpcode63Grabber[24]   = ppc_frsp<RC0>;
    SubOpcode63Grabber[25]   = ppc_frsp<RC1>;
    SubOpcode63Grabber[28]   = ppc_fctiw<RC0>;
    SubOpcode63Grabber[29]   = ppc_fctiw<RC1>;
    SubOpcode63Grabber[30]   = ppc_fctiwz<RC0>;
    SubOpcode63Grabber[31]   = ppc_fctiwz<RC1>;
    SubOpcode63Grabber[36]   = ppc_fdiv<RC0>;
    SubOpcode63Grabber[37]   = ppc_fdiv<RC1>;
    SubOpcode63Grabber[40]   = ppc_fsub<RC0>;
    SubOpcode63Grabber[41]   = ppc_fsub<RC1>;
    SubOpcode63Grabber[42]   = ppc_fadd<RC0>;
    SubOpcode63Grabber[43]   = ppc_fadd<RC1>;
    SubOpcode63Grabber[44]   = ppc_fsqrt<RC0>;
    SubOpcode63Grabber[45]   = ppc_fsqrt<RC1>;
    SubOpcode63Grabber[52]   = ppc_frsqrte<RC0>;
    SubOpcode63Grabber[53]   = ppc_frsqrte<RC1>;
    SubOpcode63Grabber[64]   = ppc_fcmpo;
    SubOpcode63Grabber[76]   = ppc_mtfsb1<RC0>;
    SubOpcode63Grabber[77]   = ppc_mtfsb1<RC1>;
    SubOpcode63Grabber[80]   = ppc_fneg<RC0>;
    SubOpcode63Grabber[81]   = ppc_fneg<RC1>;
    SubOpcode63Grabber[128]  = ppc_mcrfs;
    SubOpcode63Grabber[140]  = ppc_mtfsb0<RC0>;
    SubOpcode63Grabber[141]  = ppc_mtfsb0<RC1>;
    SubOpcode63Grabber[144]  = ppc_fmr<RC0>;
    SubOpcode63Grabber[145]  = ppc_fmr<RC1>;
    SubOpcode63Grabber[268]  = ppc_mtfsfi<RC0>;
    SubOpcode63Grabber[269]  = ppc_mtfsfi<RC1>;
    SubOpcode63Grabber[272]  = ppc_fnabs<RC0>;
    SubOpcode63Grabber[273]  = ppc_fnabs<RC1>;
    SubOpcode63Grabber[528]  = ppc_fabs<RC0>;
    SubOpcode63Grabber[529]  = ppc_fabs<RC1>;
    SubOpcode63Grabber[1166] = ppc_mffs<NOT601, RC0>;
    SubOpcode63Grabber[1167] = ppc_mffs<NOT601, RC1>;
    SubOpcode63Grabber[1422] = ppc_mtfsf<RC0>;
    SubOpcode63Grabber[1423] = ppc_mtfsf<RC1>;

    for (int i = 0; i < 2048; i += 64) {
        SubOpcode63Grabber[i + 46] = ppc_fsel<RC0>;
        SubOpcode63Grabber[i + 47] = ppc_fsel<RC1>;
        SubOpcode63Grabber[i + 50] = ppc_fmul<RC0>;
        SubOpcode63Grabber[i + 51] = ppc_fmul<RC1>;
        SubOpcode63Grabber[i + 56] = ppc_fmsub<RC0>;
        SubOpcode63Grabber[i + 57] = ppc_fmsub<RC1>;
        SubOpcode63Grabber[i + 58] = ppc_fmadd<RC0>;
        SubOpcode63Grabber[i + 59] = ppc_fmadd<RC1>;
        SubOpcode63Grabber[i + 60] = ppc_fnmsub<RC0>;
        SubOpcode63Grabber[i + 61] = ppc_fnmsub<RC1>;
        SubOpcode63Grabber[i + 62] = ppc_fnmadd<RC0>;
        SubOpcode63Grabber[i + 63] = ppc_fnmadd<RC1>;
    }
}

void ppc_fpu_init() {
    // zero all FPRs as prescribed for MPC601
    // For later PPC CPUs, GPR content is undefined
    for (int i = 0; i < 32; i++) {
        ppc_state.fpr[i].int64_r = 0;
    }

    ppc_state.fpscr = 0;
    set_host_rounding_mode(0);
}

void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, uint64_t tb_freq)
{
    int i;

    mem_ctrl_instance = mem_ctrl;

    initialize_ppc_opcode_tables();

    if (cpu_version == PPC_VER::MPC601) {
        OpcodeGrabber[19]        = ppc_opcode19<IS601>;
        SubOpcode31Grabber[740]  = ppc_illegalop; // tlbia
        SubOpcode31Grabber[742]  = ppc_illegalop; // mftb
        SubOpcode59Grabber[48]   = ppc_illegalop; // fres
        SubOpcode63Grabber[52]   = ppc_illegalop; // frsqrte
        SubOpcode63Grabber[1166] = ppc_mffs<IS601, RC0>;
        SubOpcode63Grabber[1167] = ppc_mffs<IS601, RC1>;
        for (int i = 0; i < 2048; i += 64) {
            SubOpcode63Grabber[i + 46] = ppc_illegalop; // fsel
            SubOpcode63Grabber[i + 47] = ppc_illegalop; // fsel.
        }
    }
    if (cpu_version != PPC_VER::MPC970MP) {
        SubOpcode59Grabber[44] = ppc_illegalop; // fsqrts
        SubOpcode59Grabber[45] = ppc_illegalop; // fsqrts.
        SubOpcode63Grabber[44] = ppc_illegalop; // fsqrt
        SubOpcode63Grabber[45] = ppc_illegalop; // fsqrt.
    }

    // initialize emulator timers
    TimerManager::get_instance()->set_time_now_cb(&get_virt_time_ns);
    TimerManager::get_instance()->set_notify_changes_cb(&force_cycle_counter_reload);

    // initialize time base facility
    g_icycles   = 0;
    icnt_factor = 4;
    tbr_wr_timestamp = 0;
    rtc_timestamp = 0;
    tbr_wr_value = 0;
    tbr_freq_ghz = (tb_freq << 32) / NS_PER_SEC;
    tbr_period_ns = ((uint64_t)NS_PER_SEC << 32) / tb_freq;

    exec_flags = 0;

    timebase_counter = 0;
    dec_wr_value = 0;

    /* zero all GPRs as prescribed for MPC601 */
    /* For later PPC CPUs, GPR content is undefined */
    for (i = 0; i < 32; i++) {
        ppc_state.gpr[i] = 0;
    }

    /* zero all segment registers as prescribed for MPC601 */
    /* For later PPC CPUs, SR content is undefined */
    for (i = 0; i < 16; i++) {
        ppc_state.sr[i] = 0;
    }

    ppc_state.cr    = 0;

    ppc_fpu_init();

    ppc_state.pc = 0;

    ppc_state.tbr[0] = 0;
    ppc_state.tbr[1] = 0;

    /* zero all SPRs */
    for (i = 0; i < 1024; i++) {
        ppc_state.spr[i] = 0;
    }

    ppc_state.spr[SPR::PVR] = cpu_version;
    is_601 = (cpu_version >> 16) == 1;

    if (is_601) {
        /* MPC601 sets MSR[ME] bit during hard reset / Power-On */
        ppc_state.msr = (MSR::ME + MSR::IP);
    } else {
        ppc_state.msr           = MSR::IP;
        ppc_state.spr[SPR::DEC] = 0xFFFFFFFFUL;
    }

    ppc_mmu_init();

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
    {"XER", SPR::XER},          {"LR",     SPR::LR},    {"CTR",    SPR::CTR},
    {"DEC", SPR::DEC},          {"PVR",    SPR::PVR},   {"SPRG0",  SPR::SPRG0},
    {"SPRG1", SPR::SPRG1},      {"SPRG2",  SPR::SPRG2}, {"SPRG3",  SPR::SPRG3},
    {"SRR0", SPR::SRR0},        {"SRR1",   SPR::SRR1},  {"IBAT0U", 528},
    {"IBAT0L", 529},            {"IBAT1U", 530},        {"IBAT1L", 531},
    {"IBAT2U", 532},            {"IBAT2L", 533},        {"IBAT3U", 534},
    {"IBAT3L", 535},            {"DBAT0U", 536},        {"DBAT0L", 537},
    {"DBAT1U", 538},            {"DBAT1L", 539},        {"DBAT2U", 540},
    {"DBAT2L", 541},            {"DBAT3U", 542},        {"DBAT3L", 543},
    {"HID0",   SPR::HID0},      {"HID1",   SPR::HID1},  {"IABR",   1010},
    {"DABR",   1013},           {"L2CR",   1017},       {"ICTC",   1019},
    {"THRM1",  1020},           {"THRM2",  1021},       {"THRM3",  1022},
    {"PIR",    1023},           {"TBL",    SPR::TBL_U}, {"TBU",    SPR::TBU_U},
    {"SDR1",   SPR::SDR1},      {"MQ",     SPR::MQ},    {"RTCU",   SPR::RTCU_U},
    {"RTCL",   SPR::RTCL_U},    {"DSISR",  SPR::DSISR}, {"DAR",    SPR::DAR},
    {"MMCR0",  SPR::MMCR0},     {"PMC1",   SPR::PMC1},  {"PMC2",   SPR::PMC2},
    {"SDA",    SPR::SDA},       {"SIA",    SPR::SIA},   {"MMCR1",  SPR::MMCR1}
};

uint64_t reg_op(string& reg_name, uint64_t val, bool is_write) {
    string reg_name_u, reg_num_str;
    unsigned reg_num;
    map<string, int>::iterator spr;

    if (reg_name.length() < 2)
        goto bail_out;

    reg_name_u = reg_name;

    /* convert reg_name string to uppercase */
    std::for_each(reg_name_u.begin(), reg_name_u.end(), [](char& c) {
        c = ::toupper(c);
    });

    try {
        if (reg_name_u == "PC") {
            if (is_write)
                ppc_state.pc = (uint32_t)val;
            return ppc_state.pc;
        }
        if (reg_name_u == "MSR") {
            if (is_write)
                ppc_state.msr = (uint32_t)val;
            return ppc_state.msr;
        }
        if (reg_name_u == "CR") {
            if (is_write)
                ppc_state.cr = (uint32_t)val;
            return ppc_state.cr;
        }
        if (reg_name_u == "FPSCR") {
            if (is_write)
                ppc_state.fpscr = (uint32_t)val;
            return ppc_state.fpscr;
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 1) == "R") {
            reg_num_str = reg_name_u.substr(1);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.gpr[reg_num] = (uint32_t)val;
                return ppc_state.gpr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 1) == "F") {
            reg_num_str = reg_name_u.substr(1);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 32) {
                if (is_write)
                    ppc_state.fpr[reg_num].int64_r = val;
                return ppc_state.fpr[reg_num].int64_r;
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 3) == "SPR") {
            reg_num_str = reg_name_u.substr(3);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 1024) {
                if (is_write)
                    ppc_state.spr[reg_num] = (uint32_t)val;
                return ppc_state.spr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        if (reg_name_u.substr(0, 2) == "SR") {
            reg_num_str = reg_name_u.substr(2);
            reg_num     = (unsigned)stoul(reg_num_str, NULL, 0);
            if (reg_num < 16) {
                if (is_write)
                    ppc_state.sr[reg_num] = (uint32_t)val;
                return ppc_state.sr[reg_num];
            }
        }
    } catch (...) {
    }

    try {
        spr = SPRName2Num.find(reg_name_u);
        if (spr != SPRName2Num.end()) {
            if (is_write)
                ppc_state.spr[spr->second] = (uint32_t)val;
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
