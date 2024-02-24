/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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
#include "ppcmmu.h"
#include <stdint.h>

// Affects the XER register's SO and OV Bits

inline void power_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if ((a ^ b) & (a ^ d) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= 0xC0000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
}

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t power_rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = uint32_t(0xFFFFFFFFUL << (31 - rot_me));
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

void dppc_interpreter::power_abs() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_clcs() {
    ppc_grab_regsda();
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

    ppc_store_result_regd();
}

void dppc_interpreter::power_div() {
    ppc_grab_regsdab();

    uint64_t dividend = ((uint64_t)ppc_result_a << 32) | ppc_state.spr[SPR::MQ];
    int32_t  divisor  = ppc_result_b;

    if ((ppc_result_a == 0x80000000UL && divisor == -1) || !divisor) {
        ppc_state.spr[SPR::MQ] = 0;
        ppc_result_d = 0x80000000UL; // -2^31 aka INT32_MIN
    } else {
        ppc_result_d = uint32_t(dividend / divisor);
        ppc_state.spr[SPR::MQ] = dividend % divisor;
    }

    if (oe_flag)
        power_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_divs() {
    ppc_grab_regsdab();
    ppc_result_d           = ppc_result_a / ppc_result_b;
    ppc_state.spr[SPR::MQ] = (ppc_result_a % ppc_result_b);

    if (oe_flag)
        power_setsoov(ppc_result_b, ppc_result_a, ppc_result_d);
    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_doz() {
    ppc_grab_regsdab();
    ppc_result_d = (int32_t(ppc_result_a) >= int32_t(ppc_result_b)) ? 0 :
                    ppc_result_b - ppc_result_a;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);
    if (oe_flag)
        power_setsoov(ppc_result_a, ppc_result_b, ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_dozi() {
    ppc_grab_regsdasimm();
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = simm - ppc_result_a;
    }
    ppc_store_result_regd();
}

void dppc_interpreter::power_lscbx() {
    ppc_grab_regsdab();
    ppc_effective_address = reg_a ? ppc_result_a + ppc_result_b : ppc_result_b;

    uint8_t  return_value  = 0;
    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7F);
    uint32_t bytes_copied  = 0;
    uint8_t  matching_byte = (uint8_t)(ppc_state.spr[SPR::XER] >> 8);

    // for storing each byte
    uint32_t bitmask      = 0xFF000000;
    uint8_t  shift_amount = 24;

    while (bytes_to_load > 0) {
        return_value = mmu_read_vmem<uint8_t>(ppc_effective_address);

        ppc_result_d = (ppc_result_d & ~bitmask) | (return_value << shift_amount);
        if (!shift_amount) {
            if (reg_d != reg_a && reg_d != reg_b)
                ppc_store_result_regd();
            reg_d        = (reg_d + 1) & 31;
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
        ppc_store_result_regd();

    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & ~0x7F) | bytes_copied;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);
}

void dppc_interpreter::power_maskg() {
    ppc_grab_regssab();
    uint32_t mask_start  = ppc_result_d & 31;
    uint32_t mask_end    = ppc_result_b & 31;
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

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_rega();
}

void dppc_interpreter::power_maskir() {
    ppc_grab_regssab();
    ppc_result_a = (ppc_result_a & ~ppc_result_b) | (ppc_result_d & ppc_result_b);

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_mul() {
    ppc_grab_regsdab();
    uint64_t product;

    product                = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    ppc_result_d           = ((uint32_t)(product >> 32));
    ppc_state.spr[SPR::MQ] = ((uint32_t)(product));

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_nabs() {
    ppc_grab_regsda();
    ppc_result_d = ppc_result_a & 0x80000000 ? ppc_result_a : -ppc_result_a;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_rlmi() {
    ppc_grab_regssab();
    unsigned rot_mb      = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me      = (ppc_cur_instruction >> 1) & 31;
    unsigned rot_sh      = ppc_result_b & 31;

    uint32_t r           = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask        = power_rot_mask(rot_mb, rot_me);

    ppc_result_a         = ((r & mask) | (ppc_result_a & ~mask));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_rrib() {
    ppc_grab_regssab();

    if (ppc_result_d & 0x80000000) {
        ppc_result_a |= ((ppc_result_d & 0x80000000) >> ppc_result_b);
    } else {
        ppc_result_a &= ~((ppc_result_d & 0x80000000) >> ppc_result_b);
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sle() {
    ppc_grab_regssab();
    unsigned rot_sh = ppc_result_b & 31;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    ppc_store_result_rega();

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sleq() {
    ppc_grab_regssab();
    unsigned rot_sh = ppc_result_b & 31;
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sliq() {
    ppc_grab_regssa();
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 31;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_slliq() {
    ppc_grab_regssa();
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 31;
    uint32_t r           = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask        = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sllq() {
    ppc_grab_regssab();
    unsigned rot_sh      = ppc_result_b & 31;
    uint32_t r           = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask        = power_rot_mask(0, 31 - rot_sh);

    if (ppc_result_b >= 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    }
    else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_slq() {
    ppc_grab_regssab();
    unsigned rot_sh = ppc_result_b & 31;

    if (ppc_result_b >= 0x20) {
        ppc_result_a = ppc_result_d << rot_sh;
    } else {
        ppc_result_a = 0;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
}

void dppc_interpreter::power_sraiq() {
    ppc_grab_regssa();
    unsigned rot_sh        = (ppc_cur_instruction >> 11) & 0x1F;
    uint32_t mask          = (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sraq() {
    ppc_grab_regssab();
    unsigned rot_sh        = ppc_result_b & 0x1F;
    uint32_t mask          = (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sre() {
    ppc_grab_regssab();

    unsigned rot_sh        = ppc_result_b & 31;

    ppc_result_a           = ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_srea() {
    ppc_grab_regssab();
    unsigned rot_sh        = ppc_result_b & 0x1F;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if ((ppc_result_d & 0x80000000UL) && (ppc_result_d & rot_sh)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sreq() {
    ppc_grab_regssab();
    unsigned rot_sh      = ppc_result_b & 31;
    unsigned mask        = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((rot_sh & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = rot_sh;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sriq() {
    ppc_grab_regssa();
    unsigned rot_sh        = (ppc_cur_instruction >> 11) & 31;
    ppc_result_a           = ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_srliq() {
    ppc_grab_regssa();
    unsigned rot_sh        = (ppc_cur_instruction >> 11) & 31;

    uint32_t r             = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask          = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_srlq() {
    ppc_grab_regssab();
    unsigned rot_sh = ppc_result_b & 31;
    uint32_t r      = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    if (ppc_result_b >= 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    }
    else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_srq() {
    ppc_grab_regssab();
    unsigned rot_sh = ppc_result_b & 31;

    if (ppc_result_b >= 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d >> rot_sh;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}
