//DingusPPC - Prototype 5bf2
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

// The Power-specific opcodes for the processor - ppcopcodes.cpp
// Any shared opcodes are in ppcopcodes.cpp

#include <iostream>
#include <array>
#include <stdio.h>
#include <stdexcept>
#include "ppcemu.h"
#include "ppcmmu.h"
#include <cmath>
#include <limits>

void power_abs() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;

    }
    else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }
    ppc_store_result_regd();
}


void power_absdot() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;

    }
    else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void power_abso() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        ppc_state.ppc_spr[1] |= 0x40000000;

    }
    else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }
    ppc_store_result_regd();
}

void power_absodot() {
    ppc_grab_regsda();
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        ppc_state.ppc_spr[1] |= 0x40000000;

    }
    else {
        ppc_result_d = ppc_result_a & 0x7FFFFFFF;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();
}

void power_clcs() {
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
    ppc_store_result_regd();
}

void power_clcsdot() {
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
    ppc_changecrf0(ppc_result_d);
    printf("Does RC do anything here? (TODO) \n");
    ppc_store_result_regd();
}

void power_div() {
    ppc_grab_regsdab();
    ppc_result_d = (ppc_result_a | ppc_state.ppc_spr[0]) / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a | ppc_state.ppc_spr[0]) % ppc_result_b;
    ppc_store_result_regd();
}

void power_divdot() {
    ppc_result_d = (ppc_result_a | ppc_state.ppc_spr[0]) / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a | ppc_state.ppc_spr[0]) % ppc_result_b;
}

void power_divo() {
    ppc_result_d = (ppc_result_a | ppc_state.ppc_spr[0]) / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a | ppc_state.ppc_spr[0]) % ppc_result_b;
}

void power_divodot() {
    ppc_result_d = (ppc_result_a | ppc_state.ppc_spr[0]) / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a | ppc_state.ppc_spr[0]) % ppc_result_b;
}

void power_divs() {
    ppc_grab_regsdab();
    ppc_result_d = ppc_result_a / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a % ppc_result_b);
    ppc_store_result_regd();
}

void power_divsdot() {
    ppc_result_d = ppc_result_a / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a % ppc_result_b);
}

void power_divso() {
    ppc_result_d = ppc_result_a / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a % ppc_result_b);
}

void power_divsodot() {
    ppc_result_d = ppc_result_a / ppc_result_b;
    ppc_state.ppc_spr[0] = (ppc_result_a % ppc_result_b);
}

void power_doz() {
    ppc_grab_regsdab();
    if (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) {
        ppc_result_d = 0;
    }
    else {
        ppc_result_d = ~ppc_result_a + ppc_result_b + 1;
    }
    ppc_store_result_rega();
}

void power_dozdot() {
    ppc_grab_regsdab();
    if (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) {
        ppc_result_d = 0;
    }
    else {
        ppc_result_d = ~ppc_result_a + ppc_result_b + 1;
    }
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_rega();
}

void power_dozo() {
    if (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) {
        ppc_result_d = 0;
    }
    else {
        ppc_result_d = ~ppc_result_a + ppc_result_b + 1;
    }
}

void power_dozodot() {
    if (((int32_t)ppc_result_a) > ((int32_t)ppc_result_b)) {
        ppc_result_d = 0;
    }
    else {
        ppc_result_d = ~ppc_result_a + ppc_result_b + 1;
    }
}

void power_dozi() {
    ppc_grab_regsdab();
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    }
    else {
        ppc_result_d = ~ppc_result_a + simm + 1;
    }
    ppc_store_result_rega();
}

void power_lscbx() {
    ppc_grab_regsdab();
    uint32_t bytes_copied = 0;
    bool match_found = false;
    uint32_t shift_amount = 0;
    uint8_t return_value;
    uint8_t byte_compared = (uint8_t)((ppc_state.ppc_spr[1] & 0xFF00) >> 8);
    if ((ppc_state.ppc_spr[1] & 0x7f) == 0) {
        return;
    }
    uint32_t bytes_to_load = (ppc_state.ppc_spr[1] & 0x7f) + 1;
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
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
            reg_d = (reg_d + 1) & 0x1F;
        }
        else {
            shift_amount++;
        }
    } while (bytes_to_load > 0);
    ppc_state.ppc_spr[1] = (ppc_state.ppc_spr[1] & 0xFFFFFF80) | bytes_copied;
    ppc_store_result_regd();
}

