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
#include "ppcdisasm.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <setjmp.h>
#include <stdexcept>
#include <stdio.h>
#include <string>

#ifdef __APPLE__
#include <mach/mach_time.h>
#undef EXC_SYSCALL
static struct mach_timebase_info timebase_info;
static uint64_t
ConvertHostTimeToNanos2(uint64_t host_time)
{
    if (timebase_info.numer == timebase_info.denom)
        return host_time;
    long double answer = host_time;
    answer *= timebase_info.numer;
    answer /= timebase_info.denom;
    return (uint64_t)answer;
}
#endif

using namespace std;
using namespace dppc_interpreter;

MemCtrlBase* mem_ctrl_instance = 0;

bool is_601 = false;
bool include_601 = false;

bool is_deterministic = false;

bool power_on = false;
Po_Cause power_off_reason = po_enter_debugger;

SetPRS ppc_state;

uint32_t ppc_next_instruction_address;    // Used for branching, setting up the NIA

unsigned exec_flags; // execution control flags
// FIXME: exec_timer is read by main thread ppc_main_opcode;
// written by audio dbdma DMAChannel::update_irq .. add_immediate_timer
volatile bool exec_timer;
bool int_pin = false; // interrupt request pin state: true - asserted
bool dec_exception_pending = false;

/* copy of local variable bb_start_la. Need for correct
   calculation of CPU cycles after setjmp that clobbers
   non-volatile local variables. */
uint32_t    glob_bb_start_la;

/* variables related to virtual time */
const bool g_realtime = false;
uint64_t g_nanoseconds_base;
uint64_t g_icycles;
int      icnt_factor;

/* global variables related to the timebase facility */
uint64_t tbr_wr_timestamp;  // stores vCPU virtual time of the last TBR write
uint64_t rtc_timestamp;     // stores vCPU virtual time of the last RTC write
uint64_t tbr_wr_value;      // last value written to the TBR
uint32_t tbr_freq_ghz;      // TBR/RTC driving frequency in GHz expressed as a
                            // 32 bit fraction less than 1.0 (999.999999 MHz maximum).
uint64_t tbr_period_ns;     // TBR/RTC period in ns expressed as a 64 bit value
                            // with 32 fractional bits (<1 Hz minimum).
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
#ifdef CPU_PROFILING_OPS
std::unordered_map<uint32_t, uint64_t> num_opcodes;
#endif

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

        // Generate top N op counts with readable names.
#ifdef CPU_PROFILING_OPS
        PPCDisasmContext ctx;
        ctx.instr_addr = 0;
        ctx.simplified = false;
        std::vector<std::pair<std::string, uint64_t>> op_name_counts;
        for (const auto& pair : num_opcodes) {
            ctx.instr_code = pair.first;
            auto op_name = disassemble_single(&ctx);
            op_name_counts.emplace_back(op_name, pair.second);
        }
        size_t top_ops_size = std::min(op_name_counts.size(), size_t(20));
        std::partial_sort(op_name_counts.begin(), op_name_counts.begin() + top_ops_size, op_name_counts.end(), [](const auto& a, const auto& b) {
            return b.second < a.second;
        });
        op_name_counts.resize(top_ops_size);
        for (const auto& pair : op_name_counts) {
            vars.push_back({.name = "Instruction " + pair.first,
                            .format = ProfileVarFmt::COUNT,
                            .value = pair.second,
                            .count_total = num_executed_instrs});
        }
#endif
    };

    void reset() {
        num_executed_instrs = 0;
        num_supervisor_instrs = 0;
        num_int_loads = 0;
        num_int_stores = 0;
        exceptions_processed = 0;
#ifdef CPU_PROFILING_OPS
        num_opcodes.clear();
#endif
    };
};

#endif

/** Opcode lookup table, indexed by
    primary opcode (bits 0...5) and modifier (bits 21...31). */
static PPCOpcode OpcodeGrabber[64 * 2048];

