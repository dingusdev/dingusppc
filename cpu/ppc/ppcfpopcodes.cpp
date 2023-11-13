/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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
#include "ppcmmu.h"
#include <stdlib.h>
#include <cinttypes>
#include <cmath>
#include <limits>

// Used for FP calcs
uint64_t ppc_result64_b;
uint64_t ppc_result64_d;

double ppc_dblresult64_d;

double snan = std::numeric_limits<double>::signaling_NaN();
double qnan = std::numeric_limits<double>::quiet_NaN();

// Storage and register retrieval functions for the floating point functions.

#define GET_FPR(reg)    ppc_state.fpr[(reg)].dbl64_r

double fp_return_double(uint32_t reg) {
    return ppc_state.fpr[reg].dbl64_r;
}

uint64_t fp_return_uint64(uint32_t reg) {
    return ppc_state.fpr[reg].int64_r;
}

#define ppc_store_sfpresult_int(reg)                    \
    ppc_state.fpr[(reg)].int64_r = ppc_result64_d;

#define ppc_store_sfpresult_flt(reg)                    \
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d;

#define ppc_store_dfpresult_int(reg)                    \
    ppc_state.fpr[(reg)].int64_r = ppc_result64_d;

#define ppc_store_dfpresult_flt(reg)                    \
    ppc_state.fpr[(reg)].dbl64_r = ppc_dblresult64_d;

#define ppc_grab_regsfpdb()                         \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;

#define ppc_grab_regsfpdiab()                       \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;   \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];      \
    uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpdia()                        \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsia()                        \
    int reg_s = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];

#define ppc_grab_regsfpsiab()                       \
    int reg_s = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;   \
    uint32_t val_reg_a = ppc_state.gpr[reg_a];      \
    uint32_t val_reg_b = ppc_state.gpr[reg_b];

#define ppc_grab_regsfpsab()                        \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;   \
    int crf_d = (ppc_cur_instruction >> 21) & 0x1C; \
    double db_test_a = GET_FPR(reg_a);              \
    double db_test_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdab()                        \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;   \
    double val_reg_a = GET_FPR(reg_a);              \
    double val_reg_b = GET_FPR(reg_b);

#define ppc_grab_regsfpdac()                        \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_c = (ppc_cur_instruction >>  6) & 31;   \
    double val_reg_a = GET_FPR(reg_a);              \
    double val_reg_c = GET_FPR(reg_c);

#define ppc_grab_regsfpdabc()                       \
    int reg_d = (ppc_cur_instruction >> 21) & 31;   \
    int reg_a = (ppc_cur_instruction >> 16) & 31;   \
    int reg_b = (ppc_cur_instruction >> 11) & 31;   \
    int reg_c = (ppc_cur_instruction >>  6) & 31;   \
    double val_reg_a = GET_FPR(reg_a);              \
    double val_reg_b = GET_FPR(reg_b);              \
    double val_reg_c = GET_FPR(reg_c);

void fp_save_float(float entry) {
    ppc_state.fpr[reg_d].dbl64_r = (double)entry;
    ppc_state.fpr[reg_d].int64_r = (uint64_t)entry;
}

void fp_save_double(double entry) {
    ppc_state.fpr[reg_d].dbl64_r = entry;
    ppc_state.fpr[reg_d].int64_r = *(uint64_t*)&entry;
}

void fp_save_uint64(uint64_t entry) {
    ppc_state.fpr[reg_d].int64_r = entry;
    ppc_state.fpr[reg_d].dbl64_r = (double)entry;
}

void fp_save_uint32(uint32_t entry) {
    ppc_state.fpr[reg_d].int64_r = entry;
    ppc_state.fpr[reg_d].dbl64_r = (double)entry;
}

void ppc_fp_changecrf1() {
    ppc_state.fpscr |= 0xf0000000;
}

int64_t round_to_nearest(double f) {
    if (f >= 0.0) {
        return static_cast<int32_t>(static_cast<int64_t> (std::ceil(f)));
    } else {
        return static_cast<int32_t>(static_cast<int64_t> (std::floor(f)));
    }
}

int64_t round_to_zero(double f) {
    return static_cast<int32_t>(std::trunc(f));
}

int64_t round_to_pos_inf(double f) {
    return static_cast<int32_t>(std::ceil(f));
}

int64_t round_to_neg_inf(double f) {
    return static_cast<int32_t>(std::floor(f));
}

