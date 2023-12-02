/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#include "../ppcdisasm.h"
#include "../ppcemu.h"
#include <cfenv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int ntested; // number of tested instructions
int nfailed; // number of failed instructions

void xer_ov_test(string mnem, uint32_t opcode) {
    ppc_state.gpr[3]        = 2;
    ppc_state.gpr[4]        = 2;
    ppc_state.spr[SPR::XER] = 0xFFFFFFFF;
    ppc_cur_instruction     = opcode;
    ppc_main_opcode();
    if (ppc_state.spr[SPR::XER] & 0x40000000UL) {
        cout << "Invalid " << mnem << " emulation! XER[OV] should not be set." << endl;
        nfailed++;
    }
    ntested++;
}

void xer_update_test() {
    xer_ov_test("ADDCO", 0x7C632414);
    xer_ov_test("ADDCO.", 0x7C632415);
    xer_ov_test("ADDO", 0x7C632614);
    xer_ov_test("ADDO.", 0x7C632615);
    xer_ov_test("ADDEO", 0x7C632514);
    xer_ov_test("ADDEO.", 0x7C632515);
    xer_ov_test("ADDMEO", 0x7C6305D4);
    xer_ov_test("ADDMEO.", 0x7C6305D5);
    xer_ov_test("ADDZEO", 0x7C630594);
    xer_ov_test("ADDZEO.", 0x7C630595);
    xer_ov_test("DIVWO", 0x7C6327D6);
    xer_ov_test("DIVWO.", 0x7C6327D7);
    xer_ov_test("DIVWUO", 0x7C632796);
    xer_ov_test("DIVWUO.", 0x7C632797);
    xer_ov_test("MULLWO", 0x7C6325D6);
    xer_ov_test("MULLWO.", 0x7C6325D7);
    xer_ov_test("NEGO", 0x7C6304D0);
    xer_ov_test("NEGO.", 0x7C6304D1);
    xer_ov_test("SUBFO", 0x7C632450);
    xer_ov_test("SUBFO.", 0x7C632451);
    xer_ov_test("SUBFCO", 0x7C632410);
    xer_ov_test("SUBFCO.", 0x7C632411);
    xer_ov_test("SUBFEO", 0x7C632510);
    xer_ov_test("SUBFEO.", 0x7C632511);
    xer_ov_test("SUBFMEO", 0x7C6305D0);
    xer_ov_test("SUBFMEO.", 0x7C6305D1);
    xer_ov_test("SUBFZEO", 0x7C630590);
    xer_ov_test("SUBFZEO.", 0x7C630591);
}

