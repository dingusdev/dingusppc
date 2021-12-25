/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

#ifndef PPC_DEFS_H
#define PPC_DEFS_H

#include <cinttypes>

#define GET_rD_rA_OFFS(opcode)                                  \
    uint32_t reg_d = ((opcode) >> 21) & 31;                     \
    uint32_t reg_a = ((opcode) >> 16) & 31;                     \
    int32_t  offs  = (int32_t)((int16_t)((opcode) & 0xFFFF));

#define GET_GPR(reg)        ppc_state.gpr[(reg)]
#define SET_GPR(reg, val)   ppc_state.gpr[(reg)] = (val)

#define GET_rD_iA_iB(opcode)                                \
    uint32_t reg_d = ((opcode) >> 21) & 31;                 \
    uint32_t imm_a = ppc_state.gpr[((opcode) >> 16) & 31];  \
    uint32_t imm_b = ppc_state.gpr[((opcode) >> 11) & 31];

#define GET_rD_rA_iA_iB(opcode)                             \
    uint32_t reg_d = ((opcode) >> 21) & 31;                 \
    uint32_t reg_a = ((opcode) >> 16) & 31;                 \
    uint32_t imm_a = ppc_state.gpr[reg_a];                  \
    uint32_t imm_b = ppc_state.gpr[((opcode) >> 11) & 31];

#define GET_rA_iS(opcode)                                   \
    uint32_t reg_a = ((opcode) >> 16) & 31;                 \
    uint32_t imm_d = ppc_state.gpr[((opcode) >> 21) & 31];

#define GET_rS_rA_OFFS(opcode)                                  \
    uint32_t reg_s = ((opcode) >> 21) & 31;                     \
    uint32_t reg_a = ((opcode) >> 16) & 31;                     \
    int32_t  offs  = (int32_t)((int16_t)((opcode) & 0xFFFF));

#define GET_rS_rA_iA_iB(opcode)                             \
    uint32_t reg_s = ((opcode) >> 21) & 31;                 \
    uint32_t reg_a = ((opcode) >> 16) & 31;                 \
    uint32_t imm_a = ppc_state.gpr[reg_a];                  \
    uint32_t imm_b = ppc_state.gpr[((opcode) >> 11) & 31];

#endif // PPC_DEFS_H
