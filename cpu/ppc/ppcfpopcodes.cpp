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

// The floating point opcodes for the processor - ppcfpopcodes.cpp

#include "ppcemu.h"
#include "ppcdecodemacros.h"
#include "ppcmmu.h"
#include <stdlib.h>
#include <cfenv>
#include <cinttypes>
#include <cmath>
#include <cfloat>

// Storage and register retrieval functions for the floating point functions.

double fp_return_double(uint32_t reg) {
    return ppc_state.fpr[reg].dbl64_r;
}

uint64_t fp_return_uint64(uint32_t reg) {
    return ppc_state.fpr[reg].int64_r;
}

inline static void ppc_update_cr1() {
    // copy FPSCR[FX|FEX|VX|OX] to CR1
    ppc_state.cr = (ppc_state.cr & ~CR_select::CR1_field) |
                   ((ppc_state.fpscr >> 4) & CR_select::CR1_field);
}

static int32_t round_to_nearest(double f) {
    return static_cast<int32_t>(static_cast<int64_t> (std::floor(f + 0.5)));
}

void set_host_rounding_mode(uint8_t mode) {
    switch(mode & FPSCR::RN_MASK) {
    case 0:
        std::fesetround(FE_TONEAREST);
        break;
    case 1:
        std::fesetround(FE_TOWARDZERO);
        break;
    case 2:
        std::fesetround(FE_UPWARD);
        break;
    case 3:
        std::fesetround(FE_DOWNWARD);
        break;
    }
}

void update_fpscr(uint32_t new_fpscr) {
    if ((new_fpscr & FPSCR::RN_MASK) != (ppc_state.fpscr & FPSCR::RN_MASK))
        set_host_rounding_mode(new_fpscr & FPSCR::RN_MASK);

    ppc_state.fpscr = new_fpscr;
}

static int32_t round_to_zero(double f) {
    return static_cast<int32_t>(std::trunc(f));
}

static int32_t round_to_pos_inf(double f) {
    return static_cast<int32_t>(std::ceil(f));
}

static int32_t round_to_neg_inf(double f) {
    return static_cast<int32_t>(std::floor(f));
}

inline static bool check_snan(int check_reg) {
    uint64_t check_int = ppc_state.fpr[check_reg].int64_r;
    return (((check_int & (0x7FFULL << 52)) == (0x7FFULL << 52)) &&
        ((check_int & ~(0xFFFULL << 52)) != 0ULL) &&
        ((check_int & (0x1ULL << 51)) == 0ULL));
}

inline static void snan_single_check(int reg_a) {
    if (check_snan(reg_a))
        ppc_state.fpscr |= FX | VX | VXSNAN;
}

inline static void snan_double_check(int reg_a, int reg_b) {
    if (check_snan(reg_a) || check_snan(reg_b))
        ppc_state.fpscr |= FX | VX | VXSNAN;
}

inline static void max_double_check(double value_a, double value_b) {
    if (((value_a == DBL_MAX) && (value_b == DBL_MAX)) ||
        ((value_a == -DBL_MAX) && (value_b == -DBL_MAX)))
        ppc_state.fpscr |= FX | OX | XX | FI;
}

inline static bool check_qnan(int check_reg) {
    uint64_t check_int = ppc_state.fpr[check_reg].int64_r;
    return (((check_int & (0x7FFULL << 52)) == (0x7FFULL << 52)) &&
        ((check_int & ~(0xFFFULL << 52)) == 0ULL) &&
        ((check_int & (0x1ULL << 51)) == (0x1ULL << 51)));
}

inline static void fpresult_update(double set_result) {
    if (std::isnan(set_result)) {
        ppc_state.fpscr |= FPCC_FUNAN | FPRCD;
    } else {
        if (set_result > 0.0) {
            ppc_state.fpscr |= FPCC_POS;
        } else if (set_result < 0.0) {
            ppc_state.fpscr |= FPCC_NEG;
        } else {
            ppc_state.fpscr |= FPCC_ZERO;
        }

        if (std::fetestexcept(FE_INEXACT)) {
            ppc_state.fpscr |= XX;
        }

        if (std::isinf(set_result))
            ppc_state.fpscr |= FPCC_FUNAN;
    }
}

inline static void ppc_update_vx() {
    uint32_t fpscr_check = ppc_state.fpscr & 0x1F80700U;
    if (fpscr_check)
        ppc_state.fpscr |= VX;
    else
        ppc_state.fpscr &= ~VX;
}

inline static void ppc_update_fex() {
    uint32_t fpscr_check = (((ppc_state.fpscr >> 25) & 0x1F) &
        ((ppc_state.fpscr >> 3) & 0x1F));

    if (fpscr_check)
        ppc_state.fpscr |= VX;
    else
        ppc_state.fpscr &= ~VX;
}

inline static void round_to_int(uint32_t instr, const uint8_t mode, field_rc rec) {
    ppc_grab_regsfpdb(instr);
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpscr &= ~(FPSCR::FR | FPSCR::FI);
        ppc_state.fpscr |= (FPSCR::VXCVI | FPSCR::VX);

        if (check_snan(reg_b))    // issnan
            ppc_state.fpscr |= FPSCR::VXSNAN;

        if (ppc_state.fpscr & FPSCR::VE) {
            ppc_state.fpscr |= FPSCR::FEX;    // VX=1 and VE=1 cause FEX to be set
            ppc_floating_point_exception(instr);
        } else {
            ppc_state.fpr[reg_d].int64_r = 0xFFF8000080000000ULL;
        }
    } else if (val_reg_b > static_cast<double>(0x7fffffff) || val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpscr &= ~(FPSCR::FR | FPSCR::FI);
        ppc_state.fpscr |= (FPSCR::VXCVI | FPSCR::VX);

        if (ppc_state.fpscr & FPSCR::VE) {
            ppc_state.fpscr |= FPSCR::FEX;    // VX=1 and VE=1 cause FEX to be set
            ppc_floating_point_exception(instr);
        } else {
            if (val_reg_b >= 0.0f)
                ppc_state.fpr[reg_d].int64_r = 0xFFF800007FFFFFFFULL;
            else
                ppc_state.fpr[reg_d].int64_r = 0xFFF8000080000000ULL;
        }
    } else {
        uint64_t ppc_result64_d;
        switch (mode & 0x3) {
        case 0:
            ppc_result64_d = uint32_t(round_to_nearest(val_reg_b));
            break;
        case 1:
            ppc_result64_d = uint32_t(round_to_zero(val_reg_b));
            break;
        case 2:
            ppc_result64_d = uint32_t(round_to_pos_inf(val_reg_b));
            break;
        case 3:
            ppc_result64_d = uint32_t(round_to_neg_inf(val_reg_b));
            break;
        }

        ppc_result64_d |= 0xFFF8000000000000ULL;

        ppc_store_dfpresult_int(reg_d, ppc_result64_d);
    }

    if (rec)
        ppc_update_cr1();
}

#include "ppcopmacros.h"
#include "ppcfpopcodes.include"
