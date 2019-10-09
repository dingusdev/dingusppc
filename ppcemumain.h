//DingusPPC  - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef PPCEMUMAIN_H_
#define PPCEMUMAIN_H_

#include <map>

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

extern uint32_t rom_file_setsize;

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

extern uint16_t rev_endian16(uint16_t insert_int);
extern uint32_t uimm_rev_endian16(uint32_t insert_int);
extern int32_t simm_rev_endian16(int32_t insert_int);
extern uint32_t rev_endian32(uint32_t insert_int);
extern uint64_t rev_endian64(uint64_t insert_int);

extern bool grab_branch;
extern bool grab_exception;
extern bool grab_return;

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

void ppc_tbr_update();
void ppc_exception_handler(uint32_t exception_type, uint32_t handle_args);

//MEMORY DECLARATIONS
extern unsigned char * machine_sysram_mem;
extern unsigned char * machine_sysconfig_mem;
//Mapped to 0x68000000 - extern unsigned char * machine_68kemu_mem;
extern unsigned char * machine_upperiocontrol_mem;
extern unsigned char * machine_iocontrolcdma_mem;
extern unsigned char * machine_loweriocontrol_mem;
extern unsigned char * machine_interruptack_mem;
extern unsigned char * machine_iocontrolmem_mem;
extern unsigned char * machine_f8xxxx_mem;
extern unsigned char * machine_fexxxx_mem;
extern unsigned char * machine_fecxxx_mem;
extern unsigned char * machine_feexxx_mem;
extern unsigned char * machine_ff00xx_mem;
extern unsigned char * machine_ff80xx_mem;
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

extern void ppc_main_opcode();

//All of the opcodes possible are generated from the first 6 bits
//of each instruction given by the processor.


     /**These next few tables combine the last 11 bits of an instruction
     to generate an opcode. These means different opcodes for up to
     four different forms of a function.

     There are four big families of opcodes

     Opcode 19 - General Conditional Register Functions
     Opcode 31 - General Integer Functions
     Opcode 59 - Single-Precision Floating-Point Functions
     Opcode 63 - Double-Precision Floating-Point Functions

     The last two have instructions that don't neatly feat into one
     table, partially due to instructions that use either 6 or 11 bits
     for the instruction.
     **/

static std::map<uint16_t, PPCOpcode> SubOpcode19Grabber=
    {{32, &ppc_bclr},    {33, &ppc_bclrl},  {66, &ppc_crnor},   {100, &ppc_rfi},
     {258, &ppc_crandc}, {300, &ppc_isync}, {386, &ppc_crxor}, {450, &ppc_crnand},
     {514, &ppc_crand},  {578, &ppc_creqv},  {834, &ppc_crorc}, {898, &ppc_cror},
     {1056, &ppc_bcctr}, {1057, &ppc_bcctrl}
    };

