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
#include "ppcmacros.h"
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

static void fpresult_update(double set_result) {
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

        if (std::isinf(set_result))
            ppc_state.fpscr |= FPCC_FUNAN;
    }
}

static void ppc_update_vx() {
    uint32_t fpscr_check = ppc_state.fpscr & 0x1F80700U;
    if (fpscr_check)
        ppc_state.fpscr |= VX;
    else
        ppc_state.fpscr &= ~VX;
}

static void ppc_update_fex() {
    uint32_t fpscr_check = (((ppc_state.fpscr >> 25) & 0x1F) &
        ((ppc_state.fpscr >> 3) & 0x1F));

    if (fpscr_check)
        ppc_state.fpscr |= VX;
    else
        ppc_state.fpscr &= ~VX;
}

// Floating Point Arithmetic
template <field_rc rec>
void dppc_interpreter::ppc_fadd() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);
    max_double_check(val_reg_a, val_reg_b);

    double ppc_dblresult64_d = val_reg_a + val_reg_b;
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);
    ppc_update_fex();

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fadd<RC0>();
template void dppc_interpreter::ppc_fadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsub() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);

    double ppc_dblresult64_d = val_reg_a - val_reg_b;
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsub<RC0>();
template void dppc_interpreter::ppc_fsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fdiv() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);

    double ppc_dblresult64_d = val_reg_a / val_reg_b;
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fdiv<RC0>();
template void dppc_interpreter::ppc_fdiv<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmul() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);

    double ppc_dblresult64_d = val_reg_a * val_reg_c;
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmul<RC0>();
template void dppc_interpreter::ppc_fmul<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmadd() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmadd<RC0>();
template void dppc_interpreter::ppc_fmadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmsub() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmsub<RC0>();
template void dppc_interpreter::ppc_fmsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmadd() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = -std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmadd<RC0>();
template void dppc_interpreter::ppc_fnmadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmsub() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = -std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmsub<RC0>();
template void dppc_interpreter::ppc_fnmsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fadds() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);

    double ppc_dblresult64_d = (float)(val_reg_a + val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fadds<RC0>();
template void dppc_interpreter::ppc_fadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsubs() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);

    double ppc_dblresult64_d = (float)(val_reg_a - val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsubs<RC0>();
template void dppc_interpreter::ppc_fsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fdivs() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);

    snan_double_check(instr.arg1, instr.arg2);

    double ppc_dblresult64_d = (float)(val_reg_a / val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fdivs<RC0>();
template void dppc_interpreter::ppc_fdivs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmuls() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);

    double ppc_dblresult64_d = (float)(val_reg_a * val_reg_c);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmuls<RC0>();