/** testing vehicle */
static void read_test_data() {
    string line, token;
    int i, lineno;
    uint32_t opcode, dest, src1, src2, check_xer, check_cr;

    ifstream tfstream("ppcinttests.csv");
    if (!tfstream.is_open()) {
        cout << "Could not open tests CSV file. Exiting..." << endl;
        return;
    }

    lineno = 0;

    while (getline(tfstream, line)) {
        lineno++;

        if (line.empty() || !line.rfind("#", 0))
            continue; /* skip empty/comment lines */

        istringstream lnstream(line);

        vector<string> tokens;

        while (getline(lnstream, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 5) {
            cout << "Too few values in line " << lineno << ". Skipping..." << endl;
            continue;
        }

        opcode = (uint32_t)stoul(tokens[1], NULL, 16);

        dest      = 0;
        src1      = 0;
        src2      = 0;
        check_xer = 0;
        check_cr  = 0;

        for (i = 2; i < tokens.size(); i++) {
            if (tokens[i].rfind("rD=", 0) == 0) {
                dest = (uint32_t)stoul(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("rA=", 0) == 0) {
                src1 = (uint32_t)stoul(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("rB=", 0) == 0) {
                src2 = (uint32_t)stoul(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("XER=", 0) == 0) {
                check_xer = (uint32_t)stoul(tokens[i].substr(4), NULL, 16);
            } else if (tokens[i].rfind("CR=", 0) == 0) {
                check_cr = (uint32_t)stoul(tokens[i].substr(3), NULL, 16);
            } else {
                cout << "Unknown parameter " << tokens[i] << " in line " << lineno << ". Exiting..."
                     << endl;
                exit(0);
            }
        }

        ppc_state.gpr[3]        = src1;
        ppc_state.gpr[4]        = src2;

        ppc_state.spr[SPR::XER] = 0;
        ppc_state.cr            = 0;

        ppc_cur_instruction = opcode;

        ppc_main_opcode();

        ntested++;

        if ((tokens[0].rfind("CMP") && (ppc_state.gpr[3] != dest)) ||
            (ppc_state.spr[SPR::XER] != check_xer) || (ppc_state.cr != check_cr)) {
            cout << "Mismatch: instr=" << tokens[0] << ", src1=0x" << hex << src1 << ", src2=0x"
                 << hex << src2 << endl;
            cout << "expected: dest=0x" << hex << dest << ", XER=0x" << hex << check_xer
                 << ", CR=0x" << hex << check_cr << endl;
            cout << "got: dest=0x" << hex << ppc_state.gpr[3] << ", XER=0x" << hex
                 << ppc_state.spr[SPR::XER] << ", CR=0x" << hex << ppc_state.cr << endl;
            cout << "Test file line #: " << dec << lineno << endl << endl;

            nfailed++;
        }
    }
}

double double_from_string(string str) {
    if (str ==     "snan")
        return std::numeric_limits<double>::signaling_NaN();
    if (str ==     "qnan")
        return std::numeric_limits<double>::quiet_NaN();
    if (str == "-FLT_MAX")
        return -std::numeric_limits<float>::max();
    if (str == "-FLT_MIN")
        return -std::numeric_limits<float>::min();
    if (str == "-DBL_MAX")
        return -std::numeric_limits<double>::max();
    if (str == "-DBL_MIN")
        return -std::numeric_limits<double>::min();
    if (str ==  "FLT_MIN")
        return std::numeric_limits<float>::min();
    if (str ==  "FLT_MAX")
        return std::numeric_limits<float>::max();
    if (str ==  "DBL_MIN")
        return std::numeric_limits<double>::min();
    if (str ==  "DBL_MAX")
        return std::numeric_limits<double>::max();
    return stod(str, NULL);
}

static void read_test_float_data() {
    string line, token;
    int i, lineno;

    uint32_t opcode, src1, src2, check_cr, check_fpscr;
    uint64_t dest_64;
    //float sfp_dest, sfp_src1, sfp_src2, sfp_src3;
    double dfp_src1, dfp_src2, dfp_src3;
    string rounding_mode;

    ifstream tf2stream("ppcfloattests.csv");
    if (!tf2stream.is_open()) {
        cout << "Could not open tests CSV file. Exiting..." << endl;
        return;
    }

    lineno = 0;

    while (getline(tf2stream, line)) {
        lineno++;

        if (line.empty() || !line.rfind("#", 0))
            continue; // skip empty/comment lines

        istringstream lnstream(line);

        vector<string> tokens;

        while (getline(lnstream, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 5) {
            cout << "Too few values in line " << lineno << ". Skipping..." << endl;
            continue;
        }

        opcode = (uint32_t)stoul(tokens[1], NULL, 16);

        src1        = 0;
        src2        = 0;
        check_cr    = 0;
        check_fpscr = 0;
        dfp_src1    = 0.0;
        dfp_src2    = 0.0;
        dfp_src3    = 0.0;
        dest_64     = 0;

        // switch to default rounding
        fesetround(FE_TONEAREST);

        for (i = 2; i < tokens.size(); i++) {
            if (tokens[i].rfind("frD=", 0) == 0) {
                dest_64 = stoull(tokens[i].substr(4), NULL, 16);
            } else if (tokens[i].rfind("frA=", 0) == 0) {
                dfp_src1 = double_from_string(tokens[i].substr(4));
            } else if (tokens[i].rfind("frB=", 0) == 0) {
                dfp_src2 = double_from_string(tokens[i].substr(4));
            } else if (tokens[i].rfind("frC=", 0) == 0) {
                dfp_src3 = double_from_string(tokens[i].substr(4));
            } else if (tokens[i].rfind("round=", 0) == 0) {
                rounding_mode = tokens[i].substr(6, 3);
            } else if (tokens[i].rfind("FPSCR=", 0) == 0) {
                check_fpscr = (uint32_t)stoul(tokens[i].substr(6), NULL, 16);
            } else if (tokens[i].rfind("CR=", 0) == 0) {
                check_cr = (uint32_t)stoul(tokens[i].substr(3), NULL, 16);
            } else {
                cout << "Unknown parameter " << tokens[i] << " in line " << lineno << ". Exiting..."
                     << endl;
                exit(0);
            }
        }

        if (rounding_mode.compare("RTN") == 0) {
            update_fpscr(0);
        } else if (rounding_mode.compare("RTZ") == 0) {
            update_fpscr(1);
        } else if (rounding_mode.compare("RPI") == 0) {
            update_fpscr(2);
        } else if (rounding_mode.compare("RNI") == 0) {
            update_fpscr(3);
        } else if (rounding_mode.compare("VEN") == 0) {
            update_fpscr(FPSCR::VE);
        } else {
            cout << "ILLEGAL ROUNDING METHOD: " << tokens[i] << " in line " << lineno
                 << ". Exiting..." << endl;
            exit(0);
        }

        ppc_state.gpr[3] = src1;
        ppc_state.gpr[4] = src2;

        ppc_state.fpr[3].dbl64_r = 0;
        ppc_state.fpr[4].dbl64_r = dfp_src1;
        ppc_state.fpr[5].dbl64_r = dfp_src2;
        ppc_state.fpr[6].dbl64_r = dfp_src3;

        ppc_state.cr = 0;

        ppc_cur_instruction = opcode;

        ppc_main_opcode();

        ntested++;

        // switch to default rounding
        fesetround(FE_TONEAREST);

        if ((tokens[0].rfind("FCMP") && (ppc_state.fpr[3].int64_r != dest_64)) ||
            (ppc_state.fpscr != check_fpscr) ||
            (ppc_state.cr != check_cr)) {
            cout << "Mismatch: instr=" << tokens[0] << ", src1=" << scientific << dfp_src1 << ", src2=" << scientific << dfp_src2 << ", src3=" << scientific << dfp_src3 << endl;
            cout << "expected: dest=0x" << hex << dest_64 << ", FPSCR=0x" << hex << check_fpscr
                 << ", CR=0x"
                << hex << check_cr << endl;
            cout << "got: dest=0x" << hex << ppc_state.fpr[3].int64_r << ", FPSCR=0x" << hex
                 << ppc_state.fpscr << ", CR=0x" << hex << ppc_state.cr << endl;
            cout << "Test file line #: " << dec << lineno << endl << endl;

            nfailed++;
        }
    }
}

int main() {
    initialize_ppc_opcode_tables(); //kludge

    cout << "Running DingusPPC emulator tests..." << endl << endl;

    ntested = 0;
    nfailed = 0;

    cout << "Testing XER[OV] updating..." << endl << endl;

    xer_update_test();

    cout << endl << "Testing integer instructions:" << endl;

    read_test_data();

    cout << endl << "Float IEEE suport: " << (bool)std::numeric_limits<float>::is_iec559 << endl;
    cout << endl << "Double IEEE suport: " << (bool)std::numeric_limits<double>::is_iec559 << endl;

    cout << endl << "Testing floating point instructions:" << endl;

    read_test_float_data();

    cout << "... completed." << endl;
    cout << "--> Tested instructions: " << dec << ntested << endl;
    cout << "--> Failed: " << dec << nfailed << endl << endl;

    cout << "Running PPC disassembler tests..." << endl << endl;

    return test_ppc_disasm();
}