/** Exception helpers. */

void ppc_illegalop(uint32_t opcode) {
    ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
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

/* Dispatch using primary and modifier opcode */
void ppc_main_opcode(uint32_t opcode)
{
#ifdef CPU_PROFILING
    num_executed_instrs++;
#if defined(CPU_PROFILING_OPS)
    num_opcodes[opcode]++;
#endif
#endif
    OpcodeGrabber[(opcode >> 15 & 0x1F800) | (opcode & 0x7FF)](opcode);
}

static long long cpu_now_ns() {
#ifdef __APPLE__
    return ConvertHostTimeToNanos2(mach_absolute_time());
#else
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
#endif
}

uint64_t get_virt_time_ns()
{
    if (g_realtime) {
        return cpu_now_ns() - g_nanoseconds_base;
    } else {
        return g_icycles << icnt_factor;
    }
}

static uint64_t process_events()
{
    exec_timer = false;
    uint64_t slice_ns = TimerManager::get_instance()->process_timers();
    if (slice_ns == 0) {
        // execute 25.000 cycles
        // if there are no pending timers
        return g_icycles + 25000;
    }
    return g_icycles + (slice_ns >> icnt_factor) + 1;
}

static void force_cycle_counter_reload()
{
    // tell the interpreter loop to reload cycle counter
    exec_timer = true;
}

/** Execute PPC code as long as power is on. */
// inner interpreter loop
static void ppc_exec_inner()
{
    uint64_t max_cycles;
    uint32_t page_start, eb_start, eb_end;
    uint32_t opcode;
    uint8_t* pc_real;

    max_cycles = 0;

    while (power_on) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);
        opcode = ppc_read_instruction(pc_real);

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {
            ppc_main_opcode(opcode);
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    opcode = ppc_read_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start);
                    opcode = ppc_read_instruction(pc_real);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                opcode = ppc_read_instruction(pc_real);
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

    uint8_t* pc_real = mmu_translate_imem(ppc_state.pc);
    uint32_t opcode = ppc_read_instruction(pc_real);
    ppc_main_opcode(opcode);
    g_icycles++;
    process_events();

    if (exec_flags) {
        ppc_state.pc = ppc_next_instruction_address;
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
    uint32_t opcode;

    max_cycles = 0;

    do {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);
        opcode = ppc_read_instruction(pc_real);

        // interpret execution block
        while (power_on && ppc_state.pc < eb_end) {
            ppc_main_opcode(opcode);
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    opcode = ppc_read_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start);
                    opcode = ppc_read_instruction(pc_real);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                opcode = ppc_read_instruction(pc_real);
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
    uint32_t opcode;

    max_cycles = 0;

    while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)) {
        // define boundaries of the next execution block
        // max execution block length = one memory page
        eb_start   = ppc_state.pc;
        page_start = eb_start & PPC_PAGE_MASK;
        eb_end     = page_start + PPC_PAGE_SIZE - 1;
        exec_flags = 0;

        pc_real    = mmu_translate_imem(eb_start);
        opcode = ppc_read_instruction(pc_real);

        // interpret execution block
        while (power_on && (ppc_state.pc < start_addr || ppc_state.pc >= start_addr + size)
                && (ppc_state.pc < eb_end)) {
            ppc_main_opcode(opcode);
            if (g_icycles++ >= max_cycles || exec_timer) {
                max_cycles = process_events();
            }

            if (exec_flags) {
                // define next execution block
                eb_start = ppc_next_instruction_address;
                if (!(exec_flags & EXEF_RFI) && (eb_start & PPC_PAGE_MASK) == page_start) {
                    pc_real += (int)eb_start - (int)ppc_state.pc;
                    opcode = ppc_read_instruction(pc_real);
                } else {
                    page_start = eb_start & PPC_PAGE_MASK;
                    eb_end = page_start + PPC_PAGE_SIZE - 1;
                    pc_real = mmu_translate_imem(eb_start);
                    opcode = ppc_read_instruction(pc_real);
                }
                ppc_state.pc = eb_start;
                exec_flags = 0;
            } else {
                ppc_state.pc += 4;
                pc_real += 4;
                opcode = ppc_read_instruction(pc_real);
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

/*
Opcode table macros:
- d is for dot (RC).
- o is for overflow (OV).
- c is for carry CARRY0/CARRY1. It also works for other options:
  SHFT0/SHFT1, RIGHT0/LEFT1, uint8_t/uint16_t/uint32_t, and int8_t/int16_t.
- a is for address mode (AA).
- l is for link register (LK).
- r is for raw (adding custom entries to the table)
 */

#define OP(opcode, fn) \
do { \
for (uint32_t mod = 0; mod < 2048; mod++) { \
    OpcodeGrabber[((opcode) << 11) | mod] = fn; \
} \
} while (0)

#define OPX(opcode, subopcode, fn) \
do { \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1)] = fn; \
} while (0)

#define OPXd(opcode, subopcode, fn) \
do { \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x000] = fn<RC0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x001] = fn<RC1>; \
} while (0)

