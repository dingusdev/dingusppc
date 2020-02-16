//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

// The opcodes for the processor - ppcopcodes.cpp

#include <iostream>
#include <map>
#include <unordered_map>
#include <cinttypes>
#include <array>
#include <stdio.h>
#include <stdexcept>
#include "ppcemu.h"
#include "ppcmmu.h"
#include <cfenv>
#include <cmath>
#include <limits>

//Used for FP calcs
uint64_t ppc_result64_a;
uint64_t ppc_result64_b;
uint64_t ppc_result64_c;
uint64_t ppc_result64_d;

double ppc_dblresult64_a;
double ppc_dblresult64_b;
double ppc_dblresult64_c;
double ppc_dblresult64_d;

double snan = std::numeric_limits<double>::signaling_NaN();
double qnan = std::numeric_limits<double>::quiet_NaN();

//Storage and register retrieval functions for the floating point functions.

double fp_return_double(uint32_t reg) {
    return ppc_state.ppc_fpr[reg].dbl64_r;
}

uint64_t fp_return_uint64(uint32_t reg) {
    return ppc_state.ppc_fpr[reg].int64_r;
}

void ppc_store_sfpresult(bool int_rep) {
    if (int_rep) {
        ppc_state.ppc_fpr[reg_d].int64_r = ppc_result64_d;
        ppc_state.ppc_fpr[reg_d].dbl64_r = *(double*)&ppc_result64_d;
    }
    else {
        ppc_state.ppc_fpr[reg_d].dbl64_r = ppc_dblresult64_d;
        ppc_state.ppc_fpr[reg_d].int64_r = *(uint64_t*)&ppc_dblresult64_d;
    }
}

void ppc_store_dfpresult(bool int_rep) {
    if (int_rep) {
        ppc_state.ppc_fpr[reg_d].int64_r = ppc_result64_d;
        ppc_state.ppc_fpr[reg_d].dbl64_r = *(double*)&ppc_result64_d;
    }
    else {
        ppc_state.ppc_fpr[reg_d].dbl64_r = ppc_dblresult64_d;
        ppc_state.ppc_fpr[reg_d].int64_r = *(uint64_t*)&ppc_dblresult64_d;
    }
}

void ppc_grab_regsfpdb(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    if (int_rep) {
        ppc_result64_b = ppc_state.ppc_fpr[reg_b].int64_r;
    }
    else {
        ppc_dblresult64_b = ppc_state.ppc_fpr[reg_b].dbl64_r;
    }
}

