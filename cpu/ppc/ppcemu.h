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

#ifndef PPCEMU_H
#define PPCEMU_H

#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <memaccess.h>

#include <atomic>
#include <cinttypes>
#include <functional>
#include <setjmp.h>
#include <string>

// Uncomment this to have a more graceful approach to illegal opcodes
//#define ILLEGAL_OP_SAFE 1

//#define CPU_PROFILING // enable CPU profiling

/** type of compiler used during execution */
enum EXEC_MODE:uint32_t {
    interpreter     = 0,
    debugger        = 1,
    threaded_int    = 2,
    jit             = 3
};

enum endian_switch { big_end = 0, little_end = 1 };

typedef void (*PPCOpcode)(uint32_t);

union FPR_storage {
    double dbl64_r;      // double floating-point representation
    uint64_t int64_r;    // double integer representation
};

/**
Except for the floating-point registers, all registers require
32 bits for representation. Floating-point registers need 64 bits.

  gpr = General Purpose Register
  fpr = Floating Point (FP) Register
   cr = Condition Register
  tbr = Time Base Register
fpscr = FP Status and Condition Register
  spr = Special Register
  msr = Machine State Register
   sr = Segment Register
**/

typedef struct struct_ppc_state {
    FPR_storage fpr[32];
    uint32_t pc;    // Referred as the CIA in the PPC manual
    uint32_t gpr[32];
    uint32_t cr;
    uint32_t fpscr;
    uint32_t tbr[2];
    uint32_t spr[1024];
    uint32_t msr;
    uint32_t sr[16];
    bool reserve;    // reserve bit used for lwarx and stcwx
} SetPRS;

extern SetPRS ppc_state;

/** symbolic names for frequently used SPRs */
enum SPR : int {
    MQ      = 0,   // MQ (601)
    XER     = 1,
    RTCU_U  = 4,   // user mode RTCU (601)
    RTCL_U  = 5,   // user mode RTCL (601)
    DEC_U   = 6,   // user mode decrementer (601)
    LR      = 8,
    CTR     = 9,
    DSISR   = 18,
    DAR     = 19,
    RTCU_S  = 20,  // supervisor RTCU (601)
    RTCL_S  = 21,  // supervisor RTCL (601)
    DEC_S   = 22,  // supervisor decrementer
    SDR1    = 25,
    SRR0    = 26,
    SRR1    = 27,
    TBL_U   = 268, // user mode TBL
    TBU_U   = 269, // user mode TBU
    SPRG0   = 272,
    SPRG1   = 273,
    SPRG2   = 274,
    SPRG3   = 275,
    TBL_S   = 284, // supervisor TBL
    TBU_S   = 285, // supervisor TBU
    PVR     = 287,
    MMCR0   = 952,
    PMC1    = 953,
    PMC2    = 954,
    SIA     = 955,
    MMCR1   = 956,
    SDA     = 959,
    HID0    = 1008,
    HID1    = 1009,
};

/** symbolic names for common PPC processors */
enum PPC_VER : uint32_t {
    MPC601      = 0x00010001,
    MPC603      = 0x00030001,
    MPC604      = 0x00040001,
    MPC603E     = 0x00060101,
    MPC603EV    = 0x00070101,
    MPC750      = 0x00080200,
    MPC604E     = 0x00090202,
    MPC970MP    = 0x00440100,
};

/**
typedef struct struct_ppc64_state {
    FPR_storage fpr [32];
    uint64_t pc; //Referred as the CIA in the PPC manual
    uint64_t gpr [32];
    uint32_t cr;
    uint32_t fpscr;
    uint32_t tbr [2];
    uint64_t spr [1024];
    uint32_t msr;
    uint32_t sr [16];
    bool reserve; //reserve bit used for lwarx and stcwx
} SetPRS64;

extern SetPRS64 ppc_state64;
**/

/**
Specific SPRS to be weary of:

USER MODEL
SPR 1 - XER
SPR 8 - Link Register / Branch
  b0 - Summary Overflow
  b1 - Overflow
  b2 - Carry
  b25-31 - Number of bytes to transfer
SPR 9 - Count

SUPERVISOR MODEL
19 is the Data Address Register
22 is the Decrementer
26, 27 are the Save and Restore Registers (SRR0, SRR1)
272 - 275 are the SPRGs
284 - 285 for writing to the TBR's.
528 - 535 are the Instruction BAT registers
536 - 543 are the Data BAT registers
**/

