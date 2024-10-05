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

typedef void (*PPCOpcode)(void);

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

typedef struct struct_ppc_instr {
    uint8_t arg0;
    uint8_t arg1;
    uint8_t arg2;
    uint8_t arg3;
    uint8_t arg4;
    int32_t addr;
    int32_t mask;

    union {
        int32_t i_simm;
        uint32_t i_uimm;
    };
} SetInstr;

extern SetInstr instr;

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

// Important Addressing Integers
extern uint32_t ppc_cur_instruction;
extern uint32_t ppc_next_instruction_address;

inline void ppc_set_cur_instruction(const uint8_t* ptr) {
    ppc_cur_instruction = READ_DWORD_BE_A(ptr);
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

void ppc_illegalop();
void ppc_fpu_off();
void ppc_assert_int();
void ppc_release_int();

void decode_instr();
void initialize_ppc_opcode_tables(bool include_601);

extern double fp_return_double(uint32_t reg);
extern uint64_t fp_return_uint64(uint32_t reg);

void ppc_changecrf0(uint32_t set_result);
void set_host_rounding_mode(uint8_t mode);
void update_fpscr(uint32_t new_fpscr);

/* Exception handlers. */
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
[[noreturn]] void dbg_exception_handler(Except_Type exception_type, uint32_t srr1_bits);
void ppc_floating_point_exception();
void ppc_alignment_exception(uint32_t ea);

// MEMORY DECLARATIONS
extern MemCtrlBase* mem_ctrl_instance;

extern void add_ctx_sync_action(const std::function<void()> &);
extern void do_ctx_sync(void);

// The functions used by the PowerPC processor

namespace dppc_interpreter {
template <field_lk l, field_601 for601> extern void ppc_bcctr();
template <field_lk l> extern void ppc_bclr();
extern void ppc_crand();
extern void ppc_crandc();
extern void ppc_creqv();
extern void ppc_crnand();
extern void ppc_crnor();
extern void ppc_cror();
extern void ppc_crorc();
extern void ppc_crxor();
extern void ppc_isync();

template <logical_fun logical_op, field_rc rec> extern void ppc_logical();

template <field_carry carry, field_rc rec, field_ov ov> extern void ppc_add();
template <field_rc rec, field_ov ov> extern void ppc_adde();
template <field_rc rec, field_ov ov> extern void ppc_addme();
template <field_rc rec, field_ov ov> extern void ppc_addze();
extern void ppc_cmp();
extern void ppc_cmpl();
template <field_rc rec> extern void ppc_cntlzw();
extern void ppc_dcbf();
extern void ppc_dcbi();
extern void ppc_dcbst();
extern void ppc_dcbt();
extern void ppc_dcbtst();
extern void ppc_dcbz();
template <field_rc rec, field_ov ov> extern void ppc_divw();
template <field_rc rec, field_ov ov> extern void ppc_divwu();
extern void ppc_eciwx();
extern void ppc_ecowx();
extern void ppc_eieio();
template <class T, field_rc rec>extern void ppc_exts();
extern void ppc_icbi();
extern void ppc_mftb();
extern void ppc_lhaux();
extern void ppc_lhax();
extern void ppc_lhbrx();
extern void ppc_lwarx();
extern void ppc_lwbrx();
template <class T> extern void ppc_lzx();
template <class T> extern void ppc_lzux();
extern void ppc_mcrxr();
extern void ppc_mfcr();
template <field_rc rec> extern void ppc_mulhwu();
template <field_rc rec> extern void ppc_mulhw();
template <field_rc rec, field_ov ov> extern void ppc_mullw();
template <field_rc rec, field_ov ov> extern void ppc_neg();
template <field_direction shift, field_rc rec> extern void ppc_shift();
template <field_rc rec> extern void ppc_sraw();
template <field_rc rec> extern void ppc_srawi();
template <class T> extern void ppc_stx();
template <class T> extern void ppc_stux();
extern void ppc_stfiwx();
extern void ppc_sthbrx();
extern void ppc_stwcx();
extern void ppc_stwbrx();
template <field_carry carry, field_rc rec, field_ov ov> extern void ppc_subf();
template <field_rc rec, field_ov ov> extern void ppc_subfe();
template <field_rc rec, field_ov ov> extern void ppc_subfme();
template <field_rc rec, field_ov ov> extern void ppc_subfze();
extern void ppc_sync();
extern void ppc_tlbia();
extern void ppc_tlbie();
extern void ppc_tlbli();
extern void ppc_tlbld();
extern void ppc_tlbsync();
extern void ppc_tw();

extern void ppc_lswi();
extern void ppc_lswx();
extern void ppc_stswi();
extern void ppc_stswx();

extern void ppc_mfsr();
extern void ppc_mfsrin();
extern void ppc_mtsr();
extern void ppc_mtsrin();

extern void ppc_mcrf();
extern void ppc_mtcrf();
extern void ppc_mfmsr();
extern void ppc_mfspr();
extern void ppc_mtmsr();
extern void ppc_mtspr();

template <field_rc rec> extern void ppc_mtfsb0();
template <field_rc rec> extern void ppc_mtfsb1();
extern void ppc_mcrfs();
template <field_rc rec> extern void ppc_fmr();
template <field_601 for601, field_rc rec> extern void ppc_mffs();
template <field_rc rec> extern void ppc_mtfsf();
template <field_rc rec> extern void ppc_mtfsfi();

template <field_shift shift> extern void ppc_addi();
template <field_rc rec> extern void ppc_addic();
template <field_shift shift> extern void ppc_andirc();
template <field_lk l, field_aa a> extern void ppc_b();
template <field_lk l, field_aa a> extern void ppc_bc();
extern void ppc_cmpi();
extern void ppc_cmpli();
template <class T> extern void ppc_lz();
template <class T> extern void ppc_lzu();
extern void ppc_lha();
extern void ppc_lhau();
extern void ppc_lmw();
extern void ppc_mulli();
template <field_shift shift> extern void ppc_ori();
extern void ppc_rfi();
extern void ppc_rlwimi();
extern void ppc_rlwinm();
extern void ppc_rlwnm();
extern void ppc_sc();
template <class T> extern void ppc_st();
template <class T> extern void ppc_stu();
extern void ppc_stmw();
extern void ppc_subfic();
extern void ppc_twi();
template <field_shift shift> extern void ppc_xori();

extern void ppc_lfs();
extern void ppc_lfsu();
extern void ppc_lfsx();
extern void ppc_lfsux();
extern void ppc_lfd();
extern void ppc_lfdu();
extern void ppc_lfdx();
extern void ppc_lfdux();
extern void ppc_stfs();
extern void ppc_stfsu();
extern void ppc_stfsx();
extern void ppc_stfsux();
extern void ppc_stfd();
extern void ppc_stfdu();
extern void ppc_stfdx();
extern void ppc_stfdux();

template <field_rc rec> extern void ppc_fadd();
template <field_rc rec> extern void ppc_fsub();
template <field_rc rec> extern void ppc_fmul();
template <field_rc rec> extern void ppc_fdiv();
template <field_rc rec> extern void ppc_fadds();
template <field_rc rec> extern void ppc_fsubs();
template <field_rc rec> extern void ppc_fmuls();
template <field_rc rec> extern void ppc_fdivs();
template <field_rc rec> extern void ppc_fmadd();
template <field_rc rec> extern void ppc_fmsub();
template <field_rc rec> extern void ppc_fnmadd();
template <field_rc rec> extern void ppc_fnmsub();
template <field_rc rec> extern void ppc_fmadds();
template <field_rc rec> extern void ppc_fmsubs();
template <field_rc rec> extern void ppc_fnmadds();
template <field_rc rec> extern void ppc_fnmsubs();
template <field_rc rec> extern void ppc_fabs();
template <field_rc rec> extern void ppc_fnabs();
template <field_rc rec> extern void ppc_fneg();
template <field_rc rec> extern void ppc_fsel();
template <field_rc rec> extern void ppc_fres();
template <field_rc rec> extern void ppc_fsqrts();
template <field_rc rec> extern void ppc_fsqrt();
template <field_rc rec> extern void ppc_frsqrte();
template <field_rc rec> extern void ppc_frsp();
template <field_rc rec> extern void ppc_fctiw();
template <field_rc rec> extern void ppc_fctiwz();

extern void ppc_fcmpo();
extern void ppc_fcmpu();

// Power-specific instructions
template <field_rc rec, field_ov ov> extern void power_abs();
extern void power_clcs();
template <field_rc rec, field_ov ov> extern void power_div();
template <field_rc rec, field_ov ov> extern void power_divs();
template <field_rc rec, field_ov ov> extern void power_doz();
extern void power_dozi();
template <field_rc rec> extern void power_lscbx();
template <field_rc rec> extern void power_maskg();
template <field_rc rec> extern void power_maskir();
template <field_rc rec, field_ov ov> extern void power_mul();
template <field_rc rec, field_ov ov> extern void power_nabs();
extern void power_rlmi();
template <field_rc rec> extern void power_rrib();
template <field_rc rec> extern void power_sle();
template <field_rc rec> extern void power_sleq();
template <field_rc rec> extern void power_sliq();
template <field_rc rec> extern void power_slliq();
template <field_rc rec> extern void power_sllq();
template <field_rc rec> extern void power_slq();
template <field_rc rec> extern void power_sraiq();
template <field_rc rec> extern void power_sraq();
template <field_rc rec> extern void power_sre();
template <field_rc rec> extern void power_srea();
template <field_rc rec> extern void power_sreq();
template <field_rc rec> extern void power_sriq();
template <field_rc rec> extern void power_srliq();
template <field_rc rec> extern void power_srlq();
template <field_rc rec> extern void power_srq();
}    // namespace dppc_interpreter

// AltiVec instructions

// 64-bit instructions

// G5+ instructions

extern uint64_t get_virt_time_ns(void);

extern void ppc_exec(void);
extern void ppc_exec_single(void);
extern void ppc_exec_until(uint32_t goal_addr);
extern void ppc_exec_dbg(uint32_t start_addr, uint32_t size);

/* debugging support API */
void print_fprs(void);                   /* print content of the floating-point registers  */
uint64_t get_reg(std::string reg_name); /* get content of the register reg_name */
void set_reg(std::string reg_name, uint64_t val); /* set reg_name to val */

#endif /* PPCEMU_H */
