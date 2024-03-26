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

inline void ppc_update_cr1() {
    // copy FPSCR[FX|FEX|VX|OX] to CR1
    ppc_state.cr = (ppc_state.cr & ~CR_select::CR1_field) |
                   ((ppc_state.fpscr >> 4) & CR_select::CR1_field);
}

int32_t round_to_nearest(double f) {
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

int32_t round_to_zero(double f) {
    return static_cast<int32_t>(std::trunc(f));
}

int32_t round_to_pos_inf(double f) {
    return static_cast<int32_t>(std::ceil(f));
}

int32_t round_to_neg_inf(double f) {
    return static_cast<int32_t>(std::floor(f));
}

void update_fex() {
    int fex_result  = !!((ppc_state.fpscr & (ppc_state.fpscr << 22)) & 0x3E000000);
    ppc_state.fpscr = (ppc_state.fpscr & ~0x40000000) | (fex_result << 30);
}

template <const FPOP fpop>
void ppc_confirm_inf_nan(int chosen_reg_1, int chosen_reg_2, field_rc rec = RC0) {
    double input_a = ppc_state.fpr[chosen_reg_1].dbl64_r;
    double input_b = ppc_state.fpr[chosen_reg_2].dbl64_r;

    ppc_state.fpscr &= 0x7fbfffff;

    switch (fpop) {
        case FPOP::DIV:
            if (std::isinf(input_a) && std::isinf(input_b)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXIDI);
            } else if ((input_a == FP_ZERO) && (input_b == FP_ZERO)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXZDZ);
            }
            update_fex();
            break;
        case FPOP::SUB:
            if (std::isinf(input_a) && std::isinf(input_b)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXISI);
            }
            if (std::isnan(input_a) && std::isnan(input_b)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXISI);
            }
            update_fex();
            break;
        case FPOP::ADD:
            if (std::isnan(input_a) && std::isnan(input_b)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXISI);
            }
            update_fex();
            break;
        case FPOP::SQRT:
            if (std::isnan(input_b) || (input_b == -1.0)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXSQRT);
            }
            update_fex();
            break;
        case FPOP::MUL:
            if (std::isnan(input_a) && std::isnan(input_b)) {
                ppc_state.fpscr |= (FPSCR::FX);
            }

            update_fex();
            break;
        }
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

// Floating Point Arithmetic
template <field_rc rec>
void dppc_interpreter::ppc_fadd() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_state.fpscr |= FPCC_FUNAN;
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = val_reg_a + val_reg_b;
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fadd<RC0>();
template void dppc_interpreter::ppc_fadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsub() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_state.fpscr |= FPCC_FUNAN;
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = val_reg_a - val_reg_b;
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsub<RC0>();
template void dppc_interpreter::ppc_fsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fdiv() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<DIV>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = val_reg_a / val_reg_b;
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fdiv<RC0>();
template void dppc_interpreter::ppc_fdiv<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmul() {
    ppc_grab_regsfpdac(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }

    double ppc_dblresult64_d = val_reg_a * val_reg_c;
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmul<RC0>();
template void dppc_interpreter::ppc_fmul<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmadd() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmadd<RC0>();
template void dppc_interpreter::ppc_fmadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmsub() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmsub<RC0>();
template void dppc_interpreter::ppc_fmsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmadd() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = -std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmadd<RC0>();
template void dppc_interpreter::ppc_fnmadd<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmsub() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = std::fma(-val_reg_a, val_reg_c, val_reg_b);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmsub<RC0>();
template void dppc_interpreter::ppc_fnmsub<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fadds() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    float ppc_fltresult32_d = val_reg_a + val_reg_b;
    double ppc_dblresult64_d = (double)ppc_fltresult32_d;
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);

    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fadds<RC0>();
