/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#ifndef PPC_DECODE_HELPERS_H
#define PPC_DECODE_HELPERS_H

#include <cinttypes>

// ============== Helpers for extracting operands from opcodes ================

#define decode_ops_sa(opcode)                           \
    int reg_s = (opcode >> 21) & 0x1F;                  \
    int reg_a = (opcode >> 16) & 0x1F;

#define decode_ops_da(opcode)                           \
    int reg_d = (opcode >> 21) & 0x1F;                  \
    int reg_a = (opcode >> 16) & 0x1F;

#define decode_ops_sab(opcode)                          \
    decode_ops_sa(opcode);                              \
    int reg_b = (opcode >> 11) & 0x1F;

#define decode_ops_dab(opcode)                          \
    decode_ops_da(opcode);                              \
    int reg_b = (opcode >> 11) & 0x1F;

#define decode_ops_dasimm(opcode)                       \
    decode_ops_da(opcode);                              \
    int32_t  simm = int32_t(int16_t(opcode));

#define decode_ops_sauimm(opcode)                       \
    decode_ops_sa(opcode);                              \
    uint32_t uimm = uint16_t(opcode);

#define decode_ops_dauimm(opcode)                       \
    decode_ops_da(opcode);                              \
    uint32_t uimm = uint16_t(opcode);

// ================= Helpers for the original interpreter =====================
// The helpers below do the same work as those above but also pre-load some
// register values and place them into local variables.
// That significantly reduces interpreter code.

#define ppc_grab_regsdasimm(opcode)                     \
    decode_ops_dasimm(opcode);                          \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsdauimm(opcode)                     \
    decode_ops_dauimm(opcode);                          \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsasimm(opcode)                      \
    int      reg_a        = (opcode >> 16) & 0x1F;      \
    int32_t  simm         = int32_t(int16_t(opcode));   \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regssauimm(opcode)                     \
    decode_ops_sauimm(opcode);                          \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];       \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_crfd_regsauimm(opcode)                 \
    int      crf_d        = (opcode >> 21) & 0x1C;      \
    int      reg_a        = (opcode >> 16) & 0x1F;      \
    uint32_t uimm         = uint16_t(opcode);           \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_da(opcode)                             \
    decode_ops_da(opcode)

#define ppc_grab_dab(opcode)                            \
    decode_ops_dab(opcode)

#define ppc_grab_s(opcode)                              \
    int      reg_s        = (opcode >> 21) & 0x1F;      \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];

#define ppc_grab_regsdab(opcode)                        \
    decode_ops_dab(opcode);                             \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];       \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regssab(opcode)                        \
    decode_ops_sab(opcode);                             \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];       \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];       \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regssab_stswx(opcode)                  \
    decode_ops_sab(opcode);                             \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];       \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsab(opcode)                         \
    int      reg_a        = (opcode >> 16) & 0x1F;      \
    int      reg_b        = (opcode >> 11) & 0x1F;      \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];       \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regssa(opcode)                         \
    decode_ops_sa(opcode);                              \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];       \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regssa_stmw(opcode)                    \
    decode_ops_sa(opcode);                              \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regssash(opcode)                       \
    decode_ops_sa(opcode);                              \
    unsigned rot_sh       = (opcode >> 11) & 0x1F;      \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];       \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regssash_stswi(opcode)                 \
    decode_ops_sa(opcode);                              \
    unsigned rot_sh       = (opcode >> 11) & 0x1F;      \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regssb(opcode)                         \
    int      reg_s        = (opcode >> 21) & 0x1F;      \
    int      reg_b        = (opcode >> 11) & 0x1F;      \
    uint32_t ppc_result_d = ppc_state.gpr[reg_s];       \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsda(opcode)                         \
    decode_ops_da(opcode);                              \
    uint32_t ppc_result_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsdb(opcode)                         \
    int      reg_d        = (opcode >> 21) & 0x1F;      \
    uint32_t reg_b        = (opcode >> 11) & 0x1F;      \
    uint32_t ppc_result_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpdb(opcode)                       \
    int reg_d = (opcode >> 21) & 0x1F;                  \
    int reg_b = (opcode >> 11) & 0x1F;

#define ppc_grab_regsfpdiab(opcode)                     \
    decode_ops_dab(opcode);                             \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];          \
    uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpdia(opcode)                      \
    decode_ops_da(opcode);                              \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsia(opcode)                      \
    decode_ops_sa(opcode);                              \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsiab(opcode)                     \
    decode_ops_sab(opcode);                             \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];          \
    uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpsab(opcode)                      \
    int     reg_a     = (opcode >> 16) & 0x1F;          \
    int     reg_b     = (opcode >> 11) & 0x1F;          \
    int     crf_d     = (opcode >> 21) & 0x1C;          \
    double  db_test_a = GET_FPR(reg_a);                 \
    double  db_test_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdab(opcode)                      \
    decode_ops_dab(opcode);                             \
    double  val_reg_a = GET_FPR(reg_a);                 \
    double  val_reg_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdac(opcode)                      \
    decode_ops_da(opcode);                              \
    int     reg_c     = (opcode >> 6) & 0x1F;           \
    double  val_reg_a = GET_FPR(reg_a);                 \
    double  val_reg_c = GET_FPR(reg_c);

#define ppc_grab_regsfpdabc(opcode)                     \
    decode_ops_dab(opcode);                             \
    int     reg_c     = (opcode >> 6) & 0x1F;           \
    double  val_reg_a = GET_FPR(reg_a);                 \
    double  val_reg_b = GET_FPR(reg_b);                 \
    double  val_reg_c = GET_FPR(reg_c);

#define ppc_store_iresult_reg(reg, ppc_result)          \
    ppc_state.gpr[reg] = ppc_result

#define ppc_store_fpresult_int(reg, ppc_result64_d)     \
    ppc_state.fpr[(reg)].int64_r = ppc_result64_d

#define ppc_store_fpresult_flt(reg, ppc_dblresult64_d)  \
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d

#define GET_FPR(reg)                                    \
    ppc_state.fpr[(reg)].dbl64_r

#define FPR_INT(reg)                                    \
    ppc_state.fpr[reg].int64_r

#endif // PPC_DECODE_HELPERS_H
