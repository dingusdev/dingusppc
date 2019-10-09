//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

// General opcodes for the processor - ppcopcodes.cpp

#include <iostream>
#include <map>
#include <unordered_map>
#include <cstring>
#include <cinttypes>
#include <array>
#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>
#include "ppcemumain.h"
#include "ppcmemory.h"

 uint32_t crf_d;
 uint32_t crf_s;
 uint32_t reg_s;
 uint32_t reg_d;
 uint32_t reg_a;
 uint32_t reg_b;
 uint32_t reg_c; //used only for floating point multiplication operations
 uint32_t xercon;
 uint32_t cmp_c;
 uint32_t crm;
 uint32_t uimm;
 uint32_t not_this;
 uint32_t grab_sr;
 uint32_t grab_inb; //This is for grabbing the number of immediate bytes for loading and storing
 uint32_t grab_d; //This is for grabbing d from Store and Load instructions
 uint32_t ppc_to;
 int32_t simm;
 int32_t adr_li;

//Used for GP calcs
 uint32_t ppc_result_a = 0;
 uint32_t ppc_result_b = 0;
 uint32_t ppc_result_c = 0;
 uint32_t ppc_result_d = 0;

 int32_t sidiv_result;
 uint32_t uidiv_result;
 uint64_t uiproduct;
 int64_t siproduct;

 uint32_t strwrd_replace_value;

/** Lookup tables. */

/** Primary opcode (bits 0...5) lookup table. */
static PPCOpcode OpcodeGrabber[] = {
    ppc_illegalop, ppc_illegalop, ppc_illegalop, ppc_twi,       ppc_opcode4,
    ppc_illegalop, ppc_illegalop, ppc_mulli,     ppc_subfic,    power_dozi,
    ppc_cmpli,     ppc_cmpi,      ppc_addic,     ppc_addicdot,  ppc_addi,
    ppc_addis,     ppc_opcode16,  ppc_sc,        ppc_opcode18,  ppc_opcode19,
    ppc_rlwimi,    ppc_rlwinm,    power_rlmi,    ppc_rlwnm,     ppc_ori,
    ppc_oris,      ppc_xori,      ppc_xoris,     ppc_andidot,   ppc_andisdot,
    ppc_illegalop, ppc_opcode31,  ppc_lwz,       ppc_lwzu,      ppc_lbz,
    ppc_lbzu,      ppc_stw,       ppc_stwu,      ppc_stb,       ppc_stbu,
    ppc_lhz,       ppc_lhzu,      ppc_lha,       ppc_lhau,      ppc_sth,
    ppc_sthu,      ppc_lmw,       ppc_stmw,      ppc_lfs,       ppc_lfsu,
    ppc_lfd,       ppc_lfdu,      ppc_stfs,      ppc_stfsu,     ppc_stfd,
    ppc_stfdu,     ppc_psq_l,     ppc_psq_lu,    ppc_illegalop, ppc_illegalop,
    ppc_psq_st,    ppc_psq_stu,   ppc_illegalop, ppc_opcode63
};

/** Lookup tables for branch instructions. */
static PPCOpcode SubOpcode16Grabber[] = {
    ppc_bc, ppc_bcl, ppc_bca, ppc_bcla
};

static PPCOpcode SubOpcode18Grabber[] = {
    ppc_b, ppc_bl, ppc_ba, ppc_bla
};

/**
Extract the registers desired and the values of the registers
This also takes the MSR into account, mainly to determine
what endian the numbers are to be stored in.
**/

//Storage and register retrieval functions for the integer functions.
void ppc_store_result_regd(){
    ppc_state.ppc_gpr[reg_d] = ppc_result_d;
}

void ppc_store_result_rega(){
    ppc_state.ppc_gpr[reg_a] = ppc_result_a;
}

