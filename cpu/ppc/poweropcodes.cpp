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
void dppc_interpreter::power_abs(uint32_t opcode) {
    uint32_t ppc_result_d;
    ppc_grab_regsda(opcode);
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else {
        ppc_result_d = (int32_t(ppc_result_a) < 0) ? -ppc_result_a : ppc_result_a;
        if (ov)
            ppc_state.spr[SPR::XER] &= ~XER::OV;
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_abs<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_abs<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_abs<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_abs<RC1, OV1>(uint32_t opcode);

void dppc_interpreter::power_clcs(uint32_t opcode) {
    uint32_t ppc_result_d;
    ppc_grab_da(opcode);
    switch (reg_a & 15) {
    case    0:
    case    1:
    case    2:
    case    3:
    case  0xC: //instruction cache line size
    case  0xD: //data cache line size
    case  0xE: //minimum line size
    case  0xF: //maximum line size
               ppc_result_d = is_601 ? 0x40 : 0x20; break;
    case    4:
    case    5:
    case    6: ppc_result_d = 0x20; break;
    case    7: ppc_result_d = is_601 ? 1 : 0; break;
    case    8:
    case    9: ppc_result_d = is_601 ? 8 : 4; break;
    case  0xA:
    case  0xB: ppc_result_d = is_601 ? 0x8000 : 0x4000; break;
    }

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_div(uint32_t opcode) {
    uint32_t ppc_result_d;
    ppc_grab_regsdab(opcode);

    int64_t dividend = (uint64_t(ppc_result_a) << 32) | ppc_state.spr[SPR::MQ];
    int32_t divisor = ppc_result_b;
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

    ppc_store_iresult_reg(reg_d, ppc_result_d);
    ppc_state.spr[SPR::MQ] = remainder;
}

template void dppc_interpreter::power_div<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_div<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_div<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_div<RC1, OV1>(uint32_t opcode);

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_divs(uint32_t opcode) {
    uint32_t ppc_result_d;
    int32_t remainder;
    ppc_grab_regsdab(opcode);

    if (!ppc_result_b) { // handle the "anything / 0" case
        ppc_result_d = -1;
        remainder = ppc_result_a;
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else if (ppc_result_a == 0x80000000U && ppc_result_b == 0xFFFFFFFFU) {
        ppc_result_d = 0x80000000U;
        remainder = 0;
        if (ov)
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
    } else { // normal signed devision
        ppc_result_d = int32_t(ppc_result_a) / int32_t(ppc_result_b);
        remainder = (int32_t(ppc_result_a) % int32_t(ppc_result_b));
        if (ov)
            ppc_state.spr[SPR::XER] &= ~XER::OV;
    }
    if (rec)
        ppc_changecrf0(remainder);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
    ppc_state.spr[SPR::MQ] = remainder;
}

template void dppc_interpreter::power_divs<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_divs<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_divs<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_divs<RC1, OV1>(uint32_t opcode);

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_doz(uint32_t opcode) {
    ppc_grab_regsdab(opcode);
    uint32_t ppc_result_d = (int32_t(ppc_result_a) < int32_t(ppc_result_b)) ?
        ppc_result_b - ppc_result_a : 0;

    if (ov) {
        if (int32_t(ppc_result_d) < 0) {
            ppc_state.spr[SPR::XER] |= XER::SO | XER::OV;
        } else {
            ppc_state.spr[SPR::XER] &= ~XER::OV;
        }
    }
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_doz<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_doz<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_doz<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_doz<RC1, OV1>(uint32_t opcode);

void dppc_interpreter::power_dozi(uint32_t opcode) {
    uint32_t ppc_result_d;
    ppc_grab_regsdasimm(opcode);
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = simm - ppc_result_a;
    }
    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template <field_rc rec>
void dppc_interpreter::power_lscbx(uint32_t opcode) {
    ppc_grab_regsdab(opcode);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);

    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7F);
    uint32_t bytes_remaining = bytes_to_load;
    uint8_t  matching_byte = (uint8_t)(ppc_state.spr[SPR::XER] >> 8);
    uint32_t ppc_result_d = 0;
    bool     is_match = false;

    // for storing each byte
    uint8_t  shift_amount = 24;

    while (bytes_remaining > 0) {
        uint8_t return_value = mmu_read_vmem<uint8_t>(opcode, ea);

        ppc_result_d |= return_value << shift_amount;
        if (!shift_amount) {
            if (reg_d != reg_a && reg_d != reg_b)
                ppc_store_iresult_reg(reg_d, ppc_result_d);
            reg_d        = (reg_d + 1) & 0x1F;
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
    if (shift_amount != 24 && reg_d != reg_a && reg_d != reg_b)
        ppc_store_iresult_reg(reg_d, ppc_result_d);

    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & ~0x7F) | (bytes_to_load - bytes_remaining);

    if (rec) {
        ppc_state.cr =
            (ppc_state.cr & 0x0FFFFFFFUL) |
            (is_match ? CRx_bit::CR_EQ : 0) |
            ((ppc_state.spr[SPR::XER] & XER::SO) >> 3);
    }
}

template void dppc_interpreter::power_lscbx<RC0>(uint32_t opcode);
template void dppc_interpreter::power_lscbx<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_maskg(uint32_t opcode) {
    ppc_grab_regssab(opcode);
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
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_maskg<RC0>(uint32_t opcode);
template void dppc_interpreter::power_maskg<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_maskir(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    ppc_result_a = (ppc_result_a & ~ppc_result_b) | (ppc_result_d & ppc_result_b);

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_maskir<RC0>(uint32_t opcode);
template void dppc_interpreter::power_maskir<RC1>(uint32_t opcode);

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_mul(uint32_t opcode) {
    ppc_grab_regsdab(opcode);
    int64_t product        = int64_t(int32_t(ppc_result_a)) * int32_t(ppc_result_b);
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
    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_mul<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_mul<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_mul<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_mul<RC1, OV1>(uint32_t opcode);

template <field_rc rec, field_ov ov>
void dppc_interpreter::power_nabs(uint32_t opcode) {
    ppc_grab_regsda(opcode);
    uint32_t ppc_result_d = (int32_t(ppc_result_a) < 0) ? ppc_result_a : -ppc_result_a;

    if (ov)
        ppc_state.spr[SPR::XER] &= ~XER::OV;
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_store_iresult_reg(reg_d, ppc_result_d);
}

template void dppc_interpreter::power_nabs<RC0, OV0>(uint32_t opcode);
template void dppc_interpreter::power_nabs<RC0, OV1>(uint32_t opcode);
template void dppc_interpreter::power_nabs<RC1, OV0>(uint32_t opcode);
template void dppc_interpreter::power_nabs<RC1, OV1>(uint32_t opcode);

void dppc_interpreter::power_rlmi(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_mb      = (opcode >> 6) & 0x1F;
    unsigned rot_me      = (opcode >> 1) & 0x1F;
    unsigned rot_sh      = ppc_result_b & 0x1F;

    uint32_t r           = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    uint32_t mask        = power_rot_mask(rot_mb, rot_me);

    ppc_result_a         = ((r & mask) | (ppc_result_a & ~mask));

    if ((opcode & 0x01) == 1)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template <field_rc rec>
void dppc_interpreter::power_rrib(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (int32_t(ppc_result_d) < 0) {
        ppc_result_a |= (0x80000000U >> rot_sh);
    } else {
        ppc_result_a &= ~(0x80000000U >> rot_sh);
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_rrib<RC0>(uint32_t opcode);
template void dppc_interpreter::power_rrib<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sle(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;

    ppc_store_iresult_reg(reg_a, ppc_result_a);

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sle<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sle<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sleq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sleq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sleq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sliq(uint32_t opcode) {
    ppc_grab_regssash(opcode);

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sliq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sliq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_slliq(uint32_t opcode) {
    ppc_grab_regssash(opcode);
    uint32_t r      = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_slliq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_slliq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sllq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = ppc_state.spr[SPR::MQ] & (-1U << rot_sh);
    } else {
        ppc_result_a = ((ppc_result_d << rot_sh) | (ppc_state.spr[SPR::MQ] & ((1U << rot_sh) - 1)));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sllq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sllq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_slq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d << rot_sh;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;
    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_slq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_slq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sraiq(uint32_t opcode) {
    ppc_grab_regssash(opcode);
    uint32_t mask          = (1U << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sraiq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sraiq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sraq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    uint32_t mask          = (ppc_result_b & 0x20) ? -1 : (1U << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> ((ppc_result_b & 0x20) ? 31 : rot_sh);
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh))) : ppc_result_d;

    if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sraq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sraq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sre(uint32_t opcode) {
    ppc_grab_regssab(opcode);

    unsigned rot_sh = ppc_result_b & 0x1F;
    ppc_result_a    = ppc_result_d >> rot_sh;

    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sre<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sre<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_srea(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    uint32_t r             = (rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d);
    uint32_t mask          = -1U >> rot_sh;

    if ((int32_t(ppc_result_d) < 0) && (r & ~mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
    ppc_state.spr[SPR::MQ] = r;
}

template void dppc_interpreter::power_srea<RC0>(uint32_t opcode);
template void dppc_interpreter::power_srea<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sreq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t mask   = -1U >> rot_sh;

    ppc_result_a           = (ppc_result_d >> rot_sh) | (ppc_state.spr[SPR::MQ] & ~mask);
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sreq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sreq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_sriq(uint32_t opcode) {
    ppc_grab_regssash(opcode);
    ppc_result_a           = ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_sriq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_sriq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_srliq(uint32_t opcode) {
    ppc_grab_regssash(opcode);
    uint32_t r      = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;
    unsigned mask   = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srliq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_srliq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_srlq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;
    unsigned mask   = power_rot_mask(rot_sh, 31);

    if (ppc_result_b & 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    } else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srlq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_srlq<RC1>(uint32_t opcode);

template <field_rc rec>
void dppc_interpreter::power_srq(uint32_t opcode) {
    ppc_grab_regssab(opcode);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d >> rot_sh;
    }

    ppc_state.spr[SPR::MQ] = rot_sh ? ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh))) : ppc_result_d;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_store_iresult_reg(reg_a, ppc_result_a);
}

template void dppc_interpreter::power_srq<RC0>(uint32_t opcode);
template void dppc_interpreter::power_srq<RC1>(uint32_t opcode);
