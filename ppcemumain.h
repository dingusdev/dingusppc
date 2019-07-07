//DingusPPC  - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

#ifndef PPCEMUMAIN_H_
#define PPCEMUMAIN_H_

#include <map>

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

extern uint32_t pc_return_value; //used for getting instructions

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

extern bool is_601; //For PowerPC 601 Emulation
extern bool is_gekko; //For GameCube Emulation
extern bool is_altivec; //For Altivec Emulation
extern bool is_64bit; //For PowerPC G5 Emulation

//uint64_t virtual_address;

//Important Addressing Integers
extern uint64_t ppc_virtual_address; //It's 52 bits
extern uint32_t ppc_cur_instruction;
extern uint32_t ppc_effective_address;
extern uint32_t ppc_real_address;
extern uint32_t ppc_next_instruction_address;
extern uint32_t temp_address; //used for determining where memory ends up.

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
extern void ppc_grab_regssasimm();
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
void ppc_expection_handler(uint32_t exception_type, uint32_t handle_args);

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
extern void ppc_mtfsf();

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
extern void ppc_fneg();
extern void ppc_fsel();
extern void ppc_fres();
extern void ppc_fsqrts();
extern void ppc_fsqrt();
extern void ppc_frsqrte();
extern void ppc_frsp();
extern void ppc_fctiw();
extern void ppc_fctiwz();

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

//A listing of all of the available opcodes on the PPC.

static std::map<uint8_t, PPCOpcode> OpcodeGrabber =
    {{0, &ppc_illegalop},  {1, &ppc_illegalop},  {2, &ppc_illegalop},  {3, &ppc_twi},
     {4, &ppc_opcode4},    {5, &ppc_illegalop},  {6, &ppc_illegalop},  {7, &ppc_mulli},
     {8, &ppc_subfic},     {9, &power_dozi},     {10, &ppc_cmpli},     {11, &ppc_cmpi},
     {12, &ppc_addic},     {13, &ppc_addicdot},  {14, &ppc_addi},      {15, &ppc_addis},
     {16, &ppc_opcode16},  {17, &ppc_sc},        {18, &ppc_opcode18},  {19, &ppc_opcode19},
     {20, &ppc_rlwimi},    {21, &ppc_rlwinm},    {22, &power_rlmi},    {23, &ppc_rlwnm},
     {24, &ppc_ori},       {25, &ppc_oris},      {26, &ppc_xori},      {27, &ppc_xoris},
     {28, &ppc_andidot},   {29, &ppc_andisdot},  {30, &ppc_illegalop}, {31, &ppc_opcode31},
     {32, &ppc_lwz},       {33, &ppc_lwzu},      {34, &ppc_lbz},       {35, &ppc_lbzu},
     {36, &ppc_stw},       {37, &ppc_stwu},      {38, &ppc_stb},       {39, &ppc_stbu},
     {40, &ppc_lhz},       {41, &ppc_lhzu},      {42, &ppc_lha},       {43, &ppc_lhau},
     {44, &ppc_sth},       {45, &ppc_sthu},      {46, &ppc_lmw},       {47, &ppc_stmw},
     {48, &ppc_lfs},       {49, &ppc_lfsu},      {50, &ppc_lfd},       {51, &ppc_lfdu},
     {52, &ppc_stfs},      {53, &ppc_stfsu},     {54, &ppc_stfd},      {55, &ppc_stfdu},
     {56, &ppc_psq_l},     {57, &ppc_psq_lu},    {58, &ppc_illegalop}, {59, &ppc_illegalop},
     {60, &ppc_psq_st},    {61, &ppc_psq_stu},   {62, &ppc_illegalop}, {63, &ppc_opcode63}};

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
static std::map<uint8_t, PPCOpcode> SubOpcode16Grabber=
    {{0, &ppc_bc}, {1, &ppc_bcl}, {2, &ppc_bca}, {3, &ppc_bcla}
    };