void ppc_grab_regsdasimm(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    simm = (int32_t)((int16_t)((ppc_cur_instruction) & 65535));
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsdauimm(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    uimm = (uint32_t)((uint16_t)((ppc_cur_instruction) & 65535));
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsasimm(){
    reg_a = (ppc_cur_instruction >> 16) & 31;
    simm = (int32_t)((int16_t)(ppc_cur_instruction & 65535));
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regssauimm(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    uimm = (uint32_t)((uint16_t)((ppc_cur_instruction) & 65535));
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsdab(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regssab(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regssa(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regssb(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regsda(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsdb(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}


//Affects CR Field 0 - For integer operations
void ppc_changecrf0(uint32_t set_result){
    ppc_state.ppc_cr &= 0x0FFFFFFF;

    if (set_result == 0){
        ppc_state.ppc_cr |= 0x20000000UL;
    }
    else if (set_result & 0x80000000){
        ppc_state.ppc_cr |= 0x80000000UL;
    }
    else{
        ppc_state.ppc_cr |= 0x40000000UL;
    }

    if (ppc_state.ppc_spr[1] & 0x80000000){
        ppc_state.ppc_cr |= 0x10000000UL;
    }
}

//Affects the XER register's Carry Bit
void ppc_carry(uint32_t a, uint32_t b){
    if (b < a){ // TODO: ensure it works everywhere
        ppc_state.ppc_spr[1] |= 0x20000000;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
}

//Affects the XER register's SO and OV Bits
void ppc_setsoov(uint32_t a, uint32_t b){
    uint64_t a64b = (uint64_t)a;
    uint64_t b64b = (uint64_t)b;

    if ((a64b + b64b) > 0xFFFFFFFFUL){
        ppc_state.ppc_spr[1] |= 0x40000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xBFFFFFFFUL;
    }

    if (((a64b + b64b) < 0x80000000UL) || (ppc_state.ppc_spr[1] & 0x40000000UL)){
        ppc_state.ppc_spr[1] |= 0x80000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0x7FFFFFFFUL;
    }
}

/**
Avoid some tedious steps to breaking down the opcodes,
allowing for some nice maps to cleanly organize each opcode.
**/

void ppc_illegalop(){
    uint8_t illegal_code = ppc_cur_instruction >> 26;
    uint32_t grab_it = (uint32_t) illegal_code;
    printf("Illegal opcode reported: %d Report this! \n", grab_it);
    exit(-1);
    //ppc_exception_handler(0x0700, 0x80000);
}

void ppc_illegalsubop31(){
    uint16_t illegal_subcode = ppc_cur_instruction & 2047;
    uint32_t grab_it = (uint32_t) illegal_subcode;
    printf("Illegal subopcode for 31 reported: %d Report this! \n", grab_it);
}

void ppc_opcode4(){
    printf("Reading from Opcode 4 table \n");
    uint8_t subop_grab = ppc_cur_instruction & 3;
    uint32_t regrab = (uint32_t)subop_grab;
    printf("Executing subopcode entry %d \n .. or would if I bothered to implement it. SORRY!", regrab);
    exit(0);
}

void ppc_opcode16(){
    SubOpcode16Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode18(){
    SubOpcode18Grabber[ppc_cur_instruction & 3]();
}

void ppc_opcode19(){
    uint16_t subop_grab = ppc_cur_instruction & 2047;
    #ifdef EXHAUSTIVE_DEBUG
    uint32_t regrab = (uint32_t)subop_grab;
    printf("Executing Opcode 19 table supopcode entry %d \n", regrab);
    if (SubOpcode19Grabber.count(subop_grab) == 1){
        SubOpcode19Grabber[subop_grab]();
    }
    else{
        std::cout << "ILLEGAL SUBOPCODE: " << subop_grab << std::endl;
        ppc_exception_handler(0x0700, 0x80000);
    }
    #else
    SubOpcode19Grabber[subop_grab]();
    #endif // EXHAUSTIVE_DEBUG
}

void ppc_opcode31(){
    uint16_t subop_grab = ppc_cur_instruction & 2047;
    #ifdef EXHAUSTIVE_DEBUG
    uint32_t regrab = (uint32_t)subop_grab;
    printf("Executing Opcode 31 table supopcode entry %d \n", regrab);
    if (SubOpcode31Grabber.count(subop_grab) == 1){
        SubOpcode31Grabber[subop_grab]();
    }
    else{
        std::cout << "ILLEGAL SUBOPCODE: " << subop_grab << std::endl;
        ppc_exception_handler(0x0700, 0x80000);
    }
    #else
    SubOpcode31Grabber[subop_grab]();
    #endif // EXHAUSTIVE_DEBUG
}

void ppc_opcode59(){
    uint16_t subop_grab = ppc_cur_instruction & 2047;
    #ifdef EXHAUSTIVE_DEBUG
    uint32_t regrab = (uint32_t)subop_grab;
    printf("Executing Opcode 59 table supopcode entry %d \n", regrab);
    if (SubOpcode59Grabber.count(subop_grab) == 1){
        SubOpcode59Grabber[subop_grab]();
    }
    else{
        std::cout << "ILLEGAL SUBOPCODE: " << subop_grab << std::endl;
        ppc_exception_handler(0x0700, 0x80000);
    }
    #else
    SubOpcode59Grabber[subop_grab]();
    #endif // EXHAUSTIVE_DEBUG
}

void ppc_opcode63(){
    uint16_t subop_grab = ppc_cur_instruction & 2047;
    #ifdef EXHAUSTIVE_DEBUG
    uint32_t regrab = (uint32_t)subop_grab;
    std::cout << "Executing Opcode 63 table subopcode entry " << regrab << std::endl;
    if (SubOpcode63Grabber.count(subop_grab) == 1){
        SubOpcode63Grabber[subop_grab]();
    }
    else{
        std::cout << "ILLEGAL SUBOPCODE: " << subop_grab << std::endl;
        ppc_exception_handler(0x0700, 0x80000);
    }
    #else
    SubOpcode63Grabber[subop_grab]();
    #endif // EXHAUSTIVE_DEBUG
}

void ppc_main_opcode(){
    //Grab the main opcode
    uint8_t ppc_mainop = (ppc_cur_instruction >> 26) & 63;
    OpcodeGrabber[ppc_mainop]();
}

/**
The core functionality of this PPC emulation is within all of these void functions.
This is where the opcode tables in the ppcemumain.h come into play - reducing the number of comparisons needed.
This means loads of functions, but less CPU cycles needed to determine the function (theoretically).
**/

void ppc_addi(){
    ppc_grab_regsdasimm();
    ppc_result_d = (reg_a == 0)?simm:(ppc_result_a + simm);
    ppc_store_result_regd();
}

void ppc_addic(){
    ppc_grab_regsdasimm();
    ppc_result_d = (ppc_result_a + simm);
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addicdot(){
    ppc_grab_regsdasimm();
    ppc_result_d = (ppc_result_a + simm);
    ppc_changecrf0(ppc_result_d);
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addis(){
     ppc_grab_regsdasimm();
     ppc_result_d = (reg_a == 0)?(simm << 16):(ppc_result_a + (simm << 16));
     ppc_store_result_regd();
}

void ppc_add(){
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_store_result_regd();
}

void ppc_adddot(){
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

//addo + addodot
void ppc_addo(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_store_result_regd();
}

void ppc_addodot(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addc(){
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addcdot(){
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addco(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addcodot(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_adde(){
    ppc_grab_regsdab();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + ppc_result_b + xer_ca;
    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addedot(){
    ppc_grab_regsdab();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + ppc_result_b + xer_ca;
    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addeo(){
    ppc_grab_regsdab();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + ppc_result_b + xer_ca;
    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addeodot(){
    ppc_grab_regsdab();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_setsoov(ppc_result_a, (ppc_result_b + xer_ca));
    ppc_result_d = ppc_result_a + ppc_result_b + xer_ca;
    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addme(){
    ppc_grab_regsda();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + xer_ca - 1;
    if  (((xer_ca - 1) < 0xFFFFFFFFUL) | (ppc_result_d < ppc_result_a)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addmedot(){
    ppc_grab_regsda();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + xer_ca - 1;
    if  (((xer_ca - 1) < 0xFFFFFFFFUL) | (ppc_result_d < ppc_result_a)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addmeo(){
    ppc_grab_regsda();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_setsoov(ppc_result_a, xer_ca);
    ppc_result_d = ppc_result_a + xer_ca - 1;
    if  (((xer_ca - 1) < 0xFFFFFFFFUL) | (ppc_result_d < ppc_result_a)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addmeodot(){
    ppc_grab_regsda();
    uint32_t xer_ca = !!(ppc_state.ppc_spr[1] & 0x20000000);
    ppc_setsoov(ppc_result_a, xer_ca);
    ppc_result_d = ppc_result_a + xer_ca - 1;
    ppc_changecrf0(ppc_result_d);
    if  (((xer_ca - 1) < 0xFFFFFFFFUL) | (ppc_result_d < ppc_result_a)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addze(){
    ppc_grab_regsda();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + grab_xer;
    if (ppc_result_d < ppc_result_a){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addzedot(){
    ppc_grab_regsda();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = ppc_result_a + grab_xer;
    if (ppc_result_d < ppc_result_a){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_addzeo(){
    ppc_grab_regsda();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_setsoov(ppc_result_a, grab_xer);
    ppc_result_d = ppc_result_a + grab_xer;
    if (ppc_result_d < ppc_result_a){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_addzeodot(){
    ppc_grab_regsda();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_setsoov(ppc_result_a, grab_xer);
    ppc_result_d = ppc_result_a + grab_xer;
    if (ppc_result_d < ppc_result_a){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subf(){
    ppc_grab_regsdab();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    ppc_store_result_regd();
}

void ppc_subfdot(){
    ppc_grab_regsdab();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfo(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    ppc_store_result_regd();
}

void ppc_subfodot(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfc(){
    ppc_grab_regsdab();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfcdot(){
    ppc_grab_regsdab();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfco(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfcodot(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + 1;
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfic(){
    ppc_grab_regsdasimm();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + simm + 1;
    if (ppc_result_d <= not_this) {
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_store_result_regd();
}

void ppc_subfe(){
    ppc_grab_regsdab();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + (ppc_state.ppc_spr[1] & 0x20000000);
    if (ppc_result_d <= (not_this + grab_xer)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfedot(){
    ppc_grab_regsdab();
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + ppc_result_b + grab_xer;
    if (ppc_result_d <= (not_this + grab_xer)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfme(){
    ppc_grab_regsda();
    not_this = ~ppc_result_a;
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = not_this + grab_xer - 1;
    if (ppc_result_a || grab_xer){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, grab_xer);
    ppc_store_result_regd();
}

void ppc_subfmedot(){
    ppc_grab_regsda();
    not_this = ~ppc_result_a;
    uint32_t grab_xer = (ppc_state.ppc_spr[1] & 0x20000000);
    ppc_result_d = not_this + grab_xer - 1;
    if (ppc_result_d <= (not_this + grab_xer)){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, grab_xer);
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_subfze(){
    ppc_grab_regsda();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + (ppc_state.ppc_spr[1] & 0x20000000);
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, (ppc_state.ppc_spr[1] & 0x20000000));
    ppc_store_result_regd();
}

void ppc_subfzedot(){
    ppc_grab_regsda();
    not_this = ~ppc_result_a;
    ppc_result_d = not_this + (ppc_state.ppc_spr[1] & 0x20000000);
    if (ppc_result_d <= not_this){
        ppc_state.ppc_spr[1] |= 0x20000000UL;
    }
    else{
        ppc_state.ppc_spr[1] &= 0xDFFFFFFFUL;
    }
    ppc_carry(ppc_result_a, (ppc_state.ppc_spr[1] & 0x20000000));
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_and(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ppc_result_b;
    ppc_store_result_rega();
}

void ppc_anddot(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ppc_result_b;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_andc(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ~(ppc_result_b);
    ppc_store_result_rega();
}

void ppc_andcdot(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ~(ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_andidot(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d & uimm;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_andisdot(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d & (uimm << 16);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_nand(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d & ppc_result_b);
    ppc_store_result_rega();
}

void ppc_nanddot(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d & ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_or(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ppc_result_b;
    ppc_store_result_rega();
}

void ppc_ordot(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ppc_result_b;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_orc(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ~(ppc_result_b);
    ppc_store_result_rega();
}

void ppc_orcdot(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ~(ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_ori(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d | uimm;
    ppc_store_result_rega();
}

void ppc_oris(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d | (uimm << 16);
    ppc_store_result_rega();
}

void ppc_eqv(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d ^ ppc_result_b);
    ppc_store_result_rega();
}

void ppc_eqvdot(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d ^ ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_nor(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d | ppc_result_b);
    ppc_store_result_rega();
}

void ppc_nordot(){
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d | ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_xor(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d ^ ppc_result_b;
    ppc_store_result_rega();
}

void ppc_xordot(){
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d ^ ppc_result_b;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_xori(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d ^ uimm;
    ppc_store_result_rega();
}

void ppc_xoris(){
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d ^ (uimm << 16);
    ppc_store_result_rega();
}

void ppc_neg(){
    ppc_grab_regsda();
    ppc_result_d = ~(ppc_result_a) + 1;
    ppc_store_result_regd();
}

void ppc_negdot(){
    ppc_grab_regsda();
    ppc_result_d = ~(ppc_result_a) + 1;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_nego(){
    ppc_grab_regsda();
    ppc_result_d = ~(ppc_result_a) + 1;
    ppc_state.ppc_spr[1] |= 0x40000000UL;
    ppc_setsoov(ppc_result_d, 1);
    ppc_store_result_regd();
}

void ppc_negodot(){
    ppc_grab_regsda();
    ppc_result_d = ~(ppc_result_a) + 1;
    ppc_changecrf0(ppc_result_d);
    ppc_state.ppc_spr[1] |= 0x40000000UL;
    ppc_setsoov(ppc_result_d, 1);
    ppc_store_result_regd();
}

void ppc_cntlzw(){
    ppc_grab_regssa();

    uint32_t lead = 0;
    uint32_t bit_check = ppc_result_d;

    #ifdef USE_GCC_BUILTINS
    lead = __builtin_clz(bit_check);
    #elifdef USE_VS_BUILTINS
    lead = __lzcnt(bit_check);
    #else
    do{
        bit_check >>= 1;
        ++lead;
    } while (bit_check > 0);
    #endif
    ppc_result_a = lead;
    ppc_store_result_rega();
}

void ppc_cntlzwdot(){
    ppc_grab_regssa();

    uint32_t lead = 0;
    uint32_t bit_check = ppc_result_d;

    #ifdef USE_GCC_BUILTINS
    lead = __builtin_clz(bit_check);
    #elifdef USE_VS_BUILTINS
    lead = __lzcnt(bit_check);
    #else
    do{
        bit_check >>= 1;
        ++lead;
    } while (bit_check > 0);
    #endif

    ppc_result_a = lead;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_mulhwu(){
    ppc_grab_regsdab();
    uiproduct = (uint64_t) ppc_result_a * (uint64_t) ppc_result_b;
    uiproduct = uiproduct >> 32;
    ppc_result_d = uiproduct;
    ppc_store_result_regd();
}

void ppc_mulhwudot(){
    ppc_grab_regsdab();
    uiproduct = (uint64_t) ppc_result_a * (uint64_t) ppc_result_b;
    uiproduct = uiproduct >> 32;
    ppc_result_d = uiproduct;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_mulhw(){
    ppc_grab_regsdab();
    siproduct = (int64_t) ppc_result_a * (int64_t) ppc_result_b;
    siproduct = siproduct >> 32;
    ppc_result_d = (uint32_t) siproduct;
    ppc_store_result_regd();
}

void ppc_mulhwdot(){
    ppc_grab_regsdab();
    siproduct = (int64_t) ppc_result_a * (int64_t) ppc_result_b;
    siproduct = siproduct >> 32;
    ppc_result_d = (uint32_t) siproduct;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_mullw(){
    ppc_grab_regsdab();
    siproduct = (int64_t) ppc_result_a * (int64_t) ppc_result_b;
    siproduct &= 4294967295;
    ppc_result_d = (uint32_t) siproduct;
    ppc_store_result_regd();
}

void ppc_mullwdot(){
    ppc_grab_regsdab();
    siproduct = ppc_result_a * ppc_result_b;
    siproduct &= 4294967295;
    ppc_result_d = (uint32_t) siproduct;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_mullwo(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    siproduct = ppc_result_a * ppc_result_b;
    siproduct &= 4294967295;
    ppc_result_d = (uint32_t) siproduct;
    ppc_store_result_regd();
}

void ppc_mullwodot(){
    ppc_grab_regsdab();
    ppc_setsoov(ppc_result_a, ppc_result_b);
    siproduct = ppc_result_a * ppc_result_b;
    siproduct &= 4294967295;
    ppc_result_d = (uint32_t) siproduct;
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void ppc_mulli(){
    ppc_grab_regsdasimm();
    int64_t siproduct = (int64_t)((ppc_result_a) * ((int64_t)simm));
    siproduct &= 4294967295;
    ppc_result_d = (uint32_t) siproduct;
    ppc_store_result_regd();
}


void ppc_divw(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        return;
    case 0xFFFFFFFF:
        if (ppc_result_a == 0x80000000){
            ppc_result_d = 0;
            ppc_store_result_regd();
            return;
        }
     default:
        sidiv_result = (int32_t) ppc_result_a / (int32_t) ppc_result_b;
        ppc_result_d = (uint32_t) sidiv_result;
        ppc_store_result_regd();
    }
}

void ppc_divwdot(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        ppc_state.ppc_cr &= 0x1FFFFFFF;
        return;
    case 0xFFFFFFFF:
        if (ppc_result_a == 0x80000000){
            ppc_result_d = 0;
            ppc_store_result_regd();
            ppc_state.ppc_cr &= 0x1FFFFFFF;
            return;
        }
    default:
        sidiv_result = (int32_t) ppc_result_a / (int32_t) ppc_result_b;
        ppc_result_d = (uint32_t) sidiv_result;
        ppc_changecrf0(ppc_result_d);
        ppc_store_result_regd();
    }
}

void ppc_divwo(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_state.ppc_cr |= 0x10000000;
        ppc_store_result_regd();
        return;
    case 0xFFFFFFFF:
        if (ppc_result_a == 0x80000000){
            ppc_result_d = 0;
            ppc_state.ppc_cr |= 0x10000000;
            ppc_store_result_regd();
            return;
        }
     default:
        ppc_setsoov(ppc_result_a, ppc_result_b);
        sidiv_result = (int32_t) ppc_result_a / (int32_t) ppc_result_b;
        ppc_result_d = (uint32_t) sidiv_result;
        ppc_store_result_regd();
    }
}

void ppc_divwodot(){
    ppc_grab_regsdab();

     //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        ppc_state.ppc_cr &= 0x1FFFFFFF;
        ppc_state.ppc_cr |= 0x10000000;
        return;
    case 0xFFFFFFFF:
        if (ppc_result_a == 0x80000000){
            ppc_result_d = 0;
            ppc_store_result_regd();
            ppc_state.ppc_cr &= 0x1FFFFFFF;
            ppc_state.ppc_cr |= 0x10000000;
            return;
        }
     default:
        ppc_setsoov(ppc_result_a, ppc_result_b);
        sidiv_result = (int32_t) ppc_result_a / (int32_t) ppc_result_b;
        ppc_result_d = (uint32_t) sidiv_result;
        ppc_changecrf0(ppc_result_d);
        ppc_store_result_regd();
    }
}

void ppc_divwu(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        return;
    default:
        uidiv_result = ppc_result_a / ppc_result_b;
        ppc_result_d = uidiv_result;
        ppc_store_result_regd();
    }
}

void ppc_divwudot(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        ppc_state.ppc_cr &= 0x1FFFFFFF;
        return;
    default:
        uidiv_result = ppc_result_a / ppc_result_b;
        ppc_result_d = uidiv_result;
        ppc_changecrf0(ppc_result_d);
        ppc_store_result_regd();
    }
}

void ppc_divwuo(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_state.ppc_cr |= 0x10000000;
        ppc_store_result_regd();
        return;
    default:
        ppc_setsoov(ppc_result_a, ppc_result_b);
        uidiv_result = ppc_result_a / ppc_result_b;
        ppc_result_d = uidiv_result;
        ppc_store_result_regd();
    }
}

void ppc_divwuodot(){
    ppc_grab_regsdab();

    //handle division by zero cases
    switch (ppc_result_b){
    case 0:
        ppc_result_d = 0;
        ppc_store_result_regd();
        ppc_state.ppc_cr &= 0x1FFFFFFF;
        ppc_state.ppc_cr |= 0x10000000;
        return;
    default:
        ppc_setsoov(ppc_result_a, ppc_result_b);
        uidiv_result = ppc_result_a / ppc_result_b;
        ppc_result_d = uidiv_result;
        ppc_changecrf0(ppc_result_d);
        ppc_store_result_regd();
    }
}

//Value shifting

void ppc_slw(){
    ppc_grab_regssab();
    ppc_result_a = (ppc_result_b < 32)?(ppc_result_d << ppc_result_b):0;
    ppc_store_result_rega();
}

void ppc_slwdot(){
    ppc_grab_regssab();
    ppc_result_a = (ppc_result_b < 32)?(ppc_result_d << ppc_result_b):0;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_srw(){
    ppc_grab_regssab();
    ppc_result_a = (ppc_result_b < 32)?(ppc_result_d >> ppc_result_b):0;
    ppc_store_result_rega();
}

void ppc_srwdot(){
    ppc_grab_regssab();
    ppc_result_a = (ppc_result_b < 32)?(ppc_result_d >> ppc_result_b):0;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_sraw(){
    ppc_grab_regssab();

    if (ppc_result_b > 32){
        ppc_result_a = ((ppc_result_d) > 0x7FFFFFFF)?0xFFFFFFFFUL:0x0;
        ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);
    }
    else{
        ppc_result_a = (uint32_t)((int32_t)ppc_result_d >> ppc_result_b);
        ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);
    }
    ppc_store_result_rega();
}

void ppc_srawdot(){
    ppc_grab_regssab();
    if (ppc_result_b > 32){
        ppc_result_a = ((ppc_result_d) > 0x7FFFFFFF)?0xFFFFFFFFUL:0x0;
        ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);
    }
    else{
        ppc_result_a = (uint32_t)((int32_t)ppc_result_d >> ppc_result_b);
        ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);
    }
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_srawi(){
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;

    ppc_result_a = (uint32_t)((int32_t)ppc_result_d >> rot_sh);
    ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);

    ppc_store_result_rega();
}

void ppc_srawidot(){
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;

    ppc_result_a = (uint32_t)((int32_t)ppc_result_d >> rot_sh);
    ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xDFFFFFFFUL) | (((ppc_result_d) > 0x7FFFFFFF)?0x20000000:0x0);

    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me)
{
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = 0xFFFFFFFFUL << (31 - rot_me);
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

void ppc_rlwimi(){
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask = rot_mask(rot_mb, rot_me);
    uint32_t r = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32-rot_sh)));
    ppc_result_a = (ppc_result_a & ~mask) | (r & mask);
    if ((ppc_cur_instruction & 0x01) == 1){
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void ppc_rlwinm(){
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask = rot_mask(rot_mb, rot_me);
    uint32_t r = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32-rot_sh)));
    ppc_result_a = r & mask;
    if ((ppc_cur_instruction & 0x01) == 1){
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void ppc_rlwnm(){
    ppc_grab_regssab();
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask = rot_mask(rot_mb, rot_me);
    uint32_t r = ((ppc_result_d << ppc_result_b) | (ppc_result_d >> (32-ppc_result_b)));
    ppc_result_a = r & mask;
    if ((ppc_cur_instruction & 0x01) == 1){
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void ppc_mfcr(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_cr;
}

void ppc_mtsr(){
    if ((ppc_state.ppc_msr & 0x4000) == 0){
        reg_s = (ppc_cur_instruction >> 21) & 31;
        grab_sr = (ppc_cur_instruction >> 16) & 15;
        ppc_state.ppc_sr[grab_sr] = ppc_state.ppc_gpr[reg_s];
    }
}

void ppc_mtsrin(){
    if ((ppc_state.ppc_msr & 0x4000) == 0){
        ppc_grab_regssb();
        grab_sr = ppc_result_b >> 28;
        ppc_state.ppc_sr[grab_sr] = ppc_result_d;
    }
}

void ppc_mfsr(){
    if ((ppc_state.ppc_msr & 0x4000) == 0){
        reg_d = (ppc_cur_instruction >> 21) & 31;
        grab_sr = (ppc_cur_instruction >> 16) & 15;
        ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_sr[grab_sr];
    }
}

void ppc_mfsrin(){
    if ((ppc_state.ppc_msr & 0x4000) == 0){
        ppc_grab_regssb();
        grab_sr = ppc_result_b >> 28;
        ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_sr[grab_sr];
    }
}

void ppc_mfmsr(){
    if ((ppc_state.ppc_msr & 0x4000) == 0){
        reg_d = (ppc_cur_instruction >> 21) & 31;
        ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_msr;
    }
}

void ppc_mtmsr(){
    //if ((ppc_state.ppc_msr && 0x4000) == 0){
        reg_s = (ppc_cur_instruction >> 21) & 31;
        ppc_state.ppc_msr = ppc_state.ppc_gpr[reg_s];
        msr_status_update();
    //}
}

void ppc_mfspr(){
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);
    reg_d = (ppc_cur_instruction >> 21) & 31;
    ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_spr[ref_spr];
}

void ppc_mtspr(){
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);
    reg_s = (ppc_cur_instruction >> 21) & 31;

    if (ref_spr != 287){
        ppc_state.ppc_spr[ref_spr] = ppc_state.ppc_gpr[reg_s];
    }

    switch(ref_spr){
        //Mirror the TBRs in the SPR range to the user-mode TBRs.
        case 268:
            ppc_state.ppc_tbr[0] = ppc_state.ppc_gpr[reg_s];
            break;
        case 269:
            ppc_state.ppc_tbr[1] = ppc_state.ppc_gpr[reg_s];
            break;
        case 528:
        case 529:
        case 530:
        case 531:
        case 532:
        case 533:
        case 534:
        case 535:
            ibat_update(ref_spr);
            break;
        case 536:
        case 537:
        case 538:
        case 539:
        case 540:
        case 541:
        case 542:
        case 543:
            dbat_update(ref_spr);
    }
}

void ppc_mftb(){
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);
    reg_d = (ppc_cur_instruction >> 21) & 31;
    switch(ref_spr){
        case 268:
            ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_tbr[0];
            break;
        case 269:
            ppc_state.ppc_gpr[reg_d] = ppc_state.ppc_tbr[1];
            break;
        default:
            std::cout << "Invalid TBR access attempted!" << std::endl;
    }
}

void ppc_mtcrf(){
    uint32_t cr_mask = 0;
    ppc_grab_regssa();
    crm = ((ppc_cur_instruction >> 12) & 255);
    //check this
    cr_mask += (crm & 128) ? 0xF0000000 : 0x00000000;
    cr_mask += (crm &  64) ? 0x0F000000 : 0x00000000;
    cr_mask += (crm &  32) ? 0x00F00000 : 0x00000000;
    cr_mask += (crm &  16) ? 0x000F0000 : 0x00000000;
    cr_mask += (crm &   8) ? 0x0000F000 : 0x00000000;
    cr_mask += (crm &   4) ? 0x00000F00 : 0x00000000;
    cr_mask += (crm &   2) ? 0x000000F0 : 0x00000000;
    cr_mask += (crm &   1) ? 0x0000000F : 0x00000000;
    ppc_state.ppc_cr = (ppc_result_d & cr_mask) | (ppc_state.ppc_cr & ~(cr_mask));
}

void ppc_mcrxr(){
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_state.ppc_cr = (ppc_state.ppc_cr & ~(0xF0000000UL >> crf_d)) | ((ppc_state.ppc_spr[1] & 0xF0000000UL) >> crf_d);
    ppc_state.ppc_spr[1] &= 0x0FFFFFFF;
}

void ppc_extsb(){
    ppc_grab_regssa();
    ppc_result_a = (ppc_result_d < 0x80)?(ppc_result_d & 0x000000FF):(0xFFFFFF00UL | (ppc_result_d & 0x000000FF));
    ppc_store_result_rega();
}

void ppc_extsbdot(){
    ppc_grab_regssa();
    ppc_result_a = (ppc_result_d < 0x80)?(ppc_result_d & 0x000000FF):(0xFFFFFF00UL | (ppc_result_d & 0x000000FF));
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void ppc_extsh(){
    ppc_grab_regssa();
    ppc_result_a = (ppc_result_d < 0x8000)?(ppc_result_d & 0x0000FFFF):(0xFFFF0000UL | (ppc_result_d & 0x0000FFFF));
    ppc_store_result_rega();
}

void ppc_extshdot(){
    ppc_grab_regssa();
    ppc_result_a = (ppc_result_d < 0x8000)?(ppc_result_d & 0x0000FFFF):(0xFFFF0000UL | (ppc_result_d & 0x0000FFFF));
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

//Branching Instructions

//The last two bytes of the instruction are used for determining how the branch happens.
//The middle 24 bytes are the 24-bit address to use for branching to.


void ppc_b(){
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li = (quick_test < 0x2000000)? quick_test: (0xFC000000UL + quick_test);
    ppc_next_instruction_address = (uint32_t)(ppc_state.ppc_pc + adr_li);
    grab_branch = 1;
}

void ppc_bl(){
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li = (quick_test < 0x2000000)? quick_test: (0xFC000000UL + quick_test);
    ppc_next_instruction_address = (uint32_t)(ppc_state.ppc_pc + adr_li);
    ppc_state.ppc_spr[8] = (uint32_t)(ppc_state.ppc_pc + 4);
    grab_branch = 1;
}

void ppc_ba(){
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li = (quick_test < 0x2000000)? quick_test: (0xFC000000UL + quick_test);
    ppc_next_instruction_address = adr_li;
    grab_branch = 1;
}

void ppc_bla(){
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li = (quick_test < 0x2000000)? quick_test: (0xFC000000UL + quick_test);
    ppc_next_instruction_address = adr_li;
    ppc_state.ppc_spr[8] = ppc_state.ppc_pc + 4;
    grab_branch = 1;
}

void ppc_bc()
{
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t  br_bd = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_pc + br_bd);
        grab_branch = 1;
    }
}

void ppc_bca()
{
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t  br_bd = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = br_bd;
        grab_branch = 1;
    }
}

void ppc_bcl()
{
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t  br_bd = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_pc + br_bd);
        grab_branch = 1;
    }
    ppc_state.ppc_spr[8] = ppc_state.ppc_pc + 4;
}

void ppc_bcla()
{
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t  br_bd = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = br_bd;
        grab_branch = 1;
    }
    ppc_state.ppc_spr[8] = ppc_state.ppc_pc + 4;
}

void ppc_bcctr()
{
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;

    uint32_t cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_spr[9] & 0xFFFFFFFCUL);
        grab_branch = 1;
    }
}

void ppc_bcctrl()
{
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;

    uint32_t cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_spr[9] & 0xFFFFFFFCUL);
        grab_branch = 1;
    }
    ppc_state.ppc_spr[8] = ppc_state.ppc_pc + 4;
}

void ppc_bclr()
{
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    uint32_t ctr_ok;
    uint32_t cnd_ok;

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_spr[8] & 0xFFFFFFFCUL);
        grab_branch = 1;
    }
}

void ppc_bclrl()
{
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    uint32_t ctr_ok;
    uint32_t cnd_ok;

    if (!(br_bo & 0x04)){
        (ppc_state.ppc_spr[9])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.ppc_spr[9] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.ppc_cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok){
        ppc_next_instruction_address = (ppc_state.ppc_spr[8] & 0xFFFFFFFCUL);
        grab_branch = 1;
    }
    ppc_state.ppc_spr[8] = ppc_state.ppc_pc + 4;
}
//Compare Instructions

void ppc_cmp(){
    //if (!((ppc_cur_instruction >> 21) && 0x1)){
        crf_d = (ppc_cur_instruction >> 23) & 7;
        crf_d = crf_d << 2;
        ppc_grab_regssab();
        xercon = (ppc_state.ppc_spr[1] & 0x80000000UL) >> 3;
        cmp_c = (((int32_t)ppc_result_a) == ((int32_t)ppc_result_b)) ? 0x20000000UL : (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) ? 0x40000000UL : 0x80000000UL;
        ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000UL >>  crf_d)) | ((cmp_c + xercon) >> crf_d));
    //}
    //else{
     //   printf("Warning: Invalid CMP Instruction.");
    //}
}

void ppc_cmpi(){
    #ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        printf("WARNING: invalid CMPI instruction form (L=1)!\n");
        return;
    }
    #endif

    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_grab_regsasimm();
    xercon = (ppc_state.ppc_spr[1] & 0x80000000UL) >> 3;
    cmp_c = (((int32_t)ppc_result_a) == simm) ? 0x20000000UL : (((int32_t)ppc_result_a) > simm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000UL >>  crf_d)) | ((cmp_c + xercon) >> crf_d));
}

void ppc_cmpl(){
    #ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        printf("WARNING: invalid CMPL instruction form (L=1)!\n");
        return;
    }
    #endif

    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_grab_regssab();
    xercon = (ppc_state.ppc_spr[1] & 0x80000000UL) >> 3;
    cmp_c = (ppc_result_a == ppc_result_b) ? 0x20000000UL : (ppc_result_a > ppc_result_b) ? 0x40000000UL : 0x80000000UL;
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000UL >>  crf_d)) | ((cmp_c + xercon) >> crf_d));
}

void ppc_cmpli(){
    #ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        printf("WARNING: invalid CMPLI instruction form (L=1)!\n");
        return;
    }
    #endif

    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_grab_regssauimm();
    xercon = (ppc_state.ppc_spr[1] & 0x80000000UL) >> 3;
    cmp_c = (ppc_result_a == uimm) ? 0x20000000UL : (ppc_result_a > uimm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000UL >>  crf_d)) | ((cmp_c + xercon) >> crf_d));
}

//Condition Register Changes

void ppc_crand(){
    ppc_grab_regsdab();
    if ((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) && (ppc_state.ppc_cr & (0x80000000UL >> reg_b))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_crandc(){
    ppc_grab_regsdab();
    if ((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) && !(ppc_state.ppc_cr & (0x80000000UL >> reg_b))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_creqv(){
    ppc_grab_regsdab();
    if (!((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) ^ (ppc_state.ppc_cr & (0x80000000UL >> reg_b)))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_crnand(){
    ppc_grab_regsdab();
    if (!((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) && (ppc_state.ppc_cr & (0x80000000UL >> reg_b)))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_crnor(){
    ppc_grab_regsdab();
    if (!((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) || (ppc_state.ppc_cr & (0x80000000UL >> reg_b)))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}

void ppc_cror(){
    ppc_grab_regsdab();
    if ((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) || (ppc_state.ppc_cr & (0x80000000UL >> reg_b))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_crorc(){
    ppc_grab_regsdab();
    if ((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) || !(ppc_state.ppc_cr & (0x80000000UL >> reg_b))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}
void ppc_crxor(){
    ppc_grab_regsdab();
    if ((ppc_state.ppc_cr & (0x80000000UL >> reg_a)) ^ (ppc_state.ppc_cr & (0x80000000UL >> reg_b))){
        ppc_state.ppc_cr |= (0x80000000UL >> reg_d);
    }
    else{
        ppc_state.ppc_cr &= ~(0x80000000UL >> reg_d);
    }
}

//Processor MGMT Fns.

void ppc_rfi(){
    uint32_t new_srr1_val = (ppc_state.ppc_spr[27] & 0x87C0FF73UL);
    uint32_t new_msr_val = (ppc_state.ppc_msr & ~(0x87C0FF73UL));
    ppc_state.ppc_msr = (new_msr_val | new_srr1_val) & 0xFFFBFFFFUL;
    ppc_next_instruction_address = ppc_state.ppc_spr[26] & 0xFFFFFFFCUL;

    grab_return = true;
}

void ppc_sc(){
    ppc_exception_handler(0x0C00, 0x20000);
}

void ppc_tw(){
    reg_a = (ppc_cur_instruction >> 11) & 31;
    reg_b = (ppc_cur_instruction >> 16) & 31;
    ppc_to = (ppc_cur_instruction >> 21) & 31;
    if ((((int32_t)ppc_state.ppc_gpr[reg_a] < (int32_t)ppc_state.ppc_gpr[reg_b]) & (ppc_to & 0x10)) || \
       (((int32_t)ppc_state.ppc_gpr[reg_a]  > (int32_t)ppc_state.ppc_gpr[reg_b]) & (ppc_to & 0x08)) || \
       (((int32_t)ppc_state.ppc_gpr[reg_a] == (int32_t)ppc_state.ppc_gpr[reg_b]) & (ppc_to & 0x04)) || \
       ((ppc_state.ppc_gpr[reg_a] < ppc_state.ppc_gpr[reg_b]) & (ppc_to & 0x02)) || \
       ((ppc_state.ppc_gpr[reg_a] > ppc_state.ppc_gpr[reg_b]) & (ppc_to & 0x01))){
        ppc_exception_handler(0x0700, 0x20000);
    }
}

void ppc_twi(){
    simm = (int32_t)((int16_t)((ppc_cur_instruction) & 65535));
    reg_b = (ppc_cur_instruction >> 16) & 31;
    ppc_to = (ppc_cur_instruction >> 21) & 31;
    if ((((int32_t)ppc_state.ppc_gpr[reg_a] < simm) & (ppc_to & 0x10)) || \
       (((int32_t)ppc_state.ppc_gpr[reg_a]  > simm) & (ppc_to & 0x08)) || \
       (((int32_t)ppc_state.ppc_gpr[reg_a] == simm) & (ppc_to & 0x04)) || \
       ((ppc_state.ppc_gpr[reg_a] < (uint32_t)simm) & (ppc_to & 0x02)) || \
       ((ppc_state.ppc_gpr[reg_a] > (uint32_t)simm) & (ppc_to & 0x01))){
        ppc_exception_handler(0x0700, 0x20000);
    }
}

void ppc_eieio(){
    std::cout << "Oops. Placeholder for eieio." << std::endl;
}

void ppc_isync(){
    std::cout << "Oops. Placeholder for isync." << std::endl;
}

void ppc_sync(){
    std::cout << "Oops. Placeholder for sync." << std::endl;
}

void ppc_icbi(){
    std::cout << "Oops. Placeholder for icbi." << std::endl;
}

void ppc_dcbf(){
    std::cout << "Oops. Placeholder for dcbf." << std::endl;
}

void ppc_dcbi(){
    std::cout << "Oops. Placeholder for dcbi." << std::endl;
}

void ppc_dcbst(){
    std::cout << "Oops. Placeholder for dcbst." << std::endl;
}

void ppc_dcbt(){
    //Not needed, the HDI reg is touched to no-op this instruction.
    return;
}

void ppc_dcbtst(){
    //Not needed, the HDI reg is touched to no-op this instruction.
    return;
}

void ppc_dcbz(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    if (!(ppc_state.ppc_pc & 32) && (ppc_state.ppc_pc < 0xFFFFFFE0UL)){
        ppc_grab_regsdab();
        ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
        ppc_data_page_insert(0, ppc_effective_address, 4);
        ppc_data_page_insert(0, (ppc_effective_address + 4), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 8), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 12), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 16), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 20), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 24), 4);
        ppc_data_page_insert(0, (ppc_effective_address + 28), 4);
    }
    else{
        ppc_exception_handler(0x0600, 0x00000);
    }
}

//Integer Load and Store Functions

void ppc_stb(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 1);
}

void ppc_stbx(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 1);
}

void ppc_stbu(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    if (reg_a != 0){
        ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
        ppc_data_page_insert(ppc_result_d, ppc_effective_address, 1);
    }
}

void ppc_stbux(){
    ppc_grab_regssab();
    if (reg_a != 0){
        ppc_effective_address = ppc_result_a + reg_b;
        ppc_data_page_insert(ppc_result_d, ppc_effective_address, 1);
    }
    else{
        ppc_exception_handler(0x07000, 0x20000);
    }
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_sth(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 2);
}

void ppc_sthu(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 2);
}

void ppc_sthux(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 2);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_sthx(){
    ppc_grab_regssab();
    if (reg_a != 0){
        ppc_effective_address = ppc_result_a + ppc_result_b;
    }
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 2);
}

void ppc_sthbrx(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_result_d = (uint32_t)(rev_endian16((uint16_t)ppc_result_d));
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 2);
}

void ppc_stw(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
}

void ppc_stwx(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
}

void ppc_stwcx(){
    //PLACEHOLDER CODE FOR STWCX - We need to check for reserve memory
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    if (ppc_state.ppc_reserve){
        ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
        ppc_state.ppc_cr |= (ppc_state.ppc_spr[1] & 0x80000000) ? 0x30000000 : 0x20000000;
        ppc_state.ppc_reserve = false;
    }
    else{
        ppc_state.ppc_cr |= (ppc_state.ppc_spr[1] & 0x80000000) ? 0x10000000 : 0;
    }
}

void ppc_stwu(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stwux(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stwbrx(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_result_d = rev_endian32(ppc_result_d);
    ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
}

void ppc_stmw(){
    ppc_grab_regssa();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    //How many words to store in memory - using a do-while for this
    do{
        ppc_data_page_insert(ppc_result_d, ppc_effective_address, 4);
        ppc_effective_address +=4;
        reg_d++;
    }while (reg_d < 32);
}

void ppc_lbz(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_store(ppc_effective_address, 1);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lbzu(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    if ((reg_a != reg_d) || reg_a != 0){
        ppc_effective_address = ppc_result_a + grab_d;
    }
    else{
        ppc_exception_handler(0x0700, 0x20000);
    }
    ppc_data_page_store(ppc_effective_address, 1);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}

void ppc_lbzx(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 1);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lbzux(){
    ppc_grab_regsdab();
    if ((reg_a != reg_d) || reg_a != 0){
        ppc_effective_address = ppc_result_a + ppc_result_b;
    }
    else{
        ppc_exception_handler(0x0700, 0x20000);
    }
    ppc_data_page_store(ppc_effective_address, 1);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}


void ppc_lhz(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_store(ppc_effective_address, 2);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lhzu(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = ppc_result_a + grab_d;
    ppc_data_page_store(ppc_effective_address, 2);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}

void ppc_lhzx(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 2);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lhzux(){
    ppc_grab_regsdab();
    ppc_effective_address = ppc_result_a + ppc_result_b;
    ppc_data_page_store(ppc_effective_address, 2);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}

void ppc_lha(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(uint32_t)(ppc_result_a + grab_d);
    ppc_data_page_store(ppc_effective_address, 2);
    uint16_t go_this = (uint16_t)return_value;
    if (go_this & 0x8000){
        ppc_result_d = 0xFFFF0000UL | (uint32_t)return_value;
        ppc_store_result_regd();
    }
    else{
        ppc_result_d = (uint32_t)return_value;
        ppc_store_result_regd();
    }
    return_value = 0;
}

void ppc_lhau(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(uint32_t)(ppc_result_a + grab_d);
    ppc_data_page_store(ppc_effective_address, 2);
    uint16_t go_this = (uint16_t)return_value;
    if (go_this & 0x8000){
        ppc_result_d = 0xFFFF0000UL | (uint32_t)return_value;
        ppc_store_result_regd();
    }
    else{
        ppc_result_d = (uint32_t)return_value;
        ppc_store_result_regd();
    }
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_lhaux(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 2);
    uint16_t go_this = (uint16_t)return_value;
    if (go_this & 0x8000){
        ppc_result_d = 0xFFFF0000UL | (uint32_t)return_value;
        ppc_store_result_regd();
    }
    else{
        ppc_result_d = (uint32_t)return_value;
        ppc_store_result_regd();
    }
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_lhax(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 2);
    uint16_t go_this = (uint16_t)return_value;
    if (go_this & 0x8000){
        ppc_result_d = 0xFFFF0000UL | (uint32_t)return_value;
        ppc_store_result_regd();
    }
    else{
        ppc_result_d = (uint32_t)return_value;
        ppc_store_result_regd();
    }
    return_value = 0;
}

void ppc_lhbrx(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 2);
    ppc_result_d = (uint32_t)(rev_endian16((uint16_t)ppc_result_d));
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lwz(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lwbrx(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = rev_endian32(return_value);
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lwzu(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    if ((reg_a != reg_d) || reg_a != 0){
        ppc_effective_address = ppc_result_a + grab_d;
    }
    else{
        ppc_exception_handler(0x0700, 0x20000);
    }
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_lwzx(){
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lwzux(){
    ppc_grab_regsdab();
    if ((reg_a != reg_d) || reg_a != 0){
        ppc_effective_address = ppc_result_a + ppc_result_b;
    }
    else{
        ppc_exception_handler(0x0700, 0x20000);
    }
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}

void ppc_lwarx(){
    //Placeholder - Get the reservation of memory implemented!
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    ppc_state.ppc_reserve = true;
    ppc_data_page_store(ppc_effective_address, 4);
    ppc_result_d = return_value;
    return_value = 0;
    ppc_store_result_regd();
}

void ppc_lmw(){
    ppc_grab_regsda();
    grab_d = (uint32_t)((int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF)));
    ppc_effective_address = (reg_a == 0)?grab_d:(ppc_result_a + grab_d);
    //How many words to load in memory - using a do-while for this
    do{
        ppc_data_page_store(ppc_effective_address, 4);
        ppc_state.ppc_gpr[reg_d] = return_value;
        return_value = 0;
        ppc_effective_address +=4;
        reg_d++;
    }while (reg_d < 32);
}

void ppc_lswi(){
    ppc_grab_regsda();
    ppc_effective_address = ppc_result_a;
    grab_inb = (ppc_cur_instruction >> 11) & 31;
    grab_inb = (grab_inb & 32) == 0 ? 32 : grab_inb;

    uint32_t shift_times = 0;

    while (grab_inb > 0){
        switch(shift_times){
        case 0:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_state.ppc_gpr[reg_d] = (ppc_result_d & 0x00FFFFFFUL) | (return_value << 24);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 1:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFF00FFFFUL) | (return_value << 16);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 2:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFFFF00FFUL) | (return_value << 8);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 3:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFFFFFF00UL) | return_value;
            ppc_store_result_regd();
            return_value = 0;
            break;
        default:
            printf("Something really horrible happened with lswi.");
        }
        if (shift_times == 3){
            shift_times = 0;
            reg_d = (reg_d + 1) & 0x1F;
        }
        else{
            shift_times++;
        }
        return_value = 0;
        ppc_effective_address++;
        grab_inb--;
    }
}

void ppc_lswx(){
    ppc_grab_regsdab();
    //Invalid instruction forms
    if ((ppc_result_d == 0) && (ppc_result_a == 0)){
        ppc_exception_handler(0x0700, 0x100000);
    }
    if ((ppc_result_d == ppc_result_a) || (ppc_result_a == ppc_result_b)){
        ppc_exception_handler(0x0700, 0x100000);
    }
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    grab_inb = ppc_state.ppc_spr[1] & 127;
    uint32_t shift_times = 0;
    while (grab_inb > 0){
        switch(shift_times){
        case 0:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0x00FFFFFFUL) | (return_value << 24);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 1:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFF00FFFFUL) | (return_value << 16);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 2:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFFFF00FFUL) | (return_value << 8);
            ppc_store_result_regd();
            return_value = 0;
            break;
        case 3:
            ppc_data_page_store(ppc_effective_address, 1);
            ppc_result_d = (ppc_result_d & 0xFFFFFF00UL) | return_value;
            ppc_store_result_regd();
            return_value = 0;
            break;
        default:
            printf("Something really horrible happened with lswx.");
        }
        if (shift_times == 3){
            shift_times = 0;
            reg_d = (reg_d + 1) & 0x1F;
        }
        else{
            shift_times++;
        }
        return_value = 0;
        ppc_effective_address++;
        grab_inb--;
    }

}

void ppc_stswi(){
    ppc_grab_regssa();
    ppc_effective_address = (reg_a == 0)?0:ppc_result_a;
    grab_inb = (ppc_cur_instruction >> 11) & 31;
    grab_inb = (grab_inb & 32) == 0 ? 32 : grab_inb;
    uint32_t shift_times = 0;
    while (grab_inb > 0){
        switch(shift_times){
        case 0:
            strwrd_replace_value = (ppc_result_d >> 24);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 1:
            strwrd_replace_value = (ppc_result_d >> 16);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 2:
            strwrd_replace_value = (ppc_result_d >> 8);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 3:
            strwrd_replace_value = (ppc_result_d);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        default:
            printf("Something really horrible happened with stswi.");
        }
        if (shift_times == 3){
            shift_times = 0;
            reg_s = (reg_s + 1) & 0x1F;
        }
        else{
            shift_times++;
        }
        return_value = 0;
        ppc_effective_address++;
        grab_inb--;
    }
}

void ppc_stswx(){
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:(ppc_result_a + ppc_result_b);
    grab_inb = ppc_state.ppc_spr[1] & 127;
    uint32_t shift_times = 0;
    while (grab_inb > 0){
        switch(shift_times){
        case 0:
            strwrd_replace_value = (ppc_result_d >> 24);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 1:
            strwrd_replace_value = (ppc_result_d >> 16);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 2:
            strwrd_replace_value = (ppc_result_d >> 8);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        case 3:
            strwrd_replace_value = (ppc_result_d);
            ppc_data_page_insert(strwrd_replace_value, ppc_effective_address, 1);
            break;
        default:
            printf("Something really horrible happened with stswx.");
        }
        if (shift_times == 3){
            shift_times = 0;
            reg_s = (reg_s + 1) & 0x1F;
        }
        else{
            shift_times++;
        }
        return_value = 0;
        ppc_effective_address++;
        grab_inb--;
    }
}

//TLB Instructions

void ppc_tlbie(){
    /**
    reg_b = (ppc_cur_instruction >> 11) & 31;
    uint32_t vps = ppc_state.ppc_gpr[reg_b] & 0xFFFF000;
    **/
    printf("Placeholder for tlbie \n");
}

void ppc_tlbia(){
    printf("Placeholder for tlbia \n");
}

void ppc_tlbld(){
    printf("Placeholder for tlbld - 603 only \n");
}

void ppc_tlbli(){
    printf("Placeholder for tlbli - 603 only \n");
}