void ppc_grab_regsfpdiab(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    if (int_rep == true) {

    }
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regsfpdia(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsfpsia(bool int_rep) {
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsfpsiab(bool int_rep) {
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result64_d = ppc_state.ppc_fpr[reg_s].int64_r;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regsfpsab(bool int_rep) {
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    if (int_rep) {
        ppc_result64_d = ppc_state.ppc_fpr[reg_s].int64_r;
        ppc_result64_a = ppc_state.ppc_fpr[reg_a].int64_r;
        ppc_result64_b = ppc_state.ppc_fpr[reg_b].int64_r;
    }
    else {
        ppc_dblresult64_a = fp_return_double(reg_a);
        ppc_dblresult64_c = fp_return_double(reg_c);
    }
}

void ppc_grab_regsfpdab(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    if (int_rep) {
        ppc_result64_a = fp_return_uint64(reg_a);
        ppc_result64_b = fp_return_uint64(reg_b);
    }
    else {
        ppc_dblresult64_a = fp_return_double(reg_a);
        ppc_dblresult64_b = fp_return_double(reg_b);
    }
}

void ppc_grab_regsfpdac(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_c = (ppc_cur_instruction >> 6) & 31;
    if (int_rep) {
        ppc_result64_a = fp_return_uint64(reg_a);
        ppc_result64_c = fp_return_uint64(reg_c);
    }
    else {
        ppc_dblresult64_a = fp_return_double(reg_a);
        ppc_dblresult64_c = fp_return_double(reg_c);
    }
}

void ppc_grab_regsfpdabc(bool int_rep) {
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    reg_c = (ppc_cur_instruction >> 6) & 31;
    if (int_rep) {
        ppc_result64_a = fp_return_uint64(reg_a);
        ppc_result64_b = fp_return_uint64(reg_b);
        ppc_result64_c = fp_return_uint64(reg_c);
    }
    else {
        ppc_dblresult64_a = fp_return_double(reg_a);
        ppc_dblresult64_b = fp_return_double(reg_b);
        ppc_dblresult64_c = fp_return_double(reg_c);
    }
}

void fp_save_float(float entry) {
    ppc_state.ppc_fpr[reg_d].dbl64_r = (double)entry;
    ppc_state.ppc_fpr[reg_d].int64_r = (uint64_t)entry;
}

void fp_save_double(double entry) {
    ppc_state.ppc_fpr[reg_d].dbl64_r = entry;
    ppc_state.ppc_fpr[reg_d].int64_r = *(uint64_t*)&entry;
}

void fp_save_uint64(uint64_t entry) {
    ppc_state.ppc_fpr[reg_d].int64_r = entry;
    ppc_state.ppc_fpr[reg_d].dbl64_r = (double)entry;
}

void fp_save_uint32(uint32_t entry) {
    ppc_state.ppc_fpr[reg_d].int64_r = entry;
    ppc_state.ppc_fpr[reg_d].dbl64_r = (double)entry;

}

void ppc_fp_changecrf1() {
    ppc_state.ppc_fpscr |= 0xf0000000;
}

void ppc_divbyzero(uint64_t input_a, uint64_t input_b, bool is_single) {
    if (input_b == 0) {
        ppc_state.ppc_fpscr |= 0x84000000;
        if (input_a == 0) {
            ppc_state.ppc_fpscr |= 0x200000;
        }
    }
}

int64_t round_to_nearest(double f) {
    if (f >= 0.0) {
        return (int32_t)(int64_t)(f + 0.5);
    }
    else {
        return (int32_t)(-(int64_t)(-f + 0.5));
    }
}

int64_t round_to_zero(double f) {
    return (int32_t)(f);
}

int64_t round_to_pos_inf(double f) {
    return (int32_t)(ceil(f));
}

int64_t round_to_neg_inf(double f) {
    return (int32_t)(floor(f));
}

bool ppc_confirm_inf_nan(uint64_t input_a, uint64_t input_b, bool is_single, uint32_t op) {
    if (is_single) {
        uint32_t exp_a = (input_a >> 23) & 0xff;
        uint32_t exp_b = (input_b >> 23) & 0xff;

        ppc_state.ppc_fpscr &= 0x7fbfffff;

        switch (op) {
        case 36:
            if ((exp_a == 0xff) & (exp_b == 0xff)) {
                ppc_state.ppc_fpscr |= 0x80400000;
                return true;
            }
            else if ((input_a == 0) & (input_b == 0)) {
                ppc_state.ppc_fpscr |= 0x80200000;
                return true;
            }
            break;
        case 40:
            if ((exp_a == 0xff) & (exp_b == 0xff)) {
                ppc_state.ppc_fpscr |= 0x80800000;
                return true;
            }
            break;
        case 50:
            if (((exp_a == 0xff) & (input_b == 0)) | ((exp_b == 0xff) & (input_a == 0))) {
                ppc_state.ppc_fpscr |= 0x80100000;
                return true;
            }
            break;
        case 56:
        case 58:
            if ((exp_a == 0xff) & (exp_b == 0xff)) {
                ppc_state.ppc_fpscr |= 0x80800000;
                return true;
            }
            break;
        default:
            return false;
        }
    }
    else {
        uint32_t exp_a = (input_a >> 52) & 0x7ff;
        uint32_t exp_b = (input_b >> 52) & 0x7ff;

        ppc_state.ppc_fpscr &= 0x7fbfffff;

        switch (op) {
        case 36:
            if ((exp_a == 0x7ff) & (exp_b == 0x7ff)) {
                ppc_state.ppc_fpscr |= 0x80400000;
                return true;
            }
            else if ((input_a == 0) & (input_b == 0)) {
                ppc_state.ppc_fpscr |= 0x80200000;
                return true;
            }
            break;
        case 40:
            if ((exp_a == 0x7ff) & (exp_b == 0x7ff)) {
                ppc_state.ppc_fpscr |= 0x80800000;
                return true;
            }
            break;
        case 50:
            if (((exp_a == 0x7ff) & (input_b == 0)) | ((exp_b == 0x7ff) & (input_a == 0))) {
                ppc_state.ppc_fpscr |= 0x80100000;
                return true;
            }
            break;
        case 56:
        case 58:
            if ((exp_a == 0xff) & (exp_b == 0xff)) {
                ppc_state.ppc_fpscr |= 0x80800000;
                return true;
            }
            break;
        default:
            return false;
        }
    }

    return false;
}

void ppc_fpresult_update(uint64_t set_result, bool confirm_arc) {

    bool confirm_ov = (bool)std::fetestexcept(FE_OVERFLOW);

    if (confirm_ov) {
        ppc_state.ppc_fpscr |= 0x80001000;
    }

    if (confirm_arc) {
        ppc_state.ppc_fpscr |= 0x80010000;
        ppc_state.ppc_fpscr &= 0xFFFF0FFF;

        
        if (set_result == 0) {
            ppc_state.ppc_fpscr |= 0x2000;
        }
        else {
            if (set_result < 0) {
                ppc_state.ppc_fpscr |= 0x8000;
            }
            else if (set_result > 0) {
                ppc_state.ppc_fpscr |= 0x4000;
            }
            else {
                ppc_state.ppc_fpscr |= 0x1000;
            }
        }
    }
}

void ppc_frsqrte_result() {

    if (ppc_result64_d & 0x007FF000000000000UL) {

    }
}

void ppc_changecrf1() {
    ppc_state.ppc_cr &= 0xF0FFFFFF;
    ppc_state.ppc_cr |= (ppc_state.ppc_fpscr & 0xF0000000) >> 4;
}

//Floating Point Arithmetic
void ppc_fadd() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 58)) {
        ppc_dblresult64_d = ppc_dblresult64_a + ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
}

void ppc_fadddot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 58)) {
        ppc_dblresult64_d = ppc_dblresult64_a + ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
    ppc_changecrf1();
}

