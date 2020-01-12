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
    cout << "  memdump   -- dump content of the system memory to memdump.bin" << endl;
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

void dump_mem_file()
{
    std::ofstream memdumpfile;
    memdumpfile.open("memdump.bin", std::ofstream::out | std::ofstream::binary);
    memdumpfile.write((char *)&machine_sysram_mem, sizeof(char) * 67108864);
    memdumpfile.close();
}

void enter_debugger()
{
    string inp, cmd, addr_str, last_cmd;
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
        else if (cmd == "memdump") {
            dump_mem_file();
        }
        else if (cmd == "regs") {
            dump_regs();
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
