//DingusPPC  - Prototype 5bf2
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef PPCEMU_H
#define PPCEMU_H

#include <setjmp.h>
#include "endianswap.h"
#include "devices/memctrlbase.h"

//Uncomment this to help debug the emulator further
//#define EXHAUSTIVE_DEBUG 1

//Uncomment this to have a more graceful approach to illegal opcodes
//#define ILLEGAL_OP_SAFE 1

//Uncomment this to use GCC built-in functions.
//#define USE_GCC_BUILTINS 1

//Uncomment this to use Visual Studio built-in functions.
//#define USE_VS_BUILTINS 1

enum endian_switch {big_end = 0, little_end = 1};

typedef void (*PPCOpcode)(void);

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
    uint64_t ppc_fpr [32];
    uint32_t ppc_pc; //Referred as the CIA in the PPC manual
    uint32_t ppc_gpr [32];
    uint32_t ppc_cr;
    uint32_t ppc_fpscr;
    uint32_t ppc_tbr [2];
    uint32_t ppc_spr [1024];
    uint32_t ppc_msr;
    uint32_t ppc_sr [16];
    bool ppc_reserve; //reserve bit used for lwarx and stcwx
} SetPRS;

extern SetPRS ppc_state;

/**
typedef struct struct_ppc64_state {
    uint64_t ppc_fpr [32];
    uint64_t ppc_pc; //Referred as the CIA in the PPC manual
    uint64_t ppc_gpr [32];
    uint32_t ppc_cr;
    uint32_t ppc_fpscr;
    uint32_t ppc_tbr [2];
    uint64_t ppc_spr [1024];
    uint32_t ppc_msr;
    uint32_t ppc_sr [16];
    bool ppc_reserve; //reserve bit used for lwarx and stcwx
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

extern uint32_t return_value; //used for loading from memory

extern uint32_t opcode_value; //used for interpreting opcodes
extern uint32_t ram_size_set;

extern uint32_t rom_file_begin; //where to start storing ROM files in memory
extern uint32_t pci_io_end;

extern uint32_t rom_filesize;

//Additional steps to prevent overflow?
extern int32_t add_result;
extern int32_t sidiv_result;
extern int32_t simult_result;
extern uint32_t uiadd_result;
extern uint32_t uidiv_result;
extern uint32_t uimult_result;
extern uint64_t uiproduct;
extern int64_t siproduct;

extern int32_t word_copy_location;
extern uint32_t strwrd_replace_value;

extern uint32_t crf_d;
extern uint32_t crf_s;
extern uint32_t reg_s;
extern uint32_t reg_d;
extern uint32_t reg_a;
extern uint32_t reg_b;
extern uint32_t reg_c;
extern uint32_t xercon;
extern uint32_t cmp_c;
extern uint32_t crm;
extern uint32_t br_bo;
extern uint32_t br_bi;
extern uint32_t rot_sh;
extern uint32_t rot_mb;
extern uint32_t rot_me;
extern uint32_t uimm;
extern uint32_t not_this;
extern uint32_t grab_sr;
extern uint32_t grab_inb; //This is for grabbing the number of immediate bytes for loading and storing
extern uint32_t grab_d; //This is for grabbing d from Store and Load instructions
extern uint32_t ppc_to;
extern int32_t simm;
extern int32_t adr_li;
extern int32_t br_bd;

//Used for GP calcs
extern uint32_t ppc_result_a;
extern uint32_t ppc_result_b;
extern uint32_t ppc_result_c;
extern uint32_t ppc_result_d;

//Used for FP calcs
extern uint64_t ppc_result64_a;
extern uint64_t ppc_result64_b;
extern uint64_t ppc_result64_c;
extern uint64_t ppc_result64_d;


/* The precise end of a basic block. */
enum class BB_end_kind {
    BB_NONE   = 0, /* no basic block end is reached       */
    BB_BRANCH = 1, /* a branch instruction is encountered */
    BB_EXCEPTION,  /* an exception is occured             */
    BB_RFI         /* the rfi instruction is encountered  */
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

//extern bool bb_end;
extern BB_end_kind bb_kind;

extern jmp_buf exc_env;

extern bool grab_branch;
extern bool grab_exception;
extern bool grab_return;

extern bool power_on;

extern bool is_nubus; //For early Power Mac Emulation

extern bool is_601; //For PowerPC 601 Emulation
extern bool is_gekko; //For GameCube Emulation
extern bool is_altivec; //For Altivec Emulation
extern bool is_64bit; //For PowerPC G5 Emulation

//Important Addressing Integers
extern uint32_t ppc_cur_instruction;
extern uint32_t ppc_effective_address;
extern uint32_t ppc_real_address;
extern uint32_t ppc_next_instruction_address;

//Profiling Stats
extern uint32_t mmu_operations_num;
extern uint32_t exceptions_performed;
extern uint32_t supervisor_inst_num;

//Function prototypes
void reg_init();
uint32_t reg_print();
uint32_t reg_read();
uint32_t reg_write();

void ppc_illegalop();
void ppc_opcode4();
void ppc_opcode16();
void ppc_opcode18();
void ppc_opcode19();
void ppc_opcode31();
void ppc_opcode59();
void ppc_opcode63();

extern void ppc_grab_regsda();
extern void ppc_grab_regsdb();

extern void ppc_grab_regssa();
extern void ppc_grab_regssb();

extern void ppc_grab_regsdab();
extern void ppc_grab_regssab();

extern void ppc_grab_regsdasimm();
extern void ppc_grab_regsdauimm();
extern void ppc_grab_regsasimm();
extern void ppc_grab_regssauimm();

extern void ppc_grab_regsfpdb();
extern void ppc_grab_regsfpdab();
extern void ppc_grab_regsfpdia();
extern void ppc_grab_regsfpdiab();
extern void ppc_grab_regsfpsia();
extern void ppc_grab_regsfpsiab();

extern void ppc_store_result_regd();
extern void ppc_store_result_rega();
extern void ppc_store_sfpresult();
extern void ppc_store_dfpresult();

void ppc_carry(uint32_t a, uint32_t b);
void ppc_setsoov(uint32_t a, uint32_t b);
void ppc_changecrf0(uint32_t set_result);
void ppc_fp_changecrf1();

void ppc_tbr_update();
[[noreturn]] void ppc_exception_handler(Except_Type exception_type,
                                        uint32_t srr1_bits);

//MEMORY DECLARATIONS
extern MemCtrlBase *mem_ctrl_instance;

extern unsigned char * machine_sysram_mem;
extern unsigned char * machine_sysrom_mem;

//The functions used by the PowerPC processor
extern void ppc_bcctr();
extern void ppc_bcctrl();
extern void ppc_bclr();
extern void ppc_bclrl();
extern void ppc_crand();
extern void ppc_crandc();
extern void ppc_creqv();
extern void ppc_crnand();
extern void ppc_crnor();
extern void ppc_cror();
extern void ppc_crorc();
extern void ppc_crxor();
extern void ppc_isync();

extern void ppc_add();
extern void ppc_adddot();
extern void ppc_addo();
extern void ppc_addodot();
extern void ppc_addc();
extern void ppc_addcdot();
extern void ppc_addco();
extern void ppc_addcodot();
extern void ppc_adde();
extern void ppc_addedot();
extern void ppc_addeo();
extern void ppc_addeodot();
extern void ppc_addme();
extern void ppc_addmedot();
extern void ppc_addmeo();
extern void ppc_addmeodot();
extern void ppc_addze();
extern void ppc_addzedot();
extern void ppc_addzeo();
extern void ppc_addzeodot();
extern void ppc_and();
extern void ppc_anddot();
extern void ppc_andc();
extern void ppc_andcdot();
extern void ppc_cmp();
extern void ppc_cmpl();
extern void ppc_cntlzw();
extern void ppc_cntlzwdot();
extern void ppc_dcbf();
extern void ppc_dcbi();
extern void ppc_dcbst();
extern void ppc_dcbt();
extern void ppc_dcbtst();
extern void ppc_dcbz();
extern void ppc_divw();
extern void ppc_divwdot();
extern void ppc_divwo();
extern void ppc_divwodot();
extern void ppc_divwu();
extern void ppc_divwudot();
extern void ppc_divwuo();
extern void ppc_divwuodot();
extern void ppc_eieio();
extern void ppc_eqv();
extern void ppc_eqvdot();
extern void ppc_extsb();
extern void ppc_extsbdot();
extern void ppc_extsh();
extern void ppc_extshdot();
extern void ppc_icbi();
extern void ppc_mftb();
extern void ppc_lhzux();
extern void ppc_lhzx();
extern void ppc_lhaux();
extern void ppc_lhax();
extern void ppc_lhbrx();
extern void ppc_lwarx();
extern void ppc_lbzux();
extern void ppc_lbzx();
extern void ppc_lwbrx();
extern void ppc_lwzux();
extern void ppc_lwzx();
extern void ppc_mcrxr();
extern void ppc_mfcr();
extern void ppc_mulhwu();
extern void ppc_mulhwudot();
extern void ppc_mulhw();
extern void ppc_mulhwdot();
extern void ppc_mullw();
extern void ppc_mullwdot();
extern void ppc_mullwo();
extern void ppc_mullwodot();
extern void ppc_nand();
extern void ppc_nanddot();
extern void ppc_neg();
extern void ppc_negdot();
extern void ppc_nego();
extern void ppc_negodot();
extern void ppc_nor();
extern void ppc_nordot();
extern void ppc_or();
extern void ppc_ordot();
extern void ppc_orc();
extern void ppc_orcdot();
extern void ppc_slw();
extern void ppc_slwdot();
extern void ppc_srw();
extern void ppc_srwdot();
extern void ppc_sraw();
extern void ppc_srawdot();
extern void ppc_srawi();
extern void ppc_srawidot();
extern void ppc_stbx();
extern void ppc_stbux();
extern void ppc_stfiwx();
extern void ppc_sthx();
extern void ppc_sthux();
extern void ppc_sthbrx();
extern void ppc_stwx();
extern void ppc_stwcx();
extern void ppc_stwux();
extern void ppc_stwbrx();
extern void ppc_subf();
extern void ppc_subfdot();
extern void ppc_subfo();
extern void ppc_subfodot();
extern void ppc_subfc();
extern void ppc_subfcdot();
extern void ppc_subfco();
extern void ppc_subfcodot();
extern void ppc_subfe();
extern void ppc_subfedot();
extern void ppc_subfme();
extern void ppc_subfmedot();
extern void ppc_subfze();
extern void ppc_subfzedot();
extern void ppc_sync();
extern void ppc_tlbia();
extern void ppc_tlbie();
extern void ppc_tlbli();
extern void ppc_tlbld();
extern void ppc_tlbsync();
extern void ppc_tw();
extern void ppc_xor();
extern void ppc_xordot();

extern void ppc_lswi();
extern void ppc_lswx();
extern void ppc_stswi();
extern void ppc_stswx();

extern void ppc_mfsr();
extern void ppc_mfsrin();
extern void ppc_mtsr();
extern void ppc_mtsrin();

extern void ppc_mtcrf();
extern void ppc_mfmsr();
extern void ppc_mfspr();
extern void ppc_mtmsr();
extern void ppc_mtspr();

extern void ppc_mtfsb0();
extern void ppc_mtfsb1();
extern void ppc_mcrfs();
extern void ppc_mtfsb0dot();
extern void ppc_mtfsb1dot();
extern void ppc_fmr();
extern void ppc_mffs();
extern void ppc_mffsdot();
extern void ppc_mtfsf();
extern void ppc_mtfsfdot();
extern void ppc_mtfsfi();
extern void ppc_mtfsfidot();

extern void ppc_addi();
extern void ppc_addic();
extern void ppc_addicdot();
extern void ppc_addis();
extern void ppc_andidot();
extern void ppc_andisdot();
extern void ppc_b();
extern void ppc_ba();
extern void ppc_bl();
extern void ppc_bla();
extern void ppc_bc();
extern void ppc_bca();
extern void ppc_bcl();
extern void ppc_bcla();
extern void ppc_cmpi();
extern void ppc_cmpli();
extern void ppc_lbz();
extern void ppc_lbzu();
extern void ppc_lha();
extern void ppc_lhau();
extern void ppc_lhz();
extern void ppc_lhzu();
extern void ppc_lwz();
extern void ppc_lwzu();
extern void ppc_lmw();
extern void ppc_mulli();
extern void ppc_ori();
extern void ppc_oris();
extern void ppc_rfi();
extern void ppc_rlwimi();
extern void ppc_rlwinm();
extern void ppc_rlwnm();
extern void ppc_sc();
extern void ppc_stb();
extern void ppc_stbu();
extern void ppc_sth();
extern void ppc_sthu();
extern void ppc_stw();
extern void ppc_stwu();
extern void ppc_stmw();
extern void ppc_subfic();
extern void ppc_twi();
extern void ppc_xori();
extern void ppc_xoris();

extern void ppc_lfs();
extern void ppc_lfsu();
extern void ppc_lfsux();
extern void ppc_lfsx();
extern void ppc_lfd();
extern void ppc_lfdu();
extern void ppc_stfs();
extern void ppc_stfsu();
extern void ppc_stfsux();
extern void ppc_stfd();
extern void ppc_stfdu();
extern void ppc_stfdx();
extern void ppc_stfdux();

extern void ppc_fadd();
extern void ppc_fsub();
extern void ppc_fmult();
extern void ppc_fdiv();
extern void ppc_fadds();
extern void ppc_fsubs();
extern void ppc_fmults();
extern void ppc_fdivs();
extern void ppc_fmadd();
extern void ppc_fmsub();
extern void ppc_fnmadd();
extern void ppc_fnmsub();
extern void ppc_fmadds();
extern void ppc_fmsubs();
extern void ppc_fnmadds();
extern void ppc_fnmsubs();
extern void ppc_fabs();
extern void ppc_fnabs();
extern void ppc_fneg();
extern void ppc_fsel();
extern void ppc_fres();
extern void ppc_fsqrts();
extern void ppc_fsqrt();
extern void ppc_frsqrte();
extern void ppc_frsp();
extern void ppc_fctiw();
extern void ppc_fctiwz();

extern void ppc_fadddot();
extern void ppc_fsubdot();
extern void ppc_fmultdot();
extern void ppc_fdivdot();
extern void ppc_fmadddot();
extern void ppc_fmsubdot();
extern void ppc_fnmadddot();
extern void ppc_fnmsubdot();
extern void ppc_fabsdot();
extern void ppc_fnabsdot();
extern void ppc_fnegdot();
extern void ppc_fseldot();
extern void ppc_fsqrtdot();
extern void ppc_frsqrtedot();
extern void ppc_frspdot();
extern void ppc_fctiwdot();
extern void ppc_fctiwzdot();

extern void ppc_fresdot();
extern void ppc_faddsdot();
extern void ppc_fsubsdot();
extern void ppc_fmultsdot();
extern void ppc_fdivsdot();
extern void ppc_fmaddsdot();
extern void ppc_fmsubsdot();
extern void ppc_fnmaddsdot();
extern void ppc_fnmsubsdot();
extern void ppc_fsqrtsdot();

extern void ppc_fcmpo();
extern void ppc_fcmpu();

//Power-specific instructions
extern void power_abs();
extern void power_absdot();
extern void power_abso();
extern void power_absodot();
extern void power_clcs();
extern void power_clcsdot();
extern void power_div();
extern void power_divdot();
extern void power_divo();
extern void power_divodot();
extern void power_divs();
extern void power_divsdot();
extern void power_divso();
extern void power_divsodot();
extern void power_doz();
extern void power_dozdot();
extern void power_dozo();
extern void power_dozodot();
extern void power_dozi();
extern void power_lscbx();
extern void power_lscbxdot();
extern void power_maskg();
extern void power_maskgdot();
extern void power_maskir();
extern void power_maskirdot();
extern void power_mul();
extern void power_muldot();
extern void power_mulo();
extern void power_mulodot();
extern void power_nabs();
extern void power_nabsdot();
extern void power_nabso();
extern void power_nabsodot();
extern void power_rlmi();
extern void power_rrib();
extern void power_rribdot();
extern void power_sle();
extern void power_sledot();
extern void power_sleq();
extern void power_sleqdot();
extern void power_sliq();
extern void power_sliqdot();
extern void power_slliq();
extern void power_slliqdot();
extern void power_sllq();
extern void power_sllqdot();
extern void power_slq();
extern void power_slqdot();
extern void power_sraiq();
extern void power_sraiqdot();
extern void power_sraq();
extern void power_sraqdot();
extern void power_sre();
extern void power_sredot();
extern void power_srea();
extern void power_sreadot();
extern void power_sreq();
extern void power_sreqdot();
extern void power_sriq();
extern void power_sriqdot();
extern void power_srliq();
extern void power_srliqdot();
extern void power_srlq();
extern void power_srlqdot();
extern void power_srq();
extern void power_srqdot();

//Gekko instructions
extern void ppc_psq_l();
extern void ppc_psq_lu();
extern void ppc_psq_st();
extern void ppc_psq_stu();

//AltiVec instructions

//64-bit instructions

//G5+ instructions

extern void ppc_main_opcode(void);
extern void ppc_exec(void);
extern void ppc_exec_single(void);
extern void ppc_exec_until(uint32_t goal_addr);

#endif /* PPCEMU_H */