void ppc_fsub() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 56)) {
        ppc_dblresult64_d = ppc_dblresult64_a - ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
}

void ppc_fsubdot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 56)) {
        ppc_dblresult64_d = ppc_dblresult64_a - ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
    ppc_changecrf1();
}

void ppc_fdiv() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 36)) {
        ppc_dblresult64_d = ppc_dblresult64_a / ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
}

void ppc_fdivdot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 36)) {
        ppc_dblresult64_d = ppc_dblresult64_a / ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
    ppc_changecrf1();
}

void ppc_fmult() {
    ppc_grab_regsfpdac(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 50)) {
        ppc_dblresult64_d = ppc_dblresult64_a * ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }
}

void ppc_fmultdot() {
    ppc_grab_regsfpdac(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, false, 50)) {
        ppc_dblresult64_d = ppc_dblresult64_a * ppc_dblresult64_b;
        ppc_store_dfpresult(false);
    }

    ppc_changecrf1();
}

void ppc_fmadd() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = (ppc_dblresult64_a * ppc_dblresult64_c);
    ppc_dblresult64_d += ppc_dblresult64_b;

    ppc_store_dfpresult(false);
}

void ppc_fmadddot() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = (ppc_dblresult64_a * ppc_dblresult64_c);
    ppc_dblresult64_d += ppc_dblresult64_b;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fmsub() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = (ppc_dblresult64_a * ppc_dblresult64_c);
    ppc_dblresult64_d -= ppc_dblresult64_b;

    ppc_store_dfpresult(false);
}