void power_lscbxdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_maskg() {
    ppc_grab_regssab();
    uint32_t mask_start = ppc_result_d & 31;
    uint32_t mask_end = ppc_result_b & 31;
    uint32_t insert_mask = 0;

    if (mask_start < (mask_end + 1)) {
        for (uint32_t i = mask_start; i < mask_end; i++) {
            insert_mask |= (0x80000000 >> i);
        }
    }
    else if (mask_start == (mask_end + 1)) {
        insert_mask = 0xFFFFFFFF;
    }
    else {
        insert_mask = 0xFFFFFFFF;
        for (uint32_t i = (mask_end + 1); i < (mask_start - 1); i++) {
            insert_mask &= (~(0x80000000 >> i));
        }
    }

    ppc_result_a = insert_mask;
    ppc_store_result_rega();
}

void power_maskgdot() {
    ppc_grab_regssab();
    uint32_t mask_start = ppc_result_d & 31;
    uint32_t mask_end = ppc_result_b & 31;
    uint32_t insert_mask = 0;

    if (mask_start < (mask_end + 1)) {
        for (uint32_t i = mask_start; i < mask_end; i++) {
            insert_mask |= (0x80000000 >> i);
        }
    }
    else if (mask_start == (mask_end + 1)) {
        insert_mask = 0xFFFFFFFF;
    }
    else {
        insert_mask = 0xFFFFFFFF;
        for (uint32_t i = (mask_end + 1); i < (mask_start - 1); i++) {
            insert_mask &= (~(0x80000000 >> i));
        }
    }

    ppc_result_a = insert_mask;
    ppc_store_result_rega();
}

void power_maskir() {
    ppc_grab_regssab();
    uint32_t mask_insert = ppc_result_a;
    uint32_t insert_rot = 0x80000000;
    do {
        if (ppc_result_b & insert_rot) {
            mask_insert &= ~insert_rot;
            mask_insert |= (ppc_result_d & insert_rot);
        }
        insert_rot = insert_rot >> 1;
    } while (insert_rot > 0);

    ppc_result_a = (ppc_result_d & ppc_result_b);
    ppc_store_result_rega();
}