#define OPXod(opcode, subopcode, fn) \
do { \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x000] = fn<RC0, OV0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x001] = fn<RC1, OV0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x400] = fn<RC0, OV1>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x401] = fn<RC1, OV1>; \
} while (0)

#define OPXdc(opcode, subopcode, fn, carry) \
do { \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x000] = fn<carry, RC0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x001] = fn<carry, RC1>; \
} while (0)

#define OPXcod(opcode, subopcode, fn, carry) \
do { \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x000] = fn<carry, RC0, OV0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x001] = fn<carry, RC1, OV0>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x400] = fn<carry, RC0, OV1>; \
    OpcodeGrabber[((opcode) << 11) | ((subopcode)<<1) | 0x401] = fn<carry, RC1, OV1>; \
} while (0)

#define OPla(opcode, subopcode, fn) \
do { \
for (uint32_t mod = 0; mod < 512; mod++) { \
    OpcodeGrabber[((opcode) << 11) | (mod << 2) | (subopcode)] = fn; \
} \
} while (0)

#define OPr(opcode, mod, fn) \
do { \
    OpcodeGrabber[((opcode) << 11) | (mod)] = fn; \
} while (0)

#define OP31(subopcode, fn) OPX(31, subopcode, fn)
#define OP31d(subopcode, fn) OPXd(31, subopcode, fn)
#define OP31od(subopcode, fn) OPXod(31, subopcode, fn)
#define OP31dc(subopcode, fn, carry) OPXdc(31, subopcode, fn, carry)
#define OP31cod(subopcode, fn, carry) OPXcod(31, subopcode, fn, carry)

#define OP63(subopcode, fn) OPX(63, subopcode, fn)
#define OP63d(subopcode, fn) OPXd(63, subopcode, fn)
#define OP63dc(subopcode, fn, carry) OPXdc(63, subopcode, fn, carry)

#define OP59d(subopcode, fn) \
do { \
for (uint32_t mod = 0; mod < 16; mod++) { \
    OPXd(59, (mod << 5) | (subopcode), fn); \
} \
} while (0)