void ppc_fmsubdot() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = (ppc_dblresult64_a * ppc_dblresult64_c);
    ppc_dblresult64_d -= ppc_dblresult64_b;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fnmadd() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = ppc_dblresult64_a * ppc_result64_c;
    ppc_dblresult64_d += -ppc_dblresult64_b;
    ppc_store_dfpresult(false);
}

void ppc_fnmadddot() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = ppc_dblresult64_a * ppc_result64_c;
    ppc_dblresult64_d += ppc_dblresult64_b;
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fnmsub() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = ppc_dblresult64_a * ppc_result64_c;
    ppc_dblresult64_d -= ppc_dblresult64_b;
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
}

void ppc_fnmsubdot() {
    ppc_grab_regsfpdabc(false);

    ppc_dblresult64_d = ppc_dblresult64_a * ppc_result64_c;
    ppc_dblresult64_d -= ppc_dblresult64_b;
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fadds() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 58)) {
        float intermediate = (float)ppc_dblresult64_a + (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }
}

void ppc_faddsdot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 58)) {
        float intermediate = (float)ppc_dblresult64_a + (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }

    ppc_changecrf1();
}

void ppc_fsubs() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 56)) {
        float intermediate = (float)ppc_dblresult64_a - (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }
}

void ppc_fsubsdot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 56)) {
        float intermediate = (float)ppc_dblresult64_a - (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }

    ppc_changecrf1();
}

void ppc_fmults() {
    ppc_grab_regsfpdac(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 50)) {
        float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }
}

void ppc_fmultsdot() {
    ppc_grab_regsfpdac(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 50)) {

        float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }
    ppc_changecrf1();
}

void ppc_fdivs() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 36)) {
        float intermediate = (float)ppc_dblresult64_a / (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }
}

void ppc_fdivsdot() {
    ppc_grab_regsfpdab(false);

    if (!ppc_confirm_inf_nan(ppc_result64_a, ppc_result64_b, true, 36)) {
        float intermediate = (float)ppc_dblresult64_a / (float)ppc_dblresult64_b;
        ppc_dblresult64_d = static_cast<double>(intermediate);
        ppc_store_dfpresult(false);
    }

    ppc_changecrf1();
}

void ppc_fmadds() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate += (float)ppc_dblresult64_b;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
}

void ppc_fmaddsdot() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate += (float)ppc_dblresult64_b;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fmsubs() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate -= (float)ppc_dblresult64_b;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
}

void ppc_fmsubsdot() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate -= (float)ppc_dblresult64_b;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fnmadds() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate += (float)ppc_dblresult64_b;
    intermediate = -intermediate;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
}

void ppc_fnmaddsdot() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate += (float)ppc_dblresult64_b;
    intermediate = -intermediate;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fnmsubs() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate -= (float)ppc_dblresult64_b;
    intermediate = -intermediate;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
}

void ppc_fnmsubsdot() {
    ppc_grab_regsfpdabc(false);

    float intermediate = (float)ppc_dblresult64_a * (float)ppc_dblresult64_c;
    intermediate -= (float)ppc_dblresult64_b;
    intermediate = -intermediate;
    ppc_dblresult64_d = static_cast<double>(intermediate);

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fabs() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = abs(ppc_dblresult64_b);

    ppc_store_dfpresult(false);
}

void ppc_fabsdot() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = abs(ppc_dblresult64_b);

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fnabs() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = abs(ppc_dblresult64_b);
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
}

void ppc_fnabsdot() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = abs(ppc_dblresult64_b);
    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fneg() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
}

