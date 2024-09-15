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

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t power_rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFU >> rot_mb;
    uint32_t m2 = 0xFFFFFFFFU << (31 - rot_me);
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_abs() {
    uint32_t ppc_result_d;
    if (ppc_state.gpr[instr.arg1] == 0x80000000) {
        ppc_result_d = ppc_state.gpr[instr.arg1];
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else {
        ppc_result_d = (int32_t(ppc_state.gpr[instr.arg1]) < 0) ? -ppc_state.gpr[instr.arg1] : ppc_state.gpr[instr.arg1];
        if (ov)
            ppc_state.spr[SPR::XER] &= ~XER::OV;
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
}

template void dppc_interpreter::power_abs<RC0, OV0>();
template void dppc_interpreter::power_abs<RC0, OV1>();
template void dppc_interpreter::power_abs<RC1, OV0>();
template void dppc_interpreter::power_abs<RC1, OV1>();

void dppc_interpreter::power_clcs() {
    switch (ppc_state.gpr[instr.arg1]) {
    case 12: //instruction cache line size
    case 13: //data cache line size
    case 14: //minimum line size
    case 15: //maximum line size
    default:
        ppc_state.gpr[instr.arg0] = is_601 ? 64 : 32;
        break;
    case  7:
    case 23:
        ppc_state.gpr[instr.arg0] = is_601 ? 64 : 0;
        break;
    case  8:
    case  9:
    case 24:
    case 25:
        ppc_state.gpr[instr.arg0] = is_601 ? 64 : 4;
        break;
    case 10:
    case 11:
    case 26:
    case 27:
        ppc_state.gpr[instr.arg0] = is_601 ? 64 : 0x4000;
        break;
    }
}

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_div() {
    uint32_t ppc_result_d;

    int64_t dividend = (uint64_t(ppc_state.gpr[instr.arg1]) << 32) | ppc_state.spr[SPR::MQ];
    int32_t divisor = ppc_state.gpr[instr.arg2];
    int64_t quotient;
    int32_t remainder;

    if (dividend == -0x80000000 && divisor == -1) {
        remainder = 0;
        ppc_result_d = 0x80000000U; // -2^31 aka INT32_MIN
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else if (!divisor) {
        remainder = 0;
        ppc_result_d = 0x80000000U; // -2^31 aka INT32_MIN
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else {
        quotient = dividend / divisor;
        remainder = dividend % divisor;
        ppc_result_d = uint32_t(quotient);
        if (ov) {
            if (((quotient >> 31) + 1) & ~1) {
                ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
            } else {
                ppc_state.spr[SPR::XER] &= ~XER::OV;
            }
        }
    }

    if (rec)
        ppc_changecrf0(remainder);

    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
    ppc_state.spr[SPR::MQ] = remainder;
}

template void dppc_interpreter::power_div<RC0, OV0>();
template void dppc_interpreter::power_div<RC0, OV1>();
template void dppc_interpreter::power_div<RC1, OV0>();
template void dppc_interpreter::power_div<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_divs() {
    uint32_t ppc_result_d;
    int32_t remainder;

    if (!ppc_state.gpr[instr.arg2]) { // handle the "anything / 0" case
        ppc_result_d = -1;
        remainder = ppc_state.gpr[instr.arg1];
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else if (ppc_state.gpr[instr.arg1] == 0x80000000U && ppc_state.gpr[instr.arg2] == 0xFFFFFFFFU) {
        ppc_result_d = 0x80000000U;
        remainder = 0;
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else { // normal signed devision
        ppc_result_d = int32_t(ppc_state.gpr[instr.arg1]) / int32_t(ppc_state.gpr[instr.arg2]);
        remainder = (int32_t(ppc_state.gpr[instr.arg1]) % int32_t(ppc_state.gpr[instr.arg2]));
        if (ov)
            ppc_state.spr[SPR::XER] &= ~XER::OV;
    }
    if (rec)
        ppc_changecrf0(remainder);

    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
    ppc_state.spr[SPR::MQ] = remainder;
}

template void dppc_interpreter::power_divs<RC0, OV0>();
template void dppc_interpreter::power_divs<RC0, OV1>();
template void dppc_interpreter::power_divs<RC1, OV0>();
template void dppc_interpreter::power_divs<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_doz() {
    uint32_t ppc_result_d = (int32_t(ppc_state.gpr[instr.arg1]) < int32_t(ppc_state.gpr[instr.arg2])) ?
        ppc_state.gpr[instr.arg2] - ppc_state.gpr[instr.arg1] : 0;

    if (ov) {
        if (int32_t(ppc_result_d) < 0) {
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
        } else {
            ppc_state.spr[SPR::XER] &= ~XER::OV;
        }
    }
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
}

template void dppc_interpreter::power_doz<RC0, OV0>();
template void dppc_interpreter::power_doz<RC0, OV1>();
template void dppc_interpreter::power_doz<RC1, OV0>();
template void dppc_interpreter::power_doz<RC1, OV1>();

void dppc_interpreter::power_dozi() {
    uint32_t ppc_result_d;
    if (((int32_t)ppc_state.gpr[instr.arg1]) > instr.i_simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = instr.i_simm - ppc_state.gpr[instr.arg1];
    }
    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
}

template <field_rc rec>
void dppc_interpreter::power_lscbx() {
    uint32_t ea = ppc_state.gpr[instr.arg2] + (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);

    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7F);
    uint32_t bytes_remaining = bytes_to_load;
    uint8_t  matching_byte = (uint8_t)(ppc_state.spr[SPR::XER] >> 8);
    uint32_t ppc_result_d = 0;
    bool     is_match = false;

    // for storing each byte
    uint8_t  shift_amount = 24;

    while (bytes_remaining > 0) {
        uint8_t return_value = mmu_read_vmem<uint8_t>(ea);

        ppc_result_d |= return_value << shift_amount;
        if (!shift_amount) {
            if (instr.arg0 != instr.arg1 && instr.arg0 != instr.arg2)
                ppc_store_iresult_reg(instr.arg0, ppc_result_d);
            instr.arg0   = (instr.arg0 + 1) & 0x1F;
            ppc_result_d = 0;
            shift_amount = 24;
        } else {
            shift_amount -= 8;
        }

        ea++;
        bytes_remaining--;

        if (return_value == matching_byte) {
            is_match = true;
            break;
        }
    }

    // store partially loaded register if any
    if (shift_amount != 24 && instr.arg0 != instr.arg1 && instr.arg0 != instr.arg2)
        ppc_store_iresult_reg(instr.arg0, ppc_result_d);

    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & ~0x7F) | (bytes_to_load - bytes_remaining);

    if (rec) {
        ppc_state.cr =
            (ppc_state.cr & 0x0FFFFFFFUL) |
            (is_match ? CRx_bit::CR_EQ : 0) |
            ((ppc_state.spr[SPR::XER] & XER::SO) >> 3);
    }
}

template void dppc_interpreter::power_lscbx<RC0>();
template void dppc_interpreter::power_lscbx<RC1>();

template <field_rc rec>
void dppc_interpreter::power_maskg() {
    uint32_t mask_start  = ppc_state.gpr[instr.arg0] & 0x1F;
    uint32_t mask_end    = ppc_state.gpr[instr.arg2] & 0x1F;
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

    ppc_state.gpr[instr.arg1] = insert_mask;

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);

    ppc_store_iresult_reg(instr.arg1, ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_maskg<RC0>();
template void dppc_interpreter::power_maskg<RC1>();

template <field_rc rec>
void dppc_interpreter::power_maskir() {
    ppc_state.gpr[instr.arg1] = ((ppc_state.gpr[instr.arg1] & ~ppc_state.gpr[instr.arg2]) | \
        (ppc_state.gpr[instr.arg0] & ppc_state.gpr[instr.arg2]));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_maskir<RC0>();
template void dppc_interpreter::power_maskir<RC1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_mul() {
    int64_t product        = int64_t(int32_t(ppc_state.gpr[instr.arg1])) * int32_t(ppc_state.gpr[instr.arg2]);
    uint32_t ppc_result_d  = uint32_t(uint64_t(product) >> 32);
    ppc_state.spr[SPR::MQ] = uint32_t(product);

    if (ov) {
        if (uint64_t(product >> 31) + 1 & ~1) {
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
        } else {
            ppc_state.spr[SPR::XER] &= ~XER::OV;
        }
    }
    if (rec)
        ppc_changecrf0(uint32_t(product));
    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
}

template void dppc_interpreter::power_mul<RC0, OV0>();
template void dppc_interpreter::power_mul<RC0, OV1>();
template void dppc_interpreter::power_mul<RC1, OV0>();
template void dppc_interpreter::power_mul<RC1, OV1>();

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_nabs() {
    uint32_t ppc_result_d = (int32_t(ppc_state.gpr[instr.arg1]) < 0) ? ppc_state.gpr[instr.arg1] : -ppc_state.gpr[instr.arg1];

    if (ov)
        ppc_state.spr[SPR::XER] &= ~XER::OV;
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(instr.arg0, ppc_result_d);
}

template void dppc_interpreter::power_nabs<RC0, OV0>();
template void dppc_interpreter::power_nabs<RC0, OV1>();
template void dppc_interpreter::power_nabs<RC1, OV0>();
template void dppc_interpreter::power_nabs<RC1, OV1>();

void dppc_interpreter::power_rlmi() {
    unsigned rot_sh      = ppc_state.gpr[instr.arg2] & 0x1F;

    uint32_t r =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));
    uint32_t mask = power_rot_mask(instr.arg3, instr.arg4);

    uint32_t ppc_result_a = ((r & mask) | (ppc_state.gpr[instr.arg1] & ~mask));

    if ((ppc_cur_instruction & 0x01) == 1)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(instr.arg1, ppc_result_a);
}

template <field_rc rec>
void dppc_interpreter::power_rrib() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    if (int32_t(ppc_state.gpr[instr.arg1]) < 0) {
        ppc_state.gpr[instr.arg1] |= (0x80000000U >> rot_sh);
    } else {
        ppc_state.gpr[instr.arg1] &= ~(0x80000000U >> rot_sh);
    }

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_rrib<RC0>();
template void dppc_interpreter::power_rrib<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sle() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] << rot_sh;
    ppc_state.spr[SPR::MQ] =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));

    ppc_store_iresult_reg(instr.arg1, ppc_state.gpr[instr.arg1]);

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);

    ppc_store_iresult_reg(instr.arg1, ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sle<RC0>();
template void dppc_interpreter::power_sle<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sleq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t r =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_state.gpr[instr.arg1] = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ]    = r;

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sleq<RC0>();
template void dppc_interpreter::power_sleq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sliq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] << rot_sh;
    ppc_state.spr[SPR::MQ] =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);

    ppc_store_iresult_reg(instr.arg1, ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sliq<RC0>();
template void dppc_interpreter::power_sliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_slliq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    uint32_t r =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_state.gpr[instr.arg1] = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ]    = r;

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_slliq<RC0>();
template void dppc_interpreter::power_slliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sllq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    if (ppc_state.gpr[instr.arg2] & 0x20) {
        ppc_state.gpr[instr.arg1] = ppc_state.spr[SPR::MQ] & (-1U << rot_sh);
    } else {
        ppc_state.gpr[instr.arg1] =
            ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.spr[SPR::MQ] & ((1 << rot_sh) - 1)));
    }

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sllq<RC0>();
template void dppc_interpreter::power_sllq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_slq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    if (ppc_state.gpr[instr.arg2] & 0x20) {
        ppc_state.gpr[instr.arg1] = 0;
    } else {
        ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] << rot_sh;
    }

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);

    ppc_state.spr[SPR::MQ] =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));
}