void power_maskirdot() {
    ppc_grab_regssab();
    uint32_t mask_insert = ppc_result_a;
    uint32_t insert_rot = 0x80000000;
    do {
        if (ppc_result_b & insert_rot) {
            mask_insert &= ~insert_rot;
            mask_insert |= (ppc_result_d & insert_rot);
        }
        insert_rot = insert_rot >> 1;
    } while (insert_rot > 0);

    ppc_result_a = (ppc_result_d & ppc_result_b);
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_mul() {
    ppc_grab_regsdab();
    uint64_t product;

    product = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    ppc_result_d = ((uint32_t)(product >> 32));
    ppc_state.ppc_spr[0] = ((uint32_t)(product));
    ppc_store_result_regd();

}

void power_muldot() {
    ppc_grab_regsdab();
    uint64_t product;

    product = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    ppc_result_d = ((uint32_t)(product >> 32));
    ppc_state.ppc_spr[0] = ((uint32_t)(product));
    ppc_changecrf0(ppc_result_d);
    ppc_store_result_regd();

}

void power_mulo() {
    uint64_t product;

    product = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    ppc_result_d = ((uint32_t)(product >> 32));
    ppc_state.ppc_spr[0] = ((uint32_t)(product));

}

void power_mulodot() {
    uint64_t product;

    product = ((uint64_t)ppc_result_a) * ((uint64_t)ppc_result_b);
    ppc_result_d = ((uint32_t)(product >> 32));
    ppc_state.ppc_spr[0] = ((uint32_t)(product));

}

void power_nabs() {
    ppc_grab_regsda();
    ppc_result_d = (0x80000000 | ppc_result_a);
    ppc_store_result_regd();
}

void power_nabsdot() {
    ppc_result_d = (0x80000000 | ppc_result_a);
}

void power_nabso() {
    ppc_result_d = (0x80000000 | ppc_result_a);
}

void power_nabsodot() {
    ppc_result_d = (0x80000000 | ppc_result_a);
}

void power_rlmi() {
    ppc_grab_regssab();
    unsigned rot_mb = (ppc_cur_instruction >> 6) & 31;
    unsigned rot_me = (ppc_cur_instruction >> 1) & 31;
    uint32_t rot_amt = ppc_result_b & 31;
    uint32_t insert_mask = 0;

    if (rot_mb < (rot_me + 1)) {
        for (uint32_t i = rot_mb; i < rot_me; i++) {
            insert_mask |= (0x80000000 >> i);
        }
    }
    else if (rot_mb == (rot_me + 1)) {
        insert_mask = 0xFFFFFFFF;
    }
    else {
        insert_mask = 0xFFFFFFFF;
        for (uint32_t i = (rot_me + 1); i < (rot_mb - 1); i++) {
            insert_mask &= (~(0x80000000 >> i));
        }
    }

    uint32_t step2 = (ppc_result_d << rot_amt) | (ppc_result_d >> rot_amt);
    ppc_result_a = step2 & insert_mask;
    ppc_store_result_rega();
}

void power_rrib() {
    ppc_grab_regssab();
    if (ppc_result_d & 0x80000000) {
        ppc_result_a |= (0x80000000 >> ppc_result_b);
    }
    else {
        ppc_result_a &= ~(0x80000000 >> ppc_result_b);
    }
    ppc_store_result_rega();
}

void power_rribdot() {
    ppc_grab_regssab();
    if (ppc_result_d & 0x80000000) {
        ppc_result_a |= (0x80000000 >> ppc_result_b);
    }
    else {
        ppc_result_a &= ~(0x80000000 >> ppc_result_b);
    }
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_sle() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final = ((ppc_result_d << rot_amt) | (ppc_result_d >> (32 - rot_amt)));
    ppc_state.ppc_spr[0] = insert_final & insert_mask;
    ppc_result_a = insert_final & insert_mask;
    ppc_store_result_rega();
}

void power_sledot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final = ((ppc_result_d << rot_amt) | (ppc_result_d >> (32 - rot_amt)));
    ppc_state.ppc_spr[0] = insert_final & insert_mask;
    ppc_result_a = insert_final & insert_mask;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_sleq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_amt) | (ppc_result_d >> (rot_amt - 31)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_store_result_rega();
}

void power_sleqdot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_amt) | (ppc_result_d >> (rot_amt - 31)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_sliq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (rot_sh - 31)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end & insert_mask;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_store_result_rega();
}

void power_sliqdot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (rot_sh - 31)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end & insert_mask;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_slliq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_store_result_rega();
}

void power_slliqdot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_sllq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sllqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_slq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_slqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sraiq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sraiqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sraq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sraqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sre() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final = ((ppc_result_d >> rot_amt) | (ppc_result_d << (32 - rot_amt)));
    ppc_state.ppc_spr[0] = insert_final & insert_mask;
    ppc_result_a = insert_final;
    ppc_store_result_rega();
}

void power_sredot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    uint32_t rot_amt = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_amt; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_final = ((ppc_result_d >> rot_amt) | (ppc_result_d << (32 - rot_amt)));
    ppc_state.ppc_spr[0] = insert_final & insert_mask;
    ppc_result_a = insert_final;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_srea() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sreadot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_sreq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_store_result_rega();
}

void power_sreqdot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = ppc_result_b & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_sriq() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_store_result_rega();
}

void power_sriqdot() {
    ppc_grab_regssa();
    uint32_t insert_mask = 0;
    unsigned rot_sh = (ppc_cur_instruction >> 11) & 31;
    for (uint32_t i = 31; i > rot_sh; i--) {
        insert_mask |= (1 << i);
    }
    uint32_t insert_start = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t insert_end = ppc_state.ppc_spr[0];

    for (int i = 0; i < 32; i++) {
        if (insert_mask & (1 << i)) {
            insert_end &= ~(1 << i);
            insert_end |= (insert_start & (1 << i));
        }
    }

    ppc_result_a = insert_end;
    ppc_state.ppc_spr[0] = insert_start;
    ppc_changecrf0(ppc_result_a);
    ppc_store_result_rega();
}

void power_srliq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_srliqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_srlq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_srlqdot() {
    printf("OOPS! Placeholder!!! \n");
}

void power_srq() {
    printf("OOPS! Placeholder!!! \n");
}

void power_srqdot() {
    printf("OOPS! Placeholder!!! \n");
}
