/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stdio.h>
#include <thirdparty/loguru/loguru.hpp>

// Affects the XER register's SO and OV Bits

inline void power_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if ((a ^ b) & (a ^ d) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= 0xC0000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
}

void dppc_interpreter::power_abs() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0x40000000;

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
    case 12:
    case 13:
    case 14:
    case 15:
        ppc_result_d = 65535;
        break;
    default:
        ppc_result_d = 0;
    }

    if (rc_flag) {
        ppc_changecrf0(ppc_result_d);
        printf("Does RC do anything here? (TODO) \n");
    }

    ppc_store_result_regd();
}

void dppc_interpreter::power_div() {
    ppc_grab_regsdab();
    ppc_result_d           = (ppc_result_a | ppc_state.spr[SPR::MQ]) / ppc_result_b;
    ppc_state.spr[SPR::MQ] = (ppc_result_a | ppc_state.spr[SPR::MQ]) % ppc_result_b;

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
    if (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = ~ppc_result_a + ppc_result_b + 1;
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_rega();
}

void dppc_interpreter::power_dozi() {
    ppc_grab_regsdab();
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = ~ppc_result_a + simm + 1;
    }
    ppc_store_result_rega();
}

void dppc_interpreter::power_lscbx() {
    ppc_grab_regsdab();
    uint32_t bytes_copied = 0;
    bool match_found      = false;
    uint32_t shift_amount = 0;
    uint8_t return_value;
    uint8_t byte_compared = (uint8_t)((ppc_state.spr[SPR::XER] & 0xFF00) >> 8);
    if ((ppc_state.spr[SPR::XER] & 0x7f) == 0) {
        return;
    }
    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7f) + 1;
    ppc_effective_address  = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
    do {
        ppc_effective_address++;
        bytes_to_load--;
        if (match_found == false) {
            switch (shift_amount) {
            case 0:
                return_value = mem_grab_byte(ppc_effective_address);
                ppc_result_d = (ppc_result_d & 0x00FFFFFF) | (return_value << 24);
                ppc_store_result_regd();
                break;
            case 1:
                return_value = mem_grab_byte(ppc_effective_address);
                ppc_result_d = (ppc_result_d & 0xFF00FFFF) | (return_value << 16);
                ppc_store_result_regd();
                break;
            case 2:
                return_value = mem_grab_byte(ppc_effective_address);
                ppc_result_d = (ppc_result_d & 0xFFFF00FF) | (return_value << 8);
                ppc_store_result_regd();
                break;
            case 3:
                return_value = mem_grab_byte(ppc_effective_address);
                ppc_result_d = (ppc_result_d & 0xFFFFFF00) | return_value;
                ppc_store_result_regd();
                break;
            }

            bytes_copied++;
        }
        if (return_value == byte_compared) {
            break;
        }

        if (shift_amount == 3) {
            shift_amount = 0;
            reg_d        = (reg_d + 1) & 0x1F;
        } else {
            shift_amount++;
        }
    } while (bytes_to_load > 0);
    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & 0xFFFFFF80) | bytes_copied;


    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_maskg() {
    ppc_grab_regssab();
    uint32_t mask_start  = ppc_result_d & 31;
    uint32_t mask_end    = ppc_result_b & 31;
    uint32_t insert_mask = 0;

    if (mask_start < (mask_end + 1)) {
        for (uint32_t i = mask_start; i < mask_end; i++) {
            insert_mask |= (0x80000000 >> i);
        }
    } else if (mask_start == (mask_end + 1)) {
        insert_mask = 0xFFFFFFFF;
    } else {
        insert_mask = 0xFFFFFFFF;
        for (uint32_t i = (mask_end + 1); i < (mask_start - 1); i++) {
            insert_mask &= (~(0x80000000 >> i));
        }
    }

    ppc_result_a = insert_mask;

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_rega();
}

void dppc_interpreter::power_maskir() {
    ppc_grab_regssab();
    uint32_t mask_insert = ppc_result_a;
    uint32_t insert_rot  = 0x80000000;
    do {
        if (ppc_result_b & insert_rot) {
            mask_insert &= ~insert_rot;
            mask_insert |= (ppc_result_d & insert_rot);
        }
        insert_rot = insert_rot >> 1;
    } while (insert_rot > 0);

    ppc_result_a = (ppc_result_d & ppc_result_b);

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
    ppc_result_d = (0x80000000 | ppc_result_a);

    if (rc_flag)
        ppc_changecrf0(ppc_result_d);

    ppc_store_result_regd();
}