static std::map<uint16_t, PPCOpcode> SubOpcode31Grabber=
    {{0, &ppc_cmp},          {8, &ppc_tw},            {16, &ppc_subfc},       {17, &ppc_subfcdot},
     {20, &ppc_addc},        {21, &ppc_addcdot},      {22, &ppc_mulhwu},      {23, &ppc_mulhwudot},
     {38, &ppc_mfcr},        {40, &ppc_lwarx},        {46, &ppc_lwzx},        {48, &ppc_slw},
     {49, &ppc_slwdot},      {52, &ppc_cntlzw},       {53, &ppc_cntlzwdot},   {56, &ppc_and},
     {57, &ppc_anddot},      {58, &power_maskg},      {59, &power_maskgdot},
     {64, &ppc_cmpl},        {80, &ppc_subf},         {81, &ppc_subfdot},
     {108, &ppc_dcbst},      {110, &ppc_lwzux},       {120, &ppc_andc},       {121, &ppc_andcdot},
     {150, &ppc_mulhw},      {151, &ppc_mulhwdot},    {166, &ppc_mfmsr},      {172, &ppc_dcbf},
     {174, &ppc_lbzx},       {208, &ppc_neg},         {209, &ppc_negdot},
     {214, &power_mul},      {215, &power_mulo},      {238, &ppc_lbzux},
     {248, &ppc_nor},        {249, &ppc_nordot},      {272, &ppc_subfe},      {273, &ppc_subfedot},
     {276, &ppc_adde},       {277, &ppc_addedot},     {288, &ppc_mtcrf},      {292, &ppc_mtmsr},
     {301, &ppc_stwcx},      {302, &ppc_stwx},        {304, &power_slq},      {305, &power_slqdot},
     {306, &power_sle},      {307, &power_sledot},    {366, &ppc_stwux},
     {368, &power_sliq},     {400, &ppc_subfze},
     {401, &ppc_subfzedot},  {404, &ppc_addze},       {405, &ppc_addzedot},   {420, &ppc_mtsr},
     {430, &ppc_stbx},       {432, &power_sllq},      {433, &power_sllqdot},  {434, &power_sleq},
     {436, &power_sleqdot},  {464, &ppc_subfme},      {465, &ppc_subfmedot},  {468, &ppc_addme},
     {469, &ppc_addmedot},   {470, &ppc_mullw},       {471, &ppc_mullwdot},   {484, &ppc_mtsrin},
     {492, &ppc_dcbtst},     {494, &ppc_stbux},       {496, &power_slliq},    {497, &power_slliqdot},
     {528, &power_doz},      {529, &power_dozdot},    {532, &ppc_add},        {533, &ppc_adddot},
     {554, &power_lscbx},    {555, &power_lscbxdot},  {556, &ppc_dcbt},
     {558, &ppc_lhzx},       {568, &ppc_eqv},         {569, &ppc_eqvdot},     {612, &ppc_tlbie},
     {622, &ppc_lhzux},      {632, &ppc_xor},         {633, &ppc_xordot},     {662, &power_div},
     {663, &power_divdot},   {678, &ppc_mfspr},       {686, &ppc_lhax},
     {720, &power_abs},      {721, &power_absdot},    {726, &power_divs},     {727, &power_divsdot},
     {740, &ppc_tlbia},      {742, &ppc_mftb},        {750, &ppc_lhaux},      {814, &ppc_sthx},
     {824, &ppc_orc},        {825, &ppc_orcdot},      {878, &ppc_sthx},
     {888, &ppc_or},         {889, &ppc_ordot},       {918, &ppc_divwu},      {919, &ppc_divwudot},
     {934, &ppc_mtspr},      {940, &ppc_dcbi},        {952, &ppc_nand},       {953, &ppc_nanddot},
     {976, &power_nabs},     {977, &power_nabsdot},   {982, &ppc_divw},       {983, &ppc_divwdot},
     {1024, &ppc_mcrxr},     {1040, &ppc_subfco},     {1041, &ppc_subfcodot}, {1044, &ppc_addco},
     {1045, &ppc_addcodot},  {1062, &power_clcs},     {1063, &power_clcsdot}, {1066, &ppc_lswx},
     {1068, &ppc_lwbrx},     {1070, &ppc_lfsx},       {1072, &ppc_srw},       {1073, &ppc_srwdot},
     {1074, &power_rrib},    {1075, &power_rribdot},  {1082, &power_maskir},  {1083, &power_maskirdot},
     {1104, &ppc_subfo},     {1105, &ppc_subfodot},   {1134, &ppc_lfsux},     {1190, &ppc_mfsr},
     {1194, &ppc_lswi},      {1196, &ppc_sync},       {1232, &ppc_nego},      {1233, &ppc_negodot},
     {1238, &power_mulo},    {1239, &power_mulodot},
     {1300, &ppc_addeo},     {1301, &ppc_addeodot},   {1318, &ppc_mfsrin},    {1322, &ppc_stswx},
     {1324, &ppc_stwbrx},    {1328, &power_srq},      {1329, &power_srqdot},  {1330, &power_sre},
     {1331, &power_sredot},  {1390, &ppc_stfsux},     {1392, &power_sriq},    {1393, &power_sriqdot},
     {1428, &ppc_addzeo},    {1429, &ppc_addzeodot},  {1450, &ppc_stswi},     {1454, &ppc_stfdx},
     {1456, &power_srlq},    {1457, &power_srlqdot},  {1458, &power_sreq},    {1459, &power_sreqdot},
     {1492, &ppc_addmeo},    {1493, &ppc_addmeodot},
     {1494, &ppc_mullwo},    {1495, &ppc_mullwodot},  {1518, &ppc_stfdux},    {1520, &power_srliq},
     {1521, &power_srliqdot},{1552, &power_dozo},     {1553, &power_dozodot}, {1556, &ppc_addo},
     {1557, &ppc_addodot},   {1580, &ppc_lhbrx},      {1584, &ppc_sraw},      {1585, &ppc_srawdot},
     {1648, &ppc_srawi},     {1649, &ppc_srawidot},   {1686, &power_divo},    {1687, &power_divodot},
     {1708, &ppc_eieio},     {1744, &power_abso},     {1745, &power_absodot},
     {1750, &power_divso},   {1751, &power_divsodot}, {1836, &ppc_sthbrx},
     {1840, &power_sraq},    {1841, &power_sraqdot},  {1842, &power_srea},    {1843, &power_sreadot},
     {1844, &ppc_extsh},     {1845, &ppc_extshdot},   {1904, &power_sraiq},   {1905, &power_sraiqdot},
     {1908, &ppc_extsb},     {1909, &ppc_extsbdot},   {1942, &ppc_divwuo},
     {1943, &ppc_divwuodot}, {1956, &ppc_tlbld},      {1964, &ppc_icbi},      {1966, &ppc_stfiwx},
     {2000, &power_nabso},   {2001, &power_nabsodot}, {2006, &ppc_divwo},     {2007, &ppc_divwodot},
     {2020, &ppc_tlbli},     {2028, &ppc_dcbz}
    };

