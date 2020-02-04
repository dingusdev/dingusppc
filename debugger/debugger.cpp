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

uint32_t gpr_num;

const string array_gprs[32] =
{"gpr0",  "gpr1",  "gpr2",  "gpr3",
 "gpr4",  "gpr5",  "gpr6",  "gpr7",
 "gpr8",  "gpr9",  "gpr10", "gpr11",
 "gpr12", "gpr13", "gpr14", "gpr15",
 "gpr16", "gpr17", "gpr18", "gpr19",
 "gpr20", "gpr21", "gpr22", "gpr23",
 "gpr24", "gpr25", "gpr26", "gpr27",
 "gpr28", "gpr29", "gpr30", "gpr31" };

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

    cout << "PC: "  << hex << ppc_state.ppc_pc            << endl;
    cout << "LR: "  << hex << ppc_state.ppc_spr[SPR::LR]  << endl;
    cout << "CR: "  << hex << ppc_state.ppc_cr            << endl;
    cout << "CTR: " << hex << ppc_state.ppc_spr[SPR::CTR] << endl;
    cout << "XER: " << hex << ppc_state.ppc_spr[SPR::XER] << endl;
    cout << "MSR: " << hex << ppc_state.ppc_msr           << endl;
}

bool find_gpr(string entry) {
    for (int gpr_index = 0; gpr_index < 32; gpr_index++) {
        string str_grab = array_gprs[gpr_index];
        if (str_grab.compare(entry) == 0) {
            gpr_num = gpr_index;
            return 1;
        }
    }

    return 0;
}

void enter_debugger()
{
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd, reg_value_str;
    uint32_t addr, reg_value;
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
                addr = stoul(addr_str, NULL, 0);
                ppc_state.ppc_pc = addr;
            }
            else if (find_gpr(reg_expr)){
                reg_value_str = expr_str.substr(expr_str.find("=") + 1);
                reg_value = stoul(addr_str, NULL, 0);
                ppc_state.ppc_gpr[gpr_num] = reg_value;
            }
            else {
                cout << "Unknown register " << reg_expr << endl;
            }
        } else if (cmd == "step") {
            ppc_exec_single();
        } else if (cmd == "until") {
            ss >> addr_str;
            addr = stoul(addr_str, NULL, 16);
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