void update_fex() {
    int fex_result  = !!((ppc_state.fpscr & (ppc_state.fpscr << 22)) & 0x3E000000);
    ppc_state.fpscr = (ppc_state.fpscr & ~0x40000000) | (fex_result << 30);
}

template <typename T, const FPOP fpop>
void ppc_confirm_inf_nan(int chosen_reg_1, int chosen_reg_2, int chosen_reg_3, bool rc_flag = false) {
    T input_a = T(ppc_state.fpr[chosen_reg_1].int64_r);
    T input_b = T(ppc_state.fpr[chosen_reg_2].int64_r);
    T input_c = T(ppc_state.fpr[chosen_reg_3].int64_r);

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
        case FPOP::MUL:
            if (((input_a == FP_ZERO) && (input_c == FP_INFINITE)) ||
                ((input_c == FP_ZERO) && (input_a == FP_INFINITE))) {
                ppc_state.fpscr |=
                    (FPSCR::FX | FPSCR::VXSNAN |
                     FPSCR::VXIMZ);
            }
            update_fex();
            break;
        case FPOP::FMSUB:
        case FPOP::FNMSUB:
            if (std::isnan(input_a) || std::isnan(input_b) || std::isnan(input_c)) {
                ppc_state.fpscr |= (FPSCR::FX | FPSCR::VXSNAN);
                if (((input_a == FP_ZERO) && (input_c == FP_INFINITE)) ||
                    ((input_c == FP_ZERO) && (input_a == FP_INFINITE))) {
                    ppc_state.fpscr |= FPSCR::VXIMZ;
                }
            }

            update_fex();
            break;
        case FPOP::FMADD:
        case FPOP::FNMADD:
            if (std::isnan(input_a) || std::isnan(input_b) || std::isnan(input_c)) {
                ppc_state.fpscr |= (FPSCR::VXSNAN | FPSCR::FPCC_FUNAN);
            }
            update_fex();
            break;
        }
}

static void fpresult_update(double set_result, bool confirm_arc) {
    if (ppc_state.fpscr & 0x3)
        ppc_state.cr |= 0x2;

    if (set_result > 0.0) {
        ppc_state.fpscr |= FPCC_POS;
    } else if (set_result < 0.0) {
        ppc_state.fpscr |= FPCC_NEG;
    } else {
        ppc_state.fpscr |= FPCC_ZERO;
    }

    if (std::isnan(set_result) || std::isinf(set_result)) {
            ppc_state.fpscr |= FPCC_FPRCD;
    }
}

void ppc_changecrf1(double set_result) {
    cmp_c = 0;

    /*
    if (isnan(set_result)) {
        cmp_c |= (1 << CRx_bit::CR_SO);
    }

    if (set_result > 0.0) {
        cmp_c |= (1 << CRx_bit::CR_GT);
    }

    if (set_result < 0.0) {
        cmp_c |= (1 << CRx_bit::CR_LT);
    }

    if (set_result == 0.0) {
        cmp_c |= (1 << CRx_bit::CR_EQ);
    }*/

    ppc_state.cr = ((ppc_state.cr & ~(CR_select::CR1_field)) | ((cmp_c) >> crf_d));
}