static std::map<uint16_t, PPCOpcode> SubOpcode59Grabber=
    {{36, &ppc_fdivs},    {37, &ppc_fdivsdot},  {40, &ppc_fsubs},    {41, &ppc_fsubsdot},
     {42, &ppc_fadds},    {43, &ppc_faddsdot},  {44, &ppc_fsqrts},   {45, &ppc_fsqrtsdot},
     {48, &ppc_fres},     {49, &ppc_fresdot},
     {50, &ppc_fmults},   {51, &ppc_fmultsdot}, {56, &ppc_fmsubs}, {57, &ppc_fmsubsdot},
     {58, &ppc_fmadds},   {59, &ppc_fmaddsdot}, {60, &ppc_fnmsubs},{61, &ppc_fnmsubsdot},
     {62, &ppc_fnmadds},  {63, &ppc_fnmaddsdot},
     {114, &ppc_fmults},    {115, &ppc_fmultsdot},
     {120, &ppc_fmsubs},  {121, &ppc_fmsubsdot}, {122, &ppc_fmadds},    {123, &ppc_fmadds},
     {124, &ppc_fnmsubs}, {125, &ppc_fnmsubsdot},{126, &ppc_fnmadds},   {127, &ppc_fnmaddsdot},
     {178, &ppc_fmults}, {179, &ppc_fmultsdot},
     {184, &ppc_fmsubs}, {185, &ppc_fmsubsdot},  {186, &ppc_fmadds}, {187, &ppc_fmaddsdot},
     {188, &ppc_fnmsubs},{189, &ppc_fnmsubsdot}, {190, &ppc_fnmadds},{191, &ppc_fnmaddsdot},
     {242, &ppc_fmults}, {243, &ppc_fmultsdot},
     {248, &ppc_fmsubs}, {249, &ppc_fmsubsdot},  {250, &ppc_fmadds}, {251, &ppc_fmaddsdot},
     {252, &ppc_fnmsubs},{253, &ppc_fnmsubsdot}, {254, &ppc_fnmadds},{255, &ppc_fnmaddsdot},
     {306, &ppc_fmults}, {307, &ppc_fmultsdot},
     {312, &ppc_fmsubs}, {313, &ppc_fmsubsdot}, {314, &ppc_fmadds}, {315, &ppc_fmaddsdot},
     {316, &ppc_fnmsubs},{317, &ppc_fnmsubsdot},{318, &ppc_fnmadds},{319, &ppc_fnmaddsdot},
     {370, &ppc_fmults}, {371, &ppc_fmultsdot},
     {376, &ppc_fmsubs}, {377, &ppc_fmsubsdot}, {378, &ppc_fmadds}, {379, &ppc_fmaddsdot},
     {380, &ppc_fnmsubs},{381, &ppc_fnmsubsdot},{382, &ppc_fnmadds},{383, &ppc_fnmaddsdot},
     {434, &ppc_fmults}, {435, &ppc_fmultsdot},
     {440, &ppc_fmsubs}, {441, &ppc_fmsubsdot}, {442, &ppc_fmadds}, {443, &ppc_fmaddsdot},
     {444, &ppc_fnmsubs},{445, &ppc_fnmsubsdot},{446, &ppc_fnmadds},{447, &ppc_fnmaddsdot},
     {498, &ppc_fmults}, {499, &ppc_fmultsdot},
     {504, &ppc_fmsubs}, {505, &ppc_fmsubsdot}, {506, &ppc_fmadds}, {507, &ppc_fmaddsdot},
     {508, &ppc_fnmsubs},{509, &ppc_fnmsubsdot},{510, &ppc_fnmadds},{511, &ppc_fnmaddsdot},
     {562, &ppc_fmults}, {563, &ppc_fmultsdot},
     {568, &ppc_fmsubs}, {569, &ppc_fmsubsdot}, {570, &ppc_fmadds}, {571, &ppc_fmaddsdot},
     {572, &ppc_fnmsubs},{573, &ppc_fnmsubsdot},{574, &ppc_fnmadds},{575, &ppc_fnmaddsdot},
     {626, &ppc_fmults}, {627, &ppc_fmultsdot},
     {632, &ppc_fmsubs}, {633, &ppc_fmsubsdot}, {634, &ppc_fmadds}, {635, &ppc_fmaddsdot},
     {636, &ppc_fnmsubs},{637, &ppc_fnmsubsdot},{638, &ppc_fnmadds},{639, &ppc_fnmaddsdot},
     {690, &ppc_fmults}, {691, &ppc_fmultsdot},
     {696, &ppc_fmsubs}, {697, &ppc_fmsubsdot}, {698, &ppc_fmadds}, {699, &ppc_fmaddsdot},
     {700, &ppc_fnmsubs},{701, &ppc_fnmsubsdot},{702, &ppc_fnmadds},{703, &ppc_fnmaddsdot},
     {754, &ppc_fmults}, {755, &ppc_fmultsdot},
     {760, &ppc_fmsubs}, {761, &ppc_fmsubsdot}, {762, &ppc_fmadds}, {763, &ppc_fmaddsdot},
     {764, &ppc_fnmsubs},{765, &ppc_fnmsubsdot},{766, &ppc_fnmadds},{767, &ppc_fnmaddsdot},
     {818, &ppc_fmults}, {819, &ppc_fmultsdot},
     {824, &ppc_fmsubs}, {825, &ppc_fmsubsdot}, {826, &ppc_fmadds}, {827, &ppc_fmaddsdot},
     {828, &ppc_fnmsubs},{829, &ppc_fnmsubsdot},{830, &ppc_fnmadds},{831, &ppc_fnmaddsdot},
     {882, &ppc_fmults}, {883, &ppc_fmultsdot},
     {888, &ppc_fmsubs}, {889, &ppc_fmsubsdot}, {890, &ppc_fmadds}, {891, &ppc_fmaddsdot},
     {892, &ppc_fnmsubs},{893, &ppc_fnmsubsdot},{894, &ppc_fnmadds},{895, &ppc_fnmaddsdot},
     {946, &ppc_fmults}, {947, &ppc_fmultsdot},
     {952, &ppc_fmsubs}, {953, &ppc_fmsubsdot}, {954, &ppc_fmadds}, {955, &ppc_fmaddsdot},
     {957, &ppc_fnmsubs},{958, &ppc_fnmsubsdot},{958, &ppc_fnmadds},{959, &ppc_fnmaddsdot},
     {1010, &ppc_fmults}, {1011, &ppc_fmultsdot},
     {1016, &ppc_fmsubs}, {1017, &ppc_fmsubsdot}, {1018, &ppc_fmadds}, {1019, &ppc_fmaddsdot},
     {1020, &ppc_fnmsubs},{1021, &ppc_fnmsubsdot},{1022, &ppc_fnmadds},{1023, &ppc_fnmaddsdot},
     {1074, &ppc_fmults}, {1075, &ppc_fmultsdot},
     {1080, &ppc_fmsubs}, {1081, &ppc_fmsubsdot}, {1082, &ppc_fmadds}, {1083, &ppc_fmaddsdot},
     {1084, &ppc_fnmsubs},{1085, &ppc_fnmsubsdot},{1086, &ppc_fnmadds},{1087, &ppc_fnmaddsdot},
     {1138, &ppc_fmults}, {1139, &ppc_fmultsdot},
     {1144, &ppc_fmsubs}, {1145, &ppc_fmsubsdot}, {1146, &ppc_fmadds}, {1147, &ppc_fmaddsdot},
     {1148, &ppc_fnmsubs},{1149, &ppc_fnmsubsdot},{1150, &ppc_fnmadds},{1151, &ppc_fnmaddsdot},
     {1202, &ppc_fmults}, {1203, &ppc_fmultsdot},
     {1208, &ppc_fmsubs}, {1209, &ppc_fmsubsdot}, {1210, &ppc_fmadds}, {1211, &ppc_fmaddsdot},
     {1212, &ppc_fnmsubs},{1213, &ppc_fnmsubsdot},{1214, &ppc_fnmadds},{1215, &ppc_fnmaddsdot},
     {1266, &ppc_fmults}, {1267, &ppc_fmultsdot},
     {1272, &ppc_fmsubs}, {1273, &ppc_fmsubsdot}, {1274, &ppc_fmadds}, {1275, &ppc_fmaddsdot},
     {1276, &ppc_fnmsubs},{1277, &ppc_fnmsubsdot},{1278, &ppc_fnmadds},{1279, &ppc_fnmaddsdot},
     {1330, &ppc_fmults}, {1331, &ppc_fmultsdot},
     {1336, &ppc_fmsubs}, {1337, &ppc_fmsubsdot}, {1338, &ppc_fmadds}, {1339, &ppc_fmaddsdot},
     {1340, &ppc_fnmsubs},{1341, &ppc_fnmsubsdot},{1342, &ppc_fnmadds},{1343, &ppc_fnmaddsdot},
     {1394, &ppc_fmults}, {1395, &ppc_fmultsdot},
     {1400, &ppc_fmsubs}, {1401, &ppc_fmsubsdot}, {1402, &ppc_fmadds}, {1403, &ppc_fmaddsdot},
     {1404, &ppc_fnmsubs},{1405, &ppc_fnmsubsdot},{1406, &ppc_fnmadds},{1407, &ppc_fnmaddsdot},
     {1458, &ppc_fmults}, {1459, &ppc_fmultsdot},
     {1464, &ppc_fmsubs}, {1465, &ppc_fmsubsdot}, {1466, &ppc_fmadds}, {1467, &ppc_fmaddsdot},
     {1468, &ppc_fnmsubs},{1469, &ppc_fnmsubsdot},{1470, &ppc_fnmadds},{1471, &ppc_fnmaddsdot},
     {1522, &ppc_fmults}, {1523, &ppc_fmultsdot},
     {1528, &ppc_fmsubs}, {1529, &ppc_fmsubsdot}, {1530, &ppc_fmadds}, {1531, &ppc_fmaddsdot},
     {1532, &ppc_fnmsubs},{1533, &ppc_fnmsubsdot},{1534, &ppc_fnmadds},{1535, &ppc_fnmaddsdot},
     {1586, &ppc_fmults}, {1587, &ppc_fmultsdot},
     {1592, &ppc_fmsubs}, {1593, &ppc_fmsubsdot}, {1594, &ppc_fmadds}, {1595, &ppc_fmaddsdot},
     {1596, &ppc_fnmsubs},{1597, &ppc_fnmsubsdot},{1598, &ppc_fnmadds},{1599, &ppc_fnmaddsdot},
     {1650, &ppc_fmults}, {1651, &ppc_fmultsdot},
     {1656, &ppc_fmsubs}, {1657, &ppc_fmsubsdot}, {1658, &ppc_fmadds}, {1659, &ppc_fmaddsdot},
     {1660, &ppc_fnmsubs},{1661, &ppc_fnmsubsdot},{1662, &ppc_fnmadds},{1663, &ppc_fnmaddsdot},
     {1714, &ppc_fmults}, {1715, &ppc_fmultsdot},
     {1720, &ppc_fmsubs}, {1721, &ppc_fmsubsdot}, {1722, &ppc_fmadds}, {1723, &ppc_fmaddsdot},
     {1724, &ppc_fnmsubs},{1725, &ppc_fnmsubsdot},{1726, &ppc_fnmadds},{1727, &ppc_fnmaddsdot},
     {1778, &ppc_fmults}, {1779, &ppc_fmultsdot},
     {1784, &ppc_fmsubs}, {1785, &ppc_fmsubsdot}, {1786, &ppc_fmadds}, {1787, &ppc_fmaddsdot},
     {1788, &ppc_fnmsubs},{1789, &ppc_fnmsubsdot},{1790, &ppc_fnmadds},{1791, &ppc_fnmaddsdot},
     {1842, &ppc_fmults}, {1843, &ppc_fmultsdot},
     {1848, &ppc_fmsubs}, {1849, &ppc_fmsubsdot}, {1850, &ppc_fmadds}, {1851, &ppc_fmaddsdot},
     {1852, &ppc_fnmsubs},{1853, &ppc_fnmsubsdot},{1854, &ppc_fnmadds},{1855, &ppc_fnmaddsdot},
     {1906, &ppc_fmults}, {1907, &ppc_fmultsdot},
     {1912, &ppc_fmsubs}, {1913, &ppc_fmsubsdot}, {1914, &ppc_fmadds}, {1915, &ppc_fmaddsdot},
     {1916, &ppc_fnmsubs},{1917, &ppc_fnmsubsdot},{1918, &ppc_fnmadds},{1919, &ppc_fnmaddsdot},
     {1970, &ppc_fmults}, {1971, &ppc_fmultsdot},
     {1976, &ppc_fmsubs}, {1977, &ppc_fmsubsdot}, {1978, &ppc_fmadds}, {1979, &ppc_fmaddsdot},
     {1980, &ppc_fnmsubs},{1981, &ppc_fnmsubsdot},{1982, &ppc_fnmadds},{1983, &ppc_fnmaddsdot},
     {2034, &ppc_fmults}, {2035, &ppc_fmultsdot},
     {2040, &ppc_fmsubs}, {2041, &ppc_fmsubsdot}, {2042, &ppc_fmadds}, {2043, &ppc_fmaddsdot},
     {2044, &ppc_fnmsubs},{2045, &ppc_fnmsubsdot},{2046, &ppc_fnmadds},{2047, &ppc_fnmaddsdot}
     };