template void dppc_interpreter::ppc_fadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsubs() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = (float)(val_reg_a - val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsubs<RC0>();
template void dppc_interpreter::ppc_fsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fdivs() {
    ppc_grab_regsfpdab(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<DIV>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = (float)(val_reg_a / val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fdivs<RC0>();
template void dppc_interpreter::ppc_fdivs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmuls() {
    ppc_grab_regsfpdac(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }

    double ppc_dblresult64_d = (float)(val_reg_a * val_reg_c);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmuls<RC0>();
template void dppc_interpreter::ppc_fmuls<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmadds() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = (float)std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmadds<RC0>();
template void dppc_interpreter::ppc_fmadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fmsubs() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = (float)std::fma(val_reg_a, val_reg_c, -val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmsubs<RC0>();
template void dppc_interpreter::ppc_fmsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmadds() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<ADD>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = -(float)std::fma(val_reg_a, val_reg_c, val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmadds<RC0>();
template void dppc_interpreter::ppc_fnmadds<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnmsubs() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    if (std::isnan(val_reg_a) || std::isnan(val_reg_c)) {
        ppc_confirm_inf_nan<MUL>(reg_a, reg_c, rec);
    }
    if (std::isnan(val_reg_b)) {
        ppc_confirm_inf_nan<SUB>(reg_a, reg_b, rec);
    }

    double ppc_dblresult64_d = (float)std::fma(-val_reg_a, val_reg_c, val_reg_b);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    fpresult_update(ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnmsubs<RC0>();
template void dppc_interpreter::ppc_fnmsubs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fabs() {
    ppc_grab_regsfpdb(ppc_cur_instruction);

    double ppc_dblresult64_d = abs(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fabs<RC0>();
template void dppc_interpreter::ppc_fabs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fnabs() {
    ppc_grab_regsfpdb(ppc_cur_instruction);

    double ppc_dblresult64_d = abs(GET_FPR(reg_b));
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fnabs<RC0>();
template void dppc_interpreter::ppc_fnabs<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fneg() {
    ppc_grab_regsfpdb(ppc_cur_instruction);

    double ppc_dblresult64_d = -(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fneg<RC0>();
template void dppc_interpreter::ppc_fneg<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsel() {
    ppc_grab_regsfpdabc(ppc_cur_instruction);

    double ppc_dblresult64_d = (val_reg_a >= -0.0) ? val_reg_c : val_reg_b;

    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsel<RC0>();
template void dppc_interpreter::ppc_fsel<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsqrt() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double testd2 = (double)(GET_FPR(reg_b));
    double ppc_dblresult64_d = std::sqrt(testd2);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);
    ppc_confirm_inf_nan<SQRT>(0, reg_b, rec);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsqrt<RC0>();
template void dppc_interpreter::ppc_fsqrt<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fsqrts() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double testd2     = (double)(GET_FPR(reg_b));
    double ppc_dblresult64_d = (float)std::sqrt(testd2);
    ppc_store_sfpresult_flt(reg_d, ppc_dblresult64_d);
    ppc_confirm_inf_nan<SQRT>(0, reg_b, rec);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fsqrts<RC0>();
template void dppc_interpreter::ppc_fsqrts<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_frsqrte() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double testd2 = (double)(GET_FPR(reg_b));

    double ppc_dblresult64_d = 1.0 / sqrt(testd2);
    ppc_confirm_inf_nan<SQRT>(0, reg_b, rec);

    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_frsqrte<RC0>();
template void dppc_interpreter::ppc_frsqrte<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_frsp() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double ppc_dblresult64_d = (float)(GET_FPR(reg_b));
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_frsp<RC0>();
template void dppc_interpreter::ppc_frsp<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_fres() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double start_num = GET_FPR(reg_b);
    double ppc_dblresult64_d = (float)(1.0 / start_num);
    ppc_store_dfpresult_flt(reg_d, ppc_dblresult64_d);

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
    ppc_grab_regsfpdb(ppc_cur_instruction);
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpscr &= ~(FPSCR::FR | FPSCR::FI);
        ppc_state.fpscr |= (FPSCR::VXCVI | FPSCR::VX);

        if (!(ppc_state.fpr[reg_b].int64_r & 0x0008000000000000)) // issnan
            ppc_state.fpscr |= FPSCR::VXSNAN;

        if (ppc_state.fpscr & FPSCR::VE) {
            ppc_state.fpscr |= FPSCR::FEX; // VX=1 and VE=1 cause FEX to be set
            ppc_floating_point_exception();
        } else {
            ppc_state.fpr[reg_d].int64_r = 0xFFF8000080000000ULL;
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
    ppc_grab_regsfpdia(ppc_cur_instruction);
    ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    uint32_t result = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_state.fpr[reg_d].dbl64_r = *(float*)(&result);
}

void dppc_interpreter::ppc_lfsu() {
    ppc_grab_regsfpdia(ppc_cur_instruction);
    if (reg_a) {
        ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
        ppc_effective_address += (reg_a) ? val_reg_a : 0;
        uint32_t result = mmu_read_vmem<uint32_t>(ppc_effective_address);
        ppc_state.fpr[reg_d].dbl64_r = *(float*)(&result);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfsx() {
    ppc_grab_regsfpdiab(ppc_cur_instruction);
    ppc_effective_address = val_reg_b + (reg_a ? val_reg_a : 0);
    uint32_t result       = mmu_read_vmem<uint32_t>(ppc_effective_address);
    ppc_state.fpr[reg_d].dbl64_r = *(float*)(&result);
}

void dppc_interpreter::ppc_lfsux() {
    ppc_grab_regsfpdiab(ppc_cur_instruction);
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        uint32_t result = mmu_read_vmem<uint32_t>(ppc_effective_address);
        ppc_state.fpr[reg_d].dbl64_r = *(float*)(&result);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfd() {
    ppc_grab_regsfpdia(ppc_cur_instruction);
    ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d, ppc_result64_d);
}

void dppc_interpreter::ppc_lfdu() {
    ppc_grab_regsfpdia(ppc_cur_instruction);
    if (reg_a != 0) {
        ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
        ppc_effective_address += val_reg_a;
        uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
        ppc_store_dfpresult_int(reg_d, ppc_result64_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfdx() {
    ppc_grab_regsfpdiab(ppc_cur_instruction);
    ppc_effective_address   = val_reg_b + (reg_a ? val_reg_a : 0);
    uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d, ppc_result64_d);
}

void dppc_interpreter::ppc_lfdux() {
    ppc_grab_regsfpdiab(ppc_cur_instruction);
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        uint64_t ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
        ppc_store_dfpresult_int(reg_d, ppc_result64_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfs() {
    ppc_grab_regsfpsia(ppc_cur_instruction);
    ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    float result = ppc_state.fpr[reg_s].dbl64_r;
    mmu_write_vmem<uint32_t>(ppc_effective_address, *(uint32_t*)(&result));
}

void dppc_interpreter::ppc_stfsu() {
    ppc_grab_regsfpsia(ppc_cur_instruction);
    if (reg_a != 0) {
        ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
        ppc_effective_address += val_reg_a;
        float result = ppc_state.fpr[reg_s].dbl64_r;
        mmu_write_vmem<uint32_t>(ppc_effective_address, *(uint32_t*)(&result));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfsx() {
    ppc_grab_regsfpsiab(ppc_cur_instruction);
    ppc_effective_address = val_reg_b + (reg_a ? val_reg_a : 0);
    float result = ppc_state.fpr[reg_s].dbl64_r;
    mmu_write_vmem<uint32_t>(ppc_effective_address, *(uint32_t*)(&result));
}

void dppc_interpreter::ppc_stfsux() {
    ppc_grab_regsfpsiab(ppc_cur_instruction);
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        float result = ppc_state.fpr[reg_s].dbl64_r;
        mmu_write_vmem<uint32_t>(ppc_effective_address, *(uint32_t*)(&result));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfd() {
    ppc_grab_regsfpsia(ppc_cur_instruction);
    ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
    ppc_effective_address += reg_a ? val_reg_a : 0;
    mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdu() {
    ppc_grab_regsfpsia(ppc_cur_instruction);
    if (reg_a != 0) {
        ppc_effective_address = int32_t(int16_t(ppc_cur_instruction));
        ppc_effective_address += val_reg_a;
        mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfdx() {
    ppc_grab_regsfpsiab(ppc_cur_instruction);
    ppc_effective_address = val_reg_b + (reg_a ? val_reg_a : 0);
    mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdux() {
    ppc_grab_regsfpsiab(ppc_cur_instruction);
    if (reg_a != 0) {
        ppc_effective_address = val_reg_a + val_reg_b;
        mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfiwx() {
    ppc_grab_regsfpsiab(ppc_cur_instruction);
    ppc_effective_address = val_reg_b + (reg_a ? val_reg_a : 0);
    mmu_write_vmem<uint32_t>(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
}

// Floating Point Register Transfer

template <field_rc rec>
void dppc_interpreter::ppc_fmr() {
    ppc_grab_regsfpdb(ppc_cur_instruction);
    ppc_state.fpr[reg_d].dbl64_r = ppc_state.fpr[reg_b].dbl64_r;

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_fmr<RC0>();
template void dppc_interpreter::ppc_fmr<RC1>();

template <field_601 for601, field_rc rec>
void dppc_interpreter::ppc_mffs() {
    int reg_d = (ppc_cur_instruction >> 21) & 31;

    ppc_state.fpr[reg_d].int64_r = uint64_t(ppc_state.fpscr) | (for601 ? 0xFFFFFFFF00000000ULL : 0xFFF8000000000000ULL);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mffs<NOT601, RC0>();
template void dppc_interpreter::ppc_mffs<NOT601, RC1>();
template void dppc_interpreter::ppc_mffs<IS601, RC0>();
template void dppc_interpreter::ppc_mffs<IS601, RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsf() {
    int reg_b  = (ppc_cur_instruction >> 11) & 0x1F;
    uint8_t fm = (ppc_cur_instruction >> 17) & 0xFF;

    uint32_t cr_mask = 0;

    if (fm == 0xFFU) // the fast case
        cr_mask = 0xFFFFFFFFUL;
    else { // the slow case
        if (fm & 0x80) cr_mask |= 0xF0000000UL;
        if (fm & 0x40) cr_mask |= 0x0F000000UL;
        if (fm & 0x20) cr_mask |= 0x00F00000UL;
        if (fm & 0x10) cr_mask |= 0x000F0000UL;
        if (fm & 0x08) cr_mask |= 0x0000F000UL;
        if (fm & 0x04) cr_mask |= 0x00000F00UL;
        if (fm & 0x02) cr_mask |= 0x000000F0UL;
        if (fm & 0x01) cr_mask |= 0x0000000FUL;
    }

    // ensure neither FEX nor VX will be changed
    cr_mask &= ~(FPSCR::FEX | FPSCR::VX);

    // copy FPR[reg_b] to FPSCR under control of cr_mask
    ppc_state.fpscr = (ppc_state.fpscr & ~cr_mask) | (ppc_state.fpr[reg_b].int64_r & cr_mask);

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsf<RC0>();
template void dppc_interpreter::ppc_mtfsf<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsfi() {
    int crf_d    = (ppc_cur_instruction >> 21) & 0x1C;
    uint32_t imm = (ppc_cur_instruction << 16) & 0xF0000000UL;

    // prepare field mask and ensure that neither FEX nor VX will be changed
    uint32_t mask = (0xF0000000UL >> crf_d) & ~(FPSCR::FEX | FPSCR::VX);

    // copy imm to FPSCR[crf_d] under control of the field mask
    ppc_state.fpscr = (ppc_state.fpscr & ~mask) | ((imm >> crf_d) & mask);

    // TODO: update FEX and VX according to the "usual rule"

    if (rec)
        ppc_update_cr1();
}

template void dppc_interpreter::ppc_mtfsfi<RC0>();
template void dppc_interpreter::ppc_mtfsfi<RC1>();

template <field_rc rec>
void dppc_interpreter::ppc_mtfsb0() {
    int crf_d = (ppc_cur_instruction >> 21) & 0x1F;
    if (!crf_d || (crf_d > 2)) { // FEX and VX can't be explicitely cleared
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
    if (!crf_d || (crf_d > 2)) { // FEX and VX can't be explicitely set
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
    ppc_grab_regsfpsab(ppc_cur_instruction);

    uint32_t cmp_c = 0;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        // TODO: test for SNAN operands
        // for now, assume that at least one of the operands is QNAN
        cmp_c |= CRx_bit::CR_SO;
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

    ppc_state.fpscr = (ppc_state.fpscr & ~FPSCR::FPCC_MASK) | (cmp_c >> 16); // update FPCC
    ppc_state.cr = ((ppc_state.cr & ~(0xF0000000 >> crf_d)) | (cmp_c >> crf_d));
}

void dppc_interpreter::ppc_fcmpu() {
    ppc_grab_regsfpsab(ppc_cur_instruction);

    uint32_t cmp_c = 0;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        // TODO: test for SNAN operands
        cmp_c |= CRx_bit::CR_SO;
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

    ppc_state.fpscr = (ppc_state.fpscr & ~FPSCR::FPCC_MASK) | (cmp_c >> 16); // update FPCC
    ppc_state.cr    = ((ppc_state.cr & ~(0xF0000000UL >> crf_d)) | (cmp_c >> crf_d));
}
