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

// The opcodes for the processor - ppcopcodes.cpp

#include "ppcemu.h"
#include "ppcmmu.h"
#include <array>
#include <cfenv>
#include <cinttypes>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <stdio.h>
#include <unordered_map>
#include <thirdparty/loguru/loguru.hpp>

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

void ppc_divbyzero(uint64_t input_a, uint64_t input_b, bool is_single) {
    if (input_b == 0) {
        ppc_state.fpscr |= 0x84000000;
        if (input_a == 0) {
            ppc_state.fpscr |= 0x200000;
        }
    }
}

int64_t round_to_nearest(double f) {
    if (f >= 0.0) {
        return static_cast<int32_t>(static_cast<int64_t> (f + 0.5));
    } else {
        return static_cast<int32_t>(static_cast<int64_t> (-f + 0.5));
    }
}

int64_t round_to_zero(double f) {
    return static_cast<int32_t>(trunc(f));
}

int64_t round_to_pos_inf(double f) {
    return static_cast<int32_t>(ceil(f));
}

int64_t round_to_neg_inf(double f) {
    return static_cast<int32_t>(floor(f));
}

void ppc_toggle_fpscr_fex() {
    bool fex_result = ((ppc_state.fpscr & 0x20000000) & (ppc_state.fpscr & 0x80));
    fex_result |= ((ppc_state.fpscr & 0x10000000) & (ppc_state.fpscr & 0x40));
    fex_result |= ((ppc_state.fpscr & 0x8000000) & (ppc_state.fpscr & 0x20));
    fex_result |= ((ppc_state.fpscr & 0x4000000) & (ppc_state.fpscr & 0x10));
    fex_result |= ((ppc_state.fpscr & 0x2000000) & (ppc_state.fpscr & 0x8));
    ppc_state.fpscr |= (fex_result << 30);
}