static std::map<uint16_t, PPCOpcode> SubOpcode63Grabber=
    {{0, &ppc_fcmpu},      {24, &ppc_frsp},     {25, &ppc_frspdot},
     {28, &ppc_fctiw},     {29, &ppc_fctiwdot}, {30, &ppc_fctiwz}, {31, &ppc_fctiwzdot},
     {36, &ppc_fdiv},      {37, &ppc_fdivdot},  {40, &ppc_fsub},   {41, &ppc_fsubdot},
     {42, &ppc_fadd},      {43, &ppc_fadddot},  {44, &ppc_fsqrt},  {45, &ppc_fsqrtdot},
     {46, &ppc_fsel},      {47, &ppc_fseldot},   {50, &ppc_fmult}, {51, &ppc_fmultdot},
     {52, &ppc_frsqrte},   {53, &ppc_frsqrtedot},{56, &ppc_fmsub}, {57, &ppc_fmsubdot},
     {58, &ppc_fmadd},     {59, &ppc_fmadddot},  {60, &ppc_fnmsub},{61, &ppc_fnmsubdot},
     {62, &ppc_fnmadd},    {63, &ppc_fnmadddot}, {64, &ppc_fcmpo}, {76, &ppc_mtfsb1},
     {77, &ppc_mtfsb1dot}, {80, &ppc_fneg}, {81, &ppc_fnegdot},
     {110, &ppc_fsel},   {111, &ppc_fseldot},  {114, &ppc_fmult},    {115, &ppc_fmultdot},
     {120, &ppc_fmsub},  {121, &ppc_fmsubdot}, {122, &ppc_fmadd},    {123, &ppc_fmadd},
     {124, &ppc_fnmsub}, {125, &ppc_fnmsubdot},{126, &ppc_fnmadd},   {127, &ppc_fnmadddot},
     {128, &ppc_mcrfs},  {140, &ppc_mtfsb0},   {141, &ppc_mtfsb0dot},{144, &ppc_fmr},
     {174, &ppc_fsel},  {175, &ppc_fseldot},   {178, &ppc_fmult}, {179, &ppc_fmultdot},
     {184, &ppc_fmsub}, {185, &ppc_fmsubdot},  {186, &ppc_fmadd}, {187, &ppc_fmadddot},
     {188, &ppc_fnmsub},{189, &ppc_fnmsubdot}, {190, &ppc_fnmadd},{191, &ppc_fnmadddot},
     {238, &ppc_fsel},  {239, &ppc_fseldot},   {242, &ppc_fmult}, {243, &ppc_fmultdot},
     {248, &ppc_fmsub}, {249, &ppc_fmsubdot},  {250, &ppc_fmadd}, {251, &ppc_fmadddot},
     {252, &ppc_fnmsub},{253, &ppc_fnmsubdot}, {254, &ppc_fnmadd},{255, &ppc_fnmadddot},
     {272, &ppc_fnabs}, {273, &ppc_fnabsdot},
     {302, &ppc_fsel},  {303, &ppc_fseldot},  {306, &ppc_fmult}, {307, &ppc_fmultdot},
     {312, &ppc_fmsub}, {313, &ppc_fmsubdot}, {314, &ppc_fmadd}, {315, &ppc_fmadddot},
     {316, &ppc_fnmsub},{317, &ppc_fnmsubdot},{318, &ppc_fnmadd},{319, &ppc_fnmadddot},
     {366, &ppc_fsel},  {367, &ppc_fseldot},  {370, &ppc_fmult}, {371, &ppc_fmultdot},
     {376, &ppc_fmsub}, {377, &ppc_fmsubdot}, {378, &ppc_fmadd}, {379, &ppc_fmadddot},
     {380, &ppc_fnmsub},{381, &ppc_fnmsubdot},{382, &ppc_fnmadd},{383, &ppc_fnmadddot},
     {430, &ppc_fsel},  {431, &ppc_fseldot},  {434, &ppc_fmult}, {435, &ppc_fmultdot},
     {440, &ppc_fmsub}, {441, &ppc_fmsubdot}, {442, &ppc_fmadd}, {443, &ppc_fmadddot},
     {444, &ppc_fnmsub},{445, &ppc_fnmsubdot},{446, &ppc_fnmadd},{447, &ppc_fnmadddot},
     {494, &ppc_fsel},  {495, &ppc_fseldot},  {498, &ppc_fmult}, {499, &ppc_fmultdot},
     {504, &ppc_fmsub}, {505, &ppc_fmsubdot}, {506, &ppc_fmadd}, {507, &ppc_fmadddot},
     {508, &ppc_fnmsub},{509, &ppc_fnmsubdot},{510, &ppc_fnmadd},{511, &ppc_fnmadddot},
     {528, &ppc_fabs},  {529, &ppc_fabsdot},
     {558, &ppc_fsel},  {559, &ppc_fseldot},  {562, &ppc_fmult}, {563, &ppc_fmultdot},
     {568, &ppc_fmsub}, {569, &ppc_fmsubdot}, {570, &ppc_fmadd}, {571, &ppc_fmadddot},
     {572, &ppc_fnmsub},{573, &ppc_fnmsubdot},{574, &ppc_fnmadd},{575, &ppc_fnmadddot},
     {622, &ppc_fsel},  {623, &ppc_fseldot},  {626, &ppc_fmult}, {627, &ppc_fmultdot},
     {632, &ppc_fmsub}, {633, &ppc_fmsubdot}, {634, &ppc_fmadd}, {635, &ppc_fmadddot},
     {636, &ppc_fnmsub},{637, &ppc_fnmsubdot},{638, &ppc_fnmadd},{639, &ppc_fnmadddot},
     {686, &ppc_fsel},  {687, &ppc_fseldot},  {690, &ppc_fmult}, {691, &ppc_fmultdot},
     {696, &ppc_fmsub}, {697, &ppc_fmsubdot}, {698, &ppc_fmadd}, {699, &ppc_fmadddot},
     {700, &ppc_fnmsub},{701, &ppc_fnmsubdot},{702, &ppc_fnmadd},{703, &ppc_fnmadddot},
     {750, &ppc_fsel},  {751, &ppc_fseldot},  {754, &ppc_fmult}, {755, &ppc_fmultdot},
     {760, &ppc_fmsub}, {761, &ppc_fmsubdot}, {762, &ppc_fmadd}, {763, &ppc_fmadddot},
     {764, &ppc_fnmsub},{765, &ppc_fnmsubdot},{766, &ppc_fnmadd},{767, &ppc_fnmadddot},
     {814, &ppc_fsel},  {815, &ppc_fseldot},  {818, &ppc_fmult}, {819, &ppc_fmultdot},
     {824, &ppc_fmsub}, {825, &ppc_fmsubdot}, {826, &ppc_fmadd}, {827, &ppc_fmadddot},
     {828, &ppc_fnmsub},{829, &ppc_fnmsubdot},{830, &ppc_fnmadd},{831, &ppc_fnmadddot},
     {878, &ppc_fsel},  {879, &ppc_fseldot},  {882, &ppc_fmult}, {883, &ppc_fmultdot},
     {888, &ppc_fmsub}, {889, &ppc_fmsubdot}, {890, &ppc_fmadd}, {891, &ppc_fmadddot},
     {892, &ppc_fnmsub},{893, &ppc_fnmsubdot},{894, &ppc_fnmadd},{895, &ppc_fnmadddot},
     {942, &ppc_fsel},  {943, &ppc_fseldot},  {946, &ppc_fmult}, {947, &ppc_fmultdot},
     {952, &ppc_fmsub}, {953, &ppc_fmsubdot}, {954, &ppc_fmadd}, {955, &ppc_fmadddot},
     {957, &ppc_fnmsub},{958, &ppc_fnmsubdot},{958, &ppc_fnmadd},{959, &ppc_fnmadddot},
     {1006, &ppc_fsel},  {1007, &ppc_fseldot},  {1010, &ppc_fmult}, {1011, &ppc_fmultdot},
     {1016, &ppc_fmsub}, {1017, &ppc_fmsubdot}, {1018, &ppc_fmadd}, {1019, &ppc_fmadddot},
     {1020, &ppc_fnmsub},{1021, &ppc_fnmsubdot},{1022, &ppc_fnmadd},{1023, &ppc_fnmadddot},
     {1070, &ppc_fsel},  {1071, &ppc_fseldot},  {1074, &ppc_fmult}, {1075, &ppc_fmultdot},
     {1080, &ppc_fmsub}, {1081, &ppc_fmsubdot}, {1082, &ppc_fmadd}, {1083, &ppc_fmadddot},
     {1084, &ppc_fnmsub},{1085, &ppc_fnmsubdot},{1086, &ppc_fnmadd},{1087, &ppc_fnmadddot},
     {1134, &ppc_fsel},  {1135, &ppc_fseldot},  {1138, &ppc_fmult}, {1139, &ppc_fmultdot},
     {1144, &ppc_fmsub}, {1145, &ppc_fmsubdot}, {1146, &ppc_fmadd}, {1147, &ppc_fmadddot},
     {1148, &ppc_fnmsub},{1149, &ppc_fnmsubdot},{1150, &ppc_fnmadd},{1151, &ppc_fnmadddot},
     {1166, &ppc_mffs},  {1167, &ppc_mffsdot},
     {1198, &ppc_fsel},  {1199, &ppc_fseldot},  {1202, &ppc_fmult}, {1203, &ppc_fmultdot},
     {1208, &ppc_fmsub}, {1209, &ppc_fmsubdot}, {1210, &ppc_fmadd}, {1211, &ppc_fmadddot},
     {1212, &ppc_fnmsub},{1213, &ppc_fnmsubdot},{1214, &ppc_fnmadd},{1215, &ppc_fnmadddot},
     {1262, &ppc_fsel},  {1263, &ppc_fseldot},  {1266, &ppc_fmult}, {1267, &ppc_fmultdot},
     {1272, &ppc_fmsub}, {1273, &ppc_fmsubdot}, {1274, &ppc_fmadd}, {1275, &ppc_fmadddot},
     {1276, &ppc_fnmsub},{1277, &ppc_fnmsubdot},{1278, &ppc_fnmadd},{1279, &ppc_fnmadddot},
     {1326, &ppc_fsel},  {1327, &ppc_fseldot},  {1330, &ppc_fmult}, {1331, &ppc_fmultdot},
     {1336, &ppc_fmsub}, {1337, &ppc_fmsubdot}, {1338, &ppc_fmadd}, {1339, &ppc_fmadddot},
     {1340, &ppc_fnmsub},{1341, &ppc_fnmsubdot},{1342, &ppc_fnmadd},{1343, &ppc_fnmadddot},
     {1390, &ppc_fsel},  {1391, &ppc_fseldot},  {1394, &ppc_fmult}, {1395, &ppc_fmultdot},
     {1400, &ppc_fmsub}, {1401, &ppc_fmsubdot}, {1402, &ppc_fmadd}, {1403, &ppc_fmadddot},
     {1404, &ppc_fnmsub},{1405, &ppc_fnmsubdot},{1406, &ppc_fnmadd},{1407, &ppc_fnmadddot},
     {1422, &ppc_mtfsf}, {1423, &ppc_mtfsfdot},
     {1454, &ppc_fsel},  {1455, &ppc_fseldot},  {1458, &ppc_fmult}, {1459, &ppc_fmultdot},
     {1464, &ppc_fmsub}, {1465, &ppc_fmsubdot}, {1466, &ppc_fmadd}, {1467, &ppc_fmadddot},
     {1468, &ppc_fnmsub},{1469, &ppc_fnmsubdot},{1470, &ppc_fnmadd},{1471, &ppc_fnmadddot},
     {1518, &ppc_fsel},  {1519, &ppc_fseldot},  {1522, &ppc_fmult}, {1523, &ppc_fmultdot},
     {1528, &ppc_fmsub}, {1529, &ppc_fmsubdot}, {1530, &ppc_fmadd}, {1531, &ppc_fmadddot},
     {1532, &ppc_fnmsub},{1533, &ppc_fnmsubdot},{1534, &ppc_fnmadd},{1535, &ppc_fnmadddot},
     {1582, &ppc_fsel},  {1583, &ppc_fseldot},  {1586, &ppc_fmult}, {1587, &ppc_fmultdot},
     {1592, &ppc_fmsub}, {1593, &ppc_fmsubdot}, {1594, &ppc_fmadd}, {1595, &ppc_fmadddot},
     {1596, &ppc_fnmsub},{1597, &ppc_fnmsubdot},{1598, &ppc_fnmadd},{1599, &ppc_fnmadddot},
     {1646, &ppc_fsel},  {1647, &ppc_fseldot},  {1650, &ppc_fmult}, {1651, &ppc_fmultdot},
     {1656, &ppc_fmsub}, {1657, &ppc_fmsubdot}, {1658, &ppc_fmadd}, {1659, &ppc_fmadddot},
     {1660, &ppc_fnmsub},{1661, &ppc_fnmsubdot},{1662, &ppc_fnmadd},{1663, &ppc_fnmadddot},
     {1710, &ppc_fsel},  {1711, &ppc_fseldot},  {1714, &ppc_fmult}, {1715, &ppc_fmultdot},
     {1720, &ppc_fmsub}, {1721, &ppc_fmsubdot}, {1722, &ppc_fmadd}, {1723, &ppc_fmadddot},
     {1724, &ppc_fnmsub},{1725, &ppc_fnmsubdot},{1726, &ppc_fnmadd},{1727, &ppc_fnmadddot},
     {1774, &ppc_fsel},  {1775, &ppc_fseldot},  {1778, &ppc_fmult}, {1779, &ppc_fmultdot},
     {1784, &ppc_fmsub}, {1785, &ppc_fmsubdot}, {1786, &ppc_fmadd}, {1787, &ppc_fmadddot},
     {1788, &ppc_fnmsub},{1789, &ppc_fnmsubdot},{1790, &ppc_fnmadd},{1791, &ppc_fnmadddot},
     {1838, &ppc_fsel},  {1839, &ppc_fseldot},  {1842, &ppc_fmult}, {1843, &ppc_fmultdot},
     {1848, &ppc_fmsub}, {1849, &ppc_fmsubdot}, {1850, &ppc_fmadd}, {1851, &ppc_fmadddot},
     {1852, &ppc_fnmsub},{1853, &ppc_fnmsubdot},{1854, &ppc_fnmadd},{1855, &ppc_fnmadddot},
     {1902, &ppc_fsel},  {1903, &ppc_fseldot},  {1906, &ppc_fmult}, {1907, &ppc_fmultdot},
     {1912, &ppc_fmsub}, {1913, &ppc_fmsubdot}, {1914, &ppc_fmadd}, {1915, &ppc_fmadddot},
     {1916, &ppc_fnmsub},{1917, &ppc_fnmsubdot},{1918, &ppc_fnmadd},{1919, &ppc_fnmadddot},
     {1966, &ppc_fsel},  {1967, &ppc_fseldot},  {1970, &ppc_fmult}, {1971, &ppc_fmultdot},
     {1976, &ppc_fmsub}, {1977, &ppc_fmsubdot}, {1978, &ppc_fmadd}, {1979, &ppc_fmadddot},
     {1980, &ppc_fnmsub},{1981, &ppc_fnmsubdot},{1982, &ppc_fnmadd},{1983, &ppc_fnmadddot},
     {2030, &ppc_fsel},  {2031, &ppc_fseldot},  {2034, &ppc_fmult}, {2035, &ppc_fmultdot},
     {2040, &ppc_fmsub}, {2041, &ppc_fmsubdot}, {2042, &ppc_fmadd}, {2043, &ppc_fmadddot},
     {2044, &ppc_fnmsub},{2045, &ppc_fnmsubdot},{2046, &ppc_fnmadd},{2047, &ppc_fnmadddot}
    };


#endif // PPCEMUMAIN_H
