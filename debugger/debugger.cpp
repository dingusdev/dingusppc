//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

#include <stdio.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <map>
#include "ppcemu.h"
#include "../cpu/ppc/ppcmmu.h"


using namespace std;

void show_help()
{
    cout << "Debugger commands:" << endl;
    cout << "  step      -- execute single instruction" << endl;
    cout << "  until X   -- execute until address X is reached" << endl;
    cout << "  regs      -- dump content of the GRPs" << endl;
    cout << "  set R=X   -- assign value X to register R" << endl;
#ifdef PROFILER
    cout << "  profiler  -- show stats related to the processor" << endl;
#endif
    cout << "  disas X,n -- disassemble N instructions starting at address X" << endl;
    cout << "  quit      -- quit the debugger" << endl << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

void dump_regs()
{
    for (uint32_t i = 0; i < 32; i++)
        cout << "GPR " << dec << i << " : " << hex << ppc_state.ppc_gpr[i] << endl;

    cout << "PC: "  << hex << ppc_state.ppc_pc     << endl;
    cout << "LR: "  << hex << ppc_state.ppc_spr[8] << endl;
    cout << "CR: "  << hex << ppc_state.ppc_cr     << endl;
    cout << "CTR: " << hex << ppc_state.ppc_spr[9] << endl;
    cout << "XER: " << hex << ppc_state.ppc_spr[1] << endl;
    cout << "MSR: " << hex << ppc_state.ppc_msr    << endl;
}

void enter_debugger()
{
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd;
    uint32_t addr;
    std::stringstream ss;

    cout << "Welcome to the PowerPC debugger." << endl;
    cout << "Please enter a command or 'help'." << endl << endl;

    while (1) {
        cout << "ppcdbg> ";

        /* reset string stream */
        ss.str("");
        ss.clear();

        cmd = "";
        getline(cin, inp, '\n');
        ss.str(inp);
        ss >> cmd;

        if (cmd.empty() && !last_cmd.empty()) {
            cmd = last_cmd;
            cout << cmd << endl;
        }
        if (cmd == "help") {
            show_help();
        } else if (cmd == "quit") {
            break;
        }
#ifdef PROFILER
        else if (cmd == "profiler") {
            cout << "Number of Supervisor Instructions Executed:" << supervisor_inst_num << endl;
            cout << "Exception Handler Ran:" << exceptions_performed << endl;
            cout << "Number of MMU Translations:" << mmu_translations_num << endl;
        }
#endif
        else if (cmd == "regs") {
            dump_regs();
        } else if (cmd == "set") {
            ss >> expr_str;
            reg_expr = expr_str.substr(0, expr_str.find("="));
            if (reg_expr == "pc") {
                addr_str = expr_str.substr(expr_str.find("=") + 1);
                addr = stol(addr_str, NULL, 0);
                ppc_state.ppc_pc = addr;
            } else {
                cout << "Unknown register " << reg_expr << endl;
            }
        } else if (cmd == "step") {
            ppc_exec_single();
        } else if (cmd == "until") {
            ss >> addr_str;
            addr = stol(addr_str, NULL, 16);
            ppc_exec_until(addr);
        } else if (cmd == "disas") {
            cout << "Disassembling not implemented yet. Sorry!" << endl;
        } else {
            cout << "Unknown command: " << cmd << endl;
            continue;
        }
        last_cmd = cmd;
    }
}