bool ppc_confirm_inf_nan(int chosen_reg_1, int chosen_reg_2, bool is_single, uint32_t op) {
    uint64_t input_a = ppc_state.fpr[chosen_reg_1].int64_r;
    uint64_t input_b = ppc_state.fpr[chosen_reg_2].int64_r;

    if (is_single) {
        uint32_t exp_a = (input_a >> 23) & 0xff;
        uint32_t exp_b = (input_b >> 23) & 0xff;

        ppc_state.fpscr &= 0x7fbfffff;

        switch (op) {
        case 36:
            if ((exp_a == 0xff) && (exp_b == 0xff)) {
                ppc_state.fpscr |= 0x80400000;
                ppc_toggle_fpscr_fex();
                return true;
            } else if ((input_a == 0) && (input_b == 0)) {
                ppc_state.fpscr |= 0x80200000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 40:
            if ((exp_a == 0xff) && (exp_b == 0xff)) {
                ppc_state.fpscr |= 0x80800000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 50:
            if (((exp_a == 0xff) & (input_b == 0)) | ((exp_b == 0xff) & (input_a == 0))) {
                ppc_state.fpscr |= 0x80100000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 56:
        case 58:
            if ((exp_a == 0xff) && (exp_b == 0xff)) {
                ppc_state.fpscr |= 0x80800000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        default:
            return false;
        }
    } else {
        uint32_t exp_a = (input_a >> 52) & 0x7ff;
        uint32_t exp_b = (input_b >> 52) & 0x7ff;

        ppc_state.fpscr &= 0x7fbfffff;

        switch (op) {
        case 36:
            if ((exp_a == 0x7ff) && (exp_b == 0x7ff)) {
                ppc_state.fpscr |= 0x80400000;
                ppc_toggle_fpscr_fex();
                return true;
            } else if ((input_a == 0) && (input_b == 0)) {
                ppc_state.fpscr |= 0x80200000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 40:
            if ((exp_a == 0x7ff) && (exp_b == 0x7ff)) {
                ppc_state.fpscr |= 0x80800000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 50:
            if (((exp_a == 0x7ff) & (input_b == 0)) | ((exp_b == 0x7ff) & (input_a == 0))) {
                ppc_state.fpscr |= 0x80100000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        case 56:
        case 58:
            if ((exp_a == 0xff) && (exp_b == 0xff)) {
                ppc_state.fpscr |= 0x80800000;
                ppc_toggle_fpscr_fex();
                return true;
            }
            break;
        default:
            return false;
        }
    }

    return false;
}

void fpresult_update(uint64_t set_result, bool confirm_arc) {
    bool confirm_ov = (bool)std::fetestexcept(FE_OVERFLOW);

    if (confirm_ov) {
        ppc_state.fpscr |= 0x80001000;
    }

    if (confirm_arc) {
        ppc_state.fpscr |= 0x80010000;
        ppc_state.fpscr &= 0xFFFF0FFF;


        if (set_result == 0) {
            ppc_state.fpscr |= 0x2000;
        } else {
            if (set_result < 0) {
                ppc_state.fpscr |= 0x8000;
            } else if (set_result > 0) {
                ppc_state.fpscr |= 0x4000;
            } else {
                ppc_state.fpscr |= 0x1000;
            }
        }
    }
}

void ppc_frsqrte_result() {
    if (ppc_result64_d & 0x007FF000000000000UL) {
    }
}

void ppc_changecrf1() {
    ppc_state.cr &= 0xF0FFFFFF;
    ppc_state.cr |= (ppc_state.fpscr & 0xF0000000) >> 4;
}

// Floating Point Arithmetic
void dppc_interpreter::ppc_fadd() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, false, 58)) {
        ppc_dblresult64_d = val_reg_a + val_reg_b;
        ppc_store_dfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fsub() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, false, 56)) {
        ppc_dblresult64_d = val_reg_a - val_reg_b;
        ppc_store_dfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fdiv() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, false, 36)) {
        ppc_dblresult64_d = val_reg_a / val_reg_b;
        ppc_store_dfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmul() {
    ppc_grab_regsfpdac();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        ppc_dblresult64_d = val_reg_a * val_reg_c;
        ppc_store_dfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmadd() {
    ppc_grab_regsfpdabc();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        ppc_dblresult64_d = (val_reg_a * val_reg_c);
        if (!ppc_confirm_inf_nan(reg_a, reg_b, false, 58)) {
            ppc_dblresult64_d += val_reg_b;
        }
    }

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmsub() {
    ppc_grab_regsfpdabc();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        ppc_dblresult64_d = (val_reg_a * val_reg_c);
        if (!ppc_confirm_inf_nan(reg_d, reg_b, false, 56)) {
            ppc_dblresult64_d -= val_reg_b;
        }
    }

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fnmadd() {
    ppc_grab_regsfpdabc();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        ppc_dblresult64_d = (val_reg_a * val_reg_c);
        if (!ppc_confirm_inf_nan(reg_a, reg_b, false, 58)) {
            ppc_dblresult64_d += val_reg_b;
        }
    }

    ppc_dblresult64_d = -ppc_dblresult64_d;
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fnmsub() {
    ppc_grab_regsfpdabc();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        ppc_dblresult64_d = (val_reg_a * val_reg_c);
        if (!ppc_confirm_inf_nan(reg_d, reg_b, false, 56)) {
            ppc_dblresult64_d -= val_reg_b;
        }
    }
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fadds() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 58)) {
        float intermediate = (float)val_reg_a + (float)val_reg_b;
        ppc_dblresult64_d  = static_cast<double>(intermediate);
        ppc_store_sfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fsubs() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 56)) {
        float intermediate = (float)val_reg_a - (float)val_reg_b;
        ppc_dblresult64_d  = static_cast<double>(intermediate);
        ppc_store_sfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmuls() {
    ppc_grab_regsfpdac();

    if (!ppc_confirm_inf_nan(reg_a, reg_c, true, 50)) {
        float intermediate = (float)val_reg_a * (float)val_reg_c;
        ppc_dblresult64_d  = static_cast<double>(intermediate);
        ppc_store_sfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fdivs() {
    ppc_grab_regsfpdab();

    if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 36)) {
        float intermediate = (float)val_reg_a / (float)val_reg_b;
        ppc_dblresult64_d  = static_cast<double>(intermediate);
        ppc_store_sfpresult_flt(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmadds() {
    ppc_grab_regsfpdabc();

    float intermediate;

    if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 58)) {
        intermediate = (float)val_reg_a * (float)val_reg_c;
        if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 58)) {
            intermediate += (float)val_reg_b;
            ppc_dblresult64_d = static_cast<double>(intermediate);

            ppc_store_sfpresult_flt(reg_d);
        }
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fmsubs() {
    ppc_grab_regsfpdabc();

    float intermediate;

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        intermediate = (float)val_reg_a * (float)val_reg_c;
        if (!ppc_confirm_inf_nan(reg_d, reg_b, false, 56)) {
            intermediate -= (float)val_reg_b;
            ppc_dblresult64_d = static_cast<double>(intermediate);

            ppc_store_sfpresult_flt(reg_d);
        }
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fnmadds() {
    ppc_grab_regsfpdabc();

    float intermediate;

    if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 58)) {
        intermediate = (float)val_reg_a * (float)val_reg_c;
        if (!ppc_confirm_inf_nan(reg_a, reg_b, true, 58)) {
            intermediate += (float)val_reg_b;
            intermediate = -intermediate;

            ppc_dblresult64_d = static_cast<double>(intermediate);

            ppc_store_sfpresult_flt(reg_d);
        }
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fnmsubs() {
    ppc_grab_regsfpdabc();

    float intermediate;

    if (!ppc_confirm_inf_nan(reg_a, reg_c, false, 50)) {
        intermediate = (float)val_reg_a * (float)val_reg_c;
        if (!ppc_confirm_inf_nan(reg_d, reg_b, false, 56)) {
            intermediate -= (float)val_reg_b;
            intermediate = -intermediate;

            ppc_dblresult64_d = static_cast<double>(intermediate);

            ppc_store_sfpresult_flt(reg_d);
        }
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fabs() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = abs(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fnabs() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = abs(GET_FPR(reg_b));
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fneg() {
    ppc_grab_regsfpdb();

    ppc_dblresult64_d = -(GET_FPR(reg_b));

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fsel() {
    ppc_grab_regsfpdabc();

    ppc_dblresult64_d = (val_reg_a >= 0.0) ? val_reg_c : val_reg_b;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fsqrt() {
    ppc_grab_regsfpdb();
    ppc_dblresult64_d = std::sqrt(GET_FPR(reg_b));
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fsqrts() {
    ppc_grab_regsfpdb();
    uint32_t test = (uint32_t)ppc_result64_b;
    test += 127 << 23;
    test >>= 1;
    uint64_t* pre_final = (uint64_t*)&test;
    ppc_result64_d      = *pre_final;
    ppc_store_sfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_frsqrte() {
    ppc_grab_regsfpdb();
    double testd2 = (double)ppc_result64_b;
    for (int i = 0; i < 10; i++) {
        testd2 = testd2 * (1.5 - (testd2 * .5) * testd2 * testd2);
    }
    ppc_dblresult64_d = testd2;

    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_frsp() {
    ppc_grab_regsfpdb();
    ppc_dblresult64_d = (float)(GET_FPR(reg_b));
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fres() {
    ppc_grab_regsfpdb();
    float testf2      = (float)GET_FPR(reg_b);
    testf2            = 1 / testf2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult_flt(reg_d);

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fctiw() {
    ppc_grab_regsfpdb();
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= 0x1000100;
    } 
    else if (val_reg_b > static_cast<double>(0x7fffffff)) {
        ppc_state.fpr[reg_d].int64_r = 0x7fffffff;
        ppc_state.fpscr |= 0x100;
    } 
    else if (val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= 0x100;
    }
    else {
        switch (ppc_state.fpscr & 0x3) {
        case 0:
            ppc_result64_d = round_to_nearest(val_reg_b);
        case 1:
            ppc_result64_d = round_to_zero(val_reg_b);
        case 2:
            ppc_result64_d = round_to_pos_inf(val_reg_b);
        case 3:
            ppc_result64_d = round_to_neg_inf(val_reg_b);
        }

        ppc_store_dfpresult_int(reg_d);
    
    }

    if (rc_flag)
        ppc_changecrf1();
}

void dppc_interpreter::ppc_fctiwz() {
    ppc_grab_regsfpdb();
    double val_reg_b = GET_FPR(reg_b);

    if (std::isnan(val_reg_b)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= 0x1000100;
    } 
    else if (val_reg_b > static_cast<double>(0x7fffffff)) {
        ppc_state.fpr[reg_d].int64_r = 0x7fffffff;
        ppc_state.fpscr |= 0x100;
    } 
    else if (val_reg_b < -static_cast<double>(0x80000000)) {
        ppc_state.fpr[reg_d].int64_r = 0x80000000;
        ppc_state.fpscr |= 0x100;
    } 
    else {
        ppc_result64_d = round_to_zero(val_reg_b);

        ppc_store_dfpresult_int(reg_d);
    }

    if (rc_flag)
        ppc_changecrf1();
}

// Floating Point Store and Load

void dppc_interpreter::ppc_lfs() {
    ppc_grab_regsfpdia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    ppc_result64_d = mem_grab_dword(ppc_effective_address);
    ppc_store_sfpresult_int(reg_d);
}


void dppc_interpreter::ppc_lfsu() {
    ppc_grab_regsfpdia();
    if (reg_a) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += (reg_a) ? val_reg_a : 0;
        ppc_result64_d = mem_grab_dword(ppc_effective_address);
        ppc_store_sfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfsx() {
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    ppc_result64_d        = mem_grab_dword(ppc_effective_address);
    ppc_store_sfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfsux() {
    ppc_grab_regsfpdiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        ppc_result64_d        = mem_grab_dword(ppc_effective_address);
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
    ppc_result64_d = mem_grab_qword(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfdu() {
    ppc_grab_regsfpdia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        ppc_result64_d = mem_grab_qword(ppc_effective_address);
        ppc_store_dfpresult_int(reg_d);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_lfdx() {
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    ppc_result64_d        = mem_grab_qword(ppc_effective_address);
    ppc_store_dfpresult_int(reg_d);
}

void dppc_interpreter::ppc_lfdux() {
    ppc_grab_regsfpdiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        ppc_result64_d        = mem_grab_qword(ppc_effective_address);
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
    mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
}

void dppc_interpreter::ppc_stfsu() {
    ppc_grab_regsfpsia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfsx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
}

void dppc_interpreter::ppc_stfsux() {
    ppc_grab_regsfpsiab();
    if (reg_a) {
        ppc_effective_address = val_reg_a + val_reg_b;
        mem_write_dword(ppc_effective_address, uint32_t(ppc_state.fpr[reg_s].int64_r));
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfd() {
    ppc_grab_regsfpsia();
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a) ? val_reg_a : 0;
    mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdu() {
    ppc_grab_regsfpsia();
    if (reg_a != 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += val_reg_a;
        mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfdx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
}

void dppc_interpreter::ppc_stfdux() {
    ppc_grab_regsfpsiab();
    if (reg_a != 0) {
        ppc_effective_address = val_reg_a + val_reg_b;
        mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[reg_a] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::ILLEGAL_OP);
    }
}

void dppc_interpreter::ppc_stfiwx() {
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a) ? val_reg_a + val_reg_b : val_reg_b;
    mem_write_dword(ppc_effective_address, (uint32_t)(ppc_state.fpr[reg_s].int64_r));
}

// Floating Point Register Transfer

void dppc_interpreter::ppc_fmr() {
    ppc_grab_regsfpdb();
    ppc_state.fpr[reg_d].dbl64_r = ppc_state.fpr[reg_b].dbl64_r;
}


void dppc_interpreter::ppc_mffs() {
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.fpr[reg_d].int64_r & 0xFFFFFFFF00000000;
    uint64_t fpstore2 = ppc_state.fpscr & 0x00000000FFFFFFFF;
    fpstore1 |= fpstore2;
    fp_save_uint64(fpstore1);
}

void dppc_interpreter::ppc_mffsdot() {
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.fpr[reg_d].int64_r & 0xFFFFFFFF00000000;
    uint64_t fpstore2 = ppc_state.fpscr & 0x00000000FFFFFFFF;
    fpstore1 |= fpstore2;
    fp_save_uint64(fpstore1);
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
}

void dppc_interpreter::ppc_mtfsfdot() {
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
    ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsfi() {
    ppc_result_b    = (ppc_cur_instruction >> 11) & 15;
    crf_d           = (ppc_cur_instruction >> 23) & 7;
    crf_d           = crf_d << 2;
    ppc_state.fpscr = (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        ((ppc_state.spr[SPR::XER] & 0xF0000000UL) >> crf_d);
}

void dppc_interpreter::ppc_mtfsfidot() {
    ppc_result_b    = (ppc_cur_instruction >> 11) & 15;
    crf_d           = (ppc_cur_instruction >> 23) & 7;
    crf_d           = crf_d << 2;
    ppc_state.fpscr = (ppc_state.cr & ~(0xF0000000UL >> crf_d)) |
        ((ppc_state.spr[SPR::XER] & 0xF0000000UL) >> crf_d);
    ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsb0() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr &= ~(1 << (31 - crf_d));
    }
}

void dppc_interpreter::ppc_mtfsb0dot() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr &= ~(1 << crf_d);
    }
    ppc_fp_changecrf1();
}

void dppc_interpreter::ppc_mtfsb1() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr |= (1 << crf_d);
    }
}

void dppc_interpreter::ppc_mtfsb1dot() {
    crf_d = ~(ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.fpscr |= (1 << crf_d);
    }
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

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= 0x01;
    } else if (db_test_a < db_test_b) {
        cmp_c |= 0x08;
    } else if (db_test_a > db_test_b) {
        cmp_c |= 0x04;
    } else {
        cmp_c |= 0x02;
    }

    ppc_state.fpscr |= (cmp_c << 12);
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000 >> crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)) {
        ppc_state.fpscr |= 0x1000000;
        if (ppc_state.fpscr & 0x80) {
            ppc_state.fpscr |= 0x80000;
        }
    } else if ((db_test_a == qnan) || (db_test_b == qnan)) {
        ppc_state.fpscr |= 0x80000;
    }
}

void dppc_interpreter::ppc_fcmpu() {
    ppc_grab_regsfpsab();

    ppc_state.fpscr &= 0xFFFF0FFF;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= 0x01;
    } else if (db_test_a < db_test_b) {
        cmp_c |= 0x08;
    } else if (db_test_a > db_test_b) {
        cmp_c |= 0x04;
    } else {
        cmp_c |= 0x02;
    }

    ppc_state.fpscr |= (cmp_c << 12);
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000 >> crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)) {
        ppc_state.fpscr |= 0x1000000;
    }
}
