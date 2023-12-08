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

// General opcodes for the processor - ppcopcodes.cpp

#include <core/timermanager.h>
#include <core/mathutils.h>
#include "ppcemu.h"
#include "ppcmmu.h"
#include <cinttypes>
#include <vector>

uint32_t crf_d;
uint32_t crf_s;
uint32_t reg_s;
uint32_t reg_d;
uint32_t reg_a;
uint32_t reg_b;
uint32_t reg_c;    // used only for floating point multiplication operations
uint32_t xercon;
uint32_t cmp_c;
uint32_t crm;
uint32_t uimm;
uint32_t grab_sr;
uint32_t grab_inb;    // This is for grabbing the number of immediate bytes for loading and storing
uint32_t ppc_to;
int32_t simm;
int32_t adr_li;

// Used for GP calcs
uint32_t ppc_result_a = 0;
uint32_t ppc_result_b = 0;
uint32_t ppc_result_c = 0;
uint32_t ppc_result_d = 0;

/**
Extract the registers desired and the values of the registers
This also takes the MSR into account, mainly to determine
what endian the numbers are to be stored in.
**/

// Storage and register retrieval functions for the integer functions.
void ppc_store_result_regd() {
    ppc_state.gpr[reg_d] = ppc_result_d;
}

void ppc_store_result_rega() {
    ppc_state.gpr[reg_a] = ppc_result_a;
}

void ppc_grab_regsdasimm() {
    reg_d        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    simm         = (int32_t)((int16_t)((ppc_cur_instruction)&65535));
    ppc_result_a = ppc_state.gpr[reg_a];
}

inline void ppc_grab_regsdauimm() {
    reg_d        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    uimm         = (uint32_t)((uint16_t)((ppc_cur_instruction)&65535));
    ppc_result_a = ppc_state.gpr[reg_a];
}

inline void ppc_grab_regsasimm() {
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    simm         = (int32_t)((int16_t)(ppc_cur_instruction & 65535));
    ppc_result_a = ppc_state.gpr[reg_a];
}

inline void ppc_grab_regssauimm() {
    reg_s        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    uimm         = (uint32_t)((uint16_t)((ppc_cur_instruction)&65535));
    ppc_result_d = ppc_state.gpr[reg_s];
    ppc_result_a = ppc_state.gpr[reg_a];
}

void ppc_grab_regsdab() {
    reg_d        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    reg_b        = (ppc_cur_instruction >> 11) & 31;
    ppc_result_a = ppc_state.gpr[reg_a];
    ppc_result_b = ppc_state.gpr[reg_b];
}

void ppc_grab_regssab() {
    reg_s        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    reg_b        = (ppc_cur_instruction >> 11) & 31;
    ppc_result_d = ppc_state.gpr[reg_s];
    ppc_result_a = ppc_state.gpr[reg_a];
    ppc_result_b = ppc_state.gpr[reg_b];
}

void ppc_grab_regssa() {
    reg_s        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    ppc_result_d = ppc_state.gpr[reg_s];
    ppc_result_a = ppc_state.gpr[reg_a];
}

inline void ppc_grab_regssb() {
    reg_s        = (ppc_cur_instruction >> 21) & 31;
    reg_b        = (ppc_cur_instruction >> 11) & 31;
    ppc_result_d = ppc_state.gpr[reg_s];
    ppc_result_b = ppc_state.gpr[reg_b];
}

void ppc_grab_regsda() {
    reg_d        = (ppc_cur_instruction >> 21) & 31;
    reg_a        = (ppc_cur_instruction >> 16) & 31;
    ppc_result_a = ppc_state.gpr[reg_a];
}

inline void ppc_grab_regsdb() {
    reg_d        = (ppc_cur_instruction >> 21) & 31;
    reg_b        = (ppc_cur_instruction >> 11) & 31;
    ppc_result_b = ppc_state.gpr[reg_b];
}


// Affects CR Field 0 - For integer operations
void ppc_changecrf0(uint32_t set_result) {
    ppc_state.cr &= 0x0FFFFFFFUL;

    if (set_result == 0) {
        ppc_state.cr |= 0x20000000UL;
    } else {
        if (set_result & 0x80000000) {
            ppc_state.cr |= 0x80000000UL;
        } else {
            ppc_state.cr |= 0x40000000UL;
        }
    }

    /* copy XER[SO] into CR0[SO]. */
    ppc_state.cr |= (ppc_state.spr[SPR::XER] >> 3) & 0x10000000UL;
}

// Affects the XER register's Carry Bit
inline void ppc_carry(uint32_t a, uint32_t b) {
    if (b < a) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
}

inline void ppc_carry_sub(uint32_t a, uint32_t b) {
    if (b >= a) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
}

// Affects the XER register's SO and OV Bits

inline void ppc_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if ((a ^ b) & (a ^ d) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= 0xC0000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
}

typedef std::function<void()> CtxSyncCallback;
std::vector<CtxSyncCallback> gCtxSyncCallbacks;

// perform context synchronization by executing registered actions if any
void do_ctx_sync() {
    while (!gCtxSyncCallbacks.empty()) {
        gCtxSyncCallbacks.back()();
        gCtxSyncCallbacks.pop_back();
    }
}

void add_ctx_sync_action(const CtxSyncCallback &cb) {
    gCtxSyncCallbacks.push_back(cb);
}

/**
The core functionality of this PPC emulation is within all of these void functions.
This is where the opcode tables in the ppcemumain.h come into play - reducing the number of
comparisons needed. This means loads of functions, but less CPU cycles needed to determine the
function (theoretically).
**/