static std::map<uint8_t, PPCOpcode> SubOpcode18Grabber=
    {{0, &ppc_b}, {1, &ppc_bl}, {2, &ppc_ba}, {3, &ppc_bla}
    };
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
     {1194, &ppc_lswi},      {1196, &ppc_isync},      {1232, &ppc_nego},      {1233, &ppc_negodot},
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
    {{36, &ppc_fdivs},    {40, &ppc_fsubs},     {42, &ppc_fadds},    {44, &ppc_fsqrts},     {48, &ppc_frsp},
     {50, &ppc_fmults},   {56, &ppc_fmsubs},    {58, &ppc_fmadds},   {60, &ppc_fnmsubs},    {62, &ppc_fnmadds},
     {114, &ppc_fmults},  {120, &ppc_fmsubs},   {122, &ppc_fmadds},  {124, &ppc_fnmsubs},   {126, &ppc_fnmadds},
     {178, &ppc_fmults},  {184, &ppc_fmsubs},   {186, &ppc_fmadds},  {188, &ppc_fnmsubs},   {190, &ppc_fnmadds},
     {242, &ppc_fmults},  {248, &ppc_fmsubs},   {250, &ppc_fmadds},  {252, &ppc_fnmsubs},   {254, &ppc_fnmadds},
     {306, &ppc_fmults},  {312, &ppc_fmsubs},   {314, &ppc_fmadds},  {316, &ppc_fnmsubs},   {318, &ppc_fnmadds},
     {370, &ppc_fmults},  {376, &ppc_fmsubs},   {378, &ppc_fmadds},  {380, &ppc_fnmsubs},   {382, &ppc_fnmadds},
     {434, &ppc_fmults},  {440, &ppc_fmsubs},   {442, &ppc_fmadds},  {444, &ppc_fnmsubs},   {446, &ppc_fnmadds},
     {498, &ppc_fmults},  {504, &ppc_fmsubs},   {506, &ppc_fmadds},  {508, &ppc_fnmsubs},   {510, &ppc_fnmadds},
     {562, &ppc_fmults},  {568, &ppc_fmsubs},   {570, &ppc_fmadds},  {572, &ppc_fnmsubs},   {574, &ppc_fnmadds},
     {626, &ppc_fmults},  {632, &ppc_fmsubs},   {634, &ppc_fmadds},  {636, &ppc_fnmsubs},   {638, &ppc_fnmadds},
     {690, &ppc_fmults},  {696, &ppc_fmsubs},   {698, &ppc_fmadds},  {700, &ppc_fnmsubs},   {702, &ppc_fnmadds},
     {754, &ppc_fmults},  {752, &ppc_fmsubs},   {754, &ppc_fmadds},  {764, &ppc_fnmsubs},   {766, &ppc_fnmadds},
     {818, &ppc_fmults},  {824, &ppc_fmsubs},   {826, &ppc_fmadds},  {828, &ppc_fnmsubs},   {830, &ppc_fnmadds},
     {882, &ppc_fmults},  {888, &ppc_fmsubs},   {890, &ppc_fmadds},  {892, &ppc_fnmsubs},   {894, &ppc_fnmadds},
     {946, &ppc_fmults},  {952, &ppc_fmsubs},   {954, &ppc_fmadds},  {956, &ppc_fnmsubs},   {958, &ppc_fnmadds},
     {1010, &ppc_fmults}, {1016, &ppc_fmsubs}, {1018, &ppc_fmadds}, {1020, &ppc_fnmsubs},   {1022, &ppc_fnmadds},
     {1074, &ppc_fmults}, {1080, &ppc_fmsubs}, {1082, &ppc_fmadds}, {1084, &ppc_fnmsubs},   {1086, &ppc_fnmadds},
     {1138, &ppc_fmults}, {1144, &ppc_fmsubs}, {1146, &ppc_fmadds}, {1148, &ppc_fnmsubs},   {1150, &ppc_fnmadds},
     {1202, &ppc_fmults}, {1208, &ppc_fmsubs}, {1210, &ppc_fmadds}, {1212, &ppc_fnmsubs},   {1214, &ppc_fnmadds},
     {1266, &ppc_fmults}, {1272, &ppc_fmsubs}, {1274, &ppc_fmadds}, {1276, &ppc_fnmsubs},   {1278, &ppc_fnmadds},
     {1330, &ppc_fmults}, {1336, &ppc_fmsubs}, {1338, &ppc_fmadds}, {1340, &ppc_fnmsubs},   {1342, &ppc_fnmadds},
     {1394, &ppc_fmults}, {1400, &ppc_fmsubs}, {1402, &ppc_fmadds}, {1404, &ppc_fnmsubs},   {1406, &ppc_fnmadds},
     {1458, &ppc_fmults}, {1464, &ppc_fmsubs}, {1466, &ppc_fmadds}, {1468, &ppc_fnmsubs},   {1470, &ppc_fnmadds},
     {1522, &ppc_fmults}, {1528, &ppc_fmsubs}, {1530, &ppc_fmadds}, {1532, &ppc_fnmsubs},   {1534, &ppc_fnmadds},
     {1586, &ppc_fmults}, {1592, &ppc_fmsubs}, {1594, &ppc_fmadds}, {1596, &ppc_fnmsubs},   {1598, &ppc_fnmadds},
     {1650, &ppc_fmults}, {1656, &ppc_fmsubs}, {1658, &ppc_fmadds}, {1660, &ppc_fnmsubs},   {1662, &ppc_fnmadds},
     {1714, &ppc_fmults}, {1720, &ppc_fmsubs}, {1722, &ppc_fmadds}, {1724, &ppc_fnmsubs},   {1726, &ppc_fnmadds},
     {1778, &ppc_fmults}, {1784, &ppc_fmsubs}, {1786, &ppc_fmadds}, {1788, &ppc_fnmsubs},   {1790, &ppc_fnmadds},
     {1842, &ppc_fmults}, {1848, &ppc_fmsubs}, {1850, &ppc_fmadds}, {1852, &ppc_fnmsubs},   {1854, &ppc_fnmadds},
     {1906, &ppc_fmults}, {1912, &ppc_fmsubs}, {1914, &ppc_fmadds}, {1916, &ppc_fnmsubs},   {1918, &ppc_fnmadds},
     {1970, &ppc_fmults}, {1976, &ppc_fmsubs}, {1978, &ppc_fmadds}, {1980, &ppc_fnmsubs},   {1982, &ppc_fnmadds},
     {2034, &ppc_fmults}, {2040, &ppc_fmsubs}, {2042, &ppc_fmadds}, {2044, &ppc_fnmsubs},   {2046, &ppc_fnmadds}
    };
