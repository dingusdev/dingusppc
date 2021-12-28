/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>
#include <map>
#include <sstream>
#include <stdio.h>
#include <string>
#include "../cpu/ppc/ppcdisasm.h"
#include "../cpu/ppc/ppcemu.h"
#include "../cpu/ppc/ppcmmu.h"
#include "memaccess.h"
#include "utils/profiler.h"

#ifdef ENABLE_68K_DEBUGGER // optionally defined in CMakeLists.txt
    #include <capstone/capstone.h>
#endif

using namespace std;

static uint32_t str2addr(string& addr_str) {
    try {
        return stoul(addr_str, NULL, 0);
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + addr_str);
    }
}

static uint32_t str2num(string& num_str) {
    try {
        return stol(num_str, NULL, 0);
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + num_str);
    }
}

static void show_help() {
    cout << "Debugger commands:" << endl;
    cout << "  step [N]     -- execute single instruction" << endl;
    cout << "                  N is an optional step count" << endl;
    cout << "  si [N]       -- shortcut for step" << endl;
    cout << "  next         -- same as step but treats subroutine calls" << endl;
    cout << "                  as single instructions." << endl;
    cout << "  ni           -- shortcut for next" << endl;
    cout << "  until X      -- execute until address X is reached" << endl;
    cout << "  regs         -- dump content of the GRPs" << endl;
    cout << "  set R=X      -- assign value X to register R" << endl;
    cout << "                  if R=loglevel, set the internal" << endl;
    cout << "                  log level to X whose range is -2...9" << endl;
    cout << "  dump NT,X    -- dump N memory cells of size T at address X" << endl;
    cout << "                  T can be b(byte), w(word), d(double)," << endl;
    cout << "                  q(quad) or c(character)." << endl;
    cout << "  profile C N  -- run subcommand C on profile N" << endl;
    cout << "                  supported subcommands:" << endl;
    cout << "                  'show' - show profile report" << endl;
    cout << "                  'reset' - reset profile variables" << endl;
#ifdef PROFILER
    cout << "  profiler     -- show stats related to the processor" << endl;
#endif
    cout << "  disas N,X    -- disassemble N instructions starting at address X" << endl;
    cout << "                  X can be any number or a known register name" << endl;
    cout << "                  disas with no arguments defaults to disas 1,pc" << endl;
    cout << "  da N,X       -- shortcut for disas" << endl;
#ifdef ENABLE_68K_DEBUGGER
    cout << "  context X    -- switch to the debugging context X." << endl;
    cout << "                  X can be either 'ppc' (default) or '68k'" << endl;
    cout << "                  Use 68k for debugging emulated 68k code only." << endl;
#endif
    cout << "  quit         -- quit the debugger" << endl << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

#ifdef ENABLE_68K_DEBUGGER

static void disasm_68k(uint32_t count, uint32_t address) {
    csh cs_handle;
    uint8_t code[10];
    size_t code_size;
    uint64_t dis_addr;

    if (cs_open(CS_ARCH_M68K, CS_MODE_M68K_040, &cs_handle) != CS_ERR_OK) {
        cout << "Capstone initialization error" << endl;
        return;
    }

    cs_insn* insn = cs_malloc(cs_handle);

    for (; count > 0; count--) {
        /* prefetch opcode bytes (a 68k instruction can occupy 2...10 bytes) */
        for (int i = 0; i < sizeof(code); i++) {
            code[i] = mem_read_dbg(address + i, 1);
        }

        const uint8_t *code_ptr  = code;
        code_size = sizeof(code);
        dis_addr  = address;

        // catch and handle F-Traps (Nanokernel calls) ourselves because
        // Capstone will likely return no meaningful assembly for them
        if ((code[0] & 0xF0) == 0xF0) {
            goto print_bin;
        }

        if (cs_disasm_iter(cs_handle, &code_ptr, &code_size, &dis_addr, insn)) {
            cout << uppercase << hex << insn->address << "    ";
            cout << setfill(' ');
            cout << setw(10) << left << insn->mnemonic << insn->op_str << endl;
            address = dis_addr;
        } else {
print_bin:
            cout << uppercase << hex << address << "    ";
            cout << setfill(' ');
            cout << setw(10) << left << "dc.w" << "$" << hex <<
                ((code[0] << 8) | code[1]) << endl;
            address += 2;
        }
    }

    cs_free(insn, 1);
    cs_close(&cs_handle);
}

/* emulator opcode table size --> 512 KB */
#define EMU_68K_TABLE_SIZE 0x80000

/** Execute one emulated 68k instruction. */
void exec_single_68k()
{
    string reg;
    uint32_t emu_table_virt, cur_68k_pc, cur_instr_tab_entry, ppc_pc;

    /* PPC r24 contains 68k PC advanced by two bytes
       as part of instruction prefetching */
    reg = "R24";
    cur_68k_pc = get_reg(reg) - 2;

    /* PPC r29 contains base address of the emulator opcode table */
    reg = "R29";
    emu_table_virt = get_reg(reg) & 0xFFF80000;

    /* calculate address of the current opcode table entry as follows:
       get_word(68k_PC) * entry_size + table_base */
    cur_instr_tab_entry = mmu_read_vmem<uint16_t>(cur_68k_pc) * 8 + emu_table_virt;

    /* grab the PPC PC too */
    reg = "PC";
    ppc_pc = get_reg(reg);

    //printf("cur_instr_tab_entry = %X\n", cur_instr_tab_entry);

    /* because the first two PPC instructions for each emulated 68k once
       are resided in the emulator opcode table, we need to execute them
       one by one until the execution goes outside the opcode table. */
    while (ppc_pc >= cur_instr_tab_entry && ppc_pc < cur_instr_tab_entry + 8) {
        ppc_exec_single();
        reg = "PC";
        ppc_pc = get_reg(reg);
        LOG_F(9, "Tracing within emulator table, PC = %X\n", ppc_pc);
    }

    /* Getting here means we're outside the emualtor opcode table.
       Execute PPC code until we hit the opcode table again. */
    LOG_F(9, "Tracing outside the emulator table, PC = %X\n", ppc_pc);
    ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
}

/** Execute emulated 68k code until target_addr is reached. */
void exec_until_68k(uint32_t target_addr)
{
    string reg;
    uint32_t emu_table_virt, ppc_pc;

    reg = "R29";
    emu_table_virt = get_reg(reg) & 0xFFF80000;

    reg = "R24";
    while (target_addr != (get_reg(reg) - 2)) {
        reg = "PC";
        ppc_pc = get_reg(reg);

        if (ppc_pc >= emu_table_virt && ppc_pc < (emu_table_virt + EMU_68K_TABLE_SIZE - 1)) {
            LOG_F(9, "Tracing within emulator table, PC = %X\n", ppc_pc);
            ppc_exec_single();
        } else {
            LOG_F(9, "Tracing outside the emulator table, PC = %X\n", ppc_pc);
            ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
        }
        reg = "R24";
    }
}

void print_68k_regs()
{
    int i;
    string reg;

    for (i = 0; i < 8; i++) {
        reg = "R" + to_string(i + 8);
        cout << "D" << dec << i << " : " << uppercase << hex << get_reg(reg) << endl;
    }

    for (i = 0; i < 7; i++) {
        reg = "R" + to_string(i + 16);
        cout << "A" << dec << i << " : " << uppercase << hex << get_reg(reg) << endl;
    }
    reg = "R1";
    cout << "A7 : " << uppercase << hex << get_reg(reg) << endl;

    reg = "R24";
    cout << "PC: " << uppercase << hex << get_reg(reg) - 2 << endl;

    reg = "R25";
    cout << "SR: " << uppercase << hex << ((get_reg(reg) & 0xFF) << 8) << endl;

    reg = "R26";
    cout << "CCR: " << uppercase << hex << get_reg(reg) << endl;
}

#endif // ENABLE_68K_DEBUGGER

static void dump_mem(string& params) {
    int cell_size, chars_per_line;
    bool is_char;
    uint32_t count, addr;
    uint64_t val;
    string num_type_str, addr_str;
    size_t separator_pos;

    separator_pos = params.find_first_of(",");
    if (separator_pos == std::string::npos) {
        cout << "dump: not enough arguments specified." << endl;
        return;
    }

    num_type_str = params.substr(0, params.find_first_of(","));
    addr_str     = params.substr(params.find_first_of(",") + 1);

    is_char = false;

    switch (num_type_str.back()) {
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
        is_char   = true;
        break;
    default:
        cout << "Invalid data type " << num_type_str << endl;
        return;
    }

    try {
        num_type_str = num_type_str.substr(0, num_type_str.length() - 1);
        count        = str2addr(num_type_str);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    try {
        addr = str2addr(addr_str);
    } catch (invalid_argument& exc) {
        try {
            /* number conversion failed, trying reg name */
            addr = get_reg(addr_str);
        } catch (invalid_argument& exc) {
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
                cout << setw(cell_size * 2) << setfill('0') << right;
                cout << uppercase << hex << val << "  ";
                chars_per_line += cell_size * 2 + 2;
            }
        }
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    cout << endl << endl;
}

static void disasm(uint32_t count, uint32_t address) {
    PPCDisasmContext ctx;

    ctx.instr_addr = address;
    ctx.simplified = true;

    for (int i = 0; i < count; i++) {
        ctx.instr_code = READ_DWORD_BE_A(mmu_translate_imem(ctx.instr_addr));
        cout << uppercase << hex << ctx.instr_addr;
        cout << "    " << disassemble_single(&ctx) << endl;
    }
}

static void print_gprs() {
    string reg_name;
    int i;

    for (i = 0; i < 32; i++) {
        reg_name = "R" + to_string(i);

        cout << right << std::setw(3) << setfill(' ') << reg_name << " : " <<
            setw(8) << setfill('0') << right << uppercase << hex << get_reg(reg_name);

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }
    }

    array<string,6> sprs = {"PC", "LR", "CR", "CTR", "XER", "MSR"};

    for (auto &spr : sprs) {
        cout << right << std::setw(3) << setfill(' ') << spr << " : " <<
            setw(8) << setfill('0') << uppercase << hex << get_reg(spr);

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }

        i++;
    }
}

