#include "../ppcdisasm.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

/** testing vehicle */
static vector<PPCDisasmContext> read_test_data() {
    string line, token;
    int i, lineno;
    PPCDisasmContext ctx;
    vector<PPCDisasmContext> tstvec;

    ifstream tfstream("ppcdisasmtest.csv");
    if (!tfstream.is_open()) {
        cout << "Could not open tests CSV file. Exiting..." << endl;
        return tstvec;
    }

    lineno = 0;

    while (getline(tfstream, line)) {
        lineno++;

        if (line.empty() || !line.rfind("#", 0))
            continue;    // skip empty/comment lines

        istringstream lnstream(line);

        vector<string> tokens;

        while (getline(lnstream, token, ',')) {
            // cout << "Token: " << token << endl;
            tokens.push_back(token);
        }

        if (tokens.size() < 3) {
            cout << "Too few values in line " << lineno << ". Skipping..." << endl;
            continue;
        }

        ctx            = {0};
        ctx.instr_addr = stoul(tokens[0], NULL, 16);
        ctx.instr_code = stoul(tokens[1], NULL, 16);

        /* build disassembly string out of comma-separated parts */
        ostringstream idisasm;

        /* put instruction mnemonic padded with trailing spaces */
        idisasm << tokens[2];
        idisasm << setw(8 - tokens[2].length()) << "";

        /* now add comma-separated operands */
        for (i = 3; i < tokens.size(); i++) {
            if (i > 3)
                idisasm << ", ";
            idisasm << tokens[i];
        }

        ctx.instr_str = idisasm.str();

        // cout << idisasm.str() << endl;

        tstvec.push_back(ctx);
    }

    return tstvec;
}

int test_ppc_disasm() {
    int i, nfailed;
    PPCDisasmContext ctx;

    vector<PPCDisasmContext> testdata = read_test_data();

    cout << "Imported " << testdata.size() << " test instructions." << endl;

    nfailed = 0;

    for (i = 0; i < testdata.size(); i++) {
        ctx            = {0};
        ctx.instr_addr = testdata[i].instr_addr;
        ctx.instr_code = testdata[i].instr_code;
        ctx.simplified = true;

        std::string disas = disassemble_single(&ctx);

        if (disas != testdata[i].instr_str) {
            cout << "Mismatch found, expected={" << testdata[i].instr_str << "}, got={" << disas
                 << "}" << endl;
            nfailed++;
        }
    }

    cout << "Tested " << testdata.size() << " instructions. Failed: " << nfailed << "." << endl;

    return 0;
}