void initialize_ppc_opcode_table() {
    std::fill_n(OpcodeGrabber, 64 * 2048, ppc_illegalop);
    OP(3,  ppc_twi);
    //OP(4,  ppc_opcode4); - Altivec instructions not emulated yet. Uncomment once they're implemented.
    OP(7,  ppc_mulli);
    OP(8,  ppc_subfic);
    if (is_601 || include_601) OP(9, power_dozi);
    OP(10, ppc_cmpli);
    OP(11, ppc_cmpi);
    OP(12, ppc_addic<RC0>);
    OP(13, ppc_addic<RC1>);
    OP(14, ppc_addi<SHFT0>);
    OP(15, ppc_addi<SHFT1>);
    OP(17, ppc_sc);
    OP(20, ppc_rlwimi);
    OP(21, ppc_rlwinm);
    if (is_601 || include_601) OP(22, power_rlmi);
    OP(23, ppc_rlwnm);
    OP(24, ppc_ori<SHFT0>);
    OP(25, ppc_ori<SHFT1>);
    OP(26, ppc_xori<SHFT0>);
    OP(27, ppc_xori<SHFT1>);
    OP(28, ppc_andirc<SHFT0>);
    OP(29, ppc_andirc<SHFT1>);
    OP(32, ppc_lz<uint32_t>);
    OP(33, ppc_lzu<uint32_t>);
    OP(34, ppc_lz<uint8_t>);
    OP(35, ppc_lzu<uint8_t>);
    OP(36, ppc_st<uint32_t>);
    OP(37, ppc_stu<uint32_t>);
    OP(38, ppc_st<uint8_t>);
    OP(39, ppc_stu<uint8_t>);
    OP(40, ppc_lz<uint16_t>);
    OP(41, ppc_lzu<uint16_t>);
    OP(42, ppc_lha);
    OP(43, ppc_lhau);
    OP(44, ppc_st<uint16_t>);
    OP(45, ppc_stu<uint16_t>);
    OP(46, ppc_lmw);
    OP(47, ppc_stmw);
    OP(48, ppc_lfs);
    OP(49, ppc_lfsu);
    OP(50, ppc_lfd);
    OP(51, ppc_lfdu);
    OP(52, ppc_stfs);
    OP(53, ppc_stfsu);
    OP(54, ppc_stfd);
    OP(55, ppc_stfdu);

    OPla(16, 0x0, (dppc_interpreter::ppc_bc<LK0, AA0>)); // bc
    OPla(16, 0x1, (dppc_interpreter::ppc_bc<LK1, AA0>)); // bcl
    OPla(16, 0x2, (dppc_interpreter::ppc_bc<LK0, AA1>)); // bca
    OPla(16, 0x3, (dppc_interpreter::ppc_bc<LK1, AA1>)); // bcla

    OPla(18, 0x0, (dppc_interpreter::ppc_b<LK0, AA0>)); // b
    OPla(18, 0x1, (dppc_interpreter::ppc_b<LK1, AA0>)); // bl
    OPla(18, 0x2, (dppc_interpreter::ppc_b<LK0, AA1>)); // ba
    OPla(18, 0x3, (dppc_interpreter::ppc_b<LK1, AA1>)); // bla

    OPr(19, 0, ppc_mcrf);
    OPr(19, 32, ppc_bclr<LK0>);
    OPr(19, 33, ppc_bclr<LK1>);
    OPr(19, 66, ppc_crnor);
    OPr(19, 100, ppc_rfi);
    OPr(19, 258, ppc_crandc);
    OPr(19, 300, ppc_isync);
    OPr(19, 386, ppc_crxor);
    OPr(19, 450, ppc_crnand);
    OPr(19, 514, ppc_crand);
    OPr(19, 578, ppc_creqv);
    OPr(19, 834, ppc_crorc);
    OPr(19, 898, ppc_cror);
    OPr(19, 1056, (is_601 ? ppc_bcctr<LK0, IS601> : ppc_bcctr<LK0, NOT601>));
    OPr(19, 1057, (is_601 ? ppc_bcctr<LK1, IS601> : ppc_bcctr<LK1, NOT601>));

    OP31(0,      ppc_cmp);
    OP31(4,      ppc_tw);
    OP31(32,     ppc_cmpl);

    OP31cod(8,   ppc_subf, CARRY1);
    OP31cod(40,  ppc_subf, CARRY0);
    OP31od(104,  ppc_neg);
    OP31od(136,  ppc_subfe);
    OP31od(200,  ppc_subfze);
    OP31od(232,  ppc_subfme);

    OP31cod(10,  ppc_add, CARRY1);
    OP31od(138,  ppc_adde);
    OP31od(202,  ppc_addze);
    OP31od(234,  ppc_addme);
    OP31cod(266, ppc_add, CARRY0);

    OP31d(11,    ppc_mulhwu);
    OP31d(75,    ppc_mulhw);
    OP31od(235,  ppc_mullw);
    OP31od(459,  ppc_divwu);
    OP31od(491,  ppc_divw);

    OP31(20,     ppc_lwarx);
    OP31(23,     ppc_lzx<uint32_t>);
    OP31(55,     ppc_lzux<uint32_t>);
    OP31(87,     ppc_lzx<uint8_t>);
    OP31(119,    ppc_lzux<uint8_t>);
    OP31(279,    ppc_lzx<uint16_t>);
    OP31(311,    ppc_lzux<uint16_t>);
    OP31(343,    ppc_lhax);
    OP31(375,    ppc_lhaux);
    OP31(533,    ppc_lswx);
    OP31(534,    ppc_lwbrx);
    OP31(535,    ppc_lfsx);
    OP31(567,    ppc_lfsux);
    OP31(597,    ppc_lswi);
    OP31(599,    ppc_lfdx);
    OP31(631,    ppc_lfdux);
    OP31(790,    ppc_lhbrx);

    OPr(31, (150<<1) | 1, ppc_stwcx); // No Rc=0 variant.
    OP31(151,    ppc_stx<uint32_t>);
    OP31(183,    ppc_stux<uint32_t>);
    OP31(215,    ppc_stx<uint8_t>);
    OP31(247,    ppc_stux<uint8_t>);
    OP31(407,    ppc_stx<uint16_t>);
    OP31(439,    ppc_stux<uint16_t>);
    OP31(661,    ppc_stswx);
    OP31(662,    ppc_stwbrx);
    OP31(663,    ppc_stfsx);
    OP31(695,    ppc_stfsux);
    OP31(725,    ppc_stswi);
    OP31(727,    ppc_stfdx);
    OP31(759,    ppc_stfdux);
    OP31(918,    ppc_sthbrx);
    if (!is_601) OP31(983, ppc_stfiwx);

    OP31(310,    ppc_eciwx);
    OP31(438,    ppc_ecowx);

    OP31dc(24,   ppc_shift, LEFT1); // slw
    OP31dc(28,   ppc_logical, ppc_and);
    OP31dc(60,   ppc_logical, ppc_andc);
    OP31dc(124,  ppc_logical, ppc_nor);
    OP31dc(284,  ppc_logical, ppc_eqv);
    OP31dc(316,  ppc_logical, ppc_xor);
    OP31dc(412,  ppc_logical, ppc_orc);
    OP31dc(444,  ppc_logical, ppc_or);
    OP31dc(476,  ppc_logical, ppc_nand);
    OP31dc(536,  ppc_shift, RIGHT0); // srw
    OP31d(792,   ppc_sraw);
    OP31d(824,   ppc_srawi);
    OP31dc(922,  ppc_exts, int16_t);
    OP31dc(954,  ppc_exts, int8_t);

    OP31d(26,    ppc_cntlzw);

    OP31(19,     ppc_mfcr);
    OP31(83,     ppc_mfmsr);
    OP31(144,    ppc_mtcrf);
    OP31(146,    ppc_mtmsr);
    OP31(210,    ppc_mtsr);
    OP31(242,    ppc_mtsrin);
    OP31(339,    ppc_mfspr);
    if (!is_601) OP31(371, ppc_mftb);
    OP31(467,    ppc_mtspr);
    OP31(512,    ppc_mcrxr);
    OP31(595,    ppc_mfsr);
    OP31(659,    ppc_mfsrin);

    OP31(54,     ppc_dcbst);
    OP31(86,     ppc_dcbf);
    OP31(246,    ppc_dcbtst);
    OP31(278,    ppc_dcbt);
    OP31(598,    ppc_sync);
    OP31(470,    ppc_dcbi);
    OP31(1014,   ppc_dcbz);

    if (is_601 || include_601) {
        OP31d(29,   power_maskg);
        OP31od(107, power_mul);
        OP31d(152,  power_slq);
        OP31d(153,  power_sle);
        OP31d(184,  power_sliq);
        OP31d(216,  power_sllq);
        OP31d(217,  power_sleq);
        OP31d(248,  power_slliq);
        OP31od(264, power_doz);
        OP31d(277,  power_lscbx);
        OP31od(331, power_div);
        OP31od(360, power_abs);
        OP31od(363, power_divs);
        OP31od(488, power_nabs);
        OP31(531,   power_clcs);
        OP31d(537,  power_rrib);
        OP31d(541,  power_maskir);
        OP31d(664,  power_srq);
        OP31d(665,  power_sre);
        OP31d(696,  power_sriq);
        OP31d(728,  power_srlq);
        OP31d(729,  power_sreq);
        OP31d(760,  power_srliq);
        OP31d(920,  power_sraq);
        OP31d(921,  power_srea);
        OP31d(952,  power_sraiq);
    }

    OP31(306,    ppc_tlbie);
    if (!is_601) OP31(370, ppc_tlbia);
    if (!is_601) OP31(566, ppc_tlbsync);
    OP31(854,    ppc_eieio);
    OP31(982,    ppc_icbi);
    if (!is_601) OP31(978, ppc_tlbld);
    if (!is_601) OP31(1010, ppc_tlbli);

    OP59d(18,    ppc_fdivs);
    OP59d(20,    ppc_fsubs);
    OP59d(21,    ppc_fadds);
    if (ppc_state.spr[SPR::PVR] == PPC_VER::MPC970MP) OP59d(22, ppc_fsqrts);
    if (!is_601) OP59d(24, ppc_fres);
    OP59d(25,    ppc_fmuls);
    OP59d(28,    ppc_fmsubs);
    OP59d(29,    ppc_fmadds);
    OP59d(30,    ppc_fnmsubs);
    OP59d(31,    ppc_fnmadds);

    OP63(0,      ppc_fcmpu);
    OP63d(12,    ppc_frsp);
    OP63d(14,    ppc_fctiw);
    OP63d(15,    ppc_fctiwz);
    OP63d(18,    ppc_fdiv);
    OP63d(20,    ppc_fsub);
    OP63d(21,    ppc_fadd);
    if (ppc_state.spr[SPR::PVR] == PPC_VER::MPC970MP) OP63d(22, ppc_fsqrt);
    if (!is_601) OP63d(26, ppc_frsqrte);
    OP63(32,     ppc_fcmpo);
    OP63d(38,    ppc_mtfsb1);
    OP63d(40,    ppc_fneg);
    OP63(64,     ppc_mcrfs);
    OP63d(70,    ppc_mtfsb0);
    OP63d(72,    ppc_fmr);
    OP63d(134,   ppc_mtfsfi);
    OP63d(136,   ppc_fnabs);
    OP63d(264,   ppc_fabs);
    if (is_601) OP63dc(583, ppc_mffs, IS601); else OP63dc(583, ppc_mffs, NOT601);
    OP63d(711,   ppc_mtfsf);

    for (int i = 0; i < 1024; i += 32) {
        if (!is_601) OP63d(i + 23, ppc_fsel);
        OP63d(i + 25, ppc_fmul);
        OP63d(i + 28, ppc_fmsub);
        OP63d(i + 29, ppc_fmadd);
        OP63d(i + 30, ppc_fnmsub);
        OP63d(i + 31, ppc_fnmadd);
    }
}

