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
#include <iomanip>
#include <fstream>
#include <map>
#include "ppcemu.h"
#include "../cpu/ppc/ppcmmu.h"
#include "../cpu/ppc/ppcdisasm.h"


using namespace std;

static uint32_t str2addr(string& addr_str)
{
    try {
        return stoul(addr_str, NULL, 0);
    }
    catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + addr_str);
    }
}

static void show_help()
{
    cout << "Debugger commands:" << endl;
    cout << "  step      -- execute single instruction" << endl;
    cout << "  si        -- shortcut for step" << endl;
    cout << "  next      -- same as step but treats subroutine calls" << endl;
    cout << "               as single instructions." << endl;
    cout << "  ni        -- shortcut for next" << endl;
    cout << "  until X   -- execute until address X is reached" << endl;
    cout << "  regs      -- dump content of the GRPs" << endl;
    cout << "  set R=X   -- assign value X to register R" << endl;
    cout << "  dump NT,X -- dump N memory cells of size T at address X" << endl;
    cout << "               T can be b(byte), w(word), d(double)," << endl;
    cout << "               q(quad) or c(character)." << endl;
#ifdef PROFILER
    cout << "  profiler  -- show stats related to the processor" << endl;
#endif
    cout << "  disas N,X -- disassemble N instructions starting at address X" << endl;
    cout << "               X can be any number or a known register name" << endl;
    cout << "               disas with no arguments defaults to disas 1,pc" << endl;
    cout << "  quit      -- quit the debugger" << endl << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

static void disasm(uint32_t count, uint32_t address)
{
    PPCDisasmContext ctx;

    ctx.instr_addr = address;
    ctx.simplified = true;

    for (int i = 0; i < count; i++) {
        ctx.instr_code = mem_read_dbg(ctx.instr_addr, 4);
        cout << uppercase << hex << ctx.instr_addr;
        cout << "    " << disassemble_single(&ctx) << endl;
    }
}

static void dump_mem(string& params)
{
    int cell_size, chars_per_line;
    bool is_char;
    uint32_t count, addr;
    uint64_t val;
    string num_type_str, addr_str;

    num_type_str = params.substr(0, params.find(","));
    addr_str = params.substr(params.find(",") + 1);

    is_char = false;

    switch(num_type_str.back()) {
        case 'b':
        case 'B':
            cell_size = 1;
            break;
        case 'w':
        case 'W':
            cell_size = 2;
            break;
        case 'd':
        case 'D':
            cell_size = 4;
            break;
        case 'q':
        case 'Q':
            cell_size = 8;
            break;
        case 'c':
        case 'C':
            cell_size = 1;
            is_char = true;
            break;
        default:
            cout << "Invalid data type " << num_type_str << endl;
            return;
    }

    try {
        num_type_str = num_type_str.substr(0, num_type_str.length() - 1);
        count = str2addr(num_type_str);
    }
    catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    try {
        addr  = str2addr(addr_str);
    }
    catch (invalid_argument& exc) {
        try {
            /* number conversion failed, trying reg name */
            addr = get_reg(addr_str);
        }
        catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return;
        }
    }

    cout << "Dumping memory at address " << hex << addr << ":" << endl;

    chars_per_line = 0;

    try {
        for (int i = 0; i < count; addr += cell_size, i++) {
            if (chars_per_line + cell_size * 2 > 80) {
                cout << endl;
                chars_per_line = 0;
            }
            val = mem_read_dbg(addr, cell_size);
            if (is_char) {
                cout << (char)val;
                chars_per_line += cell_size;
            } else {
                cout << setw(cell_size * 2) << setfill('0') << uppercase <<
                    hex << val << "  ";
                chars_per_line += cell_size * 2 + 2;
            }
        }
    }
    catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    cout << endl << endl;
}

void enter_debugger()
{
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd, reg_value_str, inst_string, inst_num_str;
    uint32_t addr, inst_grab;
    std::stringstream ss;

    cout << "Welcome to the DingusPPC command line debugger." << endl;
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
        }
        else if (cmd == "quit") {
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
            print_gprs();
        }
        else if (cmd == "set") {
            ss >> expr_str;
            try {
                reg_expr = expr_str.substr(0, expr_str.find("="));
                addr_str = expr_str.substr(expr_str.find("=") + 1);
                addr = str2addr(addr_str);
                set_reg(reg_expr, addr);
            }
            catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        }
        else if (cmd == "step" || cmd == "si") {
            ppc_exec_single();
        }
        else if (cmd == "next" || cmd == "ni") {
            addr_str = "PC";
            addr = get_reg(addr_str) + 4;
            ppc_exec_until(addr);
        }
        else if (cmd == "until") {
            ss >> addr_str;
            try {
                addr = str2addr(addr_str);
                ppc_exec_until(addr);
            }
            catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        }
        else if (cmd == "disas") {
            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                inst_num_str = expr_str.substr(0, expr_str.find(","));
                inst_grab = stol(inst_num_str, NULL, 0);
                addr_str = expr_str.substr(expr_str.find(",") + 1);
                try {
                    addr = str2addr(addr_str);
                }
                catch (invalid_argument& exc) {
                    try {
                        /* number conversion failed, trying reg name */
                        addr = get_reg(addr_str);
                    }
                    catch (invalid_argument& exc) {
                        cout << exc.what() << endl;
                        continue;
                    }
                }
                try {
                    disasm(inst_grab, addr);
                }
                catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            }
            else {
                addr_str = "PC";
                addr = get_reg(addr_str);
                disasm(1, addr);
            }
        }
        else if (cmd == "dump") {
            expr_str = "";
            ss >> expr_str;
            dump_mem(expr_str);
        }
        else {
            cout << "Unknown command: " << cmd << endl;
            continue;
        }
        last_cmd = cmd;
    }
}