static std::map<uint16_t, PPCOpcode> SubOpcode63Grabber=
    {{0, &ppc_fcmpu},      {24, &ppc_frsp},     {28, &ppc_fctiw},    {30, &ppc_fctiwz}, {36, &ppc_fdiv},
     {40, &ppc_fsub},      {42, &ppc_fadd},     {44, &ppc_fsqrt},  {50, &ppc_fmult},
     {52, &ppc_frsqrte},   {56, &ppc_fmsub},    {58, &ppc_fmadd},  {60, &ppc_fnmsub},
     {62, &ppc_fnmadd},    {64, &ppc_fcmpo},    {76, &ppc_mtfsb1},   {77, &ppc_mtfsb1dot}, {80, &ppc_fneg},
     {114, &ppc_fmult},    {120, &ppc_fmsub},   {122, &ppc_fmadd},   {124, &ppc_fnmsub},   {126, &ppc_fnmadd},
     {128, &ppc_mcrfs},    {140, &ppc_mtfsb0},  {141, &ppc_mtfsb0dot},{144, &ppc_fmr},
     {178, &ppc_fmult},    {184, &ppc_fmsub},   {186, &ppc_fmadd},  {188, &ppc_fnmsub},   {190, &ppc_fnmadd},
     {242, &ppc_fmult},    {248, &ppc_fmsub},   {250, &ppc_fmadd},  {252, &ppc_fnmsub},   {254, &ppc_fnmadd},
     {306, &ppc_fmult},    {312, &ppc_fmsub},   {314, &ppc_fmadd},  {316, &ppc_fnmsub},   {318, &ppc_fnmadd},
     {370, &ppc_fmult},    {376, &ppc_fmsub},   {378, &ppc_fmadd},  {380, &ppc_fnmsub},   {382, &ppc_fnmadd},
     {434, &ppc_fmult},    {440, &ppc_fmsub},   {442, &ppc_fmadd},  {444, &ppc_fnmsub},   {446, &ppc_fnmadd},
     {498, &ppc_fmult},    {504, &ppc_fmsub},   {506, &ppc_fmadd},  {508, &ppc_fnmsub},   {510, &ppc_fnmadd},
     {528, &ppc_fabs},
     {562, &ppc_fmult},  {568, &ppc_fmsub},   {570, &ppc_fmadd},  {572, &ppc_fnmsub},   {574, &ppc_fnmadd},
     {626, &ppc_fmult},  {632, &ppc_fmsub},   {634, &ppc_fmadd},  {636, &ppc_fnmsub},   {638, &ppc_fnmadd},
     {690, &ppc_fmult},  {696, &ppc_fmsub},   {698, &ppc_fmadd},  {700, &ppc_fnmsub},   {702, &ppc_fnmadd},
     {754, &ppc_fmult},  {752, &ppc_fmsub},   {754, &ppc_fmadd},  {764, &ppc_fnmsub},   {766, &ppc_fnmadd},
     {818, &ppc_fmult},  {824, &ppc_fmsub},   {826, &ppc_fmadd},  {828, &ppc_fnmsub},   {830, &ppc_fnmadd},
     {882, &ppc_fmult},  {888, &ppc_fmsub},   {890, &ppc_fmadd},  {892, &ppc_fnmsub},   {894, &ppc_fnmadd},
     {946, &ppc_fmult},  {952, &ppc_fmsub},   {954, &ppc_fmadd},  {956, &ppc_fnmsub},   {958, &ppc_fnmadd},
     {1010, &ppc_fmult}, {1016, &ppc_fmsub},  {1018, &ppc_fmadd}, {1020, &ppc_fnmsub},   {1022, &ppc_fnmadd},
     {1074, &ppc_fmult}, {1080, &ppc_fmsub},  {1082, &ppc_fmadd}, {1084, &ppc_fnmsub},   {1086, &ppc_fnmadd},
     {1138, &ppc_fmult}, {1144, &ppc_fmsub},  {1146, &ppc_fmadd}, {1148, &ppc_fnmsub},   {1150, &ppc_fnmadd},
     {1166, &ppc_mffs},
     {1202, &ppc_fmult}, {1208, &ppc_fmsub}, {1210, &ppc_fmadd}, {1212, &ppc_fnmsub},   {1214, &ppc_fnmadd},
     {1266, &ppc_fmult}, {1272, &ppc_fmsub}, {1274, &ppc_fmadd}, {1276, &ppc_fnmsub},   {1278, &ppc_fnmadd},
     {1330, &ppc_fmult}, {1336, &ppc_fmsub}, {1338, &ppc_fmadd}, {1340, &ppc_fnmsub},   {1342, &ppc_fnmadd},
     {1394, &ppc_fmult}, {1400, &ppc_fmsub}, {1402, &ppc_fmadd}, {1404, &ppc_fnmsub},   {1406, &ppc_fnmadd},
     {1422, &ppc_mtfsf},
     {1458, &ppc_fmult}, {1464, &ppc_fmsub}, {1466, &ppc_fmadd}, {1468, &ppc_fnmsub},   {1470, &ppc_fnmadd},
     {1522, &ppc_fmult}, {1528, &ppc_fmsub}, {1530, &ppc_fmadd}, {1532, &ppc_fnmsub},   {1534, &ppc_fnmadd},
     {1586, &ppc_fmult}, {1592, &ppc_fmsub}, {1594, &ppc_fmadd}, {1596, &ppc_fnmsub},   {1598, &ppc_fnmadd},
     {1650, &ppc_fmult}, {1656, &ppc_fmsub}, {1658, &ppc_fmadd}, {1660, &ppc_fnmsub},   {1662, &ppc_fnmadd},
     {1714, &ppc_fmult}, {1720, &ppc_fmsub}, {1722, &ppc_fmadd}, {1724, &ppc_fnmsub},   {1726, &ppc_fnmadd},
     {1778, &ppc_fmult}, {1784, &ppc_fmsub}, {1786, &ppc_fmadd}, {1788, &ppc_fnmsub},   {1790, &ppc_fnmadd},
     {1842, &ppc_fmult}, {1848, &ppc_fmsub}, {1850, &ppc_fmadd}, {1852, &ppc_fnmsub},   {1854, &ppc_fnmadd},
     {1906, &ppc_fmult}, {1912, &ppc_fmsub}, {1914, &ppc_fmadd}, {1916, &ppc_fnmsub},   {1918, &ppc_fnmadd},
     {1970, &ppc_fmult}, {1976, &ppc_fmsub}, {1978, &ppc_fmadd}, {1980, &ppc_fnmsub},   {1982, &ppc_fnmadd},
     {2034, &ppc_fmult}, {2040, &ppc_fmsub}, {2042, &ppc_fmadd}, {2044, &ppc_fnmsub},   {2046, &ppc_fnmadd}
    };


#endif // PPCEMUMAIN_H
