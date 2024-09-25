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

POWEROPCODEOVREC (abs,
    uint32_t ppc_result_d;
    ppc_grab_regsda(instr);
    if (ppc_result_a == 0x80000000) {
        ppc_result_d = ppc_result_a;
        if (ov)
            ppc_set_xer(XER::SO | XER::OV);
    } else {
        ppc_result_d = (int32_t(ppc_result_a) < 0) ? -ppc_result_a : ppc_result_a;
        if (ov)
            ppc_unset_xer(XER::OV);
    }

    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_state.gpr[reg_d] = ppc_result_d;
)

template void dppc_interpreter::power_abs<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_abs<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_abs<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_abs<RC1, OV1>(uint32_t instr);

POWEROPCODE (clcs, 
    ppc_grab_da(instr); 
    uint32_t ppc_result_d = 0;

    switch (reg_a) {
    case 12: //instruction cache line size
    case 13: //data cache line size
    case 14: //minimum line size
    case 15: //maximum line size
    default: 
        ppc_result_d = is_601 ? 64 :     32; break;
    case  7:
    case 23: 
        ppc_result_d = is_601 ? 64 :      0; break;
    case  8:
    case  9:
    case 24:
    case 25: 
        ppc_result_d = is_601 ? 64 :      4; break;
    case 10:
    case 11:
    case 26:
    case 27: 
        ppc_result_d = is_601 ? 64 : 0x4000; break;
    }

    ppc_state.gpr[reg_d] = ppc_result_d;
)

POWEROPCODEOVREC (div,
    uint32_t ppc_result_d;
    ppc_grab_regsdab(instr);

    int64_t dividend = (uint64_t(ppc_result_a) << 32) | ppc_state.spr[SPR::MQ];
    int32_t divisor = ppc_result_b;
    int64_t quotient;
    int32_t remainder;

    if (dividend == -0x80000000 && divisor == -1) {
        remainder = 0;
        ppc_result_d = 0x80000000U; // -2^31 aka INT32_MIN
        if (ov)
            ppc_set_xer(XER::SO | XER::OV);
    } else if (!divisor) {
        remainder = 0;
        ppc_result_d = 0x80000000U; // -2^31 aka INT32_MIN
        if (ov)
            ppc_set_xer(XER::SO | XER::OV);
    } else {
        quotient = dividend / divisor;
        remainder = dividend % divisor;
        ppc_result_d = uint32_t(quotient);
        if (ov) {
            if (((quotient >> 31) + 1) & ~1) {
                ppc_set_xer(XER::SO | XER::OV);
            } else {
                ppc_unset_xer(XER::OV);
            }
        }
    }

    if (rec)
        ppc_changecrf0(remainder);

    ppc_state.gpr[reg_d] = ppc_result_d;
    power_store_mq(remainder);
)

template void dppc_interpreter::power_div<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_div<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_div<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_div<RC1, OV1>(uint32_t instr);

POWEROPCODEOVREC (divs,
    uint32_t ppc_result_d;
    int32_t remainder;
    ppc_grab_regsdab(instr);

    if (!ppc_result_b) { // handle the "anything / 0" case
        ppc_result_d = -1;
        remainder = ppc_result_a;
        if (ov)
            ppc_set_xer(XER::SO | XER::OV);
    } else if (ppc_result_a == 0x80000000U && ppc_result_b == 0xFFFFFFFFU) {
        ppc_result_d = 0x80000000U;
        remainder = 0;
        if (ov)
            ppc_set_xer(XER::SO | XER::OV);
    } else { // normal signed devision
        ppc_result_d = int32_t(ppc_result_a) / int32_t(ppc_result_b);
        remainder = (int32_t(ppc_result_a) % int32_t(ppc_result_b));
        if (ov)
            ppc_unset_xer(XER::OV);
    }
    if (rec)
        ppc_changecrf0(remainder);

    ppc_state.gpr[reg_d] = ppc_result_d;
    power_store_mq(remainder);;
)

template void dppc_interpreter::power_divs<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_divs<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_divs<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_divs<RC1, OV1>(uint32_t instr);

POWEROPCODEOVREC (doz,
    ppc_grab_regsdab(instr);
    uint32_t ppc_result_d = (int32_t(ppc_result_a) < int32_t(ppc_result_b)) ?
        ppc_result_b - ppc_result_a : 0;

    if (ov) {
        if (int32_t(ppc_result_d) < 0) {
            ppc_set_xer(XER::SO | XER::OV);
        } else {
            ppc_unset_xer(XER::OV);
        }
    }
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_state.gpr[reg_d] = ppc_result_d;
)

template void dppc_interpreter::power_doz<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_doz<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_doz<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_doz<RC1, OV1>(uint32_t instr);

POWEROPCODE (dozi,
    uint32_t ppc_result_d;
    ppc_grab_regsdasimm(instr);
    if (((int32_t)ppc_result_a) > simm) {
        ppc_result_d = 0;
    } else {
        ppc_result_d = simm - ppc_result_a;
    }
    ppc_state.gpr[reg_d] = ppc_result_d;
)

