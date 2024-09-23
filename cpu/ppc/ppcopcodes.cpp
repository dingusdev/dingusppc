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

// General opcodes for the processor - ppcopcodes.cpp

#include <core/timermanager.h>
#include <core/mathutils.h>
#include "ppcemu.h"
#include "ppcmacros.h"
#include "ppcmmu.h"
#include <cinttypes>
#include <vector>

//Extract the registers desired and the values of the registers.

// Affects CR Field 0 - For integer operations
void ppc_changecrf0(uint32_t set_result) {
    ppc_state.cr =
        (ppc_state.cr & 0x0FFFFFFFU) // clear CR0
        | (
            (set_result == 0) ?
                CRx_bit::CR_EQ
            : (int32_t(set_result) < 0) ?
                CRx_bit::CR_LT
            :
                CRx_bit::CR_GT
        )
        | ((ppc_state.spr[SPR::XER] & XER::SO) >> 3); // copy XER[SO] into CR0[SO].
}

// Affects the XER register's Carry Bit

inline static void ppc_carry(uint32_t a, uint32_t b) {
    if (b < a) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }
}

inline static void ppc_carry_sub(uint32_t a, uint32_t b) {
    if (b >= a) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }
}

// Affects the XER register's SO and OV Bits

inline static void ppc_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if (int32_t((a ^ b) & (a ^ d)) < 0) {
        ppc_set_xer(XER::SO | XER::OV);
    } else {
        ppc_unset_xer(XER::OV);
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

void add_ctx_sync_action(const CtxSyncCallback& cb) {
    gCtxSyncCallbacks.push_back(cb);
}

/**
The core functionality of this PPC emulation is within all of these void functions.
This is where the opcode tables in the ppcemumain.h come into play - reducing the number of
comparisons needed. This means loads of functions, but less CPU cycles needed to determine the
function (theoretically).
**/

OPCODESHIFT (addi,
    ppc_grab_regsdasimm(instr);
    if (shift)
        ppc_state.gpr[reg_d] = (reg_a == 0) ? (simm << 16) : (ppc_result_a + (simm << 16));
    else
        ppc_state.gpr[reg_d] = (reg_a == 0) ? simm : (ppc_result_a + simm);
)

template void dppc_interpreter::ppc_addi<SHFT0>(uint32_t instr);
template void dppc_interpreter::ppc_addi<SHFT1>(uint32_t instr);

OPCODEREC (addic,
    ppc_grab_regsdasimm(instr);
    uint32_t ppc_result_d = (ppc_result_a + simm);
    ppc_carry(ppc_result_a, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_addic<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_addic<RC1>(uint32_t instr);

OPCODECARRY (add,
    ppc_grab_regsdab(instr);
    uint32_t ppc_result_d = ppc_result_a + ppc_result_b;

    if (carry)
        ppc_carry(ppc_result_a, ppc_result_d);
    if (ov)
        ppc_setsoov(ppc_result_a, ~ppc_result_b, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_add<CARRY0, RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY0, RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY0, RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY0, RC1, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY1, RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY1, RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY1, RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_add<CARRY1, RC1, OV1>(uint32_t instr);

OPCODEOVREC (adde,
    ppc_grab_regsdab(instr);
    uint32_t xer_ca       = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ppc_result_a + ppc_result_b + xer_ca;

    if ((ppc_result_d < ppc_result_a) || (xer_ca && (ppc_result_d == ppc_result_a))) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }

    if (ov)
        ppc_setsoov(ppc_result_a, ~ppc_result_b, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_adde<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_adde<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_adde<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_adde<RC1, OV1>(uint32_t instr);

OPCODEOVREC (addme,
    ppc_grab_regsda(instr);
    uint32_t xer_ca       = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ppc_result_a + xer_ca - 1;

    if (((xer_ca - 1) < 0xFFFFFFFFUL) || (ppc_result_d < ppc_result_a)) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }

    if (ov)
        ppc_setsoov(ppc_result_a, 0, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_addme<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_addme<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_addme<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_addme<RC1, OV1>(uint32_t instr);

OPCODEOVREC (addze,
    ppc_grab_regsda(instr);
    uint32_t grab_xer     = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ppc_result_a + grab_xer;

    if (ppc_result_d < ppc_result_a) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }

    if (ov)
        ppc_setsoov(ppc_result_a, 0xFFFFFFFFUL, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_addze<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_addze<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_addze<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_addze<RC1, OV1>(uint32_t instr);

OPCODE (subfic,
    ppc_grab_regsdasimm(instr);
    uint32_t ppc_result_d = simm - ppc_result_a;
    if (simm == -1) { 
        ppc_set_xer(XER::CA);
    } 
    else { 
        ppc_carry(~ppc_result_a, ppc_result_d);
    }
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODECARRY (subf,
    ppc_grab_regsdab(instr);
    uint32_t ppc_result_d = ppc_result_b - ppc_result_a;

    if (carry)
        ppc_carry_sub(ppc_result_a, ppc_result_b);
    if (ov)
        ppc_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_subf<CARRY0, RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY0, RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY0, RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY0, RC1, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY1, RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY1, RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY1, RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subf<CARRY1, RC1, OV1>(uint32_t instr);

OPCODEOVREC (subfe,
    ppc_grab_regsdab(instr);
    uint32_t grab_ca      = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ~ppc_result_a + ppc_result_b + grab_ca;
    if (grab_ca && ppc_result_b == 0xFFFFFFFFUL)
        ppc_set_xer(XER::CA);
    else
        ppc_carry(~ppc_result_a, ppc_result_d);

    if (ov)
        ppc_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_subfe<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfe<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subfe<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfe<RC1, OV1>(uint32_t instr);

OPCODEOVREC (subfme,
    ppc_grab_regsda(instr);
    uint32_t grab_ca      = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ~ppc_result_a + grab_ca - 1;

    if (ppc_result_a == 0xFFFFFFFFUL && !grab_ca)
        ppc_unset_xer(XER::CA);
    else
        ppc_set_xer(XER::CA);

    if (ov) {
        if (ppc_result_d == ppc_result_a && int32_t(ppc_result_d) > 0)
            ppc_set_xer(XER::SO | XER::OV);
        else
            ppc_unset_xer(XER::OV);
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_subfme<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfme<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subfme<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfme<RC1, OV1>(uint32_t instr);

OPCODEOVREC (subfze,
    ppc_grab_regsda(instr);
    uint32_t grab_ca      = !!(ppc_state.spr[SPR::XER] & XER::CA);
    uint32_t ppc_result_d = ~ppc_result_a + grab_ca;

    if (!ppc_result_d && grab_ca) {    // special case: ppc_result_d = 0 and CA=1
        ppc_set_xer(XER::CA);
    } 
    else { 
        ppc_unset_xer(XER::CA); 
    }

    if (ov) {
        if (ppc_result_d && ppc_result_d == ppc_result_a) {
            ppc_set_xer(XER::SO | XER::OV);
        } 
        else {
            ppc_unset_xer(XER::OV);
        }
    }

    if (rec) {   
        ppc_changecrf0(ppc_result_d); 
    }

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_subfze<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfze<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_subfze<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_subfze<RC1, OV1>(uint32_t instr);

OPCODESHIFT (andirc,
    ppc_grab_regssauimm(instr);
    ppc_result_a = shift ? (ppc_result_d & (uimm << 16)) : (ppc_result_d & uimm);
    ppc_changecrf0(ppc_result_a);
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_andirc<SHFT0>(uint32_t instr);
template void dppc_interpreter::ppc_andirc<SHFT1>(uint32_t instr);

OPCODESHIFT (ori,
    ppc_grab_regssauimm(instr);
    ppc_result_a = shift ? (ppc_result_d | (uimm << 16)) : (ppc_result_d | uimm);
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_ori<SHFT0>(uint32_t instr);
template void dppc_interpreter::ppc_ori<SHFT1>(uint32_t instr);

OPCODESHIFT (xori,
    ppc_grab_regssauimm(instr);
    ppc_result_a = shift ? (ppc_result_d ^ (uimm << 16)) : (ppc_result_d ^ uimm);
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_xori<SHFT0>(uint32_t instr);
template void dppc_interpreter::ppc_xori<SHFT1>(uint32_t instr);

OPCODELOGIC(logical,
    ppc_grab_regssab(instr);
    if (logical_op == logical_fun::ppc_and)
        ppc_result_a = ppc_result_d & ppc_result_b;
    else if (logical_op == logical_fun::ppc_andc)
        ppc_result_a = ppc_result_d & ~(ppc_result_b);
    else if (logical_op == logical_fun::ppc_eqv)
        ppc_result_a = ~(ppc_result_d ^ ppc_result_b);
    else if (logical_op == logical_fun::ppc_nand)
        ppc_result_a = ~(ppc_result_d & ppc_result_b);
    else if (logical_op == logical_fun::ppc_nor)
        ppc_result_a = ~(ppc_result_d | ppc_result_b);
    else if (logical_op == logical_fun::ppc_or)
        ppc_result_a = ppc_result_d | ppc_result_b;
    else if (logical_op == logical_fun::ppc_orc)
        ppc_result_a = ppc_result_d | ~(ppc_result_b);
    else if (logical_op == logical_fun::ppc_xor)
        ppc_result_a = ppc_result_d ^ ppc_result_b;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_logical<ppc_and, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_andc, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_eqv, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_nand, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_nor, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_or, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_orc, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_xor, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_and, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_andc, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_eqv, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_nand, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_nor, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_or, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_orc, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_logical<ppc_xor, RC1>(uint32_t instr);

OPCODEOVREC (neg,
    ppc_grab_regsda(instr);
    uint32_t ppc_result_d = ~(ppc_result_a) + 1;

    if (ov) {
        if (ppc_result_a == 0x80000000)
            ppc_set_xer(XER::SO | XER::OV);
        else
            ppc_unset_xer(XER::OV);
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_neg<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_neg<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_neg<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_neg<RC1, OV1>(uint32_t instr);

OPCODEREC (cntlzw,
    ppc_grab_regssa(instr);

    uint32_t bit_check = ppc_result_d;

    #ifdef __builtin_clz //for GCC and Clang users
        uint32_t lead = !bit_check ? 32 : __builtin_clz(bit_check);
    #elif defined __lzcnt //for Visual C++ users
        uint32_t lead = __lzcnt(bit_check);
    #else
    
    uint32_t lead, mask = 0;

    for (mask = 0x80000000UL, lead = 0; mask != 0; lead++, mask >>= 1) {
        if (bit_check & mask) {
            break;
        }
    }
#endif
    ppc_result_a = lead;

    if (rec) 
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_cntlzw<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_cntlzw<RC1>(uint32_t instr);

OPCODEREC (mulhwu,
    ppc_grab_regsdab(instr);
    uint64_t product = uint64_t(ppc_result_a) * uint64_t(ppc_result_b);
    uint32_t ppc_result_d = uint32_t(product >> 32);

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_mulhwu<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_mulhwu<RC1>(uint32_t instr);

OPCODEREC (mulhw,
    ppc_grab_regsdab(instr);
    int64_t product = int64_t(int32_t(ppc_result_a)) * int64_t(int32_t(ppc_result_b));
    uint32_t ppc_result_d = product >> 32;

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_mulhw<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_mulhw<RC1>(uint32_t instr);

OPCODEOVREC (mullw,
    ppc_grab_regsdab(instr);
    int64_t product = int64_t(int32_t(ppc_result_a)) * int64_t(int32_t(ppc_result_b));

    if (ov) {
        if (product != int64_t(int32_t(product))) {
            ppc_set_xer(XER::SO | XER::OV);
        } else {
            ppc_unset_xer(XER::OV);
        }
    }

    uint32_t ppc_result_d = (uint32_t)product;

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_mullw<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_mullw<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_mullw<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_mullw<RC1, OV1>(uint32_t instr);

OPCODE (mulli,
    ppc_grab_regsdasimm(instr);
    int64_t product          = int64_t(int32_t(ppc_result_a)) * int64_t(int32_t(simm));
    uint32_t ppc_result_d    = uint32_t(product);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODEOVREC (divw,
    uint32_t ppc_result_d = 0;
    ppc_grab_regsdab(instr);

    if (!ppc_result_b) { // handle the "anything / 0" case
        ppc_result_d = 0; // tested on G4 in Mac OS X 10.4 and Open Firmware.
        // ppc_result_d = (int32_t(ppc_result_a) < 0) ? -1 : 0; /* UNDOCUMENTED! */

        if (ov)
            ppc_set_xer(XER::SO | XER::OV);

    } else if (ppc_result_a == 0x80000000UL && ppc_result_b == 0xFFFFFFFFUL) {
        ppc_result_d = 0; // tested on G4 in Mac OS X 10.4 and Open Firmware.

        if (ov)
            ppc_set_xer(XER::SO | XER::OV);

    } else { // normal signed devision
        ppc_result_d = int32_t(ppc_result_a) / int32_t(ppc_result_b);

        if (ov)
            ppc_unset_xer(XER::OV);
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_divw<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_divw<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_divw<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_divw<RC1, OV1>(uint32_t instr);

OPCODEOVREC (divwu,
    uint32_t ppc_result_d = 0;
    ppc_grab_regsdab(instr);

    if (!ppc_result_b) { // division by zero

        if (ov)
            ppc_set_xer(XER::SO | XER::OV);

        if (rec)
            ppc_state.cr |= XER::CA;

    } else {
        ppc_result_d = ppc_result_a / ppc_result_b;

        if (ov)
            ppc_unset_xer(XER::OV);
    }
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_divwu<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_divwu<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::ppc_divwu<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::ppc_divwu<RC1, OV1>(uint32_t instr);

// Value shifting

OPCODESHIFTREC (shift,
    ppc_grab_regssab(instr);
    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    }
    else {
        ppc_result_a = isleft ? (ppc_result_d << (ppc_result_b & 0x1F))
                              : (ppc_result_d >> (ppc_result_b & 0x1F));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_shift<RIGHT0, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_shift<RIGHT0, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_shift<LEFT1, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_shift<LEFT1, RC1>(uint32_t instr);

OPCODEREC (sraw,
    ppc_grab_regssab(instr);

    // clear XER[CA] by default
    ppc_unset_xer(XER::CA);

    if (ppc_result_b & 0x20) {
        // fill rA with the sign bit of rS
        ppc_result_a = int32_t(ppc_result_d) >> 31;
        if (ppc_result_a) // if rA is negative
            ppc_set_xer(XER::CA);
    } else {
        uint32_t shift = ppc_result_b & 0x1F;
        ppc_result_a   = int32_t(ppc_result_d) >> shift;
        if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & ((1U << shift) - 1)))
            ppc_set_xer(XER::CA);
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_sraw<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_sraw<RC1>(uint32_t instr);

OPCODEREC (srawi,
    ppc_grab_regssash(instr);

    // clear XER[CA] by default
    ppc_unset_xer(XER::CA);

    if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & ((1U << rot_sh) - 1))) {
        ppc_set_xer(XER::CA);
    }

    ppc_result_a = int32_t(ppc_result_d) >> rot_sh;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
)

template void dppc_interpreter::ppc_srawi<RC0>(uint32_t instr);
template void dppc_interpreter::ppc_srawi<RC1>(uint32_t instr);

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = uint32_t(0xFFFFFFFFUL << (31 - rot_me));
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

OPCODE(rlwimi,
    ppc_grab_regssash(instr);
    unsigned rot_mb = (instr >> 6) & 0x1F;
    unsigned rot_me = (instr >> 1) & 0x1F;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) |
                      (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    ppc_result_a    = (ppc_result_a & ~mask) | (r & mask);
    if ((instr & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

OPCODE (rlwinm,
    ppc_grab_regssash(instr);
    unsigned rot_mb = (instr >> 6) & 0x1F;
    unsigned rot_me = (instr >> 1) & 0x1F;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) |
                      (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    ppc_result_a    = r & mask;
    if ((instr & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

OPCODE (rlwnm,
    ppc_grab_regssab(instr);
    ppc_result_b &= 0x1F;
    unsigned rot_mb = (instr >> 6) & 0x1F;
    unsigned rot_me = (instr >> 1) & 0x1F;
    uint32_t mask   = rot_mask(rot_mb, rot_me);
    uint32_t rot    = ppc_result_b & 0x1F;
    uint32_t r      = rot ? ((ppc_result_d << rot) |
                      (ppc_result_d >> (32 - rot))) : ppc_result_d;
    ppc_result_a    = r & mask;
    if ((instr & 0x01) == 1) {
        ppc_changecrf0(ppc_result_a);
    }
    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

OPCODE(mfcr,
    int reg_d            = (instr >> 21) & 0x1F;
    ppc_state.gpr[reg_d] = ppc_state.cr;
)

OPCODE (mtsr,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    int reg_s             = (instr >> 21) & 0x1F;
    uint32_t grab_sr      = (instr >> 16) & 0x0F;
   if (ppc_state.sr[grab_sr] != ppc_state.gpr[reg_s]) {
        ppc_state.sr[grab_sr] = ppc_state.gpr[reg_s];
        mmu_pat_ctx_changed();
   }
)

OPCODE (mtsrin,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    ppc_grab_regssb(instr);
    uint32_t grab_sr      = ppc_result_b >> 28;
    if (ppc_state.sr[grab_sr] != ppc_result_d) {
        ppc_state.sr[grab_sr] = ppc_result_d;
        mmu_pat_ctx_changed();
    }
)

OPCODE (mfsr,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    int reg_d            = (instr >> 21) & 0x1F;
    uint32_t grab_sr     = (instr >> 16) & 0x0F;
    ppc_state.gpr[reg_d] = ppc_state.sr[grab_sr];
)

OPCODE (mfsrin,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    ppc_grab_regsdb(instr);
    uint32_t grab_sr     = ppc_result_b >> 28;
    ppc_state.gpr[reg_d] = ppc_state.sr[grab_sr];
)

OPCODE (mfmsr,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    uint32_t reg_d       = (instr >> 21) & 0x1F;
    ppc_state.gpr[reg_d] = ppc_state.msr;
)

OPCODE (mtmsr,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    if (ppc_state.msr & MSR::PR) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
    }
    uint32_t reg_s = (instr >> 21) & 0x1F;
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
)

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
    return (dec_wr_value - static_cast<uint32_t>(dec_adj));
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

OPCODE (mfspr,
    ppc_grab_dab(instr);
    uint32_t ref_spr = (reg_b << 5) | reg_a;

    if (ref_spr & 0x10) {
#ifdef CPU_PROFILING
        num_supervisor_instrs++;
#endif
        if (ppc_state.msr & MSR::PR) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
        }
    }

    switch (ref_spr) {
    case SPR::MQ:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        ppc_state.gpr[reg_d] = ppc_state.spr[ref_spr];
        break;
    case SPR::RTCL_U:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        calc_rtcl_value();
        ppc_state.gpr[reg_d] =
        ppc_state.spr[SPR::RTCL_S] = rtc_lo & 0x3FFFFF80UL;
        ppc_state.spr[SPR::RTCU_S] = rtc_hi;
        break;
    case SPR::RTCU_U:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        calc_rtcl_value();
        ppc_state.gpr[reg_d] =
        ppc_state.spr[SPR::RTCU_S] = rtc_hi;
        ppc_state.spr[SPR::RTCL_S] = rtc_lo;
        break;
    case SPR::DEC_U:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        // fallthrough
    case SPR::DEC_S:
        ppc_state.gpr[reg_d] = ppc_state.spr[SPR::DEC_S] = calc_dec_value();
        break;
    default:
        // FIXME: Unknown SPR should be noop or illegal instruction.
        ppc_state.gpr[reg_d] = ppc_state.spr[ref_spr];
    }
)

OPCODE (mtspr,
    ppc_grab_dab(instr);
    uint32_t ref_spr = (reg_b << 5) | reg_a;

    if (ref_spr & 0x10) {
#ifdef CPU_PROFILING
        num_supervisor_instrs++;
#endif
        if (ppc_state.msr & MSR::PR) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::NOT_ALLOWED);
        }
    }

    uint32_t val = ppc_state.gpr[reg_d];

    switch (ref_spr) {
    case SPR::MQ:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        ppc_state.spr[ref_spr] = val;
        break;
    case SPR::RTCL_U:
    case SPR::RTCU_U:
    case SPR::DEC_U:
        if (!is_601) {
            ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
        }
        break;
    case SPR::XER:
        ppc_state.spr[ref_spr] = val & 0xe000ff7f;
        break;
    case SPR::SDR1:
        if (ppc_state.spr[ref_spr] != val) {
            ppc_state.spr[ref_spr] = val;
            mmu_pat_ctx_changed(); // adapt to SDR1 changes
        }
        break;
    case SPR::RTCL_S:
        calc_rtcl_value();
        ppc_state.spr[RTCL_S] = rtc_lo = val & 0x3FFFFF80UL;
        ppc_state.spr[RTCU_S] = rtc_hi;
        break;
    case SPR::RTCU_S:
        calc_rtcl_value();
        ppc_state.spr[RTCL_S] = rtc_lo;
        ppc_state.spr[RTCU_S] = rtc_hi = val;
        break;
    case SPR::DEC_S:
        ppc_state.spr[DEC_S] = val;
        update_decrementer(val);
        break;
    case SPR::TBL_S:
        update_timebase(0xFFFFFFFF00000000ULL, val);
        ppc_state.spr[TBL_S] = val;
        ppc_state.spr[TBU_S] = tbr_wr_value >> 32;
        break;
    case SPR::TBU_S:
        update_timebase(0x00000000FFFFFFFFULL, uint64_t(val) << 32);
        ppc_state.spr[TBL_S] = (uint32_t)tbr_wr_value;
        ppc_state.spr[TBU_S] = val;
        break;
    case SPR::PVR:
        break;
    case 528:
    case 529:
    case 530:
    case 531:
    case 532:
    case 533:
    case 534:
    case 535:
        ppc_state.spr[ref_spr] = val;
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
        ppc_state.spr[ref_spr] = val;
        dbat_update(ref_spr);
    default:
        // FIXME: Unknown SPR should be noop or illegal instruction.
        ppc_state.spr[ref_spr] = val;
    }
)

OPCODE (mftb,
    ppc_grab_dab(instr);
    uint32_t ref_spr = (reg_b << 5) | reg_a;

    uint64_t tbr_value = calc_tbr_value();

    switch (ref_spr) {
    case SPR::TBL_U:
        ppc_state.gpr[reg_d] =
        ppc_state.spr[TBL_S] = uint32_t(tbr_value);
        ppc_state.spr[TBU_S] = uint32_t(tbr_value >> 32);
        break;
    case SPR::TBU_U:
        ppc_state.gpr[reg_d] =
        ppc_state.spr[TBU_S] = uint32_t(tbr_value >> 32);
        ppc_state.spr[TBL_S] = uint32_t(tbr_value);
        break;
    default:
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
     }
)

OPCODE (mtcrf,
    ppc_grab_s(instr);
    uint8_t crm = (instr >> 12) & 0xFFU;

    uint32_t cr_mask = 0;

    if (crm == 0xFFU) // the fast case
        cr_mask = 0xFFFFFFFFUL;
    else { // the slow case
        if (crm & 0x80) cr_mask |= 0xF0000000UL;
        if (crm & 0x40) cr_mask |= 0x0F000000UL;
        if (crm & 0x20) cr_mask |= 0x00F00000UL;
        if (crm & 0x10) cr_mask |= 0x000F0000UL;
        if (crm & 0x08) cr_mask |= 0x0000F000UL;
        if (crm & 0x04) cr_mask |= 0x00000F00UL;
        if (crm & 0x02) cr_mask |= 0x000000F0UL;
        if (crm & 0x01) cr_mask |= 0x0000000FUL;
    }
    ppc_state.cr = (ppc_state.cr & ~cr_mask) | (ppc_result_d & cr_mask);
)

OPCODE (mcrxr,
    int crf_d    = (instr >> 21) & 0x1C;
    ppc_state.cr = (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        ((ppc_state.spr[SPR::XER] & 0xF0000000UL) >> crf_d);
    ppc_state.spr[SPR::XER] &= 0x0FFFFFFF;
)

OPCODEEXTSIGN (exts,
    ppc_grab_regssa(instr);
    ppc_result_a = int32_t(T(ppc_result_d));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);;
)

template void dppc_interpreter::ppc_exts<int8_t, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_exts<int16_t, RC0>(uint32_t instr);
template void dppc_interpreter::ppc_exts<int8_t, RC1>(uint32_t instr);
template void dppc_interpreter::ppc_exts<int16_t, RC1>(uint32_t instr);

// Branching Instructions

OPCODELKAA (b,
    int32_t adr_li = int32_t((instr & ~3UL) << 6) >> 6;

    if (a)
        ppc_next_instruction_address = adr_li;
    else
        ppc_next_instruction_address = uint32_t(ppc_state.pc + adr_li);

    if (l)
        ppc_state.spr[SPR::LR] = uint32_t(ppc_state.pc + 4);

    exec_flags = EXEF_BRANCH;
)

template void dppc_interpreter::ppc_b<LK0, AA0>(uint32_t instr);
template void dppc_interpreter::ppc_b<LK0, AA1>(uint32_t instr);
template void dppc_interpreter::ppc_b<LK1, AA0>(uint32_t instr);
template void dppc_interpreter::ppc_b<LK1, AA1>(uint32_t instr);

OPCODELKAA (bc, 
    uint32_t ctr_ok = 0;
    uint32_t cnd_ok = 0;
    uint32_t br_bo = (instr >> 21) & 0x1F;
    uint32_t br_bi = (instr >> 16) & 0x1F;
    int32_t br_bd  = int32_t(int16_t(instr & ~3UL));

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) | ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) | (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        if (a)
            ppc_next_instruction_address = br_bd;
        else
            ppc_next_instruction_address = uint32_t(ppc_state.pc + br_bd);
        exec_flags = EXEF_BRANCH;
    }

    if (l)
        ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
)

template void dppc_interpreter::ppc_bc<LK0, AA0>(uint32_t instr);
template void dppc_interpreter::ppc_bc<LK0, AA1>(uint32_t instr);
template void dppc_interpreter::ppc_bc<LK1, AA0>(uint32_t instr);
template void dppc_interpreter::ppc_bc<LK1, AA1>(uint32_t instr);

OPCODE601L (bcctr,
    uint32_t ctr_ok = 0;
    uint32_t cnd_ok = 0;
    uint32_t br_bo = (instr >> 21) & 0x1F;
    uint32_t br_bi = (instr >> 16) & 0x1F;

    uint32_t ctr = ppc_state.spr[SPR::CTR];
    uint32_t new_ctr;
    if (for601) {
        new_ctr = ctr - 1;
        if (!(br_bo & 0x04)) {
            ppc_state.spr[SPR::CTR] = new_ctr; /* decrement CTR */
        }
    }
    else {
        new_ctr = ctr;
    }
    ctr_ok = (br_bo & 0x04) | ((new_ctr != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) | (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ctr & ~3UL);
        exec_flags = EXEF_BRANCH;
    }

    if (l)
        ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
)

template void dppc_interpreter::ppc_bcctr<LK0, NOT601>(uint32_t instr);
template void dppc_interpreter::ppc_bcctr<LK0, IS601>(uint32_t instr);
template void dppc_interpreter::ppc_bcctr<LK1, NOT601>(uint32_t instr);
template void dppc_interpreter::ppc_bcctr<LK1, IS601>(uint32_t instr);

OPCODEL (bclr,
    uint32_t br_bo = (instr >> 21) & 0x1F;
    uint32_t br_bi = (instr >> 16) & 0x1F;
    uint32_t ctr_ok = 0;
    uint32_t cnd_ok = 0;

    if (!(br_bo & 0x04)) {
        (ppc_state.spr[SPR::CTR])--; /* decrement CTR */
    }
    ctr_ok = (br_bo & 0x04) | ((ppc_state.spr[SPR::CTR] != 0) == !(br_bo & 0x02));
    cnd_ok = (br_bo & 0x10) | (!(ppc_state.cr & (0x80000000UL >> br_bi)) == !(br_bo & 0x08));

    if (ctr_ok && cnd_ok) {
        ppc_next_instruction_address = (ppc_state.spr[SPR::LR] & ~3UL);
        exec_flags = EXEF_BRANCH;
    }

    if (l)
        ppc_state.spr[SPR::LR] = ppc_state.pc + 4;
)

template void dppc_interpreter::ppc_bclr<LK0>(uint32_t instr);
template void dppc_interpreter::ppc_bclr<LK1>(uint32_t instr);

// Compare Instructions

OPCODE (cmp,
#ifdef CHECK_INVALID
    if (instr & 0x200000) {
        LOG_F(WARNING, "Invalid CMP instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (instr >> 21) & 0x1C;
    ppc_grab_regsab(instr);
    uint32_t xercon = (ppc_state.spr[SPR::XER] & XER::SO) >> 3;
    uint32_t cmp_c = (int32_t(ppc_result_a) == int32_t(ppc_result_b)) ? 0x20000000UL : \
        (int32_t(ppc_result_a) > int32_t(ppc_result_b)) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
)

OPCODE (cmpi,
#ifdef CHECK_INVALID
    if (instr & 0x200000) {
        LOG_F(WARNING, "Invalid CMPI instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (instr >> 21) & 0x1C;
    ppc_grab_regsasimm(instr);
    uint32_t xercon = (ppc_state.spr[SPR::XER] & XER::SO) >> 3;
    uint32_t cmp_c = (int32_t(ppc_result_a) == simm) ? 0x20000000UL : \
        (int32_t(ppc_result_a) > simm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
)

OPCODE (cmpl,
#ifdef CHECK_INVALID
    if (instr & 0x200000) {
        LOG_F(WARNING, "Invalid CMPL instruction form (L=1)!");
        return;
    }
#endif

    int crf_d = (instr >> 21) & 0x1C;
    ppc_grab_regsab(instr);
    uint32_t xercon = (ppc_state.spr[SPR::XER] & XER::SO) >> 3;
    uint32_t cmp_c = (ppc_result_a == ppc_result_b) ? 0x20000000UL : \
        (ppc_result_a > ppc_result_b) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
)

OPCODE (cmpli,
#ifdef CHECK_INVALID
    if (instr & 0x200000) {
        LOG_F(WARNING, "Invalid CMPLI instruction form (L=1)!");
        return;
    }
#endif
    ppc_grab_crfd_regsauimm(instr);
    uint32_t xercon = (ppc_state.spr[SPR::XER] & XER::SO) >> 3;
    uint32_t cmp_c = (ppc_result_a == uimm) ? 0x20000000UL : \
        (ppc_result_a > uimm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> crf_d)) | ((cmp_c + xercon) >> crf_d));
)

// Condition Register Changes

OPCODE (mcrf,
    int crf_d       = (instr >> 21) & 0x1C;
    int crf_s       = (instr >> 16) & 0x1C;

    // extract and right justify source flags field
    uint32_t grab_s = (ppc_state.cr >> (28 - crf_s)) & 0xF;

    ppc_state.cr = (ppc_state.cr & ~(0xf0000000UL >> crf_d)) | (grab_s << (28 - crf_d));
)

OPCODE (crand,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_set_cr(0x80000000UL >> reg_d);
    } else {
        ppc_unset_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (crandc,
    ppc_grab_dab(instr);
    if ((ppc_state.cr & (0x80000000UL >> reg_a)) && !(ppc_state.cr & (0x80000000UL >> reg_b))) {
        ppc_set_cr(0x80000000UL >> reg_d);
    } else {
        ppc_unset_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (creqv,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) { // compliment is implemented by swapping the following if/else bodies
        ppc_unset_cr(0x80000000UL >> reg_d);
    } else {
        ppc_set_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (crnand,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) & (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_unset_cr(0x80000000UL >> reg_d);
    } else {
        ppc_set_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (crnor,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_unset_cr(0x80000000UL >> reg_d);
    } else {
        ppc_set_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (cror,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) | (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_set_cr(0x80000000UL >> reg_d);
    } else {
        ppc_unset_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (crorc,
    ppc_grab_dab(instr);
    if ((ppc_state.cr & (0x80000000UL >> reg_a)) || !(ppc_state.cr & (0x80000000UL >> reg_b))) {
        ppc_set_cr(0x80000000UL >> reg_d);
    } else {
        ppc_unset_cr(0x80000000UL >> reg_d);
    }
)

OPCODE (crxor,
    ppc_grab_dab(instr);
    uint8_t ir = (ppc_state.cr >> (31 - reg_a)) ^ (ppc_state.cr >> (31 - reg_b));
    if (ir & 1) {
        ppc_set_cr(0x80000000UL >> reg_d);
    } else {
        ppc_unset_cr(0x80000000UL >> reg_d);
    }
)

// Processor MGMT Fns.

OPCODE (rfi,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    uint32_t new_srr1_val   = (ppc_state.spr[SPR::SRR1] & 0x87C0FF73UL);
    uint32_t new_msr_val    = (ppc_state.msr & ~0x87C0FF73UL);
    ppc_state.msr           = (new_msr_val | new_srr1_val) & 0xFFFBFFFFUL;

    // generate External Interrupt Exception
    // if CPU interrupt line is still asserted
    if (ppc_state.msr & MSR::EE && int_pin) {
        uint32_t save_srr0 = ppc_state.spr[SPR::SRR0] & ~3UL;
        ppc_exception_handler(Except_Type::EXC_EXT_INT, 0);
        ppc_state.spr[SPR::SRR0] = save_srr0;
        return;
    }

    if ((ppc_state.msr & MSR::EE) && dec_exception_pending) {
        dec_exception_pending = false;
        //LOG_F(WARNING, "decrementer exception from rfi msr:0x%X", ppc_state.msr);
        uint32_t save_srr0 = ppc_state.spr[SPR::SRR0] & ~3UL;
        ppc_exception_handler(Except_Type::EXC_DECR, 0);
        ppc_state.spr[SPR::SRR0] = save_srr0;
        return;
    }

    ppc_next_instruction_address = ppc_state.spr[SPR::SRR0] & ~3UL;

    do_ctx_sync(); // RFI is context synchronizing

    mmu_change_mode();

    exec_flags = EXEF_RFI;
)

OPCODE (sc,
    do_ctx_sync(); // SC is context synchronizing!
    ppc_exception_handler(Except_Type::EXC_SYSCALL, 0x20000);
)

OPCODE (tw,
    uint32_t reg_a  = (instr >> 11) & 0x1F;
    uint32_t reg_b  = (instr >> 16) & 0x1F;
    uint32_t ppc_to = (instr >> 21) & 0x1F;
    if (((int32_t(ppc_state.gpr[reg_a]) < int32_t(ppc_state.gpr[reg_b])) && (ppc_to & 0x10)) ||
        ((int32_t(ppc_state.gpr[reg_a]) > int32_t(ppc_state.gpr[reg_b])) && (ppc_to & 0x08)) ||
        ((int32_t(ppc_state.gpr[reg_a]) == int32_t(ppc_state.gpr[reg_b])) && (ppc_to & 0x04)) ||
        ((ppc_state.gpr[reg_a] < ppc_state.gpr[reg_b]) && (ppc_to & 0x02)) ||
        ((ppc_state.gpr[reg_a] > ppc_state.gpr[reg_b]) && (ppc_to & 0x01))) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::TRAP);
    }
)

OPCODE (twi,
    int32_t simm    = int32_t(int16_t(instr));
    uint32_t reg_a  = (instr >> 16) & 0x1F;
    uint32_t ppc_to = (instr >> 21) & 0x1F;
    if (((int32_t(ppc_state.gpr[reg_a]) < simm) && (ppc_to & 0x10)) ||
        ((int32_t(ppc_state.gpr[reg_a]) > simm) && (ppc_to & 0x08)) ||
        ((int32_t(ppc_state.gpr[reg_a]) == simm) && (ppc_to & 0x04)) ||
        (ppc_state.gpr[reg_a] < uint32_t(simm) && (ppc_to & 0x02)) ||
        (ppc_state.gpr[reg_a] > uint32_t(simm) && (ppc_to & 0x01))) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::TRAP);
    }
)

OPCODE (eieio,
    /* placeholder */
)

OPCODE (isync,
    do_ctx_sync();
)

OPCODE (sync,
    /* placeholder */
)

OPCODE (icbi,
    /* placeholder */
)

OPCODE (dcbf,
    /* placeholder */
)

OPCODE (dcbi,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
)

OPCODE (dcbst,
    /* placeholder */
)

OPCODE (dcbt,
    // Not needed, the HDI reg is touched to no-op this instruction.
    return;
)

OPCODE (dcbtst,
    // Not needed, the HDI reg is touched to no-op this instruction.
    return;
)

OPCODE (dcbz,
    ppc_grab_regsab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);

    ea &= 0xFFFFFFE0UL; // align EA on a 32-byte boundary

    // the following is not especially efficient but necessary
    // to make BlockZero under Mac OS 8.x and later to work
    mmu_write_vmem<uint64_t>(ea + 0, instr, 0);
    mmu_write_vmem<uint64_t>(ea + 8, instr, 0);
    mmu_write_vmem<uint64_t>(ea + 16, instr, 0);
    mmu_write_vmem<uint64_t>(ea + 24, instr, 0);
)


// Integer Load and Store Functions

OPCODEMEM (st,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa(instr);
    uint32_t ea = int32_t(int16_t(instr));
    ea += reg_a ? ppc_result_a : 0;
    mmu_write_vmem<T>(ea, instr, ppc_result_d);
)

template void dppc_interpreter::ppc_st<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_st<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_st<uint32_t>(uint32_t instr);

OPCODEMEM (stx,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    mmu_write_vmem<T>(ea, instr, ppc_result_d);
)

template void dppc_interpreter::ppc_stx<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_stx<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_stx<uint32_t>(uint32_t instr);

OPCODEMEM (stu,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa(instr);
    if (reg_a != 0) {
        uint32_t ea = int32_t(int16_t(instr));
        ea += ppc_result_a;
        mmu_write_vmem<T>(ea, instr, ppc_result_d);
        ppc_state.gpr[reg_a] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

template void dppc_interpreter::ppc_stu<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_stu<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_stu<uint32_t>(uint32_t instr);

OPCODEMEM (stux,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab(instr);
    if (reg_a != 0) {
        uint32_t ea = ppc_result_a + ppc_result_b;
        mmu_write_vmem<T>(ea, instr, ppc_result_d);
        ppc_state.gpr[reg_a] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

template void dppc_interpreter::ppc_stux<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_stux<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_stux<uint32_t>(uint32_t instr);

OPCODE (sthbrx,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    ppc_result_d = uint32_t(BYTESWAP_16(uint16_t(ppc_result_d)));
    mmu_write_vmem<uint16_t>(ea, instr, ppc_result_d);
)

OPCODE (stwcx,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab(instr);
    uint32_t ea = (reg_a == 0) ? ppc_result_b : (ppc_result_a + ppc_result_b);
    ppc_state.cr &= 0x0FFFFFFFUL; // clear CR0
    ppc_state.cr |= (ppc_state.spr[SPR::XER] & XER::SO) >> 3; // copy XER[SO] to CR0[SO]
    if (ppc_state.reserve) {
        mmu_write_vmem<uint32_t>(ea, instr, ppc_result_d);
        ppc_state.reserve = false;
        ppc_state.cr |= 0x20000000UL; // set CR0[EQ]
    }
)

OPCODE (stwbrx,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    ppc_result_d          = BYTESWAP_32(ppc_result_d);
    mmu_write_vmem<uint32_t>(ea, instr, ppc_result_d);
)

OPCODE (stmw,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssa_stmw(instr);
    uint32_t ea = int32_t(int16_t(instr));
    ea += reg_a ? ppc_result_a : 0;

    /* what should we do if EA is unaligned? */
    if (ea & 3) {
        ppc_alignment_exception(ea, instr);
    }

    for (; reg_s <= 31; reg_s++) {
        mmu_write_vmem<uint32_t>(ea, instr, ppc_state.gpr[reg_s]);
        ea += 4;
    }
)

OPCODEMEM (lz,
    #ifdef CPU_PROFILING
        num_int_loads++;
    #endif
    ppc_grab_regsda(instr);
    uint32_t ea = int32_t(int16_t(instr));
    ea += reg_a ? ppc_result_a : 0;
    uint32_t ppc_result_d = mmu_read_vmem<T>(ea, instr);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_lz<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_lz<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_lz<uint32_t>(uint32_t instr);

OPCODEMEM (lzu,
    #ifdef CPU_PROFILING
        num_int_loads++;
    #endif
    ppc_grab_regsda(instr);
    uint32_t ea = int32_t(int16_t(instr));
    if ((reg_a != reg_d) && reg_a != 0) {
        ea += ppc_result_a;
        uint32_t ppc_result_d = mmu_read_vmem<T>(ea, instr);
        uint32_t ppc_result_a = ea;
        ppc_store_iresult_reg(reg_d, ppc_result_d);
        ppc_store_iresult_reg(reg_a, ppc_result_a);;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

template void dppc_interpreter::ppc_lzu<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzu<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzu<uint32_t>(uint32_t instr);

OPCODEMEM (lzx,
    #ifdef CPU_PROFILING
        num_int_loads++;
    #endif
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    uint32_t ppc_result_d = mmu_read_vmem<T>(ea, instr);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

template void dppc_interpreter::ppc_lzx<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzx<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzx<uint32_t>(uint32_t instr);

OPCODEMEM (lzux,
    #ifdef CPU_PROFILING
        num_int_loads++;
    #endif
    ppc_grab_regsdab(instr);
    if ((reg_a != reg_d) && reg_a != 0) {
        uint32_t ea = ppc_result_a + ppc_result_b;
        uint32_t ppc_result_d = mmu_read_vmem<T>(ea, instr);
        ppc_result_a          = ea;
        ppc_store_iresult_reg(reg_d, ppc_result_d);
        ppc_store_iresult_reg(reg_a, ppc_result_a);;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

template void dppc_interpreter::ppc_lzux<uint8_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzux<uint16_t>(uint32_t instr);
template void dppc_interpreter::ppc_lzux<uint32_t>(uint32_t instr);

OPCODE (lha,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda(instr);
    uint32_t ea = int32_t(int16_t(instr));
    ea += (reg_a ? ppc_result_a : 0);
    int16_t val = mmu_read_vmem<uint16_t>(ea, instr);
    ppc_state.gpr[reg_d] = int32_t(val);
)

OPCODE (lhau,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda(instr);
    if ((reg_a != reg_d) && reg_a != 0) {
        uint32_t ea = int32_t(int16_t(instr));
        ea += ppc_result_a;
        int16_t val = mmu_read_vmem<uint16_t>(ea, instr);
        ppc_state.gpr[reg_d] = int32_t(val);
        uint32_t ppc_result_a = ea;
        ppc_store_iresult_reg(reg_a, ppc_result_a);;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

OPCODE (lhaux,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab(instr);
    if ((reg_a != reg_d) && reg_a != 0) {
        uint32_t ea = ppc_result_a + ppc_result_b;
        int16_t val = mmu_read_vmem<uint16_t>(ea, instr);
        ppc_state.gpr[reg_d] = int32_t(val);
        uint32_t ppc_result_a = ea;
        ppc_store_iresult_reg(reg_a, ppc_result_a);;
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
)

OPCODE (lhax,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    int16_t val = mmu_read_vmem<uint16_t>(ea, instr);
    ppc_state.gpr[reg_d] = int32_t(val);
)

OPCODE (lhbrx,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    uint32_t ppc_result_d = uint32_t(BYTESWAP_16(mmu_read_vmem<uint16_t>(ea, instr)));
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODE (lwbrx,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    uint32_t ppc_result_d = BYTESWAP_32(mmu_read_vmem<uint32_t>(ea, instr));
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODE (lwarx,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    // Placeholder - Get the reservation of memory implemented!
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    ppc_state.reserve     = true;
    uint32_t ppc_result_d = mmu_read_vmem<uint32_t>(ea, instr);
    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODE (lmw,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda(instr);
    uint32_t ea = int32_t(int16_t(instr));
    ea += (reg_a ? ppc_result_a : 0);
    // How many words to load in memory - using a do-while for this
    do {
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ea, instr);
       ea += 4;
       reg_d++;
    } while (reg_d < 32);
)

OPCODE (lswi,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsda(instr);
    uint32_t ea = reg_a ? ppc_result_a : 0;
    uint32_t grab_inb = (instr >> 11) & 0x1F;
    grab_inb          = grab_inb ? grab_inb : 32;

    while (grab_inb >= 4) {
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ea, instr);
        reg_d++;
        if (reg_d >= 32) {    // wrap around through GPR0
            reg_d = 0;
        }
        ea += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint8_t>(ea, instr) << 24;
        break;
    case 2:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ea, instr) << 16;
        break;
    case 3:
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ea, instr) << 16;
        ppc_state.gpr[reg_d] += mmu_read_vmem<uint8_t>(ea + 2, instr) << 8;
        break;
    default:
        break;
    }
)

OPCODE (lswx,
#ifdef CPU_PROFILING
    num_int_loads++;
#endif
    ppc_grab_regsdab(instr);

/*
    // Invalid instruction forms
    if ((reg_d == 0 && reg_a == 0) || (reg_d == reg_a) || (reg_d == reg_b)) {
        // UNTESTED! Does invalid form really cause exception?
        // G4 doesn't do exception
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
*/

    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    uint32_t grab_inb      = ppc_state.spr[SPR::XER] & 0x7F;

    for (;;) {
        if (is_601 && (reg_d == reg_b || (reg_a != 0 && reg_d == reg_a))) {
            // UNTESTED! MPC601 manual is inconsistant on whether reg_b is skipped or not
            reg_d = (reg_d + 1) & 0x1F; // wrap around through GPR0
        }
        switch (grab_inb) {
        case 0:
            return;
        case 1:
            ppc_state.gpr[reg_d] = mmu_read_vmem<uint8_t>(ea, instr) << 24;
            return;
        case 2:
            ppc_state.gpr[reg_d] = mmu_read_vmem<uint16_t>(ea, instr) << 16;
            return;
        case 3:
            ppc_state.gpr[reg_d] = (mmu_read_vmem<uint16_t>(ea, instr) << 16) |
                (mmu_read_vmem<uint8_t>(ea + 2, instr) << 8);
            return;
        }
        ppc_state.gpr[reg_d] = mmu_read_vmem<uint32_t>(ea, instr);
        reg_d = (reg_d + 1) & 0x1F; // wrap around through GPR0
        ea += 4;
        grab_inb -= 4;
    }
)

OPCODE (stswi,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssash_stswi(instr);
    uint32_t ea = reg_a ? ppc_result_a : 0;
    uint32_t grab_inb = rot_sh ? rot_sh : 32;

    while (grab_inb >= 4) {
        mmu_write_vmem<uint32_t>(ea, instr, ppc_state.gpr[reg_s]);
        reg_s++;
        if (reg_s >= 32) {    // wrap around through GPR0
            reg_s = 0;
        }
        ea += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        mmu_write_vmem<uint8_t>(ea, instr, ppc_state.gpr[reg_s] >> 24);
        break;
    case 2:
        mmu_write_vmem<uint16_t>(ea, instr, ppc_state.gpr[reg_s] >> 16);
        break;
    case 3:
        mmu_write_vmem<uint16_t>(ea, instr, ppc_state.gpr[reg_s] >> 16);
        mmu_write_vmem<uint8_t>(ea + 2, instr, (ppc_state.gpr[reg_s] >> 8) & 0xFF);
        break;
    default:
        break;
    }
)

OPCODE (stswx,
#ifdef CPU_PROFILING
    num_int_stores++;
#endif
    ppc_grab_regssab_stswx(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);
    uint32_t grab_inb = ppc_state.spr[SPR::XER] & 127;

    while (grab_inb >= 4) {
        mmu_write_vmem<uint32_t>(ea, instr, ppc_state.gpr[reg_s]);
        reg_s++;
        if (reg_s >= 32) {    // wrap around through GPR0
            reg_s = 0;
        }
        ea += 4;
        grab_inb -= 4;
    }

    // handle remaining bytes
    switch (grab_inb) {
    case 1:
        mmu_write_vmem<uint8_t>(ea, instr, ppc_state.gpr[reg_s] >> 24);
        break;
    case 2:
        mmu_write_vmem<uint16_t>(ea, instr, ppc_state.gpr[reg_s] >> 16);
        break;
    case 3:
        mmu_write_vmem<uint16_t>(ea, instr, ppc_state.gpr[reg_s] >> 16);
        mmu_write_vmem<uint8_t>(ea + 2, instr, (ppc_state.gpr[reg_s] >> 8) & 0xFF);
        break;
    default:
        break;
    }
)

OPCODE (eciwx,
    uint32_t ear_enable = 0x80000000;

    // error if EAR[E] != 1
    if (!(ppc_state.spr[282] && ear_enable)) {
        ppc_exception_handler(Except_Type::EXC_DSI, 0x0);
    }

    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);

    if (ea & 0x3) {
        ppc_alignment_exception(ea, instr);
    }

    uint32_t ppc_result_d = mmu_read_vmem<uint32_t>(ea, instr);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
)

OPCODE (ecowx,
    uint32_t ear_enable = 0x80000000;

    // error if EAR[E] != 1
    if (!(ppc_state.spr[282] && ear_enable)) {
        ppc_exception_handler(Except_Type::EXC_DSI, 0x0);
    }

    ppc_grab_regssab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);

    if (ea & 0x3) {
        ppc_alignment_exception(ea, instr);
    }

    mmu_write_vmem<uint32_t>(ea, instr, ppc_result_d);
)

// TLB Instructions

OPCODE (tlbie,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif

    tlb_flush_entry(ppc_state.gpr[(instr >> 11) & 0x1F]);
)

OPCODE (tlbia,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
)

OPCODE (tlbld,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
)

OPCODE (tlbli,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
)

OPCODE (tlbsync,
#ifdef CPU_PROFILING
    num_supervisor_instrs++;
#endif
    /* placeholder */
)
