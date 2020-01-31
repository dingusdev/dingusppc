#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
//#include "ppcdisasm.h"
#include "../ppcemu.h"

using namespace std;

int ntested; /* number of tested instructions */
int nfailed; /* number of failed instructions */

/** testing vehicle */
void read_test_data()
{
    string  line, token;
    int     i, lineno;
    //PPCDisasmContext ctx;
    //vector<PPCDisasmContext> tstvec;
    uint32_t opcode, dest, src1, src2, check_xer, check_cr;

    ifstream    tfstream("ppcinttests.csv");
    if (!tfstream.is_open()) {
        cout << "Could not open tests CSV file. Exiting..." << endl;
        return;
    }

    lineno = 0;

    while(getline(tfstream, line)) {
        lineno++;

        if (line.empty() || !line.rfind("#", 0))
            continue; /* skip empty/comment lines */

        istringstream lnstream(line);

        vector<string> tokens;

        while(getline(lnstream, token, ',' )) {
            tokens.push_back(token);
        }

        if (tokens.size() < 6) {
            cout << "Too few values in line " << lineno << ". Skipping..." << endl;
            continue;
        }

        opcode = stol(tokens[1], NULL, 16);

        dest = 0;
        src1 = 0;
        src2 = 0;
        check_xer = 0;
        check_cr  = 0;

        for (i = 2; i < tokens.size(); i++) {
            if (tokens[i].rfind("rD=", 0) == 0) {
                dest = stol(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("rA=", 0) == 0) {
                src1 = stol(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("rB=", 0) == 0) {
                src2 = stol(tokens[i].substr(3), NULL, 16);
            } else if (tokens[i].rfind("XER=", 0) == 0) {
                check_xer = stol(tokens[i].substr(4), NULL, 16);
            } else if (tokens[i].rfind("CR=", 0) == 0) {
                check_cr = stol(tokens[i].substr(3), NULL, 16);
            } else {
                cout << "Unknown parameter " << tokens[i] << " in line " << lineno <<
                        ". Exiting..." << endl;
                exit(0);
            }
        }

        ppc_state.ppc_gpr[3] = src1;
        ppc_state.ppc_gpr[4] = src2;
        ppc_state.ppc_spr[SPR::XER] = 0;
        ppc_state.ppc_cr = 0;

        ppc_cur_instruction = opcode;

        ppc_main_opcode();

        ntested++;

        if ((ppc_state.ppc_gpr[3] != dest) ||
            (ppc_state.ppc_spr[SPR::XER] != check_xer) ||
            (ppc_state.ppc_cr != check_cr)) {
            cout << "Mismatch: instr=" << tokens[0] << ", src1=0x" << hex << src1
                 << ", src2=0x" << hex << src2 << endl;
            cout << "expected: dest=0x" << hex << dest << ", XER=0x" << hex
                 << check_xer << ", CR=0x" << hex << check_cr << endl;
            cout << "got: dest=0x" << hex << ppc_state.ppc_gpr[3] << ", XER=0x"
                 << hex << ppc_state.ppc_spr[SPR::XER] << ", CR=0x" << hex
                 << ppc_state.ppc_cr << endl;

            nfailed++;
        }
    }
}

int main()
{
    int i;

    cout << "Running DingusPPC emulator tests..." << endl << endl;

    cout << "Testing integer instructions:" << endl;

    nfailed = 0;

    read_test_data();

    cout << "... completed." << endl;
    cout << "--> Tested instructions: " << dec << ntested << endl;
    cout << "--> Failed: " << dec << nfailed << endl;

    return 0;
}