void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, bool do_include_601, uint64_t tb_freq)
{
    mem_ctrl_instance = mem_ctrl;

    std::memset(&ppc_state, 0, sizeof(ppc_state));
    set_host_rounding_mode(0);

    ppc_state.spr[SPR::PVR] = cpu_version;
    is_601 = (cpu_version >> 16) == 1;
    include_601 = !is_601 & do_include_601;

    initialize_ppc_opcode_table();

    // initialize emulator timers
    TimerManager::get_instance()->set_time_now_cb(&get_virt_time_ns);
    TimerManager::get_instance()->set_notify_changes_cb(&force_cycle_counter_reload);

    // initialize time base facility
#ifdef __APPLE__
    mach_timebase_info(&timebase_info);
#endif
    g_nanoseconds_base = cpu_now_ns();
    g_icycles = 0;
    //icnt_factor      = 6;
    icnt_factor = 4;
    tbr_wr_timestamp = 0;
    rtc_timestamp = 0;
    tbr_wr_value = 0;
    tbr_freq_ghz = (tb_freq << 32) / NS_PER_SEC;
    tbr_period_ns = ((uint64_t)NS_PER_SEC << 32) / tb_freq;

    exec_flags = 0;
    exec_timer = false;

    timebase_counter = 0;
    dec_wr_value = 0;


    if (is_601) {
        /* MPC601 sets MSR[ME] bit during hard reset / Power-On */
        ppc_state.msr = (MSR::ME + MSR::IP);
    } else {
        ppc_state.msr             = MSR::IP;
        ppc_state.spr[SPR::DEC_S] = 0xFFFFFFFFUL;
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
    {"XER",    SPR::XER},       {"LR",     SPR::LR},    {"CTR",    SPR::CTR},
    {"DEC",    SPR::DEC_S},     {"PVR",    SPR::PVR},   {"SPRG0",  SPR::SPRG0},
    {"SPRG1",  SPR::SPRG1},     {"SPRG2",  SPR::SPRG2}, {"SPRG3",  SPR::SPRG3},
    {"SRR0",   SPR::SRR0},      {"SRR1",   SPR::SRR1},  {"IBAT0U", 528},
    {"IBAT0L", 529},            {"IBAT1U", 530},        {"IBAT1L", 531},
    {"IBAT2U", 532},            {"IBAT2L", 533},        {"IBAT3U", 534},
    {"IBAT3L", 535},            {"DBAT0U", 536},        {"DBAT0L", 537},
    {"DBAT1U", 538},            {"DBAT1L", 539},        {"DBAT2U", 540},
    {"DBAT2L", 541},            {"DBAT3U", 542},        {"DBAT3L", 543},
    {"HID0",   SPR::HID0},      {"HID1",   SPR::HID1},  {"IABR",   1010},
    {"DABR",   1013},           {"L2CR",   1017},       {"ICTC",   1019},
    {"THRM1",  1020},           {"THRM2",  1021},       {"THRM3",  1022},
    {"PIR",    1023},           {"TBL",    SPR::TBL_S}, {"TBU",    SPR::TBU_S},
    {"SDR1",   SPR::SDR1},      {"MQ",     SPR::MQ},    {"RTCU",   SPR::RTCU_S},
    {"RTCL",   SPR::RTCL_S},    {"DSISR",  SPR::DSISR}, {"DAR",    SPR::DAR},
    {"MMCR0",  SPR::MMCR0},     {"PMC1",   SPR::PMC1},  {"PMC2",   SPR::PMC2},
    {"SDA",    SPR::SDA},       {"SIA",    SPR::SIA},   {"MMCR1",  SPR::MMCR1}
};

static uint64_t reg_op(string& reg_name, uint64_t val, bool is_write) {
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
                switch (reg_num) {
                case SPR::DEC_U  : reg_num = SPR::DEC_S  ; break;
                case SPR::RTCL_U : reg_num = SPR::RTCL_S ; break;
                case SPR::RTCU_U : reg_num = SPR::RTCU_S ; break;
                case SPR::TBL_U  : reg_num = SPR::TBL_S  ; break;
                case SPR::TBU_U  : reg_num = SPR::TBU_S  ; break;
                }
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