POWEROPCODEREC (lscbx,
    ppc_grab_regsdab(instr);
    uint32_t ea = ppc_result_b + (reg_a ? ppc_result_a : 0);

    uint32_t bytes_to_load = (ppc_state.spr[SPR::XER] & 0x7F);
    uint32_t bytes_remaining = bytes_to_load;
    uint8_t  matching_byte = (uint8_t)(ppc_state.spr[SPR::XER] >> 8);
    uint32_t ppc_result_d = 0;
    bool     is_match = false;

    // for storing each byte
    uint8_t  shift_amount = 24;

    while (bytes_remaining > 0) {
        uint8_t return_value = mmu_read_vmem<uint8_t>(ea, instr);

        ppc_result_d |= return_value << shift_amount;
        if (!shift_amount) {
            if (reg_d != reg_a && reg_d != reg_b)
                ppc_state.gpr[reg_d] = ppc_result_d;
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
        ppc_state.gpr[reg_d] = ppc_result_d;

    ppc_state.spr[SPR::XER] = (ppc_state.spr[SPR::XER] & ~0x7F) | (bytes_to_load - bytes_remaining);

    if (rec) {
        ppc_state.cr =
            (ppc_state.cr & 0x0FFFFFFFUL) |
            (is_match ? CRx_bit::CR_EQ : 0) |
            ((ppc_state.spr[SPR::XER] & XER::SO) >> 3);
    }
)

template void dppc_interpreter::power_lscbx<RC0>(uint32_t instr);
template void dppc_interpreter::power_lscbx<RC1>(uint32_t instr);

POWEROPCODEREC (maskg,
    ppc_grab_regssab(instr);
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

    ppc_state.gpr[reg_a] = ppc_result_a;
}

template void dppc_interpreter::power_maskg<RC0>(uint32_t instr);
template void dppc_interpreter::power_maskg<RC1>(uint32_t instr);

POWEROPCODEREC (maskir,
    ppc_grab_regssab(instr);
    ppc_result_a = (ppc_result_a & ~ppc_result_b) | (ppc_result_d & ppc_result_b);

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
}

template void dppc_interpreter::power_maskir<RC0>(uint32_t instr);
template void dppc_interpreter::power_maskir<RC1>(uint32_t instr);

POWEROPCODEOVREC (mul,
    ppc_grab_regsdab(instr);
    int64_t product        = int64_t(int32_t(ppc_result_a)) * int32_t(ppc_result_b);
    uint32_t ppc_result_d  = uint32_t(uint64_t(product) >> 32);
    ppc_state.spr[SPR::MQ] = uint32_t(product);

    if (ov) {
        if (uint64_t(product >> 31) + 1 & ~1) {
            ppc_set_xer(XER::SO | XER::OV);
        } else {
            ppc_unset_xer(XER::OV);
        }
    }
    if (rec)
        ppc_changecrf0(uint32_t(product));
    ppc_state.gpr[reg_d] = ppc_result_d;
}

template void dppc_interpreter::power_mul<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_mul<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_mul<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_mul<RC1, OV1>(uint32_t instr);

POWEROPCODEOVREC (nabs,
    ppc_grab_regsda(instr);
    uint32_t ppc_result_d = (int32_t(ppc_result_a) < 0) ? ppc_result_a : -ppc_result_a;

    if (ov)
        ppc_unset_xer(XER::OV);
    if (rec)
        ppc_changecrf0(ppc_result_d);

    ppc_state.gpr[reg_d] = ppc_result_d;
}

template void dppc_interpreter::power_nabs<RC0, OV0>(uint32_t instr);
template void dppc_interpreter::power_nabs<RC0, OV1>(uint32_t instr);
template void dppc_interpreter::power_nabs<RC1, OV0>(uint32_t instr);
template void dppc_interpreter::power_nabs<RC1, OV1>(uint32_t instr);

POWEROPCODE (rlmi,
    ppc_grab_regssab(instr);
    unsigned rot_mb      = (instr >> 6) & 0x1F;
    unsigned rot_me      = (instr >> 1) & 0x1F;
    unsigned rot_sh      = ppc_result_b & 0x1F;

    uint32_t r           = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask        = power_rot_mask(rot_mb, rot_me);

    ppc_result_a         = ((r & mask) | (ppc_result_a & ~mask));

    if ((instr & 0x01) == 1)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
}

POWEROPCODEREC (rrib,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (int32_t(ppc_result_d) < 0) {
        ppc_result_a |= (0x80000000U >> rot_sh);
    } else {
        ppc_result_a &= ~(0x80000000U >> rot_sh);
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
}

template void dppc_interpreter::power_rrib<RC0>(uint32_t instr);
template void dppc_interpreter::power_rrib<RC1>(uint32_t instr);

POWEROPCODEREC (sle,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    ppc_state.gpr[reg_a] = ppc_result_a;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sle<RC0>(uint32_t instr);
template void dppc_interpreter::power_sle<RC1>(uint32_t instr);

POWEROPCODEREC (sleq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sleq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sleq<RC1>(uint32_t instr);

POWEROPCODEREC (sliq,
    ppc_grab_regssash(instr);

    ppc_result_a           = ppc_result_d << rot_sh;
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sliq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sliq<RC1>(uint32_t instr);

POWEROPCODEREC (slliq,
    ppc_grab_regssash(instr);
    uint32_t r      = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    uint32_t mask   = power_rot_mask(0, 31 - rot_sh);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_slliq<RC0>(uint32_t instr);
template void dppc_interpreter::power_slliq<RC1>(uint32_t instr);

POWEROPCODEREC (sllq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = ppc_state.spr[SPR::MQ] & (-1U << rot_sh);
    } else {
        ppc_result_a = ((ppc_result_d << rot_sh) | (ppc_state.spr[SPR::MQ] & ((1 << rot_sh) - 1)));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sllq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sllq<RC1>(uint32_t instr);

POWEROPCODEREC (slq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d << rot_sh;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));
    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_slq<RC0>(uint32_t instr);
template void dppc_interpreter::power_slq<RC1>(uint32_t instr);

POWEROPCODEREC (sraiq,
    ppc_grab_regssash(instr);
    uint32_t mask          = (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sraiq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sraiq<RC1>(uint32_t instr);

POWEROPCODEREC (sraq,
    ppc_grab_regssab(instr);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    uint32_t mask          = (ppc_result_b & 0x20) ? -1 : (1 << rot_sh) - 1;
    ppc_result_a           = (int32_t)ppc_result_d >> ((ppc_result_b & 0x20) ? 31 : rot_sh);
    ppc_state.spr[SPR::MQ] = ((ppc_result_d << rot_sh) | (ppc_result_d >> (32 - rot_sh)));

    if ((int32_t(ppc_result_d) < 0) && (ppc_result_d & mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sraq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sraq<RC1>(uint32_t instr);

POWEROPCODEREC (sre,
    ppc_grab_regssab(instr);

    unsigned rot_sh = ppc_result_b & 0x1F;
    ppc_result_a    = ppc_result_d >> rot_sh;

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sre<RC0>(uint32_t instr);
template void dppc_interpreter::power_sre<RC1>(uint32_t instr);

POWEROPCODEREC (srea,
    ppc_grab_regssab(instr);
    unsigned rot_sh        = ppc_result_b & 0x1F;
    ppc_result_a           = (int32_t)ppc_result_d >> rot_sh;
    uint32_t r             = ((ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh)));
    uint32_t mask          = -1U >> rot_sh;

    if ((int32_t(ppc_result_d) < 0) && (r & ~mask)) {
        ppc_state.spr[SPR::XER] |= XER::CA;
    } else {
        ppc_state.spr[SPR::XER] &= ~XER::CA;
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
    ppc_state.spr[SPR::MQ] = r;
)

template void dppc_interpreter::power_srea<RC0>(uint32_t instr);
template void dppc_interpreter::power_srea<RC1>(uint32_t instr);

POWEROPCODEREC (sreq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t mask   = -1U >> rot_sh;

    ppc_result_a           = (ppc_result_d >> rot_sh) | (ppc_state.spr[SPR::MQ] & ~mask);
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sreq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sreq<RC1>(uint32_t instr);

POWEROPCODEREC (sriq,
    ppc_grab_regssash(instr);
    ppc_result_a           = ppc_result_d >> rot_sh;
    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_sriq<RC0>(uint32_t instr);
template void dppc_interpreter::power_sriq<RC1>(uint32_t instr);

POWEROPCODEREC (srliq,
    ppc_grab_regssash(instr);
    uint32_t r      = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    ppc_result_a           = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    ppc_state.spr[SPR::MQ] = r;

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_srliq<RC0>(uint32_t instr);
template void dppc_interpreter::power_srliq<RC1>(uint32_t instr);

POWEROPCODEREC (srlq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;
    uint32_t r      = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));
    unsigned mask   = power_rot_mask(rot_sh, 31);

    if (ppc_result_b & 0x20) {
        ppc_result_a = (ppc_state.spr[SPR::MQ] & mask);
    } else {
        ppc_result_a = ((r & mask) | (ppc_state.spr[SPR::MQ] & ~mask));
    }

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_srlq<RC0>(uint32_t instr);
template void dppc_interpreter::power_srlq<RC1>(uint32_t instr);

POWEROPCODEREC (srq,
    ppc_grab_regssab(instr);
    unsigned rot_sh = ppc_result_b & 0x1F;

    if (ppc_result_b & 0x20) {
        ppc_result_a = 0;
    } else {
        ppc_result_a = ppc_result_d >> rot_sh;
    }

    ppc_state.spr[SPR::MQ] = (ppc_result_d >> rot_sh) | (ppc_result_d << (32 - rot_sh));

    if (rec)
        ppc_changecrf0(ppc_result_a);

    ppc_state.gpr[reg_a] = ppc_result_a;
)

template void dppc_interpreter::power_srq<RC0>(uint32_t instr);
template void dppc_interpreter::power_srq<RC1>(uint32_t instr);