template void dppc_interpreter::power_slq<RC0>();
template void dppc_interpreter::power_slq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sraiq() {
    uint32_t mask         = (1 << instr.arg2) - 1;
    uint32_t ppc_result_a  = (int32_t)ppc_state.gpr[instr.arg0] >> instr.arg2;
    ppc_state.spr[SPR::MQ] = (ppc_state.gpr[instr.arg0] >> instr.arg2) |
        (ppc_state.gpr[instr.arg0] << (32 - instr.arg2));

    if ((int32_t(ppc_state.gpr[instr.arg1]) < 0) && (ppc_state.gpr[instr.arg1] & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(instr.arg1, ppc_result_a);
}

template void dppc_interpreter::power_sraiq<RC0>();
template void dppc_interpreter::power_sraiq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sraq() {
    unsigned rot_sh        = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t mask          = (ppc_state.gpr[instr.arg2] & 0x20) ? -1 : (1 << rot_sh) - 1;
    ppc_state.gpr[instr.arg1] = (int32_t)ppc_state.gpr[instr.arg0] >>
        ((ppc_state.gpr[instr.arg2] & 0x20) ? 31 : rot_sh);
    ppc_state.spr[SPR::MQ] =
        ((ppc_state.gpr[instr.arg0] << rot_sh) | (ppc_state.gpr[instr.arg0] >> (32 - rot_sh)));

    if ((int32_t(ppc_state.gpr[instr.arg0]) < 0) && (ppc_state.gpr[instr.arg0] & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    ppc_state.spr[SPR::MQ] = (ppc_state.gpr[instr.arg0] >> rot_sh) |
        (ppc_state.gpr[instr.arg0] << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sraq<RC0>();
template void dppc_interpreter::power_sraq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sre() {
    unsigned rot_sh           = ppc_state.gpr[instr.arg2] & 0x1F;
    ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] >> rot_sh;

    ppc_state.spr[SPR::MQ] = (ppc_state.gpr[instr.arg0] >> rot_sh) |
        (ppc_state.gpr[instr.arg0] << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sre<RC0>();
template void dppc_interpreter::power_sre<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srea() {
    unsigned rot_sh        = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t ppc_result_a  = (int32_t)ppc_state.gpr[instr.arg0] >> rot_sh;
    uint32_t r             = ((ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh)));
    uint32_t mask          = -1U >> rot_sh;

    if ((int32_t(ppc_state.gpr[instr.arg0]) < 0) && (r & ~mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(instr.arg1, ppc_result_a);
    ppc_state.spr[SPR::MQ] = r;
}

template void dppc_interpreter::power_srea<RC0>();
template void dppc_interpreter::power_srea<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sreq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t mask   = -1U >> rot_sh;

    uint32_t ppc_result_a  = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.spr[SPR::MQ] & ~mask);
    ppc_state.spr[SPR::MQ] = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(instr.arg1, ppc_result_a);
}

template void dppc_interpreter::power_sreq<RC0>();
template void dppc_interpreter::power_sreq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_sriq() {
    uint32_t rot_sh           = ppc_state.gpr[instr.arg2] & 0x1F;
    ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] >> rot_sh;
    ppc_state.spr[SPR::MQ]    = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_sriq<RC0>();
template void dppc_interpreter::power_sriq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srliq() {
    uint32_t rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t r      = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh));
    unsigned mask = power_rot_mask(ppc_state.gpr[instr.arg2], 31);

    ppc_state.gpr[instr.arg1] = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_srliq<RC0>();
template void dppc_interpreter::power_srliq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srlq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;
    uint32_t r      = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    if (ppc_state.gpr[instr.arg2] & 0x20) {
        ppc_state.gpr[instr.arg1] = (ppc_state.spr[SPR::MQ] & mask);
    } else {
        ppc_state.gpr[instr.arg1] = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_srlq<RC0>();
template void dppc_interpreter::power_srlq<RC1>();

template <field_rc rec>
void dppc_interpreter::power_srq() {
    unsigned rot_sh = ppc_state.gpr[instr.arg2] & 0x1F;

    if (ppc_state.gpr[instr.arg2] & 0x20) {
        ppc_state.gpr[instr.arg1] = 0;
    } else {
        ppc_state.gpr[instr.arg1] = ppc_state.gpr[instr.arg0] >> rot_sh;
    }

    ppc_state.spr[SPR::MQ] = (ppc_state.gpr[instr.arg0] >> rot_sh) | (ppc_state.gpr[instr.arg0] << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_state.gpr[instr.arg1]);
}

template void dppc_interpreter::power_srq<RC0>();
template void dppc_interpreter::power_srq<RC1>();