void dppc_interpreter::power_rlmi() {
    ppc_grab_regssab();
    unsigned rot_mb      = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me      = (ppc_cur_instruction >> 1) & 31;
    uint32_t rot_amt     = ppc_result_b & 31;
    uint32_t insert_mask = 0;

    if (rot_mb < (rot_me + 1)) {
        for (uint32_t i = rot_mb; i < rot_me; i++) {
            insert_mask |= (0x80000000 >> i);
        }
    } else if (rot_mb == (rot_me + 1)) {
        insert_mask = 0xFFFFFFFF;
    } else {
        insert_mask = 0xFFFFFFFF;
        for (uint32_t i = (rot_me + 1); i < (rot_mb - 1); i++) {
            insert_mask &= (~(0x80000000 >> i));
        }
    }

    uint32_t step2 = (ppc_result_d << rot_amt) | (ppc_result_d >> rot_amt);
    ppc_result_a   = step2 & insert_mask;
    ppc_store_result_rega();
}

void dppc_interpreter::power_rrib() {
    ppc_grab_regssab();

    if (ppc_result_d & 0x80000000) {
        ppc_result_a |= (0x80000000 >> ppc_result_b);
    } else {
        ppc_result_a &= ~(0x80000000 >> ppc_result_b);
    }

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sle() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt     = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final  = ((ppc_result_d << rot_amt) | (ppc_result_d >> (32 - rot_amt)));
    ppc_state.spr[SPR::MQ] = insert_final & insert_mask;
    ppc_result_a           = insert_final & insert_mask;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sleq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt     = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_amt) | (ppc_result_d >> (rot_amt - 31)));
    uint32_t insert_end   = ppc_state.spr[SPR::MQ];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a           = insert_end;
    ppc_state.spr[SPR::MQ] = insert_start;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sliq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (rot_sh - 31)));
    uint32_t insert_end   = ppc_state.spr[SPR::MQ];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a           = insert_end & insert_mask;
    ppc_state.spr[SPR::MQ] = insert_start;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_slliq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t insert_end   = ppc_state.spr[SPR::MQ];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a           = insert_end;
    ppc_state.spr[SPR::MQ] = insert_start;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sllq() {
    LOG_F(WARNING, "OOPS! Placeholder for sllq!!! \n");
}

void dppc_interpreter::power_slq() {
    LOG_F(WARNING, "OOPS! Placeholder for slq!!! \n");
}

void dppc_interpreter::power_sraiq() {
    LOG_F(WARNING, "OOPS! Placeholder for sraiq!!! \n");
}

void dppc_interpreter::power_sraq() {
    LOG_F(WARNING, "OOPS! Placeholder for sraq!!! \n");
}

void dppc_interpreter::power_sre() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt     = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final  = ((ppc_result_d >> rot_amt) | (ppc_result_d << (32 - rot_amt)));
    ppc_state.spr[SPR::MQ] = insert_final & insert_mask;
    ppc_result_a           = insert_final;
    if (rc_flag)
        ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void dppc_interpreter::power_srea() {
    LOG_F(WARNING, "OOPS! Placeholder for srea!!! \n");
}

void dppc_interpreter::power_sreq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh      = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end   = ppc_state.spr[SPR::MQ];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a           = insert_end;
    ppc_state.spr[SPR::MQ] = insert_start;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_sriq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh      = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end   = ppc_state.spr[SPR::MQ];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a           = insert_end;
    ppc_state.spr[SPR::MQ] = insert_start;

    if (rc_flag)
        ppc_changecrf0(ppc_result_a);

    ppc_store_result_rega();
}

void dppc_interpreter::power_srliq() {
    LOG_F(WARNING, "OOPS! Placeholder for slriq!!! \n");
}

void dppc_interpreter::power_srlq() {
    LOG_F(WARNING, "OOPS! Placeholder for slrq!!! \n");
}

void dppc_interpreter::power_srq() {
    LOG_F(WARNING, "OOPS! Placeholder for srq!!! \n");
}