//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

// The opcodes for the processor - ppcopcodes.cpp

#include <iostream>
#include <map>
#include <unordered_map>
#include <cinttypes>
#include <array>
#include <stdio.h>
#include <stdexcept>
#include "ppcemumain.h"
#include "ppcmemory.h"
#include <cmath>
#include <limits>

//Used for FP calcs
 uint64_t ppc_result64_a;
 uint64_t ppc_result64_b;
 uint64_t ppc_result64_c;
 uint64_t ppc_result64_d;

 double snan = std::numeric_limits<double>::signaling_NaN();
 double qnan = std::numeric_limits<double>::quiet_NaN();

//Storage and register retrieval functions for the floating point functions.

void ppc_store_sfpresult(){
    ppc_state.ppc_fpr[reg_d] = (uint64_t)ppc_result_d;
}

void ppc_store_dfpresult(){
    ppc_state.ppc_fpr[reg_d] = ppc_result64_d;
}

void ppc_grab_regsfpdb(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result64_b = ppc_state.ppc_fpr[reg_b];
}

void ppc_grab_regsfpdiab(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regsfpdia(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsfpsia(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    ppc_result_d = ppc_state.ppc_gpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
}

void ppc_grab_regsfpsiab(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result64_d = ppc_state.ppc_fpr[reg_s];
    ppc_result_a = ppc_state.ppc_gpr[reg_a];
    ppc_result_b = ppc_state.ppc_gpr[reg_b];
}

void ppc_grab_regsfpsab(){
    reg_s = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result64_d = ppc_state.ppc_fpr[reg_s];
    ppc_result64_a = ppc_state.ppc_fpr[reg_a];
    ppc_result64_b = ppc_state.ppc_fpr[reg_b];
}

void ppc_grab_regsfpdab(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    ppc_result64_a = ppc_state.ppc_fpr[reg_a];
    ppc_result64_b = ppc_state.ppc_fpr[reg_b];
}

void ppc_grab_regsfpdac(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_c = (ppc_cur_instruction >> 6) & 31;
    ppc_result64_a = ppc_state.ppc_fpr[reg_a];
    ppc_result64_c = ppc_state.ppc_fpr[reg_c];
}

void ppc_grab_regsfpdabc(){
    reg_d = (ppc_cur_instruction >> 21) & 31;
    reg_a = (ppc_cur_instruction >> 16) & 31;
    reg_b = (ppc_cur_instruction >> 11) & 31;
    reg_c = (ppc_cur_instruction >> 6) & 31;
    ppc_result64_a = ppc_state.ppc_fpr[reg_a];
    ppc_result64_b = ppc_state.ppc_fpr[reg_b];
    ppc_result64_c = ppc_state.ppc_fpr[reg_c];
}

//Floating Point Arithmetic
void ppc_fadd(){
    ppc_grab_regsfpdab();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;

    double testd3 = testd1 + testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fsub(){
    ppc_grab_regsfpdab();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;

    double testd3 = testd1 - testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fmult(){
    ppc_grab_regsfpdac();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_c;

    double testd3 = testd1 * testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fdiv(){
    ppc_grab_regsfpdab();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;

    double testd3 = testd1 / testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fmadd(){
    ppc_grab_regsfpdabc();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;
    double testd3 = (double)ppc_result64_c;

    double testd4 = (testd1 * testd3) + testd2;

    ppc_result64_d = (uint64_t)testd4;
    ppc_store_dfpresult();
}

void ppc_fmsub(){
    ppc_grab_regsfpdabc();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;
    double testd3 = (double)ppc_result64_c;

    double testd4 = (testd1 * testd3) - testd2;

    ppc_result64_d = (uint64_t)testd4;
    ppc_store_dfpresult();
}

void ppc_fnmadd(){
    ppc_grab_regsfpdabc();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;
    double testd3 = (double)ppc_result64_c;

    double testd4 = -((testd1 * testd3) + testd2);

    ppc_result64_d = (uint64_t)testd4;
    ppc_store_dfpresult();
}

void ppc_fnmsub(){
    ppc_grab_regsfpdabc();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;
    double testd3 = (double)ppc_result64_c;

    double testd4 = -((testd1 * testd3) - testd2);

    ppc_result64_d = (uint64_t)testd4;
    ppc_store_dfpresult();
}

void ppc_fadds(){
    ppc_grab_regsfpdab();
    float testd1 = (float)ppc_result64_a;
    float testd2 = (float)ppc_result64_b;

    float testd3 = testd1 + testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fsubs(){
    ppc_grab_regsfpdab();
    float testd1 = (float)ppc_result64_a;
    float testd2 = (float)ppc_result64_b;

    float testd3 = testd1 - testd2;

    ppc_result64_d = (uint64_t)testd3;
    ppc_store_dfpresult();
}

void ppc_fmults(){
    ppc_grab_regsfpdac();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_c;

    float testf3 = testf1 * testf2;

    ppc_result64_d = (uint64_t)testf3;
    ppc_store_dfpresult();
}

void ppc_fdivs(){
    ppc_grab_regsfpdab();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_b;

    float testf3 = testf1 / testf2;

    ppc_result64_d = (uint64_t)testf3;
    ppc_store_dfpresult();
}


void ppc_fmadds(){
    ppc_grab_regsfpdabc();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_b;
    float testf3 = (float)ppc_result64_c;

    float testf4 = (testf1 * testf3) + testf2;

    ppc_result64_d = (uint64_t)testf4;
    ppc_store_dfpresult();
}

void ppc_fmsubs(){
    ppc_grab_regsfpdabc();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_b;
    float testf3 = (float)ppc_result64_c;

    float testf4 = (testf1 * testf3) - testf2;

    ppc_result64_d = (uint64_t)testf4;
    ppc_store_dfpresult();
}

void ppc_fnmadds(){
    ppc_grab_regsfpdabc();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_b;
    float testf3 = (float)ppc_result64_c;

    float testf4 = -((testf1 * testf3) + testf2);

    ppc_result64_d = (uint64_t)testf4;
    ppc_store_dfpresult();
}

void ppc_fnmsubs(){
    ppc_grab_regsfpdabc();
    float testf1 = (float)ppc_result64_a;
    float testf2 = (float)ppc_result64_b;
    float testf3 = (float)ppc_result64_c;

    float testf4 = -((testf1 * testf3) - testf2);

    ppc_result64_d = (uint64_t)testf4;
    ppc_store_dfpresult();
}

void ppc_fabs(){
    ppc_grab_regsfpdb();
    double testd1 = (double)ppc_result64_b;

    ppc_result64_d = (uint64_t)(abs(testd1));

    ppc_store_dfpresult();
}

void ppc_fneg(){
    ppc_grab_regsfpdb();
    double testd1 = (double)ppc_result64_a;

    double testd3 = -testd1;

    ppc_result64_d = (double)testd3;
    ppc_store_dfpresult();
}

void ppc_fsel(){
    ppc_grab_regsfpdabc();
    double testd1 = (double)ppc_result64_a;
    double testd2 = (double)ppc_result64_b;
    double testd3 = (double)ppc_result64_c;

    if (testd1 >= 0.0){
        ppc_result64_d = (uint64_t)testd3;
    }
    else{
        ppc_result64_d = (uint64_t)testd2;
    }
    ppc_store_dfpresult();
}

void ppc_fsqrt(){
    ppc_grab_regsfpdb();
    double test = (double)ppc_result64_b;
    std::sqrt(test);
    ppc_result64_d = (uint64_t)test;
    ppc_store_dfpresult();
}

void ppc_fsqrts(){
    ppc_grab_regsfpdb();
    uint32_t test = (uint32_t)ppc_result64_b;
    test += 127 << 23;
    test >>= 1;
    ppc_result64_d = (uint64_t)test;
    ppc_store_dfpresult();
}

void ppc_frsqrte(){
    ppc_grab_regsfpdb();
    double testd2 = (double)ppc_result64_b;
    for (int i = 0; i < 10; i++){
        testd2 = testd2 * (1.5 - (testd2 * .5) * testd2 * testd2);
    }
    ppc_result64_d = (uint64_t) testd2;
    ppc_store_dfpresult();
}

void ppc_frsp(){
    ppc_grab_regsfpdb();
    double testd2 = (double)ppc_result64_b;
    float testf2 = (float) testd2;
    ppc_result64_d = (uint64_t) testf2;
    ppc_store_dfpresult();
}

void ppc_fres(){
    ppc_grab_regsfpdb();
    float testf2 = (float)ppc_result64_b;
    testf2 = 1/testf2;
    ppc_result64_d = (uint64_t) testf2;
    ppc_store_dfpresult();
}

void ppc_fctiw(){
    //PLACEHOLDER!
    ppc_grab_regsfpdiab();
    double testd1 = (double)ppc_result64_b;

    ppc_result_d = (uint32_t)(testd1);

    ppc_store_result_regd();
}

void ppc_fctiwz(){
    //PLACEHOLDER!
    ppc_grab_regsfpdiab();
    double testd1 = (double)ppc_result64_a;

    ppc_result_d = (uint32_t)(testd1);

    ppc_store_result_regd();
}

//Floating Point Store and Load

void ppc_lfs(){
    ppc_grab_regsfpdia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    ppc_result64_d = (uint64_t)return_value;
    ppc_store_dfpresult();
}


void ppc_lfsu(){
    ppc_grab_regsfpdia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    ppc_result64_d = (uint64_t)return_value;
    ppc_result_a = ppc_effective_address;
    ppc_store_dfpresult();
    ppc_store_result_rega();
}

void ppc_lfsx(){
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    ppc_result64_d = (uint64_t)return_value;
    ppc_store_dfpresult();
}


void ppc_lfsux(){
    ppc_grab_regsfpdiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    ppc_result64_d = (uint64_t)return_value;
    ppc_result_a = ppc_effective_address;
    ppc_store_dfpresult();
    ppc_store_result_rega();
}

void ppc_lfd(){
    ppc_grab_regsfpdia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    uint64_t combine_result1 = (uint64_t)(return_value);
    address_quickgrab_translate((ppc_effective_address + 4), ppc_state.ppc_fpr[reg_d], 4);
    uint64_t combine_result2 = (uint64_t)(return_value);
    ppc_result64_d = (combine_result1 << 32) | combine_result2;
    ppc_store_dfpresult();
}


void ppc_lfdu(){
    ppc_grab_regsfpdia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    address_quickgrab_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_d], 4);
    uint64_t combine_result1 = (uint64_t)(return_value);
    address_quickgrab_translate((ppc_effective_address + 4), ppc_state.ppc_fpr[reg_d], 4);
    uint64_t combine_result2 = (uint64_t)(return_value);
    ppc_result64_d = (combine_result1 << 32) | combine_result2;
    ppc_store_dfpresult();
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}


void ppc_stfs(){
    ppc_grab_regsfpsia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    address_quickinsert_translate(ppc_effective_address, ppc_state.ppc_fpr[reg_s], 4);
}

void ppc_stfsu(){
    ppc_grab_regsfpsia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    uint32_t split_result1 = (uint32_t)((float)(ppc_state.ppc_fpr[reg_s]));
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stfsux(){
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    uint32_t split_result1 = (uint32_t)((float)(ppc_state.ppc_fpr[reg_s]));
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stfd(){
    ppc_grab_regsfpsia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    uint32_t split_result1 = (uint32_t)(ppc_state.ppc_fpr[reg_s] >> 32);
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    uint32_t split_result2 = (uint32_t)(ppc_state.ppc_fpr[reg_s]);
    address_quickinsert_translate(ppc_effective_address, split_result2, 4);
}

void ppc_stfdu(){
    ppc_grab_regsfpsia();
    grab_d = (int32_t)((int16_t)(ppc_cur_instruction & 0xFFFF));
    ppc_effective_address = (reg_a == 0)?grab_d:ppc_result_a + grab_d;
    uint32_t split_result1 = (uint32_t)(ppc_state.ppc_fpr[reg_s] >> 32);
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    uint32_t split_result2 = (uint32_t)(ppc_state.ppc_fpr[reg_s]);
    address_quickinsert_translate(ppc_effective_address, split_result2, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stfdx(){
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    uint32_t split_result1 = (uint32_t)(ppc_state.ppc_fpr[reg_s] >> 32);
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    uint32_t split_result2 = (uint32_t)(ppc_state.ppc_fpr[reg_s]);
    address_quickinsert_translate(ppc_effective_address, split_result2, 4);
}

void ppc_stfdux(){
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    uint32_t split_result1 = (uint32_t)(ppc_state.ppc_fpr[reg_s] >> 32);
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
    uint32_t split_result2 = (uint32_t)(ppc_state.ppc_fpr[reg_s]);
    address_quickinsert_translate(ppc_effective_address, split_result2, 4);
    ppc_result_a = ppc_effective_address;
    ppc_store_result_rega();
}

void ppc_stfiwx(){
    ppc_grab_regsfpsiab();
    ppc_effective_address = (reg_a == 0)?ppc_result_b:ppc_result_a + ppc_result_b;
    uint32_t split_result1 = (uint32_t)(ppc_state.ppc_fpr[reg_s] & 0xFFFFFFFF);
    address_quickinsert_translate(ppc_effective_address, split_result1, 4);
}
//Floating Point Register Transfer

void ppc_fmr(){
    ppc_grab_regsfpdb();
    ppc_state.ppc_fpr[reg_d] = ppc_state.ppc_fpr[reg_b];
    ppc_store_dfpresult();
}

void ppc_mffs(){
    ppc_grab_regsda();
    uint64_t fpstore1 = ppc_state.ppc_fpr[reg_d] & 0xFFFFFFFF00000000;
    uint64_t fpstore2 = ppc_state.ppc_fpscr & 0x00000000FFFFFFFF;
    fpstore1 |= fpstore2;
    ppc_state.ppc_fpr[reg_d] = fpstore1;
    ppc_store_sfpresult();
}

void ppc_mtfsf(){
    reg_b = (ppc_cur_instruction >> 11) & 31;
    uint32_t fm_mask = (ppc_cur_instruction >> 17) & 255;
    crm += ((fm_mask & 1) == 1)? 0xF0000000 : 0x00000000;
    crm += ((fm_mask & 2) == 1)? 0x0F000000 : 0x00000000;
    crm += ((fm_mask & 4) == 1)? 0x00F00000 : 0x00000000;
    crm += ((fm_mask & 8) == 1)? 0x000F0000 : 0x00000000;
    crm += ((fm_mask & 16) == 1)? 0x0000F000 : 0x00000000;
    crm += ((fm_mask & 32) == 1)? 0x00000F00 : 0x00000000;
    crm += ((fm_mask & 64) == 1)? 0x000000F0 : 0x00000000;
    crm += ((fm_mask & 128) == 1)? 0x0000000F : 0x00000000;
    uint32_t quickfprval = (uint32_t)ppc_state.ppc_fpr[reg_b];
    ppc_state.ppc_fpscr = (quickfprval & crm) | (quickfprval & ~(crm));
}

void ppc_mtfsb0(){
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)){
        ppc_state.ppc_fpscr &= ~(1 << (31 - crf_d));
    }
}

void ppc_mtfsb0dot(){
    //PLACEHOLDER
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)){
        ppc_state.ppc_fpscr &= ~(1 << crf_d);
    }
}

void ppc_mtfsb1(){
    crf_d = (ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)){
        ppc_state.ppc_fpscr |= (1 << crf_d);
    }
}

void ppc_mtfsb1dot(){
    //PLACEHOLDER
    crf_d = ~(ppc_cur_instruction >> 21) & 0x31;
    if ((crf_d == 0) || (crf_d > 2)){
        ppc_state.ppc_fpscr |= (1 << crf_d);
    }

}

void ppc_mcrfs(){
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;
    crf_s = (ppc_cur_instruction >> 18) & 7;
    crf_s = crf_d << 2;
    ppc_state.ppc_cr = ~(ppc_state.ppc_cr & ((15 << (28- crf_d)))) & (ppc_state.ppc_fpscr & (15 << (28- crf_s)));
}

//Floating Point Comparisons

void ppc_fcmpo(){
    ppc_grab_regsfpsab();
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;

    double db_test_a = (double)ppc_result64_a;
    double db_test_b = (double)ppc_result64_b;

    ppc_state.ppc_fpscr &= 0xFFFF0FFF;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)){
        cmp_c |= 0x01;
    }
    else if (db_test_a < db_test_b){
        cmp_c |= 0x08;
    }
    else if (db_test_a > db_test_b){
        cmp_c |= 0x04;
    }
    else{
        cmp_c |= 0x02;
    }

    ppc_state.ppc_fpscr |= (cmp_c << 12);
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000 >>  crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)){
        ppc_state.ppc_fpscr |= 0x1000000;
        if (ppc_state.ppc_fpscr && 0x80){
            ppc_state.ppc_fpscr |= 0x80000;
        }
    }
    else if ((db_test_a == qnan) || (db_test_b == qnan)){
        ppc_state.ppc_fpscr |= 0x80000;
    }

}

void ppc_fcmpu(){
    ppc_grab_regsfpsab();
    crf_d = (ppc_cur_instruction >> 23) & 7;
    crf_d = crf_d << 2;

    double db_test_a = (double)ppc_result64_a;
    double db_test_b = (double)ppc_result64_b;

    ppc_state.ppc_fpscr &= 0xFFFF0FFF;

    if (std::isnan(db_test_a) || std::isnan(db_test_b)){
        cmp_c |= 0x01;
    }
    else if (db_test_a < db_test_b){
        cmp_c |= 0x08;
    }
    else if (db_test_a > db_test_b){
        cmp_c |= 0x04;
    }
    else{
        cmp_c |= 0x02;
    }

    ppc_state.ppc_fpscr |= (cmp_c << 12);
    ppc_state.ppc_cr = ((ppc_state.ppc_cr & ~(0xf0000000 >>  crf_d)) | ((cmp_c + xercon) >> crf_d));

    if ((db_test_a == snan) || (db_test_b == snan)){
        ppc_state.ppc_fpscr |= 0x1000000;
    }
}