void dppc_interpreter::ppc_addi() {
    ppc_grab_regsdasimm();
    ppc_result_d = (reg_a == 0) ? simm : (ppc_result_a + simm);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addic() {
    ppc_grab_regsdasimm();
    ppc_result_d = (ppc_result_a + simm);
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addicdot() {
    ppc_grab_regsdasimm();
    ppc_result_d = (ppc_result_a + simm);
    ppc_changecrf0(ppc_result_d);
    ppc_carry(ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addis() {
    ppc_grab_regsdasimm();
    ppc_result_d = (reg_a == 0) ? (simm << 16) : (ppc_result_a + (simm << 16));
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_add() {
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    if (oe_flag)
        ppc_setsoov(ppc_result_a, ~ppc_result_b, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addc() {
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a + ppc_result_b;
    ppc_carry(ppc_result_a, ppc_result_d);

    if (oe_flag)
        ppc_setsoov(ppc_result_a, ~ppc_result_b, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_adde() {
    ppc_grab_regsdab();
    uint32_t xer_ca = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_result_d    = ppc_result_a + ppc_result_b + xer_ca;

    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (oe_flag)
        ppc_setsoov(ppc_result_a, ~ppc_result_b, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addme() {
    ppc_grab_regsda();
    uint32_t xer_ca = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_result_d    = ppc_result_a + xer_ca - 1;

    if (((xer_ca - 1) < 0xFFFFFFFFUL) || (ppc_result_d < ppc_result_a)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (oe_flag)
        ppc_setsoov(ppc_result_a, 0, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_addze() {
    ppc_grab_regsda();
    uint32_t grab_xer = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_result_d      = ppc_result_a + grab_xer;

    if (ppc_result_d < ppc_result_a) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (oe_flag)
        ppc_setsoov(ppc_result_a, 0xFFFFFFFFUL, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subf() {
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_b - ppc_result_a;

    if (oe_flag)
        ppc_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subfc() {
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_b - ppc_result_a;
    ppc_carry_sub(ppc_result_a, ppc_result_b);

    if (oe_flag)
        ppc_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subfic() {
    ppc_grab_regsdasimm();
    ppc_result_d = simm - ppc_result_a;
    ppc_carry(~ppc_result_a, ppc_result_d);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subfe() {
    ppc_grab_regsdab();
    uint32_t grab_xer = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_result_d      = ~ppc_result_a + ppc_result_b + grab_xer;
    ppc_carry(~ppc_result_a, ppc_result_d);

    if (oe_flag)
        ppc_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subfme() {
    ppc_grab_regsda();
    uint32_t grab_xer = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_result_d      = ~ppc_result_a + grab_xer - 1;
    ppc_carry(~ppc_result_a, ppc_result_d);

    if (oe_flag)
        ppc_setsoov(0xFFFFFFFF, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_subfze() {
    ppc_grab_regsda();
    ppc_result_d = ~ppc_result_a + (ppc_state.spr[SPR::XER] & 0x20000000);
    ppc_carry(~ppc_result_a, ppc_result_d);

    if (oe_flag)
        ppc_setsoov(0, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}


void dppc_interpreter::ppc_and() {
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ppc_result_b;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_andc() {
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d & ~(ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_andidot() {
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d & uimm;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_andisdot() {
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d & (uimm << 16);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_nand() {
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d & ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_or() {
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ppc_result_b;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_orc() {
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d | ~(ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_ori() {
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d | uimm;
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_oris() {
    ppc_grab_regssauimm();
    ppc_result_a = (uimm << 16) | ppc_result_d;
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_eqv() {
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d ^ ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_nor() {
    ppc_grab_regssab();
    ppc_result_a = ~(ppc_result_d | ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_xor() {
    ppc_grab_regssab();
    ppc_result_a = ppc_result_d ^ ppc_result_b;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_xori() {
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d ^ uimm;
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_xoris() {
    ppc_grab_regssauimm();
    ppc_result_a = ppc_result_d ^ (uimm << 16);
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_neg() {
    ppc_grab_regsda();
    ppc_result_d = ~(ppc_result_a) + 1;

    if (oe_flag) {
        if (ppc_result_a == 0x80000000)
            ppc_state.spr[SPR::XER] |= 0xC0000000;
        else
            ppc_state.spr[SPR::XER] &= 0xBFFFFFFF;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_cntlzw() {
    ppc_grab_regssa();

    uint32_t bit_check = ppc_result_d;

#ifdef USE_GCC_BUILTINS
    uint32_t lead = !bit_check ? 32 : __builtin_clz(bit_check);
#elif defined USE_VS_BUILTINS
    uint32_t lead = __lzcnt(bit_check);
#else
    uint32_t lead, mask;

    for (mask = 0x80000000UL, lead = 0; mask != 0; lead++, mask >>= 1) {
        if (bit_check & mask)
            break;
    }
#endif
    ppc_result_a = lead;

    if (rc_flag) {
        ppc_changecrf0(ppc_result_a);
    }

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_mulhwu() {
    ppc_grab_regsdab();
    uint64_t product = (uint64_t)ppc_result_a * (uint64_t)ppc_result_b;
    ppc_result_d     = (uint32_t)(product >> 32);

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_mulhw() {
    ppc_grab_regsdab();
    int64_t product = (int64_t)(int32_t)ppc_result_a * (int64_t)(int32_t)ppc_result_b;
    ppc_result_d    = product >> 32;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_mullw() {
    ppc_grab_regsdab();
    int64_t product = (int64_t)(int32_t)ppc_result_a * (int64_t)(int32_t)ppc_result_b;

    if (oe_flag) {
        if (product != (int64_t)(int32_t)product) {
            ppc_state.spr[SPR::XER] |= 0xC0000000;
        } else {
            ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
        }
    }

    ppc_result_d = (uint32_t)product;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_mulli() {
    ppc_grab_regsdasimm();
    int64_t product = (int64_t)(int32_t)ppc_result_a * (int64_t)(int32_t)simm;
    ppc_result_d    = (uint32_t)product;
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_divw() {
    ppc_grab_regsdab();

    if (!ppc_result_b) {                                     /* handle the "anything / 0" case */
        ppc_result_d = (ppc_result_a & 0x80000000) ? -1 : 0; /* UNDOCUMENTED! */

        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else if (ppc_result_a == 0x80000000UL && ppc_result_b == 0xFFFFFFFFUL) {
        ppc_result_d = 0xFFFFFFFF;

        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else { /* normal signed devision */
        ppc_result_d = (int32_t)ppc_result_a / (int32_t)ppc_result_b;

        if (oe_flag)
            ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_divwu() {
    ppc_grab_regsdab();

    if (!ppc_result_b) { /* division by zero */
        ppc_result_d = 0;

        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

        if (rc_flag)
            ppc_state.cr |= 0x20000000;

    } else {
        ppc_result_d = ppc_result_a / ppc_result_b;

        if (oe_flag)
            ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

// Value shifting

void dppc_interpreter::ppc_slw() {
    ppc_grab_regssab();
    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d << (ppc_result_b & 0x1F);
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_srw() {
    ppc_grab_regssab();
    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d >> (ppc_result_b & 0x1F);
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_sraw() {
    ppc_grab_regssab();
    if (ppc_result_b & 0x20) {
        ppc_result_a = (int32_t)ppc_result_d >> 31;
        ppc_state.spr[SPR::XER] |= (ppc_result_a & 1) << 29;
    } else {
        uint32_t shift = ppc_result_b & 0x1F;
        uint32_t mask  = (1U << shift) - 1;
        ppc_result_a   = (int32_t)ppc_result_d >> shift;
        if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
            ppc_state.spr[SPR::XER] |= 0x20000000UL;
        } else {
            ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
        }
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_srawi() {
    ppc_grab_regssa();
    unsigned shift = (ppc_cur_instruction >> 11) & 0x1F;
    uint32_t mask  = (1U << shift) - 1;
    ppc_result_a   = (int32_t)ppc_result_d >> shift;
    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = (uint32_t)(0xFFFFFFFFUL << (31 - rot_me));
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

void dppc_interpreter::ppc_rlwimi() {
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    ppc_result_a    = (ppc_result_a & ~mask) | (r & mask);
    if ((ppc_cur_instruction & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_rlwinm() {
    ppc_grab_regssa();
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    ppc_result_a    = r & mask;
    if ((ppc_cur_instruction & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_rlwnm() {
    ppc_grab_regssab();
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t rot    = ppc_result_b & 0x1F;
    uint32_t r      = rot ? ((ppc_result_d << rot) | (ppc_result_d >> (32 - rot))) : ppc_result_d;
    ppc_result_a    = r & mask;
    if ((ppc_cur_instruction & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_mfcr() {
    reg_d                = (ppc_cur_instruction >> 21) & 31;
    ppc_state.gpr[reg_d] = ppc_state.cr;
}

void dppc_interpreter::ppc_mtsr() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    reg_s                 = (ppc_cur_instruction >> 21) & 31;
    grab_sr               = (ppc_cur_instruction >> 16) & 15;
    ppc_state.sr[grab_sr] = ppc_state.gpr[reg_s];
    mmu_pat_ctx_changed();
}

void dppc_interpreter::ppc_mtsrin() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    ppc_grab_regssb();
    grab_sr               = ppc_result_b >> 28;
    ppc_state.sr[grab_sr] = ppc_result_d;
    mmu_pat_ctx_changed();
}

void dppc_interpreter::ppc_mfsr() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    reg_d                = (ppc_cur_instruction >> 21) & 31;
    grab_sr              = (ppc_cur_instruction >> 16) & 15;
    ppc_state.gpr[reg_d] = ppc_state.sr[grab_sr];
}

void dppc_interpreter::ppc_mfsrin() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    ppc_grab_regsdb();
    grab_sr              = ppc_result_b >> 28;
    ppc_state.gpr[reg_d] = ppc_state.sr[grab_sr];
}

void dppc_interpreter::ppc_mfmsr() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    reg_d                = (ppc_cur_instruction >> 21) & 31;
    ppc_state.gpr[reg_d] = ppc_state.msr;
}

void dppc_interpreter::ppc_mtmsr() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    reg_s         = (ppc_cur_instruction >> 21) & 31;
    ppc_state.msr = ppc_state.gpr[reg_s];

    // generate External Interrupt Exception
    // if CPU interrupt line is asserted
    if (ppc_state.msr & MSR::EE && int_pin) {
        //LOG_F(WARNING, "MTMSR: CPU INT pending, generate CPU exception");
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
    } else if ((ppc_state.msr & MSR::EE) && dec_exception_pending) {
        dec_exception_pending = false;
        //LOG_F(WARNING, "MTMSR: decrementer exception triggered");
        ppc_exception_handler(Except_Type::EXC_DECR, 0);
    } else {
        mmu_change_mode();
    }
}

static inline void calc_rtcl_value()
{
    uint64_t new_ts = get_virt_time_ns();
    uint64_t rtc_l = new_ts - rtc_timestamp + rtc_lo;
    if (rtc_l >= ONE_BILLION_NS) { // check RTCL overflow
        rtc_hi += (uint32_t)(rtc_l / ONE_BILLION_NS);
        rtc_lo  = rtc_l % ONE_BILLION_NS;
    }
    else {
        rtc_lo = (uint32_t)rtc_l;
    }
    rtc_timestamp = new_ts;
}

static inline uint64_t calc_tbr_value()
{
    uint64_t tbr_inc;
    uint32_t tbr_inc_lo;
    uint64_t diff = get_virt_time_ns() - tbr_wr_timestamp;
    _u32xu64(tbr_freq_ghz, diff, tbr_inc, tbr_inc_lo);
    return (tbr_wr_value + tbr_inc);
}

static inline uint32_t calc_dec_value() {
    uint64_t dec_adj;
    uint32_t dec_adj_lo;
    uint64_t diff = get_virt_time_ns() - dec_wr_timestamp;
    _u32xu64(tbr_freq_ghz, diff, dec_adj, dec_adj_lo);
    return (dec_wr_value - dec_adj);
}

static void update_timebase(uint64_t mask, uint64_t new_val)
{
    uint64_t tbr_value = calc_tbr_value();
    tbr_wr_value = (tbr_value & mask) | new_val;
    tbr_wr_timestamp = get_virt_time_ns();
}


static uint32_t decrementer_timer_id = 0;

static void trigger_decrementer_exception() {
    decrementer_timer_id = 0;
    dec_wr_value = -1;
    dec_wr_timestamp = get_virt_time_ns();
    if (ppc_state.msr & MSR::EE) {
        dec_exception_pending = false;
        //LOG_F(WARNING, "decrementer exception triggered");
        ppc_exception_handler(Except_Type::EXC_DECR, 0);
    }
    else {
        //LOG_F(WARNING, "decrementer exception pending");
        dec_exception_pending = true;
    }
}

static void update_decrementer(uint32_t val) {
    dec_wr_value = val;
    dec_wr_timestamp = get_virt_time_ns();

    dec_exception_pending = false;

    if (is_601)
        return;

    if (decrementer_timer_id) {
        //LOG_F(WARNING, "decrementer cancel timer");
        TimerManager::get_instance()->cancel_timer(decrementer_timer_id);
    }

    uint64_t time_out;
    uint32_t time_out_lo;
    _u32xu64(val, tbr_period_ns, time_out, time_out_lo);
    //LOG_F(WARNING, "decrementer:0x%08X ns:%llu", val, time_out);
    decrementer_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        time_out,
        trigger_decrementer_exception
    );
}

void dppc_interpreter::ppc_mfspr() {
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);

#ifdef CPU_PROFILING
    if (ref_spr > 31) {
        num_supervisor_instrs++;
    }
#endif

    switch (ref_spr) {
    case SPR::RTCL_U:
        calc_rtcl_value();
        ppc_state.spr[SPR::RTCL_U] = rtc_lo & 0x3FFFFF80UL;
        break;
    case SPR::RTCU_U:
        calc_rtcl_value();
        ppc_state.spr[SPR::RTCU_U] = rtc_hi;
        break;
    case SPR::DEC:
        ppc_state.spr[SPR::DEC] = calc_dec_value();
        break;
    }

    ppc_state.gpr[(ppc_cur_instruction >> 21) & 31] = ppc_state.spr[ref_spr];
}

void dppc_interpreter::ppc_mtspr() {
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);
    reg_s            = (ppc_cur_instruction >> 21) & 31;

#ifdef CPU_PROFILING
    if (ref_spr > 31) {
        num_supervisor_instrs++;
    }
#endif

    uint32_t val = ppc_state.gpr[reg_s];

    if (ref_spr != SPR::PVR) { // prevent writes to the read-only PVR
        ppc_state.spr[ref_spr] = val;
    }

    if (ref_spr == SPR::SDR1) { // adapt to SDR1 changes
        mmu_pat_ctx_changed();
    }

    switch (ref_spr) {
    case SPR::RTCL_S:
        calc_rtcl_value();
        rtc_lo = val & 0x3FFFFF80UL;
        break;
    case SPR::RTCU_S:
        calc_rtcl_value();
        rtc_hi = val;
        break;
    case SPR::DEC:
        update_decrementer(val);
        break;
    case SPR::TBL_S:
        update_timebase(0xFFFFFFFF00000000ULL, val);
        break;
    case SPR::TBU_S:
        update_timebase(0x00000000FFFFFFFFULL, (uint64_t)(val) << 32);
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

void dppc_interpreter::ppc_mftb() {
    uint32_t ref_spr = (((ppc_cur_instruction >> 11) & 31) << 5) | ((ppc_cur_instruction >> 16) & 31);
    reg_d = (ppc_cur_instruction >> 21) & 31;

    uint64_t tbr_value = calc_tbr_value();

    switch (ref_spr) {
    case SPR::TBL_U:
        ppc_state.gpr[reg_d] = tbr_value & 0xFFFFFFFFUL;
        break;
    case SPR::TBU_U:
        ppc_state.gpr[reg_d] = (tbr_value >> 32) & 0xFFFFFFFFUL;
        break;
    default:
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
     }
}

void dppc_interpreter::ppc_mtcrf() {
    uint32_t cr_mask = 0;
    ppc_grab_regssa();
    crm = ((ppc_cur_instruction >> 12) & 255);
    // check this
    cr_mask = (crm & 128) ? 0xF0000000 : 0x00000000;
    cr_mask += (crm & 64) ? 0x0F000000 : 0x00000000;
    cr_mask += (crm & 32) ? 0x00F00000 : 0x00000000;
    cr_mask += (crm & 16) ? 0x000F0000 : 0x00000000;
    cr_mask += (crm & 8) ? 0x0000F000 : 0x00000000;
    cr_mask += (crm & 4) ? 0x00000F00 : 0x00000000;
    cr_mask += (crm & 2) ? 0x000000F0 : 0x00000000;
    cr_mask += (crm & 1) ? 0x0000000F : 0x00000000;
    ppc_state.cr = (ppc_result_d & cr_mask) | (ppc_state.cr & ~(cr_mask));
}

void dppc_interpreter::ppc_mcrxr() {
    int crf_d    = (ppc_cur_instruction >> 21) & 0x1C;
    ppc_state.cr = (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        ((ppc_state.spr[SPR::XER] & 0xF0000000UL) >> crf_d);
    ppc_state.spr[SPR::XER] &= 0x0FFFFFFF;
}

void dppc_interpreter::ppc_extsb() {
    ppc_grab_regssa();
    ppc_result_d = ppc_result_d & 0xFF;
    ppc_result_a = (ppc_result_d < 0x80) ? (ppc_result_d & 0x000000FF)
                                         : (0xFFFFFF00UL | (ppc_result_d & 0x000000FF));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::ppc_extsh() {
    ppc_grab_regssa();
    ppc_result_d = ppc_result_d & 0xFFFF;
    ppc_result_a = (ppc_result_d < 0x8000) ? (ppc_result_d & 0x0000FFFF)
                                           : (0xFFFF0000UL | (ppc_result_d & 0x0000FFFF));
    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

// Branching Instructions

// The last two bytes of the instruction are used for determining how the branch happens.
// The middle 24 bytes are the 24-bit address to use for branching to.


void dppc_interpreter::ppc_b() {
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li              = (quick_test < 0x2000000) ? quick_test : (0xFC000000UL + quick_test);
    ppc_next_instruction_address = (uint32_t)(ppc_state.pc + adr_li);
    exec_flags = EXEF_BRANCH;
}

void dppc_interpreter::ppc_bl() {
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li              = (quick_test < 0x2000000) ? quick_test : (0xFC000000UL + quick_test);
    ppc_next_instruction_address = (uint32_t)(ppc_state.pc + adr_li);
    ppc_state.spr[SPR::LR]       = (uint32_t)(ppc_state.pc + 4);
    exec_flags = EXEF_BRANCH;
}

void dppc_interpreter::ppc_ba() {
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li              = (quick_test < 0x2000000) ? quick_test : (0xFC000000UL + quick_test);
    ppc_next_instruction_address = adr_li;
    exec_flags = EXEF_BRANCH;
}

void dppc_interpreter::ppc_bla() {
    uint32_t quick_test = (ppc_cur_instruction & 0x03FFFFFC);
    adr_li              = (quick_test < 0x2000000) ? quick_test : (0xFC000000UL + quick_test);
    ppc_next_instruction_address = adr_li;
    ppc_state.spr[SPR::LR]       = ppc_state.pc + 4;
    exec_flags = EXEF_BRANCH;
}

void dppc_interpreter::ppc_bc() {
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t br_bd  = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ppc_state.pc + br_bd);
        exec_flags = EXEF_BRANCH;
    }
}

void dppc_interpreter::ppc_bca() {
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t br_bd  = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = br_bd;
        exec_flags = EXEF_BRANCH;
    }
}

void dppc_interpreter::ppc_bcl() {
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t br_bd  = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ppc_state.pc + br_bd);
        exec_flags = EXEF_BRANCH;
    }
    ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
}

void dppc_interpreter::ppc_bcla() {
    uint32_t ctr_ok;
    uint32_t cnd_ok;
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    int32_t br_bd  = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFC));

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = br_bd;
        exec_flags = EXEF_BRANCH;
    }
    ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
}

void dppc_interpreter::ppc_bcctr() {
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;

    uint32_t cnd_ok = (br_bo & 0x10) ||
        (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (cnd_ok) {
        ppc_next_instruction_address = (ppc_state.spr[SPR::CTR] & 0xFFFFFFFCUL);
        exec_flags = EXEF_BRANCH;
    }
}

void dppc_interpreter::ppc_bcctrl() {
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;

    uint32_t cnd_ok = (br_bo & 0x10) ||
        (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (cnd_ok) {
        ppc_next_instruction_address = (ppc_state.spr[SPR::CTR] & 0xFFFFFFFCUL);
        exec_flags = EXEF_BRANCH;
    }
    ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
}

void dppc_interpreter::ppc_bclr() {
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    uint32_t ctr_ok;
    uint32_t cnd_ok;

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ppc_state.spr[SPR::LR] & 0xFFFFFFFCUL);
        exec_flags = EXEF_BRANCH;
    }
}

void dppc_interpreter::ppc_bclrl() {
    uint32_t br_bo = (ppc_cur_instruction >> 21) & 31;
    uint32_t br_bi = (ppc_cur_instruction >> 16) & 31;
    uint32_t ctr_ok;
    uint32_t cnd_ok;

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) || ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) || (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ppc_state.spr[SPR::LR] & 0xFFFFFFFCUL);
        exec_flags = EXEF_BRANCH;
    }
    ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
}
// Compare Instructions

void dppc_interpreter::ppc_cmp() {
#ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        LOG_F(WARNING, "Invalid CMP instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (ppc_cur_instruction >> 21) & 0x1C;
    ppc_grab_regssab();
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (((int32_t)ppc_result_a) == ((int32_t)ppc_result_b))
        ? 0x20000000UL
        : (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
}

void dppc_interpreter::ppc_cmpi() {
#ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        LOG_F(WARNING, "Invalid CMPI instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (ppc_cur_instruction >> 21) & 0x1C;
    ppc_grab_regsasimm();
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (((int32_t)ppc_result_a) == simm)
        ? 0x20000000UL
        : (((int32_t)ppc_result_a) > simm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
}

void dppc_interpreter::ppc_cmpl() {
#ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        LOG_F(WARNING, "Invalid CMPL instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (ppc_cur_instruction >> 21) & 0x1C;
    ppc_grab_regssab();
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (ppc_result_a == ppc_result_b)
        ? 0x20000000UL
        : (ppc_result_a > ppc_result_b) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
}

void dppc_interpreter::ppc_cmpli() {
#ifdef CHECK_INVALID
    if (ppc_cur_instruction & 0x200000) {
        LOG_F(WARNING, "Invalid CMPLI instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (ppc_cur_instruction >> 21) & 0x1C;
    ppc_grab_regssauimm();
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (ppc_result_a == uimm) ? 0x20000000UL
                                   : (ppc_result_a > uimm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
}

// Condition Register Changes

void dppc_interpreter::ppc_mcrf() {
    int crf_d       = (ppc_cur_instruction >> 21) & 0x1C;
    int crf_s       = (ppc_cur_instruction >> 16) & 0x1C;

    // extract and right justify source flags field
    uint32_t grab_s = (ppc_state.cr >> (28 - crf_s)) & 0xF;

    ppc_state.cr = (ppc_state.cr & ~(0xf0000000UL >> crf_d)) | (grab_s << (28 - crf_d));
}

void dppc_interpreter::ppc_crand() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    } else {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    }
}

void dppc_interpreter::ppc_crandc() {
    ppc_grab_regsdab();
    if ((ppc_state.cr & (0x80000000UL >> reg_a)) && !(ppc_state.cr & (0x80000000UL >> reg_b))) {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    } else {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    }
}
void dppc_interpreter::ppc_creqv() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) { // compliment is implemented by swapping the following if/else bodies
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    } else {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    }
}
void dppc_interpreter::ppc_crnand() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    } else {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    }
}

void dppc_interpreter::ppc_crnor() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    } else {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    }
}

void dppc_interpreter::ppc_cror() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    } else {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    }
}

void dppc_interpreter::ppc_crorc() {
    ppc_grab_regsdab();
    if ((ppc_state.cr & (0x80000000UL >> reg_a)) || !(ppc_state.cr & (0x80000000UL >> reg_b))) {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    } else {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    }
}
void dppc_interpreter::ppc_crxor() {
    ppc_grab_regsdab();
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_state.cr |= (0x80000000UL >> reg_d);
    } else {
        ppc_state.cr &= ~(0x80000000UL >> reg_d);
    }
}

// Processor MGMT Fns.

void dppc_interpreter::ppc_rfi() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    uint32_t new_srr1_val        = (ppc_state.spr[SPR::SRR1] & 0x87C0FF73UL);
    uint32_t new_msr_val         = (ppc_state.msr & ~(0x87C0FF73UL));
    ppc_state.msr                = (new_msr_val | new_srr1_val) & 0xFFFBFFFFUL;

    // generate External Interrupt Exception
    // if CPU interrupt line is still asserted
    if (ppc_state.msr & MSR::EE && int_pin) {
        uint32_t save_srr0 = ppc_state.spr[SPR::SRR0] & 0xFFFFFFFCUL;
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
        ppc_state.spr[SPR::SRR0] = save_srr0;
        return;
    }

    if ((ppc_state.msr & MSR::EE) && dec_exception_pending) {
        dec_exception_pending = false;
        //LOG_F(WARNING, "decrementer exception from rfi msr:0x%X", ppc_state.msr);
        uint32_t save_srr0 = ppc_state.spr[SPR::SRR0] & 0xFFFFFFFCUL;
        ppc_exception_handler(Except_Type::EXC_DECR, 0);
        ppc_state.spr[SPR::SRR0] = save_srr0;
        return;
    }

    ppc_next_instruction_address = ppc_state.spr[SPR::SRR0] & 0xFFFFFFFCUL;

    do_ctx_sync(); // RFI is context synchronizing

    mmu_change_mode();

    grab_return = true;
    exec_flags = EXEF_RFI;
}

void dppc_interpreter::ppc_sc() {
    do_ctx_sync(); // SC is context synchronizing!
    ppc_exception_handler(Except_Type::EXC_SYSCALL, 0x20000);
}

void dppc_interpreter::ppc_tw() {
    reg_a  = (ppc_cur_instruction >> 11) & 31;
    reg_b  = (ppc_cur_instruction >> 16) & 31;
    ppc_to = (ppc_cur_instruction >> 21) & 31;
    if ((((int32_t)ppc_state.gpr[reg_a] < (int32_t)ppc_state.gpr[reg_b]) && (ppc_to & 0x10)) ||
        (((int32_t)ppc_state.gpr[reg_a] > (int32_t)ppc_state.gpr[reg_b]) && (ppc_to & 0x08)) ||
        (((int32_t)ppc_state.gpr[reg_a] == (int32_t)ppc_state.gpr[reg_b]) && (ppc_to & 0x04)) ||
        ((ppc_state.gpr[reg_a] < ppc_state.gpr[reg_b]) && (ppc_to & 0x02)) ||
        ((ppc_state.gpr[reg_a] > ppc_state.gpr[reg_b]) && (ppc_to & 0x01))) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::TRAP);
    }
}

void dppc_interpreter::ppc_twi() {
    simm   = (int32_t)((int16_t)((ppc_cur_instruction)&0xFFFF));
    reg_a  = (ppc_cur_instruction >> 16) & 0x1F;
    ppc_to = (ppc_cur_instruction >> 21) & 0x1F;
    if ((((int32_t)ppc_state.gpr[reg_a] < simm) && (ppc_to & 0x10)) ||
        (((int32_t)ppc_state.gpr[reg_a] > simm) && (ppc_to & 0x08)) ||
        (((int32_t)ppc_state.gpr[reg_a] == simm) && (ppc_to & 0x04)) ||
        ((ppc_state.gpr[reg_a] < (uint32_t)simm) && (ppc_to & 0x02)) ||
        ((ppc_state.gpr[reg_a] > (uint32_t)simm) && (ppc_to & 0x01))) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::TRAP);
    }
}

void dppc_interpreter::ppc_eieio() {
    /* placeholder */
}

void dppc_interpreter::ppc_isync() {
    do_ctx_sync();
}

void dppc_interpreter::ppc_sync() {
    /* placeholder */
}

void dppc_interpreter::ppc_icbi() {
    /* placeholder */
}

void dppc_interpreter::ppc_dcbf() {
    /* placeholder */
}

void dppc_interpreter::ppc_dcbi() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
}

void dppc_interpreter::ppc_dcbst() {
    /* placeholder */
}

void dppc_interpreter::ppc_dcbt() {
    // Not needed, the HDI reg is touched to no-op this instruction.
    return;
}

void dppc_interpreter::ppc_dcbtst() {
    // Not needed, the HDI reg is touched to no-op this instruction.
    return;
}

void dppc_interpreter::ppc_dcbz() {
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);

    ppc_effective_address &= 0xFFFFFFE0UL; // align EA on a 32-byte boundary

    // the following is not especially efficient but necessary
    // to make BlockZero under Mac OS 8.x and later to work
    mmu_write_vmem<uint64_t>(ppc_effective_address +  0, 0);
    mmu_write_vmem<uint64_t>(ppc_effective_address +  8, 0);
    mmu_write_vmem<uint64_t>(ppc_effective_address + 16, 0);
    mmu_write_vmem<uint64_t>(ppc_effective_address + 24, 0);
}


// Integer Load and Store Functions

void dppc_interpreter::ppc_stb() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += reg_a ? ppc_result_a : 0;
    mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_result_d);
    //mem_write_byte(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stbx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_result_d);
    //mem_write_byte(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stbu() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_result_d);
        //mem_write_byte(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stbux() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    if (reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_result_d);
        //mem_write_byte(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_sth() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_result_d);
    //mem_write_word(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_sthu() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_result_d);
        //mem_write_word(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_sthux() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    if (reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_result_d);
        //mem_write_word(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_sthx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_result_d);
    //mem_write_word(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_sthbrx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);
    ppc_result_d          = (uint32_t)(BYTESWAP_16((uint16_t)ppc_result_d));
    mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_result_d);
    //mem_write_word(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stw() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += reg_a ? ppc_result_a : 0;
    mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
    //mem_write_dword(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stwx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
    //mem_write_dword(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stwcx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    if (rc_flag == 0) {
        ppc_illegalop();
    } else {
        ppc_grab_regssab();
        ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);
        ppc_state.cr &= 0x0FFFFFFFUL; // clear CR0
        ppc_state.cr |= (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3; // copy XER[SO] to CR0[SO]
        if (ppc_state.reserve) {
            mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
            ppc_state.reserve = false;
            ppc_state.cr |= 0x20000000UL; // set CR0[EQ]
        }
    }
}

void dppc_interpreter::ppc_stwu() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
        //mem_write_dword(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stwux() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    if (reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
        //mem_write_dword(ppc_effective_address, ppc_result_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stwbrx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    ppc_result_d          = BYTESWAP_32(ppc_result_d);
    mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);
    //mem_write_dword(ppc_effective_address, ppc_result_d);
}

void dppc_interpreter::ppc_stmw() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;

    /* what should we do if EA is unaligned? */
    if (ppc_effective_address & 3) {
        ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x00000);
    }

    for (; reg_s <= 31; reg_s++) {
        mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_state.gpr[reg_s]);
        //mem_write_dword(ppc_effective_address, ppc_state.gpr[reg_s]);
        ppc_effective_address += 4;
    }
}

void dppc_interpreter::ppc_lbz() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    //ppc_result_d = mem_grab_byte(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint8_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lbzu() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address += ppc_result_a;
        //ppc_result_d = mem_grab_byte(ppc_effective_address);
        ppc_result_d = mmu_read_vmem<uint8_t>(ppc_effective_address);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_regd();
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lbzx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //ppc_result_d          = mem_grab_byte(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint8_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lbzux() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        //ppc_result_d          = mem_grab_byte(ppc_effective_address);
        ppc_result_d = mmu_read_vmem<uint8_t>(ppc_effective_address);
        ppc_result_a          = ppc_effective_address;
        ppc_store_result_regd();
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}


void dppc_interpreter::ppc_lhz() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += reg_a ? ppc_result_a : 0;
    //ppc_result_d = mem_grab_word(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint16_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lhzu() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        //ppc_result_d = mem_grab_word(ppc_effective_address);
        ppc_result_d = mmu_read_vmem<uint16_t>(ppc_effective_address);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_regd();
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lhzx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //ppc_result_d         = mem_grab_word(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint16_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lhzux() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        //ppc_result_d          = mem_grab_word(ppc_effective_address);
        ppc_result_d = mmu_read_vmem<uint16_t>(ppc_effective_address);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_regd();
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lha() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    //uint16_t val = mem_grab_word(ppc_effective_address);
    uint16_t val = mmu_read_vmem<uint16_t>(ppc_effective_address);
    if (val & 0x8000) {
        ppc_result_d = 0xFFFF0000UL | (uint32_t)val;
    } else {
        ppc_result_d = (uint32_t)val;
    }
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lhau() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        //uint16_t val = mem_grab_word(ppc_effective_address);
        uint16_t val = mmu_read_vmem<uint16_t>(ppc_effective_address);
        if (val & 0x8000) {
            ppc_result_d = 0xFFFF0000UL | (uint32_t)val;
        } else {
            ppc_result_d = (uint32_t)val;
        }
        ppc_store_result_regd();
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lhaux() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //uint16_t val          = mem_grab_word(ppc_effective_address);
    uint16_t val = mmu_read_vmem<uint16_t>(ppc_effective_address);
    if (val & 0x8000) {
        ppc_result_d = 0xFFFF0000UL | (uint32_t)val;
    } else {
        ppc_result_d = (uint32_t)val;
    }
    ppc_store_result_regd();
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_lhax() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //uint16_t val          = mem_grab_word(ppc_effective_address);
    uint16_t val = mmu_read_vmem<uint16_t>(ppc_effective_address);
    if (val & 0x8000) {
        ppc_result_d = 0xFFFF0000UL | (uint32_t)val;
    } else {
        ppc_result_d = (uint32_t)val;
    }
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lhbrx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //ppc_result_d          = (uint32_t)(BYTESWAP_16(mem_grab_word(ppc_effective_address)));
    ppc_result_d = (uint32_t)(BYTESWAP_16(mmu_read_vmem<uint16_t>(ppc_effective_address)));
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lwz() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    //ppc_result_d = mem_grab_dword(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lwbrx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //ppc_result_d          = BYTESWAP_32(mem_grab_dword(ppc_effective_address));
    ppc_result_d = BYTESWAP_32(mmu_read_vmem<uint32_t>(ppc_effective_address));
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lwzu() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address += ppc_result_a;
        //ppc_result_d = mem_grab_dword(ppc_effective_address);
        ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
        ppc_store_result_regd();
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lwzx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    //ppc_result_d          = mem_grab_dword(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lwzux() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();
    if ((reg_a != reg_d) || reg_a != 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
    //ppc_result_d = mem_grab_dword(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_regd();
    ppc_store_result_rega();
}

void dppc_interpreter::ppc_lwarx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    // Placeholder - Get the reservation of memory implemented!
    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);
    ppc_state.reserve     = true;
    //ppc_result_d          = mem_grab_dword(ppc_effective_address);
    ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_store_result_regd();
}

void dppc_interpreter::ppc_lmw() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    // How many words to load in memory - using a do-while for this
    do {
        //ppc_state.gpr[reg_d] = mem_grab_dword(ppc_effective_address);
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ppc_effective_address);
        ppc_effective_address += 4;
        reg_d++;
    } while (reg_d < 32);
}

void dppc_interpreter::ppc_lswi() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda();
    ppc_effective_address = reg_a ? ppc_result_a : 0;
    grab_inb              = (ppc_cur_instruction >> 11) & 0x1F;
    grab_inb              = grab_inb ? grab_inb : 32;

    while (grab_inb >= 4) {
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ppc_effective_address);
        reg_d++;
        if (reg_d >= 32) {    // wrap around through GPR0
            reg_d = 0;
        }
        ppc_effective_address += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint8_t>(ppc_effective_address) << 24;
        break;
    case 2:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ppc_effective_address) << 16;
        break;
    case 3:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ppc_effective_address) << 16;
        ppc_state.gpr[reg_d] += mmu_read_vmem<uint8_t>(ppc_effective_address + 2) << 8;
        break;
    default:
        break;
    }
}

void dppc_interpreter::ppc_lswx() {
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab();

    // Invalid instruction forms
    if ((reg_d | reg_a) == 0 || (reg_d == reg_a) || (reg_d == reg_b)) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }

    ppc_effective_address  = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    grab_inb               = ppc_state.spr[SPR::XER] & 0x7F;

    while (grab_inb >= 4) {
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ppc_effective_address);
        reg_d++;
        if (reg_d >= 32) {    // wrap around through GPR0
            reg_d = 0;
        }
        ppc_effective_address += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint8_t>(ppc_effective_address) << 24;
        break;
    case 2:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ppc_effective_address) << 16;
        break;
    case 3:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ppc_effective_address) << 16;
        ppc_state.gpr[reg_d] += mmu_read_vmem<uint8_t>(ppc_effective_address + 2) << 8;
        break;
    default:
        break;
    }
}

void dppc_interpreter::ppc_stswi() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa();
    ppc_effective_address = reg_a ? ppc_result_a : 0;
    grab_inb              = (ppc_cur_instruction >> 11) & 0x1F;
    grab_inb              = grab_inb ? grab_inb : 32;

    while (grab_inb >= 4) {
        mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_state.gpr[reg_s]);
        reg_s++;
        if (reg_s >= 32) {    // wrap around through GPR0
            reg_s = 0;
        }
        ppc_effective_address += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 24);
        break;
    case 2:
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 16);
        break;
    case 3:
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 16);
        mmu_write_vmem<uint8_t>(ppc_effective_address + 2, (ppc_state.gpr[reg_s] >> 8) & 0xFF);
        break;
    default:
        break;
    }
}

void dppc_interpreter::ppc_stswx() {
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab();
    ppc_effective_address = reg_a ? (ppc_result_a + ppc_result_b) : ppc_result_b;
    grab_inb              = ppc_state.spr[SPR::XER] & 127;

    while (grab_inb >= 4) {
        mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_state.gpr[reg_s]);
        reg_s++;
        if (reg_s >= 32) {    // wrap around through GPR0
            reg_s = 0;
        }
        ppc_effective_address += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        mmu_write_vmem<uint8_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 24);
        break;
    case 2:
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 16);
        break;
    case 3:
        mmu_write_vmem<uint16_t>(ppc_effective_address, ppc_state.gpr[reg_s] >> 16);
        mmu_write_vmem<uint8_t>(ppc_effective_address + 2, (ppc_state.gpr[reg_s] >> 8) & 0xFF);
        break;
    default:
        break;
    }
}

void dppc_interpreter::ppc_eciwx() {
    uint32_t ear_enable = 0x80000000;

    // error if EAR[E] != 1
    if (!(ppc_state.spr[282] && ear_enable)) {
        ppc_exception_handler(Except_Type::EXC_DSI, 0x0);
    }

    ppc_grab_regsdab();
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);

    if (ppc_effective_address & 0x3) {
        ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x0);
    }

    ppc_result_d = mmu_read_vmem<uint32_t>(ppc_effective_address);

    ppc_store_result_regd();
}

void dppc_interpreter::ppc_ecowx() {
    uint32_t ear_enable = 0x80000000;

    // error if EAR[E] != 1
    if (!(ppc_state.spr[282] && ear_enable)) {
        ppc_exception_handler(Except_Type::EXC_DSI, 0x0);
    }

    ppc_grab_regssab();
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);

    if (ppc_effective_address & 0x3) {
        ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x0);
    }

    mmu_write_vmem<uint32_t>(ppc_effective_address, ppc_result_d);

    ppc_store_result_regd();
}

// TLB Instructions

void dppc_interpreter::ppc_tlbie() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif

    tlb_flush_entry(ppc_state.gpr[(ppc_cur_instruction >> 11) & 31]);
}

void dppc_interpreter::ppc_tlbia() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
}

void dppc_interpreter::ppc_tlbld() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
}

void dppc_interpreter::ppc_tlbli() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
}

void dppc_interpreter::ppc_tlbsync() {
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
}