void enter_debugger() {
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd, reg_value_str,
           inst_string, inst_num_str, profile_name, sub_cmd;
    uint32_t addr, inst_grab;
    std::stringstream ss;
    int log_level, context;
    size_t separator_pos;

    context = 1; /* start with the PowerPC context */

    cout << "Welcome to the DingusPPC command line debugger." << endl;
    cout << "Please enter a command or 'help'." << endl << endl;

    while (1) {
        cout << "dingusdbg> ";

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
        } else if (cmd == "profile") {
            ss >> sub_cmd;
            ss >> profile_name;

            if (sub_cmd == "show") {
                gProfilerObj->print_profile(profile_name);
            } else if (sub_cmd == "reset") {
                gProfilerObj->reset_profile(profile_name);
            } else {
                cout << "Unknown/empty subcommand " << sub_cmd << endl;
            }
        }
        else if (cmd == "regs") {
            if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                print_68k_regs();
#endif
            } else {
                print_gprs();
            }
        } else if (cmd == "set") {
            ss >> expr_str;

            separator_pos = expr_str.find_first_of("=");
            if (separator_pos == std::string::npos) {
                cout << "set: not enough arguments specified." << endl;
                continue;
            }

            try {
                reg_expr = expr_str.substr(0, expr_str.find_first_of("="));
                addr_str = expr_str.substr(expr_str.find_first_of("=") + 1);
                if (reg_expr == "loglevel") {
                    log_level = str2num(addr_str);
                    if (log_level < -2 || log_level > 9) {
                        cout << "Log level must be in the range -2...9!" << endl;
                        continue;
                    }
                    loguru::g_stderr_verbosity = log_level;
                } else {
                    addr = str2addr(addr_str);
                    set_reg(reg_expr, addr);
                }
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        } else if (cmd == "step" || cmd == "si") {
            int count;

            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                try {
                    count = str2num(expr_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    count = 1;
                }
            } else {
                count = 1;
            }

            if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                for (; --count >= 0;) {
                    exec_single_68k();
                }
#endif
            } else {
                for (; --count >= 0;) {
                    ppc_exec_single();
                }
            }
        } else if (cmd == "next" || cmd == "ni") {
            addr_str = "PC";
            addr     = get_reg(addr_str) + 4;
            ppc_exec_until(addr);
        } else if (cmd == "until") {
            ss >> addr_str;
            try {
                addr = str2addr(addr_str);
                if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                    exec_until_68k(addr);
#endif
                } else {
                    auto start_time = std::chrono::steady_clock::now();
                    ppc_exec_until(addr);
                    auto end_time = std::chrono::steady_clock::now();
                    auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
                    LOG_F(INFO, "Time elapsed: %lld ns", time_elapsed.count());
                }
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        } else if (cmd == "disas" || cmd == "da") {
            expr_str = "";
            ss >> expr_str;
            if (expr_str.length() > 0) {
                separator_pos = expr_str.find_first_of(",");
                if (separator_pos == std::string::npos) {
                    cout << "disas: not enough arguments specified." << endl;
                    continue;
                }

                inst_num_str = expr_str.substr(0, expr_str.find_first_of(","));
                try {
                    inst_grab = str2num(inst_num_str);
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                    continue;
                }

                addr_str = expr_str.substr(expr_str.find_first_of(",") + 1);
                try {
                    addr = str2addr(addr_str);
                } catch (invalid_argument& exc) {
                    try {
                        /* number conversion failed, trying reg name */
                        if (context == 2 && (addr_str == "pc" || addr_str == "PC")) {
                            addr_str = "R24";
                            addr = get_reg(addr_str) - 2;
                        } else {
                            addr = get_reg(addr_str);
                        }
                    } catch (invalid_argument& exc) {
                        cout << exc.what() << endl;
                        continue;
                    }
                }
                try {
                    if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                        disasm_68k(inst_grab, addr);
#endif
                    } else {
                        disasm(inst_grab, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            } else {
                /* disas without arguments defaults to disas 1,pc */
                try {
                    if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                        addr_str = "R24";
                        addr     = get_reg(addr_str);
                        disasm_68k(1, addr - 2);
#endif
                    } else {
                        addr_str = "PC";
                        addr     = get_reg(addr_str);
                        disasm(1, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            }
        } else if (cmd == "dump") {
            expr_str = "";
            ss >> expr_str;
            dump_mem(expr_str);
#ifdef ENABLE_68K_DEBUGGER
        } else if (cmd == "context") {
            expr_str = "";
            ss >> expr_str;
            if (expr_str == "ppc" || expr_str == "PPC") {
                context = 1;
            } else if (expr_str == "68k" || expr_str == "68K") {
                context = 2;
            } else {
                cout << "Unknown debugging context: " << expr_str << endl;
            }
#endif
        } else {
            cout << "Unknown command: " << cmd << endl;
            continue;
        }
        last_cmd = cmd;
    }
}