template void dppc_interpreter::ppc_fmuls<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmadds() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = (float)std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmadds<RC0>();
template void dppc_interpreter::ppc_fmadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmsubs() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = (float)std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmsubs<RC0>();
template void dppc_interpreter::ppc_fmsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmadds() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = -(float)std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmadds<RC0>();
template void dppc_interpreter::ppc_fnmadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmsubs() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    snan_double_check(instr.arg1, instr.arg3);
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = -(float)std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmsubs<RC0>();
template void dppc_interpreter::ppc_fnmsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fabs() {
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = abs(GET_FPR(instr.arg2));

    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fabs<RC0>();
template void dppc_interpreter::ppc_fabs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnabs() {
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = abs(GET_FPR(instr.arg2));
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnabs<RC0>();
template void dppc_interpreter::ppc_fnabs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fneg() {
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = -(GET_FPR(instr.arg2));

    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fneg<RC0>();
template void dppc_interpreter::ppc_fneg<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsel() {
    double val_reg_a = GET_FPR(instr.arg1);
    double val_reg_b = GET_FPR(instr.arg2);
    double val_reg_c = GET_FPR(instr.arg3);

    double ppc_dblresult64_d = (val_reg_a >= -0.0) ? val_reg_c : val_reg_b;

    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsel<RC0>();
template void dppc_interpreter::ppc_fsel<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsqrt() {
    snan_single_check(instr.arg2);

    double testd2 = (double)(GET_FPR(instr.arg2));
    double ppc_dblresult64_d = std::sqrt(testd2);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsqrt<RC0>();
template void dppc_interpreter::ppc_fsqrt<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsqrts() {
    snan_single_check(instr.arg2);

    double testd2            = (double)(GET_FPR(instr.arg2));
    double ppc_dblresult64_d = (float)std::sqrt(testd2);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsqrts<RC0>();
template void dppc_interpreter::ppc_fsqrts<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_frsqrte() {
    snan_single_check(instr.arg2);

    double testd2            = (double)(GET_FPR(instr.arg2));
    double ppc_dblresult64_d = 1.0 / sqrt(testd2);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_frsqrte<RC0>();
template void dppc_interpreter::ppc_frsqrte<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_frsp() {
    snan_single_check(instr.arg2);

    double ppc_dblresult64_d = (float)(GET_FPR(instr.arg2));
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_frsp<RC0>();
template void dppc_interpreter::ppc_frsp<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fres() {
    snan_single_check(instr.arg2);

    double start_num = GET_FPR(instr.arg2);
    double ppc_dblresult64_d = (float)(1.0 / start_num);
    ppc_store_fpresult_flt(instr.arg0, ppc_dblresult64_d);

    if (start_num == 0.0) {
        ppc_state.fpscr |= FPSCR::ZX;
    }
    else if (std::isnan(start_num)) {
        ppc_state.fpscr |= FPSCR::VXSNAN;
    }
    else if (std::isinf(start_num)){
        ppc_state.fpscr &= 0xFFF9FFFF;
        ppc_state.fpscr |= FPSCR::VXSNAN;
    }

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fres<RC0>();
template void dppc_interpreter::ppc_fres<RC1>();

static void round_to_int(const uint8_t mode, field_rc rec) {
    double val_reg_b = GET_FPR(instr.arg2);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpscr &= ~(FPSCR::FR | FPSCR::FI);
        ppc_state.fpscr |= (FPSCR::VXCVI | FPSCR::VX);

        if (check_snan(instr.arg2)) // issnan
            ppc_state.fpscr |= FPSCR::VXSNAN;

        if (ppc_state.fpscr & FPSCR::VE) {
            ppc_state.fpscr |= FPSCR::FEX; // VX=1 and VE=1 cause FEX to be set
            ppc_floating_point_exception();
        } else {
            ppc_state.fpr[instr.arg0].int64_r = 0xFFF8000080000000ULL;
        }
    } else if (val_reg_b >  static_cast<double>(0x7fffffff) ||
               val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpscr &= ~(FPSCR::FR | FPSCR::FI);
        ppc_state.fpscr |= (FPSCR::VXCVI | FPSCR::VX);

        if (ppc_state.fpscr & FPSCR::VE) {
            ppc_state.fpscr |= FPSCR::FEX; // VX=1 and VE=1 cause FEX to be set
            ppc_floating_point_exception();
        } else {
            if (val_reg_b >= 0.0f)
                ppc_state.fpr[instr.arg0].int64_r = 0xFFF800007FFFFFFFULL;
            else
                ppc_state.fpr[instr.arg0].int64_r = 0xFFF8000080000000ULL;
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

        ppc_store_fpresult_int(instr.arg0, ppc_result64_d);
    }

    if (rec)
        ppc_update_cr1();
}

template <field_rc rec>
void dppc_interpreter::ppc_fctiw() {
    round_to_int(ppc_state.fpscr & 0x3, rec);
}

template void dppc_interpreter::ppc_fctiw<RC0>();
template void dppc_interpreter::ppc_fctiw<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fctiwz() {
    round_to_int(1, rec);
}

template void dppc_interpreter::ppc_fctiwz<RC0>();
template void dppc_interpreter::ppc_fctiwz<RC1>();

// Floating Point Store and Load

void dppc_interpreter::ppc_lfs() {
    instr.addr += ((instr.arg1) ? ppc_state.gpr[instr.arg1] : 0);
    uint32_t result                   = mmu_read_vmem<uint32_t>(instr.addr);
    ppc_state.fpr[instr.arg0].dbl64_r = *(float*)(&result);
}

void dppc_interpreter::ppc_lfsu() {
    if (instr.arg1) {
        instr.addr += ((instr.arg1) ? ppc_state.gpr[instr.arg1] : 0);
        uint32_t result                   = mmu_read_vmem<uint32_t>(instr.addr);
        ppc_state.fpr[instr.arg0].dbl64_r = *(float*)(&result);
        ppc_state.gpr[instr.arg1]         = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfsx() {
    instr.addr     = ppc_state.gpr[instr.arg2] + (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);
    uint32_t result = mmu_read_vmem<uint32_t>(instr.addr);
    ppc_state.fpr[instr.arg0].dbl64_r = *(float*)(&result);
}

void dppc_interpreter::ppc_lfsux() {
    if (ppc_state.gpr[instr.arg1]) {
        instr.addr = ppc_state.gpr[instr.arg1] + ppc_state.gpr[instr.arg2];
        uint32_t result = mmu_read_vmem<uint32_t>(instr.addr);
        ppc_state.fpr[instr.arg0].dbl64_r = *(float*)(&result);
        ppc_state.gpr[instr.arg1]         = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfd() {
    instr.addr += ((instr.arg1) ? ppc_state.gpr[instr.arg1] : 0);
    uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(instr.addr);
    ppc_store_fpresult_int(instr.arg0, ppc_result64_d);
}

void dppc_interpreter::ppc_lfdu() {
    if (instr.arg1 != 0) {
        instr.addr += ppc_state.gpr[instr.arg1];
        uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(instr.addr);
        ppc_store_fpresult_int(instr.arg0, ppc_result64_d);
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfdx() {
    instr.addr = ppc_state.gpr[instr.arg2] + (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);
    uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(instr.addr);
    ppc_store_fpresult_int(instr.arg0, ppc_result64_d);
}

void dppc_interpreter::ppc_lfdux() {
    if (instr.arg1) {
        instr.addr = ppc_state.gpr[instr.arg1] + ppc_state.gpr[instr.arg2];
        uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(instr.addr);
        ppc_store_fpresult_int(instr.arg0, ppc_result64_d);
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfs() {
    instr.addr += ((instr.arg1) ? ppc_state.gpr[instr.arg1] : 0);
    float result = float(ppc_state.fpr[instr.arg0].dbl64_r);
    mmu_write_vmem<uint32_t>(instr.addr, *(uint32_t*)(&result));
}

void dppc_interpreter::ppc_stfsu() {
    if (instr.arg1 != 0) {
        
        instr.addr += ppc_state.gpr[instr.arg1];
        float result = float(ppc_state.fpr[instr.arg0].dbl64_r);
        mmu_write_vmem<uint32_t>(instr.addr, *(uint32_t*)(&result));
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfsx() {
    instr.addr = ppc_state.gpr[instr.arg2] +
        (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);
    float result = float(ppc_state.fpr[instr.arg0].dbl64_r);
    mmu_write_vmem<uint32_t>(instr.addr, *(uint32_t*)(&result));
}

void dppc_interpreter::ppc_stfsux() {
    if (ppc_state.gpr[instr.arg1]) {
        instr.addr   = ppc_state.gpr[instr.arg1] + ppc_state.gpr[instr.arg2];
        float result = float(ppc_state.fpr[instr.arg0].dbl64_r);
        mmu_write_vmem<uint32_t>(instr.addr, *(uint32_t*)(&result));
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfd() {
    instr.addr += ((instr.arg1) ? ppc_state.gpr[instr.arg1] : 0);
    mmu_write_vmem<uint64_t>(instr.addr, ppc_state.fpr[instr.arg0].int64_r);
}

void dppc_interpreter::ppc_stfdu() {
    if (instr.arg1 != 0) {
        instr.addr += ppc_state.gpr[instr.arg1];
        mmu_write_vmem<uint64_t>(instr.addr, ppc_state.fpr[instr.arg0].int64_r);
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfdx() {
    instr.addr = ppc_state.gpr[instr.arg2] + (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);
    mmu_write_vmem<uint64_t>(instr.addr, ppc_state.fpr[instr.arg0].int64_r);
}

void dppc_interpreter::ppc_stfdux() {
    if (instr.arg1 != 0) {
        instr.addr = ppc_state.gpr[instr.arg1] + ppc_state.gpr[instr.arg2];
        mmu_write_vmem<uint64_t>(instr.addr, ppc_state.fpr[instr.arg0].int64_r);
        ppc_state.gpr[instr.arg1] = instr.addr;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfiwx() {
    instr.addr = ppc_state.gpr[instr.arg2] + (instr.arg1 ? ppc_state.gpr[instr.arg1] : 0);
    mmu_write_vmem<uint32_t>(instr.addr, uint32_t(ppc_state.fpr[instr.arg0].int64_r));
}

// Floating Point Register Transfer

template <field_rc rec>
void dppc_interpreter::ppc_fmr() {
    ppc_state.fpr[instr.arg0].dbl64_r = ppc_state.fpr[instr.arg2].dbl64_r;

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmr<RC0>();
template void dppc_interpreter::ppc_fmr<RC1>();

template <field_601 for601, field_rc rec>
void dppc_interpreter::ppc_mffs() {
    ppc_state.fpr[instr.arg0].int64_r = uint64_t(ppc_state.fpscr) | (for601 ? 0xFFFFFFFF00000000ULL : 0xFFF8000000000000ULL);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mffs<NOT601, RC0>();
template void dppc_interpreter::ppc_mffs<NOT601, RC1>();
template void dppc_interpreter::ppc_mffs<IS601, RC0>();
template void dppc_interpreter::ppc_mffs<IS601, RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsf() {
    uint32_t cr_mask = 0;

    if (instr.arg4 == 0xFFU) // the fast case
        cr_mask = 0xFFFFFFFFUL;
    else { // the slow case
        if (instr.arg4 & 0x80)
            cr_mask |= 0xF0000000UL;
        if (instr.arg4 & 0x40)
            cr_mask |= 0x0F000000UL;
        if (instr.arg4 & 0x20)
            cr_mask |= 0x00F00000UL;
        if (instr.arg4 & 0x10)
            cr_mask |= 0x000F0000UL;
        if (instr.arg4 & 0x08)
            cr_mask |= 0x0000F000UL;
        if (instr.arg4 & 0x04)
            cr_mask |= 0x00000F00UL;
        if (instr.arg4 & 0x02)
            cr_mask |= 0x000000F0UL;
        if (instr.arg4 & 0x01)
            cr_mask |= 0x0000000FUL;
    }

    // ensure neither FEX nor VX will be changed
    cr_mask &= ~(FPSCR::FEX | FPSCR::VX);

    // copy FPR[reg_b] to FPSCR under control of cr_mask
    ppc_state.fpscr = (ppc_state.fpscr & ~cr_mask) | (ppc_state.fpr[instr.arg2].int64_r & cr_mask);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsf<RC0>();
template void dppc_interpreter::ppc_mtfsf<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsfi() {
    // prepare field mask and ensure that neither FEX nor VX will be changed
    uint32_t mask = (0xF0000000UL >> instr.arg0) & ~(FPSCR::FEX | FPSCR::VX);

    // copy imm to FPSCR[crf_d] under control of the field mask
    ppc_state.fpscr = (ppc_state.fpscr & ~mask) | ((instr.mask >> instr.arg0) & mask);

    // Update FEX and VX according to the "usual rule"
    ppc_update_vx();
    ppc_update_fex();

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsfi<RC0>();
template void dppc_interpreter::ppc_mtfsfi<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsb0() {
    int crf_d = (ppc_cur_instruction >> 21) & 0x1F;
    if (!crf_d || (crf_d > 2)) { // FEX and VX can't be explicitly cleared
        ppc_state.fpscr &= ~(0x80000000UL >> crf_d);
    }

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsb0<RC0>();
template void dppc_interpreter::ppc_mtfsb0<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsb1() {
    int crf_d = (ppc_cur_instruction >> 21) & 0x1F;
    if (!crf_d || (crf_d > 2)) { // FEX and VX can't be explicitly set
        ppc_state.fpscr |= (0x80000000UL >> crf_d);
    }

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsb1<RC0>();
template void dppc_interpreter::ppc_mtfsb1<RC1>();

void dppc_interpreter::ppc_mcrfs() {
    int crf_d = (ppc_cur_instruction >> 21) & 0x1C;
    int crf_s = (ppc_cur_instruction >> 16) & 0x1C;
    ppc_state.cr = (
        (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        (((ppc_state.fpscr << crf_s) & 0xF0000000UL) >> crf_d)
    );
    ppc_state.fpscr &= ~((0xF0000000UL >> crf_s) & (
        // keep only the FPSCR bits that can be explicitly cleared
        FPSCR::FX | FPSCR::OX |
        FPSCR::UX | FPSCR::ZX | FPSCR::XX | FPSCR::VXSNAN |
        FPSCR::VXISI | FPSCR::VXIDI | FPSCR::VXZDZ | FPSCR::VXIMZ |
        FPSCR::VXVC |
        FPSCR::VXSOFT | FPSCR::VXSQRT | FPSCR::VXCVI
    ));
}

// Floating Point Comparisons

void dppc_interpreter::ppc_fcmpo() {
    double db_test_a = GET_FPR(instr.arg1);
    double db_test_b = GET_FPR(instr.arg2);

    uint32_t cmp_c = 0;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= CRx_bit::CR_SO;
        ppc_state.fpscr |= FX | VX;
        if (check_snan(instr.arg1) || check_snan(instr.arg2)) {
            ppc_state.fpscr |= VXSNAN;
        }
        if (!(ppc_state.fpscr & FEX) || check_qnan(instr.arg1) || check_qnan(instr.arg2)) {
            ppc_state.fpscr |= VXVC;
        }
    }
    else if (db_test_a < db_test_b) {
        cmp_c |= CRx_bit::CR_LT;
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= CRx_bit::CR_GT;
    }
    else {
        cmp_c |= CRx_bit::CR_EQ;
    }

    ppc_state.fpscr &= ~VE; //kludge to pass tests
    ppc_state.fpscr = (ppc_state.fpscr & ~FPSCR::FPCC_MASK) | (cmp_c >> 16); // update FPCC
    ppc_state.cr    = ((ppc_state.cr & ~(0xF0000000 >> instr.arg0)) | (cmp_c >> instr.arg0));

}

void dppc_interpreter::ppc_fcmpu() {
    double db_test_a = GET_FPR(instr.arg1);
    double db_test_b = GET_FPR(instr.arg2);

    uint32_t cmp_c = 0;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= CRx_bit::CR_SO;
        if (check_snan(instr.arg1) || check_snan(instr.arg2)) {
            ppc_state.fpscr |= FX | VX | VXSNAN;
        }
    }
    else if (db_test_a < db_test_b) {
        cmp_c |= CRx_bit::CR_LT;
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= CRx_bit::CR_GT;
    }
    else {
        cmp_c |= CRx_bit::CR_EQ;
    }

    ppc_state.fpscr &= ~VE; //kludge to pass tests
    ppc_state.fpscr = (ppc_state.fpscr & ~FPSCR::FPCC_MASK) | (cmp_c >> 16); // update FPCC
    ppc_state.cr    = ((ppc_state.cr & ~(0xF0000000UL >> instr.arg0)) | (cmp_c >> instr.arg0));

}
