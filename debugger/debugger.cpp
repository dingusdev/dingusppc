#include <stdio.h>
#include <string>
#include <sstream>
#include <iostream>
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

void execute_single_instr()
{
    quickinstruction_translate(ppc_state.ppc_pc);
    //cout << "Current instruction: " << hex << ppc_cur_instruction << endl;
    ppc_main_opcode();
    if (grab_branch && !grab_exception) {
        //cout << "Grab branch, EA: " << hex << ppc_next_instruction_address << endl;
        ppc_state.ppc_pc = ppc_next_instruction_address;
        grab_branch = 0;
        ppc_tbr_update();
    }
    else if (grab_return || grab_exception) {
        ppc_state.ppc_pc = ppc_next_instruction_address;
        grab_exception = 0;
        grab_return = 0;
        ppc_tbr_update();
    }
    else {
        ppc_state.ppc_pc += 4;
        ppc_tbr_update();
    }
}

void execute_until(uint32_t goal_addr)
{
    while(ppc_state.ppc_pc != goal_addr)
        execute_single_instr();
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
        } else if (cmd == "regs") {
            dump_regs();
        } else if (cmd == "step") {
            execute_single_instr();
        } else if (cmd == "until") {
            ss >> addr_str;
            addr = stol(addr_str, NULL, 16);
            execute_until(addr);
        } else if (cmd == "disas") {
            cout << "Disassembling not implemented yet. Sorry!" << endl;
        } else {
            cout << "Unknown command: " << cmd << endl;
            continue;
        }
        last_cmd = cmd;
    }
}