extern uint64_t timebase_counter;
extern uint64_t tbr_wr_timestamp;
extern uint64_t dec_wr_timestamp;
extern uint64_t rtc_timestamp;
extern uint64_t tbr_wr_value;
extern uint32_t dec_wr_value;
extern uint32_t tbr_freq_ghz;
extern uint64_t tbr_period_ns;
extern uint32_t rtc_lo, rtc_hi;

/* Flags for controlling interpreter execution. */
enum {
    EXEF_BRANCH    = 1 << 0,
    EXEF_EXCEPTION = 1 << 1,
    EXEF_RFI       = 1 << 2,
};

enum CR_select : int32_t {
    CR0_field = (0xF << 28),
    CR1_field = (0xF << 24),
};

// Define bit masks for CR0.
// To use them in other CR fields, just right shift it by 4*CR_num bits.
enum CRx_bit : uint32_t {
    CR_SO = 1UL << 28,
    CR_EQ = 1UL << 29,
    CR_GT = 1UL << 30,
    CR_LT = 1UL << 31
};

enum CR1_bit : uint32_t {
    CR1_OX = 24,
    CR1_VX,
    CR1_FEX,
    CR1_FX,
};

enum FPSCR : uint32_t {
    RN_MASK     = 0x3,
    NI          = 1UL << 2,
    XE          = 1UL << 3,
    ZE          = 1UL << 4,
    UE          = 1UL << 5,
    OE          = 1UL << 6,
    VE          = 1UL << 7,
    VXCVI       = 1UL << 8,
    VXSQRT      = 1UL << 9,
    VXSOFT      = 1UL << 10,
    FPCC_FUNAN  = 1UL << 12,
    FPCC_ZERO   = 1UL << 13,
    FPCC_POS    = 1UL << 14,
    FPCC_NEG    = 1UL << 15,
    FPCC_MASK   = FPCC_NEG | FPCC_POS | FPCC_ZERO | FPCC_FUNAN,
    FPRCD       = 1UL << 16,
    FPRF_MASK   = FPRCD | FPCC_MASK,
    FI          = 1UL << 17,
    FR          = 1UL << 18,
    VXVC        = 1UL << 19,
    VXIMZ       = 1UL << 20,
    VXZDZ       = 1UL << 21,
    VXIDI       = 1UL << 22,
    VXISI       = 1UL << 23,
    VXSNAN      = 1UL << 24,
    XX          = 1UL << 25,
    ZX          = 1UL << 26,
    UX          = 1UL << 27,
    OX          = 1UL << 28,
    VX          = 1UL << 29,
    FEX         = 1UL << 30,
    FX          = 1UL << 31
};

enum MSR : int {
    LE  = 0x1,  //little endian mode
    RI  = 0x2,
    DR  = 0x10,
    IR  = 0x20,
    IP  = 0x40,
    FE1 = 0x100,
    BE  = 0x200,
    SE  = 0x400,
    FE0 = 0x800,
    ME  = 0x1000,
    FP  = 0x2000,
    PR  = 0x4000,
    EE  = 0x8000, //external interrupt
    ILE = 0x10000,
    POW = 0x40000
};

enum XER : uint32_t {
    CA = 1UL << 29,
    OV = 1UL << 30,
    SO = 1UL << 31
};

//for inf and nan checks
enum FPOP : int {
    DIV    = 0x12,
    SUB    = 0x14,
    ADD    = 0x15,
    SQRT   = 0x16,
    MUL    = 0x19
};

/** PowerPC exception types. */
enum class Except_Type {
    EXC_SYSTEM_RESET = 1,
    EXC_MACHINE_CHECK,
    EXC_DSI,
    EXC_ISI,
    EXC_EXT_INT,
    EXC_ALIGNMENT,
    EXC_PROGRAM,
    EXC_NO_FPU,
    EXC_DECR,
    EXC_SYSCALL = 12,
    EXC_TRACE   = 13
};