void ppc_fnegdot() {
    ppc_grab_regsfpdb(false);

    ppc_dblresult64_d = -ppc_dblresult64_d;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fsel() {
    ppc_grab_regsfpdabc(false);

    if (ppc_dblresult64_a >= 0.0) {
        ppc_dblresult64_d = ppc_dblresult64_c;
    }
    else {
        ppc_dblresult64_d = ppc_dblresult64_b;
    }

    ppc_store_dfpresult(false);
}

void ppc_fseldot() {
    ppc_grab_regsfpdabc(false);

    if (ppc_dblresult64_a >= 0.0) {
        ppc_dblresult64_d = ppc_dblresult64_c;
    }
    else {
        ppc_dblresult64_d = ppc_dblresult64_b;
    }

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fsqrt() {
    ppc_grab_regsfpdb(false);
    ppc_dblresult64_d = std::sqrt(ppc_dblresult64_b);
    ppc_store_dfpresult(false);
}

void ppc_fsqrtdot() {
    ppc_grab_regsfpdb(false);
    ppc_dblresult64_d = std::sqrt(ppc_dblresult64_b);
    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fsqrts() {
    ppc_grab_regsfpdb(true);
    uint32_t test = (uint32_t)ppc_result64_b;
    test += 127 << 23;
    test >>= 1;
    uint64_t* pre_final = (uint64_t*)&test;
    ppc_result64_d = *pre_final;
    ppc_store_dfpresult(true);
}

void ppc_fsqrtsdot() {
    ppc_grab_regsfpdb(true);
    uint32_t test = (uint32_t)ppc_result64_b;
    test += 127 << 23;
    test >>= 1;
    uint64_t* pre_final = (uint64_t*)&test;
    ppc_result64_d = *pre_final;
    ppc_store_dfpresult(true);
    ppc_changecrf1();
}

void ppc_frsqrte() {
    ppc_grab_regsfpdb(false);
    double testd2 = (double)ppc_result64_b;
    for (int i = 0; i < 10; i++) {
        testd2 = testd2 * (1.5 - (testd2 * .5) * testd2 * testd2);
    }
    ppc_dblresult64_d = testd2;

    ppc_store_dfpresult(false);
}

void ppc_frsqrtedot() {
    ppc_grab_regsfpdb(false);
    double testd2 = (double)ppc_result64_b;
    for (int i = 0; i < 10; i++) {
        testd2 = testd2 * (1.5 - (testd2 * .5) * testd2 * testd2);
    }
    ppc_dblresult64_d = testd2;

    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_frsp() {
    ppc_grab_regsfpdb(false);
    double testd2 = (double)ppc_result64_b;
    float testf2 = (float)testd2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult(false);
}

void ppc_frspdot() {
    ppc_grab_regsfpdb(false);
    double testd2 = (double)ppc_result64_b;
    float testf2 = (float)testd2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fres() {
    ppc_grab_regsfpdb(false);
    float testf2 = (float)ppc_dblresult64_b;
    testf2 = 1 / testf2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult(false);
}

void ppc_fresdot() {
    ppc_grab_regsfpdb(false);
    float testf2 = (float)ppc_dblresult64_b;
    testf2 = 1 / testf2;
    ppc_dblresult64_d = (double)testf2;
    ppc_store_dfpresult(false);
    ppc_changecrf1();
}

void ppc_fctiw() {
    ppc_grab_regsfpdb(false);

    switch (ppc_state.ppc_fpscr & 0x3) {
    case 0:
        ppc_result64_d = round_to_nearest(ppc_dblresult64_b);
    case 1:
        ppc_result64_d = round_to_zero(ppc_dblresult64_b);
    case 2:
        ppc_result64_d = round_to_pos_inf(ppc_dblresult64_b);
    case 3:
        ppc_result64_d = round_to_neg_inf(ppc_dblresult64_b);
    }

    ppc_store_dfpresult(true);

}

void ppc_fctiwdot() {
    ppc_grab_regsfpdb(false);

    switch (ppc_state.ppc_fpscr & 0x3) {
    case 0:
        ppc_result64_d = round_to_nearest(ppc_dblresult64_b);
    case 1:
        ppc_result64_d = round_to_zero(ppc_dblresult64_b);
    case 2:
        ppc_result64_d = round_to_pos_inf(ppc_dblresult64_b);
    case 3:
        ppc_result64_d = round_to_neg_inf(ppc_dblresult64_b);
    }

    ppc_store_dfpresult(true);
    ppc_changecrf1();
}

void ppc_fctiwz() {
    ppc_grab_regsfpdb(false);
    ppc_result64_d = round_to_zero(ppc_dblresult64_b);

    ppc_store_result_regd();
}

void ppc_fctiwzdot() {
    ppc_grab_regsfpdb(false);
    ppc_result64_d = round_to_zero(ppc_dblresult64_b);

    ppc_store_result_regd();
    ppc_changecrf1();
}

//Floating Point Store and Load

void ppc_lfs() {
    ppc_grab_regsfpdia(true);
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    ppc_result64_d = mem_grab_dword(ppc_effective_address);
    ppc_store_dfpresult(true);
}


void ppc_lfsu() {
    ppc_grab_regsfpdia(true);
    if (reg_a == 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
        ppc_result64_d = mem_grab_dword(ppc_effective_address);
        ppc_result_a = ppc_effective_address;
        ppc_store_dfpresult(true);
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_lfsx() {
    ppc_grab_regsfpdiab(true);
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
    ppc_result64_d = mem_grab_dword(ppc_effective_address);
    ppc_store_dfpresult(true);
}

void ppc_lfsux() {
    ppc_grab_regsfpdiab(true);
    if (reg_a == 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        ppc_result64_d = mem_grab_dword(ppc_effective_address);
        ppc_result_a = ppc_effective_address;
        ppc_store_dfpresult(true);
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_lfd() {
    ppc_grab_regsfpdia(true);
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    ppc_result64_d = mem_grab_qword(ppc_effective_address);
    ppc_store_dfpresult(true);
}

void ppc_lfdu() {
    ppc_grab_regsfpdia(true);
    if (reg_a == 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        ppc_result64_d = mem_grab_qword(ppc_effective_address);
        ppc_store_dfpresult(true);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_lfdx() {
    ppc_grab_regsfpdia(true);
    ppc_effective_address += (reg_a > 0) ? ppc_result_a + ppc_result_b : ppc_result_b;
    ppc_result64_d = mem_grab_qword(ppc_effective_address);
    ppc_store_dfpresult(true);
}

void ppc_lfdux() {
    ppc_grab_regsfpdiab(true);
    if (reg_a == 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        ppc_result64_d = mem_grab_qword(ppc_effective_address);
        ppc_store_dfpresult(true);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_stfs() {
    ppc_grab_regsfpsia(true);
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    mem_write_dword(ppc_effective_address, uint32_t(ppc_state.ppc_fpr[reg_s].int64_r));
}

void ppc_stfsu() {
    ppc_grab_regsfpsia(true);
    if (reg_a == 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        mem_write_dword(ppc_effective_address, uint32_t(ppc_state.ppc_fpr[reg_s].int64_r));
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_stfsx() {
    ppc_grab_regsfpsiab(true);
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
    mem_write_dword(ppc_effective_address, uint32_t(ppc_state.ppc_fpr[reg_s].int64_r));
}

void ppc_stfsux() {
    ppc_grab_regsfpsiab(true);
    if (reg_a == 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        mem_write_dword(ppc_effective_address, uint32_t(ppc_state.ppc_fpr[reg_s].int64_r));
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_stfd() {
    ppc_grab_regsfpsia(true);
    ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address += (reg_a > 0) ? ppc_result_a : 0;
    mem_write_qword(ppc_effective_address, ppc_state.ppc_fpr[reg_s].int64_r);
}

void ppc_stfdu() {
    ppc_grab_regsfpsia(true);
    if (reg_a == 0) {
        ppc_effective_address = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
        ppc_effective_address += ppc_result_a;
        mem_write_qword(ppc_effective_address, ppc_state.ppc_fpr[reg_s].int64_r);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_stfdx() {
    ppc_grab_regsfpsiab(true);
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
    mem_write_qword(ppc_effective_address, ppc_state.ppc_fpr[reg_s].int64_r);
}

void ppc_stfdux() {
    ppc_grab_regsfpsiab(true);
    if (reg_a == 0) {
        ppc_effective_address = ppc_result_a + ppc_result_b;
        mem_write_qword(ppc_effective_address, ppc_state.ppc_fpr[reg_s].int64_r);
        ppc_result_a = ppc_effective_address;
        ppc_store_result_rega();
    }
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
}

void ppc_stfiwx() {
    ppc_grab_regsfpsiab(true);
    ppc_effective_address = (reg_a == 0) ? ppc_result_b : ppc_result_a + ppc_result_b;
    mem_write_dword(ppc_effective_address, (uint32_t)(ppc_state.ppc_fpr[reg_s].int64_r));
}

//Floating Point Register Transfer

void ppc_fmr() {
    ppc_grab_regsfpdb(true);
    ppc_state.ppc_fpr[reg_d] = ppc_state.ppc_fpr[reg_b];
    ppc_store_dfpresult(true);
}


void ppc_mffs() {
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.ppc_fpr[reg_d].int64_r & 0xFFFFFFFF00000000;
    uint64_t fpstore2 = ppc_state.ppc_fpscr & 0x00000000FFFFFFFF;
    fpstore1 |= fpstore2;
    fp_save_uint64(fpstore1);
}

void ppc_mffsdot() {
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.ppc_fpr[reg_d].int64_r & 0xFFFFFFFF00000000;
    uint64_t fpstore2 = ppc_state.ppc_fpscr & 0x00000000FFFFFFFF;
    fpstore1 |= fpstore2;
    fp_save_uint64(fpstore1);
    ppc_fp_changecrf1();
}

void ppc_mtfsf() {
    reg_b = (ppc_cur_instruction >> 11) & 31;
    uint32_t fm_mask = (ppc_cur_instruction >> 17) & 255;
    crm += ((fm_mask & 1) == 1) ? 0xF0000000 : 0x00000000;
    crm += (((fm_mask >> 1) & 1) == 1) ? 0x0F000000 : 0x00000000;
    crm += (((fm_mask >> 2) & 1) == 1) ? 0x00F00000 : 0x00000000;
    crm += (((fm_mask >> 3) & 1) == 1) ? 0x000F0000 : 0x00000000;
    crm += (((fm_mask >> 4) & 1) == 1) ? 0x0000F000 : 0x00000000;
    crm += (((fm_mask >> 5) & 1) == 1) ? 0x00000F00 : 0x00000000;
    crm += (((fm_mask >> 6) & 1) == 1) ? 0x000000F0 : 0x00000000;
    crm += (((fm_mask >> 7) & 1) == 1) ? 0x0000000F : 0x00000000;
    uint32_t quickfprval = (uint32_t)ppc_state.ppc_fpr[reg_b].int64_r;
    ppc_state.ppc_fpscr = (quickfprval & crm) | (quickfprval & ~(crm));
}

void ppc_mtfsfdot() {
    reg_b = (ppc_cur_instruction >> 11) & 31;
    uint32_t fm_mask = (ppc_cur_instruction >> 17) & 255;
    crm += ((fm_mask & 1) == 1) ? 0xF0000000 : 0x00000000;
    crm += (((fm_mask >> 1) & 1) == 1) ? 0x0F000000 : 0x00000000;
    crm += (((fm_mask >> 2) & 1) == 1) ? 0x00F00000 : 0x00000000;
    crm += (((fm_mask >> 3) & 1) == 1) ? 0x000F0000 : 0x00000000;
    crm += (((fm_mask >> 4) & 1) == 1) ? 0x0000F000 : 0x00000000;
    crm += (((fm_mask >> 5) & 1) == 1) ? 0x00000F00 : 0x00000000;
    crm += (((fm_mask >> 6) & 1) == 1) ? 0x000000F0 : 0x00000000;
    crm += (((fm_mask >> 7) & 1) == 1) ? 0x0000000F : 0x00000000;
    uint32_t quickfprval = (uint32_t)ppc_state.ppc_fpr[reg_b].int64_r;
    ppc_state.ppc_fpscr = (quickfprval & crm) | (quickfprval & ~(crm));
    ppc_fp_changecrf1();
}

void ppc_mtfsfi() {
    ppc_result_b = (ppc_cur_instruction >> 11) & 15;
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_state.ppc_fpscr = (ppc_state.ppc_cr & ~(0xF0000000UL >> crf_d)) | ((ppc_state.ppc_spr[SPR::XER] & 0xF0000000UL) >> crf_d);
}

void ppc_mtfsfidot() {
    ppc_result_b = (ppc_cur_instruction >> 11) & 15;
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    ppc_state.ppc_fpscr = (ppc_state.ppc_cr & ~(0xF0000000UL >> crf_d)) | ((ppc_state.ppc_spr[SPR::XER] & 0xF0000000UL) >> crf_d);
    ppc_fp_changecrf1();
}

void ppc_mtfsb0() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.ppc_fpscr &= ~(1 << (31 - crf_d));
    }
}

void ppc_mtfsb0dot() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.ppc_fpscr &= ~(1 << crf_d);
    }
    ppc_fp_changecrf1();
}

void ppc_mtfsb1() {
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.ppc_fpscr |= (1 << crf_d);
    }
}

void ppc_mtfsb1dot() {
    crf_d = ~(ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)) {
        ppc_state.ppc_fpscr |= (1 << crf_d);
    }
    ppc_fp_changecrf1();
}

void ppc_mcrfs() {
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    crf_s = (ppc_cur_instruction >> 18) & 7;
    crf_s = crf_d << 2;
    ppc_state.ppc_cr = ~(ppc_state.ppc_cr & ((15 << (28 - crf_d)))) & (ppc_state.ppc_fpscr & (15 << (28 - crf_s)));
}

//Floating Point Comparisons

void ppc_fcmpo() {
    ppc_grab_regsfpsab(true);

    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;

    double db_test_a = (double)ppc_result64_a;
    double db_test_b = (double)ppc_result64_b;

    ppc_state.ppc_fpscr &= 0xFFFF0FFF;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= 0x01;
    }
    else if (db_test_a < db_test_b) {
        cmp_c |= 0x08;
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= 0x04;
    }
    else {
        cmp_c |= 0x02;
    }

    ppc_state.ppc_fpscr |= (cmp_c << 12);
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000 >> crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)) {
        ppc_state.ppc_fpscr |= 0x1000000;
        if (ppc_state.ppc_fpscr & 0x80) {
            ppc_state.ppc_fpscr |= 0x80000;
        }
    }
    else if ((db_test_a == qnan) || (db_test_b == qnan)) {
        ppc_state.ppc_fpscr |= 0x80000;
    }

}

void ppc_fcmpu() {
    ppc_grab_regsfpsab(true);

    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;

    double db_test_a = (double)ppc_result64_a;
    double db_test_b = (double)ppc_result64_b;

    ppc_state.ppc_fpscr &= 0xFFFF0FFF;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)) {
        cmp_c |= 0x01;
    }
    else if (db_test_a < db_test_b) {
        cmp_c |= 0x08;
    }
    else if (db_test_a > db_test_b) {
        cmp_c |= 0x04;
    }
    else {
        cmp_c |= 0x02;
    }

    ppc_state.ppc_fpscr |= (cmp_c << 12);
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000 >> crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)) {
        ppc_state.ppc_fpscr |= 0x1000000;
    }
}
