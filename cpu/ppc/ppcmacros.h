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

#define ppc_grab_regsdasimm(opcode) \
    int reg_d             = (opcode >> 21) & 31; \
    int reg_a             = (opcode >> 16) & 31; \
    int32_t simm          = int32_t(int16_t(opcode)); \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsdauimm(opcode)  \
    int reg_d             = (opcode >> 21) & 31; \
    int reg_a             = (opcode >> 16) & 31; \
    uint32_t uimm         = uint16_t(opcode); \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#    define ppc_grab_regsasimm(opcode) \
    int reg_a             = (opcode >> 16) & 31; \
    int32_t simm          = int32_t(int16_t(opcode)); \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#    define ppc_grab_regssauimm(opcode) \
    int reg_s             = (opcode >> 21) & 31; \
    int reg_a             = (opcode >> 16) & 31; \
    uint32_t uimm         = uint16_t(opcode); \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s]; \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_dab(opcode) \
    int reg_d = (opcode >> 21) & 31; \
    int reg_a = (opcode >> 16) & 31; \
    int reg_b = (opcode >> 11) & 31;

#define ppc_grab_regsdab(opcode) \
    int reg_d             = (opcode >> 21) & 31; \
    uint32_t reg_a        = (opcode >> 16) & 31; \
    uint32_t reg_b        = (opcode >> 11) & 31; \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a]; \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b]; 

#define ppc_grab_regssab(opcode) \
    uint32_t reg_s        = (opcode >> 21) & 31; \
    uint32_t reg_a        = (opcode >> 16) & 31; \
    uint32_t reg_b        = (opcode >> 11) & 31; \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s]; \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a]; \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b]; \

#define ppc_grab_regssa(opcode) \
    uint32_t reg_s        = (opcode >> 21) & 31; \
    uint32_t reg_a        = (opcode >> 16) & 31; \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s]; \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];


#define ppc_grab_regssb(opcode) \
    uint32_t reg_s = (opcode >> 21) & 31; \
    uint32_t reg_b = (opcode >> 11) & 31; \
    uint32_t ppc_result_d   = ppc_state.gpr[reg_s]; \
    uint32_t ppc_result_b   = ppc_state.gpr[reg_b]; \


#define ppc_grab_regsda(opcode) \
    int reg_d             = (opcode >> 21) & 31; \
    uint32_t reg_a        = (opcode >> 16) & 31; \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];


#define ppc_grab_regsdb(opcode) \
    int reg_d = (opcode >> 21) & 31; \
    uint32_t reg_b = (opcode >> 11) & 31; \
    uint32_t ppc_result_b   = ppc_state.gpr[reg_b];

#define ppc_store_iresult_reg(reg, ppc_result)\
    ppc_state.gpr[reg] = ppc_result;

#define ppc_store_sfpresult_int(reg, ppc_result64_d)\
     ppc_state.fpr[(reg)].int64_r = ppc_result64_d;

#define ppc_store_sfpresult_flt(reg, ppc_dblresult64_d)\
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d;

#define ppc_store_dfpresult_int(reg, ppc_result64_d)\
    ppc_state.fpr[(reg)].int64_r = ppc_result64_d;

#define ppc_store_dfpresult_flt(reg, ppc_dblresult64_d)\
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d;

#define ppc_grab_regsfpdb(opcode) \
        int reg_d = (opcode >> 21) & 31; \
        int reg_b = (opcode >> 11) & 31;

#define GET_FPR(reg)                                                                               \
    ppc_state.fpr[(reg)].dbl64_r 

#define ppc_grab_regsfpdiab(opcode) \
        int reg_d          = (opcode >> 21) & 31; \
        int reg_a          = (opcode >> 16) & 31; \
        int reg_b          = (opcode >> 11) & 31; \
        uint32_t val_reg_a = ppc_state.gpr[reg_a]; \
        uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpdia(opcode) \
        int reg_d          = (opcode >> 21) & 31; \
        int reg_a          = (opcode >> 16) & 31; \
        uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsia(opcode) \
        int reg_s          = (opcode >> 21) & 31; \
        int reg_a          = (opcode >> 16) & 31; \
        uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsiab(opcode) \
        int reg_s          = (opcode >> 21) & 31; \
        int reg_a          = (opcode >> 16) & 31; \
        int reg_b          = (opcode >> 11) & 31; \
        uint32_t val_reg_a = ppc_state.gpr[reg_a]; \
        uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpsab(opcode) \
        int reg_a        = (opcode >> 16) & 31; \
        int reg_b        = (opcode >> 11) & 31; \
        int crf_d        = (opcode >> 21) & 0x1C; \
        double db_test_a = GET_FPR(reg_a); \
        double db_test_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdab(opcode) \
        int reg_d        = (opcode >> 21) & 31; \
        int reg_a        = (opcode >> 16) & 31; \
        int reg_b        = (opcode >> 11) & 31; \
        double val_reg_a = GET_FPR(reg_a); \
        double val_reg_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdac(opcode) \
        int reg_d        = (opcode >> 21) & 31; \
        int reg_a        = (opcode >> 16) & 31; \
        int reg_c        = (opcode >> 6) & 31;  \
        double val_reg_a = GET_FPR(reg_a); \
        double val_reg_c = GET_FPR(reg_c);

#define ppc_grab_regsfpdabc(opcode) \
        int reg_d        = (opcode >> 21) & 31; \
        int reg_a        = (opcode >> 16) & 31; \
        int reg_b        = (opcode >> 11) & 31; \
        int reg_c        = (opcode >> 6) & 31; \
        double val_reg_a = GET_FPR(reg_a); \
        double val_reg_b = GET_FPR(reg_b); \
        double val_reg_c = GET_FPR(reg_c);

#endif    // PPC_MACROS_H