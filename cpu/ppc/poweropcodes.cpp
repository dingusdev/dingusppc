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

// The Power-specific opcodes for the processor - ppcopcodes.cpp
// Any shared opcodes are in ppcopcodes.cpp

#include "ppcemu.h"
#include "ppcmacros.h"
#include "ppcmmu.h"
#include <stdint.h>

// Affects the XER register's SO and OV Bits

inline void power_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if ((a ^ b) & (a ^ d) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::OV;
    }
}

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t power_rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = uint32_t(0xFFFFFFFFUL << (31 - rot_me));
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_abs() {
    uint32_t ppc_result_d;
    ppc_grab_regsda(ppc_cur_instruction);
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;

    } else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_abs<RC0, OV0>();
template void dppc_interpreter::power_abs<RC0, OV1>();
template void dppc_interpreter::power_abs<RC1, OV0>();
template void dppc_interpreter::power_abs<RC1, OV1>();

void dppc_interpreter::power_clcs() {
    uint32_t ppc_result_d;
    ppc_grab_regsda(ppc_cur_instruction);
    switch (reg_a) {
    case 12: //instruction cache line size
    case 13: //data cache line size
    case 14: //minimum line size
    case 15: //maximum line size
        ppc_result_d = 64;
        break;
    default:
        ppc_result_d = 0;
    }

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_div() {
    uint32_t ppc_result_d;
    ppc_grab_regsdab(ppc_cur_instruction);

    uint64_t dividend = ((uint64_t)ppc_result_a << 32) | ppc_state.spr[SPR::MQ];
    int32_t  divisor  = ppc_result_b;

    if ((ppc_result_a == 0x80000000UL && divisor == -1) || !divisor) {
        ppc_state.spr[SPR::MQ] = 0;
        ppc_result_d  = 0x80000000UL;    // -2^31 aka INT32_MIN
    } else {
        ppc_result_d  = uint32_t(dividend / divisor);
        ppc_state.spr[SPR::MQ] = dividend % divisor;
    }

    if (ov)
        power_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_div<RC0, OV0>();
template void dppc_interpreter::power_div<RC0, OV1>();
template void dppc_interpreter::power_div<RC1, OV0>();
template void dppc_interpreter::power_div<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_divs() {
    ppc_grab_regsdab(ppc_cur_instruction);
    uint32_t ppc_result_d  = ppc_result_a / ppc_result_b;
    ppc_state.spr[SPR::MQ] = (ppc_result_a % ppc_result_b);

    if (ov)
        power_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_divs<RC0, OV0>();
template void dppc_interpreter::power_divs<RC0, OV1>();
template void dppc_interpreter::power_divs<RC1, OV0>();
template void dppc_interpreter::power_divs<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_doz() {
    ppc_grab_regsdab(ppc_cur_instruction);
    uint32_t ppc_result_d = (int32_t(ppc_result_a) >= int32_t(ppc_result_b)) ? 0 :
                             ppc_result_b - ppc_result_a;

    if (rec)
        ppc_changecrf0(ppc_result_d);
    if (ov)
        power_setsoov(ppc_result_a, ppc_result_b, ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_doz<RC0, OV0>();
template void dppc_interpreter::power_doz<RC0, OV1>();
template void dppc_interpreter::power_doz<RC1, OV0>();
template void dppc_interpreter::power_doz<RC1, OV1>();

void dppc_interpreter::power_dozi() {
    uint32_t ppc_result_d;
    ppc_grab_regsdasimm(ppc_cur_instruction);
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = simm - ppc_result_a;
    }
    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template <field_rc rec>
void dppc_interpreter::power_lscbx() {
    ppc_grab_regsdab(ppc_cur_instruction);
    ppc_effective_address = ppc_result_b + (reg_a ? ppc_result_a : 0);

    uint8_t  return_value  = 0;
    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7F);
    uint32_t bytes_copied  = 0;
    uint8_t  matching_byte = (uint8_t)(ppc_state.spr[SPR::XER] >> 8);
    uint32_t ppc_result_d  = 0;

    // for storing each byte
    uint32_t bitmask      = 0xFF000000;
    uint8_t  shift_amount = 24;

    while (bytes_to_load > 0) {
        return_value = mmu_read_vmem<uint8_t>(ppc_effective_address);

        ppc_result_d = (ppc_result_d & ~bitmask) | (return_value << shift_amount);
        if (!shift_amount) {
            if (reg_d != reg_a && reg_d != reg_b)
                ppc_store_iresult_reg(reg_d, ppc_result_d);
            reg_d        = (reg_d + 1) & 0x1F;
            bitmask      = 0xFF000000;
            shift_amount = 24;
        } else {
            bitmask >>= 8;
            shift_amount -= 8;
        }

        ppc_effective_address++;
        bytes_copied++;
        bytes_to_load--;

        if (return_value == matching_byte)
            break;
    }

    // store partiallly loaded register if any
    if (shift_amount != 24 && reg_d != reg_a && reg_d != reg_b)
        ppc_store_iresult_reg(reg_d, ppc_result_d);

    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & ~0x7F) | bytes_copied;

    if (rec)
        ppc_changecrf0(ppc_result_d);
}

template void dppc_interpreter::power_lscbx<RC0>();
template void dppc_interpreter::power_lscbx<RC1>();

template <field_rc rec>
void dppc_interpreter::power_maskg() {
    ppc_grab_regssab(ppc_cur_instruction);
    uint32_t mask_start  = ppc_result_d & 0x1F;
    uint32_t mask_end    = ppc_result_b & 0x1F;
    uint32_t insert_mask = 0;

    if (mask_start < (mask_end + 1)) {
        insert_mask = power_rot_mask(mask_start, mask_end);
    }
    else if (mask_start == (mask_end + 1)) {
        insert_mask = 0xFFFFFFFF;
    }
    else {
        insert_mask = ~(power_rot_mask(mask_end + 1, mask_start - 1));
    }

    ppc_result_a = insert_mask;

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_maskg<RC0>();
template void dppc_interpreter::power_maskg<RC1>();

template <field_rc rec>
void dppc_interpreter::power_maskir() {
    ppc_grab_regssab(ppc_cur_instruction);
    ppc_result_a = (ppc_result_a & ~ppc_result_b) | (ppc_result_d & ppc_result_b);

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_maskir<RC0>();
template void dppc_interpreter::power_maskir<RC1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_mul() {
    ppc_grab_regsdab(ppc_cur_instruction);
    uint64_t product;

    product                = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    uint32_t ppc_result_d  = ((uint32_t)(product >> 32));
    ppc_state.spr[SPR::MQ] = ((uint32_t)(product));

    if (rec)
        ppc_changecrf0(ppc_result_d);
    if (ov)
        power_setsoov(ppc_result_a, ppc_result_b, ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_mul<RC0, OV0>();
template void dppc_interpreter::power_mul<RC0, OV1>();
template void dppc_interpreter::power_mul<RC1, OV0>();
template void dppc_interpreter::power_mul<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_nabs() {
    ppc_grab_regsda(ppc_cur_instruction);
    uint32_t ppc_result_d = ppc_result_a & 0x80000000 ? ppc_result_a : -ppc_result_a;

    if (rec)
        ppc_changecrf0(ppc_result_d);
    if (ov)
        ppc_state.spr[SPR::XER] &= ~XER::OV;

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_nabs<RC0, OV0>();
template void dppc_interpreter::power_nabs<RC0, OV1>();
template void dppc_interpreter::power_nabs<RC1, OV0>();
template void dppc_interpreter::power_nabs<RC1, OV1>();

void dppc_interpreter::power_rlmi() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_mb      = (ppc_cur_instruction >> 6) & 0x1F;
    unsigned rot_me      = (ppc_cur_instruction >> 1) & 0x1F;
    unsigned rot_sh      = ppc_result_b & 0x1F;

    uint32_t r           = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask        = power_rot_mask(rot_mb, rot_me);

    ppc_result_a         = ((r & mask) | (ppc_result_a & ~mask));

    if ((ppc_cur_instruction & 0x01) == 1)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template <field_rc rec>
void dppc_interpreter::power_rrib() {
    ppc_grab_regssab(ppc_cur_instruction);

    if (ppc_result_d & 0x80000000) {
        ppc_result_a |= ((ppc_result_d & 0x80000000) >> ppc_result_b);
    } else {
        ppc_result_a &= ~((ppc_result_d & 0x80000000) >> ppc_result_b);
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_rrib<RC0>();
template void dppc_interpreter::power_rrib<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sle() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    ppc_store_iresult_reg(reg_a, ppc_result_a);

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sle<RC0>();
template void dppc_interpreter::power_sle<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sleq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sleq<RC0>();
template void dppc_interpreter::power_sleq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sliq() {
    ppc_grab_regssa(ppc_cur_instruction);
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 0x1F;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sliq<RC0>();
template void dppc_interpreter::power_sliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_slliq() {
    ppc_grab_regssa(ppc_cur_instruction);
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 0x1F;
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_slliq<RC0>();
template void dppc_interpreter::power_slliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sllq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    if (ppc_result_b >= 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    }
    else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sllq<RC0>();
template void dppc_interpreter::power_sllq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_slq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b >= 0x20) {
        ppc_result_a = ppc_result_d << rot_sh;
    } else {
        ppc_result_a = 0;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
}

template void dppc_interpreter::power_slq<RC0>();
template void dppc_interpreter::power_slq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sraiq() {
    ppc_grab_regssa(ppc_cur_instruction);
    unsigned rot_sh        = (ppc_cur_instruction >> 11) & 0x1F;
    uint32_t mask          = (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sraiq<RC0>();
template void dppc_interpreter::power_sraiq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sraq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    uint32_t mask          = (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sraq<RC0>();
template void dppc_interpreter::power_sraq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sre() {
    ppc_grab_regssab(ppc_cur_instruction);

    unsigned rot_sh = ppc_result_b & 0x1F;
    ppc_result_a    = ppc_result_d >> rot_sh;

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sre<RC0>();
template void dppc_interpreter::power_sre<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srea() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & rot_sh)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srea<RC0>();
template void dppc_interpreter::power_srea<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sreq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;
    unsigned mask   = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((rot_sh & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = rot_sh;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sreq<RC0>();
template void dppc_interpreter::power_sreq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sriq() {
    ppc_grab_regssa(ppc_cur_instruction);
    unsigned rot_sh        = (ppc_cur_instruction >> 11) & 0x1F;
    ppc_result_a           = ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sriq<RC0>();
template void dppc_interpreter::power_sriq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srliq() {
    ppc_grab_regssa(ppc_cur_instruction);
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 0x1F;
    uint32_t r      = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srliq<RC0>();
template void dppc_interpreter::power_srliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srlq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    if (ppc_result_b >= 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    }
    else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srlq<RC0>();
template void dppc_interpreter::power_srlq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srq() {
    ppc_grab_regssab(ppc_cur_instruction);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b >= 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d >> rot_sh;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srq<RC0>();
template void dppc_interpreter::power_srq<RC1>();