/** Program Exception subclasses. */
enum Exc_Cause : uint32_t {
    FPU_OFF     = 1 << (31 - 11),
    ILLEGAL_OP  = 1 << (31 - 12),
    NOT_ALLOWED = 1 << (31 - 13),
    TRAP        = 1 << (31 - 14),
};

extern unsigned exec_flags;

extern jmp_buf exc_env;

enum Po_Cause : int {
    po_none,
    po_starting_up,
    po_quit,
    po_quitting,
    po_shut_down,
    po_shutting_down,
    po_restart,
    po_restarting,
    po_disassemble_on,
    po_disassemble_off,
    po_enter_debugger,
    po_entered_debugger,
    po_signal_interrupt,
};

extern bool power_on;
extern Po_Cause power_off_reason;
extern bool int_pin;
extern bool dec_exception_pending;

extern bool is_601;        // For PowerPC 601 Emulation
extern bool is_altivec;    // For Altivec Emulation
extern bool is_64bit;      // For PowerPC G5 Emulation

extern uint32_t ppc_next_instruction_address;

inline uint32_t ppc_set_cur_instruction(const uint8_t* ptr) {
    return READ_DWORD_BE_A(ptr);
}

// Profiling Stats
#ifdef CPU_PROFILING
extern uint64_t num_executed_instrs;
extern uint64_t num_supervisor_instrs;
extern uint64_t num_int_loads;
extern uint64_t num_int_stores;
extern uint64_t exceptions_processed;
#endif

// instruction enums
typedef enum {
    ppc_and  = 1,
    ppc_andc = 2,
    ppc_eqv  = 3,
    ppc_nand = 4,
    ppc_nor  = 5,
    ppc_or   = 6,
    ppc_orc  = 7,
    ppc_xor  = 8,
} logical_fun;

typedef enum {
    LK0,
    LK1,
} field_lk;

typedef enum {
    AA0,
    AA1,
} field_aa;

typedef enum {
    SHFT0,
    SHFT1,
} field_shift;

typedef enum {
    RIGHT0,
    LEFT1,
} field_direction;

typedef enum {
    RC0,
    RC1,
} field_rc;

typedef enum {
    OV0,
    OV1,
} field_ov;

typedef enum {
    CARRY0,
    CARRY1,
} field_carry;

typedef enum {
    NOT601,
    IS601,
} field_601;

// Function prototypes
extern void ppc_cpu_init(MemCtrlBase* mem_ctrl, uint32_t cpu_version, bool include_601, uint64_t tb_freq);
extern void ppc_mmu_init();

void ppc_illegalop(uint32_t null_val=0);
void ppc_fpu_off();
void ppc_assert_int();
void ppc_release_int();

void initialize_ppc_opcode_tables(bool include_601);

extern double fp_return_double(uint32_t reg);
extern uint64_t fp_return_uint64(uint32_t reg);

void ppc_changecrf0(uint32_t set_result);
void set_host_rounding_mode(uint8_t mode);
void update_fpscr(uint32_t new_fpscr);

/* Exception handlers. */
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
[[noreturn]] void dbg_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
void ppc_floating_point_exception(uint32_t instr);
void ppc_alignment_exception(uint32_t ea, uint32_t instr);

// MEMORY DECLARATIONS
extern MemCtrlBase* mem_ctrl_instance;

extern void add_ctx_sync_action(const std::function<void()> &);
extern void do_ctx_sync(void);

extern uint64_t get_virt_time_ns(void);

extern void ppc_main_opcode(uint32_t instr);
extern void ppc_exec(void);
extern void ppc_exec_single(void);
extern void ppc_exec_until(uint32_t goal_addr);
extern void ppc_exec_dbg(uint32_t start_addr, uint32_t size);

/* debugging support API */
void print_fprs(void);                   /* print content of the floating-point registers  */
uint64_t get_reg(std::string reg_name); /* get content of the register reg_name */
void set_reg(std::string reg_name, uint64_t val); /* set reg_name to val */

// The functions used by the PowerPC processor
#include "ppcmacros_prototypes.h"

namespace dppc_interpreter {
#include "ppcopcodes.include"
#include "poweropcodes.include"
#include "ppcfpopcodes.include"
}    // namespace dppc_interpreter

#endif /* PPCEMU_H */
