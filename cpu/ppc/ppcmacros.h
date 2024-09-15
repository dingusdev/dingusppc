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

#ifndef PPC_MACROS_H
#define PPC_MACROS_H

#include <cinttypes>
#include "ppcemu.h"

#define reg_d(opcode)   ((opcode >> 21) & 0x1F)
#define reg_a(opcode)   ((opcode >> 16) & 0x1F)
#define reg_b(opcode)   ((opcode >> 11) & 0x1F)
#define reg_c(opcode)   ((opcode >> 6) & 0x1F)
#define reg_s(opcode)   reg_d(opcode)
#define trap_to(opcode) reg_d(opcode)
#define simm(opcode)    int32_t(int16_t(opcode))
#define uimm(opcode)    uint16_t(opcode)
#define rot_sh(opcode)  reg_b(opcode)
#define rot_mb(opcode)  ((opcode >> 6) & 0x1F)
#define rot_me(opcode)  ((opcode >> 1) & 0x1F)
#define crf_d(opcode)   ((opcode >> 21) & 0x1C)
#define crf_s(opcode)   ((opcode >> 16) & 0x1C)
#define crm(opcode)     ((opcode >> 12) & 0xFFU)
#define fcrm(opcode)    crm(opcode)
#define fmmask(opcode)  ((opcode >> 17) & 0xFFU)
#define br_bo(opcode)   reg_d(opcode)
#define br_bi(opcode)   reg_a(opcode)
#define br_bd(opcode)   int32_t(int16_t(opcode & ~3UL))
#define adr_li(opcode)  int32_t((opcode & ~3UL) << 6) >> 6
#define adr_d(opcode)   int32_t(int16_t(opcode))
#define segreg(opcode)  ((opcode >> 16) & 0xF)
#define mtfsfi_mask(opcode) ((opcode << 16) & 0xF0000000UL)

#define ppc_grab_twi(opcode) \
    instr.i_simm = simm(opcode); \
    instr.arg1   = reg_a(opcode); \
    instr.arg0   = trap_to(opcode); 

#define ppc_grab_tw(opcode) \
    instr.i_simm = simm(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.arg0 = trap_to(opcode); \

#define ppc_grab_crfd(opcode) \
    instr.arg0 = crf_d(opcode);

#define ppc_grab_mcrxr(opcode) \
    instr.arg0 = crf_d(opcode); \
    instr.arg4 = crf_d(opcode); \

#define ppc_grab_sr(opcode) \
    instr.arg0 = reg_d(opcode); \
    instr.arg1 = segreg(opcode); \

#define ppc_grab_mtfsfi(opcode) \
    instr.arg0 = reg_d(opcode); \
    instr.mask = mtfsfi_mask(opcode); \

#define ppc_grab_mtfsf(opcode) \
    instr.arg2 = reg_b(opcode); \
    instr.mask = fmmask(opcode);\

#define ppc_grab_regs_dasimm(opcode) \
    instr.arg0 = reg_d(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.i_simm = simm(opcode); \

#define ppc_grab_regs_sauimm(opcode)  \
    instr.arg0 = reg_s(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.i_simm = simm(opcode); \

#define ppc_grab_regs_da_addr(opcode) \
    instr.arg0   = reg_d(opcode); \
    instr.arg1   = reg_a(opcode); \
    instr.addr   = adr_d(opcode); \

#define ppc_grab_branch(opcode) \
    instr.addr = adr_li(opcode); \

#define ppc_grab_branch_cond(opcode) \
    instr.arg0 = br_bo(opcode); \
    instr.arg1 = br_bi(opcode); \

#define ppc_grab_d(opcode) \
    instr.arg0 = reg_d(opcode);

#define ppc_grab_dab(opcode) \
    instr.arg0 = reg_d(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.arg2 = reg_b(opcode); \

#define ppc_grab_regsasimm(opcode) \
    instr.arg0    = reg_s(opcode); \
    instr.arg1    = reg_a(opcode); \
    instr.i_simm  = simm(opcode); \

#define ppc_grab_regssauimm(opcode) \
    instr.arg0 = reg_s(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.i_simm = simm(opcode); \

#define ppc_grab_crfd_regsauimm(opcode) \
    instr.arg0 = crf_d(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.i_simm = simm(opcode); \

#define ppc_grab_crfd_regsab(opcode) \
    instr.arg0 = crf_d(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.arg2 = reg_b(opcode); 

#define ppc_grab_s(opcode) \
    instr.arg0 = reg_s(opcode); \

#define ppc_grab_regs_crfds(opcode) \
    instr.arg0 = crf_d(opcode); \
    instr.arg1 = crf_s(opcode); \

#define ppc_grab_regsdab(opcode) \
    instr.arg0 = reg_d(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.arg2 = reg_b(opcode); \

#define ppc_grab_regs_sab_rot(opcode) \
    instr.arg0 = reg_s(opcode); \
    instr.arg1 = reg_a(opcode); \
    instr.arg2 = reg_b(opcode); \
    instr.arg3 = rot_mb(opcode); \
    instr.arg4 = rot_me(opcode); \

#define ppc_store_iresult_reg(reg, ppc_result)\
    ppc_state.gpr[reg] = ppc_result;

#define ppc_store_fpresult_int(reg, ppc_result64_d)\
    ppc_state.fpr[(reg)].int64_r = ppc_result64_d;

#define ppc_store_fpresult_flt(reg, ppc_dblresult64_d)\
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d;

#define ppc_grab_regsfpdb(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg2 = reg_b(opcode); \

#define GET_FPR(reg) \
    ppc_state.fpr[(reg)].dbl64_r 

#define ppc_grab_regsfpdiab(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg2 = reg_b(opcode); \

#define ppc_grab_regsfpdia(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \

#define ppc_grab_regsfpsia(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \

#define ppc_grab_regsfpsiab(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg2 = reg_b(opcode); \

#define ppc_grab_regsfpsab(opcode) \
        instr.arg0 = crf_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg2 = reg_b(opcode); \

#define ppc_grab_regsfpdab(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg2 = reg_b(opcode); \

#define ppc_grab_regsfpdac(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg3 = reg_c(opcode); \

#define ppc_grab_regsfpdabc(opcode) \
        instr.arg0 = reg_d(opcode); \
        instr.arg1 = reg_a(opcode); \
        instr.arg2 = reg_b(opcode); \
        instr.arg3 = reg_c(opcode); \

#endif    // PPC_MACROS_H