// Floating Point Arithmetic
void dppc_interpreter::ppc_fadd() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d = double(val_reg_a + val_reg_b);

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    }
    else {
        ppc_confirm_inf_nan<double, ADD>(reg_a, reg_b, 0, rc_flag);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fsub() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d = double(val_reg_a - val_reg_b);

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, SUB>(reg_a, reg_b, 0, rc_flag);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fdiv() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d = val_reg_a / val_reg_b;

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, DIV>(reg_a, reg_b, 0, rc_flag);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmul() {
    ppc_grab_regsfpdac();

    ppc_dblresult64_d = val_reg_a * val_reg_c;

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, MUL>(reg_a, reg_b, 0, rc_flag);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmadd() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = std::fma(val_reg_a, val_reg_c, val_reg_b);

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, FMADD>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmsub() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = (val_reg_a * val_reg_c);
    ppc_dblresult64_d -= val_reg_b;

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, FMSUB>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fnmadd() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = (val_reg_a * val_reg_c);
    ppc_dblresult64_d += val_reg_b;
    ppc_dblresult64_d = -(ppc_dblresult64_d);

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    }
    else {
        ppc_confirm_inf_nan<double, FNMADD>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fnmsub() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = (val_reg_a * val_reg_c);
    ppc_dblresult64_d -= val_reg_b;
    ppc_dblresult64_d = -(ppc_dblresult64_d);

    if (!std::isnan(ppc_dblresult64_d) || !std::isinf(ppc_dblresult64_d)) {
        ppc_store_dfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<double, FNMSUB>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fadds() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d = (float)(val_reg_a + val_reg_b);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    }
    else {
        ppc_confirm_inf_nan<float, ADD>(reg_a, reg_b, 0);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fsubs() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d = (float)(val_reg_a - val_reg_b);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, SUB>(reg_a, reg_b, 0);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fdivs() {
    ppc_grab_regsfpdab();

    ppc_dblresult64_d  = (float)(val_reg_a / val_reg_b);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, DIV>(reg_a, reg_b, 0);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmuls() {
    ppc_grab_regsfpdac();

    ppc_dblresult64_d = (float)(val_reg_a * val_reg_c);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, MUL>(reg_a, 0, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmadds() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = static_cast<double>(
        std::fma((float)val_reg_a, (float)val_reg_c, (float)val_reg_b));

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, FMADD>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fmsubs() {
    ppc_grab_regsfpdabc();

    float intermediate = float(val_reg_a * val_reg_c);
    intermediate -= (float)val_reg_b;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, FMSUB>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fnmadds() {
    ppc_grab_regsfpdabc();

    float intermediate = (float)val_reg_a * (float)val_reg_c;
    intermediate      += (float)val_reg_b;
    intermediate       = -intermediate;
    ppc_dblresult64_d  = static_cast<double>(intermediate);

    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, FNMADD>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fnmsubs() {
    ppc_grab_regsfpdabc();

    float intermediate = (float)val_reg_a * (float)val_reg_c;
    intermediate      -= (float)val_reg_b;
    intermediate       = -intermediate;
    ppc_dblresult64_d  = static_cast<double>(intermediate);


    if (!std::isnan(ppc_dblresult64_d)) {
        ppc_store_sfpresult_flt(reg_d);
        fpresult_update(ppc_dblresult64_d, rc_flag);
    } else {
        ppc_confirm_inf_nan<float, FNMSUB>(reg_a, reg_b, reg_c);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fabs() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = abs(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fnabs() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = abs(GET_FPR(reg_b));
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fneg() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = -(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fsel() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = (val_reg_a >= -0.0) ? val_reg_c : val_reg_b;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fsqrt() {
    ppc_grab_regsfpdb();
    ppc_dblresult64_d = std::sqrt(GET_FPR(reg_b));
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fsqrts() {
    ppc_grab_regsfpdb();
    uint32_t test = (uint32_t)(GET_FPR(reg_b));
    test += 127 << 23;
    test >>= 1;
    uint64_t* pre_final = (uint64_t*)&test;
    ppc_result64_d      = *pre_final;
    ppc_store_sfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_frsqrte() {
    ppc_grab_regsfpdb();
    double testd2 = (double)(GET_FPR(reg_b));
    for (int i = 0; i < 10; i++) {
        testd2 = testd2 * (1.5 - (testd2 * .5) * testd2 * testd2);
    }
    ppc_dblresult64_d = testd2;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_frsp() {
    ppc_grab_regsfpdb();
    ppc_dblresult64_d = (float)(GET_FPR(reg_b));
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fres() {
    ppc_grab_regsfpdb();
    double start_num  = GET_FPR(reg_b);
    float testf2      = (float)start_num;
    testf2            = 1 / testf2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult_flt(reg_d);

    if (start_num == 0.0) {
        ppc_state.fpscr |= FPSCR::ZX;
    }
    else if (std::isnan(start_num)) {
        ppc_state.fpscr |= FPSCR::VXSNAN;
    }
    else if (std::isinf(start_num)){
        ppc_state.fpscr |= FPSCR::VXSNAN;
        ppc_state.fpscr &= 0xFFF9FFFF;
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fctiw() {
    ppc_grab_regsfpdb();
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= FPSCR::VXSNAN | FPSCR::VXCVI;
    }
    else if (val_reg_b > static_cast<double>(0x7fffffff)) {
        ppc_state.fpr[reg_d].int64_r = 0x7fffffff;
        ppc_state.fpscr |= FPSCR::VXCVI;
    }
    else if (val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= FPSCR::VXCVI;
    }
    else {
        switch (ppc_state.fpscr & 0x3) {
        case 0:
            ppc_result64_d = round_to_nearest(val_reg_b);
            break;
        case 1:
            ppc_result64_d = round_to_zero(val_reg_b);
            break;
        case 2:
            ppc_result64_d = round_to_pos_inf(val_reg_b);
            break;
        case 3:
            ppc_result64_d = round_to_neg_inf(val_reg_b);
            break;
        }

        ppc_store_dfpresult_int(reg_d);

    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

void dppc_interpreter::ppc_fctiwz() {
    ppc_grab_regsfpdb();
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= FPSCR::VXSNAN | FPSCR::VXCVI;
    }
    else if (val_reg_b > static_cast<double>(0x7fffffff)) {
        ppc_state.fpr[reg_d].int64_r = 0x7fffffff;
        ppc_state.fpscr |= FPSCR::VXCVI;
    }
    else if (val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= FPSCR::VXCVI;
    }
    else {
        ppc_result64_d = round_to_zero(val_reg_b);

        ppc_store_dfpresult_int(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1(ppc_dblresult64_d);
}

// Floating Point Store and Load

void dppc_interpreter::ppc_lfs() {
    ppc_grab_regsfpdia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    ppc_result64_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
    //ppc_result64_d = mem_grab_dword(ppc_effective_address);
    ppc_store_sfpresult_int(reg_d);
}


void dppc_interpreter::ppc_lfsu() {
    ppc_grab_regsfpdia();
    if (reg_a) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += (reg_a) ? val_reg_a : 0;
        ppc_result64_d = mmu_read_vmem<uint32_t>(ppc_effective_address);
        //ppc_result64_d = mem_grab_dword(ppc_effective_address);
        ppc_store_sfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfsx() {
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    ppc_result64_d        = mmu_read_vmem<uint32_t>(ppc_effective_address);
    //ppc_result64_d        = mem_grab_dword(ppc_effective_address);
    ppc_store_sfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfsux() {
    ppc_grab_regsfpdiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        ppc_result64_d        = mmu_read_vmem<uint32_t>(ppc_effective_address);
        //ppc_result64_d        = mem_grab_dword(ppc_effective_address);
        ppc_store_sfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfd() {
    ppc_grab_regsfpdia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    //ppc_result64_d = mem_grab_qword(ppc_effective_address);
    ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfdu() {
    ppc_grab_regsfpdia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        //ppc_result64_d = mem_grab_qword(ppc_effective_address);
        ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
        ppc_store_dfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfdx() {
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    //ppc_result64_d        = mem_grab_qword(ppc_effective_address);
    ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfdux() {
    ppc_grab_regsfpdiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        //ppc_result64_d        = mem_grab_qword(ppc_effective_address);
        ppc_result64_d = mmu_read_vmem<uint64_t>(ppc_effective_address);
        ppc_store_dfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfs() {
    ppc_grab_regsfpsia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    mmu_write_vmem<uint32_t>(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
    //mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
}

void dppc_interpreter::ppc_stfsu() {
    ppc_grab_regsfpsia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        mmu_write_vmem<uint32_t>(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        //mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfsx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mmu_write_vmem<uint32_t>(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
    //mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
}

void dppc_interpreter::ppc_stfsux() {
    ppc_grab_regsfpsiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        mmu_write_vmem<uint32_t>(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        //mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfd() {
    ppc_grab_regsfpsia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
    //mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdu() {
    ppc_grab_regsfpsia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        //mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfdx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
    //mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdux() {
    ppc_grab_regsfpsiab();
    if (reg_a != 0) {
        ppc_effective_address = val_reg_a + val_reg_b;
        mmu_write_vmem<uint64_t>(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        //mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfiwx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mmu_write_vmem<uint32_t>(ppc_effective_address, (uint32_t)(ppc_state.fpr[reg_s].int64_r));
    //mem_write_dword(ppc_effective_address, (uint32_t)(ppc_state.fpr[reg_s].int64_r));
}

// Floating Point Register Transfer

void dppc_interpreter::ppc_fmr() {
    ppc_grab_regsfpdb();
    ppc_state.fpr[reg_d].dbl64_r = ppc_state.fpr[reg_b].dbl64_r;
}

void dppc_interpreter::ppc_mffs() {
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.fpr[reg_d].int64_r & ((uint64_t)0xFFF80000 << 32);
    fpstore1          |= (uint64_t)ppc_state.fpscr;
    fp_save_uint64(fpstore1);

    if (rc_flag)
        ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsf() {
    reg_b            = (ppc_cur_instruction >> 11) & 31;
    uint32_t fm_mask = (ppc_cur_instruction >> 17) & 255;
    crm += ((fm_mask & 1) == 1) ? 0xF0000000 : 0x00000000;
    crm += (((fm_mask >> 1) & 1) == 1) ? 0x0F000000 : 0x00000000;
    crm += (((fm_mask >> 2) & 1) == 1) ? 0x00F00000 : 0x00000000;
    crm += (((fm_mask >> 3) & 1) == 1) ? 0x000F0000 : 0x00000000;
    crm += (((fm_mask >> 4) & 1) == 1) ? 0x0000F000 : 0x00000000;
    crm += (((fm_mask >> 5) & 1) == 1) ? 0x00000F00 : 0x00000000;
    crm += (((fm_mask >> 6) & 1) == 1) ? 0x000000F0 : 0x00000000;
    crm += (((fm_mask >> 7) & 1) == 1) ? 0x0000000F : 0x00000000;
    uint32_t quickfprval = (uint32_t)ppc_state.fpr[reg_b].int64_r;
    ppc_state.fpscr      = (quickfprval & crm) | (quickfprval & ~(crm));

    if (rc_flag)
        ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsfi() {
    ppc_result_b    = (ppc_cur_instruction >> 11) & 15;
    crf_d           = (ppc_cur_instruction >> 23) & 7;
    crf_d           = crf_d << 2;
    ppc_state.fpscr = (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        ((ppc_state.spr[SPR::XER] & 0xF0000000UL) >> crf_d);

    if (rc_flag)
        ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsb0() {
    crf_d = (ppc_cur_instruction >> 21) & 0x1F;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr &= ~(0x80000000UL >> crf_d);
    }

    if (rc_flag)
        ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsb1() {
    crf_d = (ppc_cur_instruction >> 21) & 0x1F;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr |= (0x80000000UL >> crf_d);
    }

    if (rc_flag)
        ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mcrfs() {
    crf_d        = (ppc_cur_instruction >> 23) & 7;
    crf_d        = crf_d << 2;
    crf_s        = (ppc_cur_instruction >> 18) & 7;
    crf_s        = crf_d << 2;
    ppc_state.cr = ~(ppc_state.cr & ((15 << (28 - crf_d)))) +
        (ppc_state.fpscr & (15 << (28 - crf_s)));
}

// Floating Point Comparisons

void dppc_interpreter::ppc_fcmpo() {
    ppc_grab_regsfpsab();

    ppc_state.fpscr &= 0xFFFF0FFF;
    cmp_c = 0;
    crf_d = 4;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= (1 << CRx_bit::CR_SO);
    }

    if (db_test_a < db_test_b) {
        cmp_c |= (1 << CRx_bit::CR_LT);
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= (1 << CRx_bit::CR_GT);
    }
    else {
        cmp_c |= (1 << CRx_bit::CR_EQ);
    }

    ppc_state.fpscr = (cmp_c >> 16);
    ppc_state.cr = ((ppc_state.cr & ~(0xF0000000 >> crf_d)) | ((cmp_c) >> crf_d));

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        ppc_state.fpscr |= FPSCR::FX | FPSCR::VXSNAN;
        if ((ppc_state.fpscr & FPSCR::VE) == 0) {
            ppc_state.fpscr |= FPSCR::VXVC;
        }
    } else if ((db_test_a == qnan) || (db_test_b == qnan)) {
        ppc_state.fpscr |= FPSCR::VXVC;
    }
}

void dppc_interpreter::ppc_fcmpu() {
    ppc_grab_regsfpsab();

    cmp_c = 0;
    crf_d = 4;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= (1 << CRx_bit::CR_SO);
    }

    if (db_test_a < db_test_b) {
        cmp_c |= (1 << CRx_bit::CR_LT);
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= (1 << CRx_bit::CR_GT);
    }
    else {
        cmp_c |= (1 << CRx_bit::CR_EQ);
    }

    ppc_state.fpscr = (cmp_c >> 16);
    ppc_state.cr = ((ppc_state.cr & ~(0xF0000000 >> crf_d)) | ((cmp_c) >> crf_d));

    //if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
    //    ppc_state.fpscr |= FPSCR::VX | FPSCR::VXSNAN;
    //}
}
