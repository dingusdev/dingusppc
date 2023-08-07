/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#include <cpu/ppc/ppcdisasm.h>
#include <cpu/ppc/ppcemu.h>
#include <cpu/ppc/ppcmmu.h>
#include <debugger/debugger.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/ofnvram.h>
#include "memaccess.h"
#include <utils/profiler.h>

#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <loguru.hpp>
#include <map>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <string>

#ifdef DEBUG_CPU_INT
#include <machines/machinebase.h>
#include <devices/common/viacuda.h>
#endif

#ifdef ENABLE_68K_DEBUGGER // optionally defined in CMakeLists.txt
    #include <capstone/capstone.h>
#endif

using namespace std;

#define COUTX uppercase << hex
#define COUT0_X(w) setfill('0') << setw(w) << right << COUTX
#define COUT016X COUT0_X(16)
#define COUT08X COUT0_X(8)
#define COUT04X COUT0_X(4)

static uint32_t str2addr(string& addr_str) {
    try {
        return static_cast<uint32_t>(stoul(addr_str, NULL, 0));
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + addr_str);
    }
}

static uint32_t str2num(string& num_str) {
    try {
        return static_cast<uint32_t>(stol(num_str, NULL, 0));
    } catch (invalid_argument& exc) {
        throw invalid_argument(string("Cannot convert ") + num_str);
    }
}

static void show_help() {
    cout << "Debugger commands:" << endl;
    cout << "  step [N]       -- execute single instruction" << endl;
    cout << "                    N is an optional step count" << endl;
    cout << "  si [N]         -- shortcut for step" << endl;
    cout << "  next           -- same as step but treats subroutine calls" << endl;
    cout << "                    as single instructions." << endl;
    cout << "  ni             -- shortcut for next" << endl;
    cout << "  until X        -- execute until address X is reached" << endl;
    cout << "  go             -- exit debugger and continue emulator execution" << endl;
    cout << "  regs           -- dump content of the GPRs" << endl;
    cout << "  fregs          -- dump content of the FPRs" << endl;
    cout << "  mregs          -- dump content of the MMU registers" << endl;
    cout << "  set R=X        -- assign value X to register R" << endl;
    cout << "                    if R=loglevel, set the internal" << endl;
    cout << "                    log level to X whose range is -2...9" << endl;
    cout << "  dump NT,X      -- dump N memory cells of size T at address X" << endl;
    cout << "                    T can be b(byte), w(word), d(double)," << endl;
    cout << "                    q(quad) or c(character)." << endl;
    cout << "  setmem X=V.T   -- set memory at address X to value V of size T" << endl;
    cout << "                    T can be b(byte), w(word), d(double)," << endl;
    cout << "                    q(quad) or c(character)." << endl;
    cout << "  regions        -- dump memory regions" << endl;
    cout << "  profile C N    -- run subcommand C on profile N" << endl;
    cout << "                    supported subcommands:" << endl;
    cout << "                    'show' - show profile report" << endl;
    cout << "                    'reset' - reset profile variables" << endl;
#ifdef PROFILER
    cout << "  profiler       -- show stats related to the processor" << endl;
#endif
    cout << "  disas N,X      -- disassemble N instructions starting at address X" << endl;
    cout << "                    X can be any number or a known register name" << endl;
    cout << "                    disas with no arguments defaults to disas 1,pc" << endl;
    cout << "  da N,X         -- shortcut for disas" << endl;
#ifdef ENABLE_68K_DEBUGGER
    cout << "  context X      -- switch to the debugging context X." << endl;
    cout << "                    X can be either 'ppc' (default) or '68k'" << endl;
    cout << "                    Use 68k for debugging emulated 68k code only." << endl;
#endif
    cout << "  printenv       -- print current NVRAM settings." << endl;
    cout << "  setenv V N     -- set NVRAM variable V to value N." << endl;
    cout << endl;
    cout << "  restart        -- restart the machine" << endl;
    cout << "  quit           -- quit the debugger" << endl;
    cout << endl;
    cout << "Pressing ENTER will repeat last command." << endl;
}

#ifdef ENABLE_68K_DEBUGGER

static uint32_t disasm_68k(uint32_t count, uint32_t address) {
    csh cs_handle;
    uint8_t code[12]{};
    size_t code_size;
    uint64_t dis_addr;

    if (cs_open(CS_ARCH_M68K, CS_MODE_M68K_040, &cs_handle) != CS_ERR_OK) {
        cout << "Capstone initialization error" << endl;
        return address;
    }

    cs_insn* insn = cs_malloc(cs_handle);

    for (; power_on && count > 0; count--) {
        // prefetch opcode bytes (a 68k instruction can occupy 2...12 bytes)
        for (int i = 0; i < sizeof(code); i++) {
            try {
                code[i] = mem_read_dbg(address + i, 1);
            }
            catch(...) {
                printf("<memerror>");
            }
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
            cout << COUTX << insn->address << "    ";
            cout << setfill(' ');
            cout << setw(10) << left << insn->mnemonic << insn->op_str << right << endl;
            address = static_cast<uint32_t>(dis_addr);
        } else {
print_bin:
            cout << COUTX << address << "    ";
            cout << setfill(' ');
            cout << setw(10) << left << "dc.w" << "$" << hex <<
                ((code[0] << 8) | code[1]) << right << endl;
            address += 2;
        }
    }

    cs_free(insn, 1);
    cs_close(&cs_handle);
    return address;
}

/* emulator opcode table size --> 512 KB */
constexpr auto EMU_68K_TABLE_SIZE = 0x80000;

/** Execute one emulated 68k instruction. */
void exec_single_68k()
{
    uint32_t emu_table_virt, cur_68k_pc, cur_instr_tab_entry, ppc_pc;

    /* PPC r24 contains 68k PC advanced by two bytes
       as part of instruction prefetching */
    cur_68k_pc = static_cast<uint32_t>(get_reg(string("R24")) - 2);

    /* PPC r29 contains base address of the emulator opcode table */
    emu_table_virt = get_reg(string("R29")) & 0xFFF80000;

    /* calculate address of the current opcode table entry as follows:
       get_word(68k_PC) * entry_size + table_base */
    cur_instr_tab_entry = mmu_read_vmem<uint16_t>(NO_OPCODE, cur_68k_pc) * 8 + emu_table_virt;

    /* grab the PPC PC too */
    ppc_pc = static_cast<uint32_t>(get_reg(string("PC")));

    //printf("cur_instr_tab_entry = %X\n", cur_instr_tab_entry);

    /* because the first two PPC instructions for each emulated 68k opcode
       are resided in the emulator opcode table, we need to execute them
       one by one until the execution goes outside the opcode table. */
    while (power_on && ppc_pc >= cur_instr_tab_entry && ppc_pc < cur_instr_tab_entry + 8) {
        ppc_exec_single();
        ppc_pc = static_cast<uint32_t>(get_reg(string("PC")));
    }

    /* Getting here means we're outside the emualtor opcode table.
       Execute PPC code until we hit the opcode table again. */
    ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
}

/** Execute emulated 68k code until target_addr is reached. */
void exec_until_68k(uint32_t target_addr)
{
    uint32_t emu_table_virt, ppc_pc;

    emu_table_virt = get_reg(string("R29")) & 0xFFF80000;

    while (power_on && target_addr != (get_reg(string("R24")) - 2)) {
        ppc_pc = static_cast<uint32_t>(get_reg(string("PC")));

        if (ppc_pc >= emu_table_virt && ppc_pc < (emu_table_virt + EMU_68K_TABLE_SIZE - 1)) {
            ppc_exec_single();
        } else {
            ppc_exec_dbg(emu_table_virt, EMU_68K_TABLE_SIZE - 1);
        }
    }
}

void print_68k_regs()
{
    int i;
    string reg;

    for (i = 0; i < 8; i++) {
        reg = "R" + to_string(i + 8);
        cout << "   D" << dec << i << " : " << COUT08X << get_reg(reg) << endl;
    }

    for (i = 0; i < 7; i++) {
        reg = "R" + to_string(i + 16);
        cout << "   A" << dec << i << " : " << COUT08X << get_reg(reg) << endl;
    }

    cout << "   A7 : " << COUT08X << get_reg(string("R1")) << endl;

    cout << "   PC : " << COUT08X << get_reg(string("R24")) - 2 << endl;

    cout << "   SR : " << COUT08X << ((get_reg("R25") & 0xFF) << 8) << endl;

    cout << "  CCR : " << COUT08X << get_reg(string("R26")) << endl;
    cout << dec << setfill(' ');
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
                cout << COUT0_X(cell_size * 2) << val << "  ";
                chars_per_line += cell_size * 2 + 2;
            }
        }
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    cout << endl << endl;
}

static void patch_mem(string& params) {
    int value_size = 1;
    uint32_t addr, value;
    string addr_str, value_str, size_str;

    if (params.find_first_of("=") == std::string::npos) {
        cout << "setmem: not enough arguments specified. Try 'help'." << endl;
        return;
    }

    addr_str  = params.substr(0, params.find_first_of("="));
    value_str = params.substr(params.find_first_of("=") + 1);

    if (value_str.find_first_of(".") == std::string::npos) {
        cout << "setmem: no value size specified. Try 'help'." << endl;
        return;
    }

    size_str  = value_str.substr(value_str.find_first_of(".") + 1);
    value_str = value_str.substr(0, value_str.find_first_of("."));

    switch (size_str.back()) {
    case 'b':
    case 'B':
        value_size = 1;
        break;
    case 'w':
    case 'W':
        value_size = 2;
        break;
    case 'd':
    case 'D':
        value_size = 4;
        break;
    case 'q':
    case 'Q':
        value_size = 8;
        break;
    case 'c':
    case 'C':
        value_size = 1;
        break;
    default:
        cout << "Invalid value size " << size_str << endl;
        return;
    }

    try {
        addr = str2addr(addr_str);
    } catch (invalid_argument& exc) {
        try {
            // number conversion failed, trying reg name
            addr = get_reg(addr_str);
        } catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return;
        }
    }

    try {
        value = str2num(value_str);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
        return;
    }

    try {
        mem_write_dbg(addr, value, value_size);
    } catch (invalid_argument& exc) {
        cout << exc.what() << endl;
    }
}

static uint32_t disasm(PPCDisasmContext &ctx) {
    ctx.instr_code = READ_DWORD_BE_A(mmu_translate_imem(ctx.instr_addr));
    cout << COUT08X << ctx.instr_addr;
    cout << ": " << COUT08X << ctx.instr_code;
    cout << "    " << disassemble_single(&ctx) << setfill(' ') << right << dec;
    return ctx.instr_addr;
}

static uint32_t disasm(uint32_t count, uint32_t address) {
    PPCDisasmContext ctx;
    ctx.instr_addr = address;
    ctx.simplified = true;
    for (int i = 0; power_on && i < count; i++) {
        disasm(ctx);
        cout << endl;
    }
    return ctx.instr_addr;
}

static void disasm_in(PPCDisasmContext &ctx, uint32_t address) {
    ctx.instr_addr = address;
    ctx.simplified = true;
    disasm(ctx);
    if (ctx.regs_in.size() > 0 || ctx.regs_out.size() > 0) {
        if (ctx.instr_str.length() < 28)
            cout << setw(28 - (int)ctx.instr_str.length()) << " ";
        cout << " ;";
        if (ctx.regs_in.size() > 0) {
            cout << " in{" << COUTX;
            for (auto & reg_name : ctx.regs_in) {
                cout << " " << reg_name << ":" << get_reg(reg_name);
            }
            cout << " }" << dec;
        }
    }
}

static void disasm_out(PPCDisasmContext &ctx) {
    if (ctx.regs_out.size() > 0) {
        cout << " out{" << COUTX;
        for (auto & reg_name : ctx.regs_out) {
            cout << " " << reg_name << ":" << get_reg(reg_name);
        }
        cout << " }" << dec;
    }
    cout << endl;
}

static void print_gprs() {
    string reg_name;
    int i;

    for (i = 0; i < 32; i++) {
        reg_name = "r" + to_string(i);

        cout << right << std::setw(5) << setfill(' ') << reg_name << " : " <<
            COUT08X << get_reg(reg_name) << setfill(' ');

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }
    }

    array<string, 8> sprs = {"pc", "lr", "cr", "ctr", "xer", "msr", "srr0", "srr1"};

    for (auto &spr : sprs) {
        cout << right << std::setw(5) << setfill(' ') << spr << " : " <<
            COUT08X << get_reg(spr) << setfill(' ');

        if (i & 1) {
            cout << endl;
        } else {
            cout << "\t\t";
        }

        i++;
    }
}

static void print_fprs() {
    string reg_name;
    for (int i = 0; i < 32; i++) {
        reg_name = "f" + to_string(i);
        cout << right << std::setw(6) << setfill(' ') << reg_name << " : " <<
            COUT016X << ppc_state.fpr[i].int64_r <<
            " = " << left << setfill(' ') << ppc_state.fpr[i].dbl64_r << endl;
    }
    cout << right << std::setw(6) << setfill(' ') << "fpscr" << " : " <<
        COUT08X << ppc_state.fpscr << setfill(' ') << endl;
}

extern bool is_601;

static void print_mmu_regs()
{
    printf(" msr : %08X\n", ppc_state.msr);

    printf("\nBAT registers:\n");

    for (int i = 0; i < 4; i++) {
        printf(" ibat%du : %08X   ibat%dl : %08X\n",
              i, ppc_state.spr[528+i*2],
              i, ppc_state.spr[529+i*2]);
    }

    if (!is_601) {
        for (int i = 0; i < 4; i++) {
            printf(" dbat%du : %08X   dbat%dl : %08X\n",
                  i, ppc_state.spr[536+i*2],
                  i, ppc_state.spr[537+i*2]);
        }
    }

    printf("\n");
    printf(" sdr1 : %08X\n", ppc_state.spr[SPR::SDR1]);
    printf("\nSegment registers:\n");

    for (int i = 0; i < 16; i++) {
        printf(" %ssr%d : %08X\n", (i < 10) ? " " : "", i, ppc_state.sr[i]);
    }
}

#ifndef _WIN32

#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

static struct sigaction    old_act_sigint, new_act_sigint;
static struct sigaction    old_act_sigterm, new_act_sigterm;
static struct termios      orig_termios;

static void mysig_handler(int signum)
{
    // restore original terminal state
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

    // restore original signal handler for SIGINT
    signal(SIGINT, old_act_sigint.sa_handler);
    signal(SIGTERM, old_act_sigterm.sa_handler);

    LOG_F(INFO, "Old terminal state restored, SIG#=%d", signum);

    // re-post signal
    raise(signum);
}
#endif

static void delete_prompt() {
#ifndef _WIN32
    // move up, carriage return (move to column 0), erase from cursor to end of line
    cout << "\e[A\r\e[0K";
#endif
}

DppcDebugger::DppcDebugger() {

}

void DppcDebugger::enter_debugger() {
    string inp, cmd, addr_str, expr_str, reg_expr, last_cmd, reg_value_str,
           inst_string, inst_num_str, profile_name, sub_cmd;
    uint32_t addr, inst_grab;
    std::stringstream ss;
    int log_level, context;
    size_t separator_pos;
    bool did_message = false;
    uint32_t next_addr_ppc;
#ifdef ENABLE_68K_DEBUGGER
    uint32_t next_addr_68k;
#endif
    bool cmd_repeat;
    int repeat_count = 0;

    unique_ptr<OfConfigUtils> ofnvram = unique_ptr<OfConfigUtils>(new OfConfigUtils);

    context = 1; /* start with the PowerPC context */

#ifndef _WIN32
    struct winsize win_size_previous;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size_previous);
#endif

    while (1) {
        if (power_off_reason == po_shut_down) {
            power_off_reason = po_shutting_down;
            break;
        }
        if (power_off_reason == po_restart) {
            power_off_reason = po_restarting;
            break;
        }
        if (power_off_reason == po_quit) {
            power_off_reason = po_quitting;
            break;
        }
        power_on = true;

        if (power_off_reason == po_starting_up) {
            power_off_reason = po_none;
            cmd = "go";
        }
        else if (power_off_reason == po_disassemble_on) {
            inp = "si 1000000000";
            ss.str("");
            ss.clear();
            ss.str(inp);
            ss >> cmd;
        }
        else if (power_off_reason == po_disassemble_off) {
            power_off_reason = po_none;
            cmd = "go";
        }
        else
        {
            if (power_off_reason == po_enter_debugger) {
                power_off_reason = po_entered_debugger;
            }
            if (!did_message) {
                cout << endl;
                cout << "Welcome to the DingusPPC command line debugger." << endl;
                cout << "Please enter a command or 'help'." << endl << endl;
                did_message = true;
            }

            printf("%08X: dingusdbg> ", ppc_state.pc);

            while (power_on) {
                /* reset string stream */
                ss.str("");
                ss.clear();

                cmd = "";
                std::cin.clear();
                getline(cin, inp, '\n');
                ss.str(inp);
                ss >> cmd;

    #ifndef _WIN32
                struct winsize win_size_current;
                ioctl(STDIN_FILENO, TIOCGWINSZ, &win_size_current);
                if (win_size_current.ws_col != win_size_previous.ws_col || win_size_current.ws_row != win_size_previous.ws_row) {
                    win_size_previous = win_size_current;
                    if (cmd.empty()) {
                        continue;
                    }
                }
    #endif
                break;
            }
        }

        if (power_off_reason == po_signal_interrupt) {
            power_off_reason = po_enter_debugger;
            // ignore command if interrupt happens because the input line is probably incomplete.
            last_cmd = "";
            continue;
        }

        if (feof(stdin)) {
            printf("eof -> quit\n");
            cmd = "quit";
        }

        cmd_repeat = cmd.empty() && !last_cmd.empty();
        if (cmd_repeat) {
            cmd = last_cmd;
            repeat_count++;
        }
        else {
            repeat_count = 1;
        }
        if (cmd == "help") {
            cmd = "";
            show_help();
        } else if (cmd == "quit") {
            cmd = "";
            break;
        } else if (cmd == "restart") {
            cmd = "";
            power_on = false;
            power_off_reason = po_restart;
        } else if (cmd == "profile") {
            cmd = "";
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
            cmd = "";
            if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                print_68k_regs();
#endif
            } else {
                print_gprs();
            }
        } else if (cmd == "fregs") {
            cmd = "";
            print_fprs();
        } else if (cmd == "mregs") {
            cmd = "";
            print_mmu_regs();
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
                if (cmd_repeat) {
                    delete_prompt();
                }
                for (; --count >= 0;) {
                    addr_str = "R24";
                    addr     = static_cast<uint32_t>(get_reg(addr_str) - 2);
                    disasm_68k(1, addr);
                    exec_single_68k();
                }
#endif
            } else {
                if (cmd_repeat) {
                    delete_prompt();
                }
                for (; --count >= 0;) {
                    addr = ppc_state.pc;
                    PPCDisasmContext ctx;
                    disasm_in(ctx, addr);
                    ppc_exec_single();
                    disasm_out(ctx);
                }
            }
        } else if (cmd == "next" || cmd == "ni") {
            addr_str = "PC";
            addr     = static_cast<uint32_t>(get_reg(addr_str) + 4);
            ppc_exec_until(addr);
        } else if (cmd == "until") {
            if (cmd_repeat) {
                delete_prompt();
                cout << repeat_count << "> " << cmd << " " << addr_str << endl;
            }
            else {
                ss >> addr_str;
            }
            try {
                addr = str2addr(addr_str);
                if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                    exec_until_68k(addr);
#endif
                } else {
                    ppc_exec_until(addr);
                }
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
            }
        } else if (cmd == "go") {
            cmd = "";
            power_on = true;
            ppc_exec();
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
                        next_addr_68k = disasm_68k(inst_grab, addr);
#endif
                    } else {
                        next_addr_ppc = disasm(inst_grab, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            } else {
                /* disas without arguments defaults to disas 1,pc */
                try {
                    if (context == 2) {
#ifdef ENABLE_68K_DEBUGGER
                        if (cmd_repeat) {
                            delete_prompt();
                            addr = next_addr_68k;
                        }
                        else {
                        addr_str = "R24";
                            addr     = static_cast<uint32_t>(get_reg(addr_str) - 2);
                        }
                        next_addr_68k = disasm_68k(1, addr);
#endif
                    } else {
                        if (cmd_repeat) {
                            delete_prompt();
                            addr = next_addr_ppc;
                        }
                        else {
                        addr_str = "PC";
                            addr     = static_cast<uint32_t>(get_reg(addr_str));
                        }
                        next_addr_ppc = disasm(1, addr);
                    }
                } catch (invalid_argument& exc) {
                    cout << exc.what() << endl;
                }
            }
        } else if (cmd == "dump") {
            expr_str = "";
            ss >> expr_str;
            dump_mem(expr_str);
        } else if (cmd == "setmem") {
            expr_str = "";
            ss >> expr_str;
            patch_mem(expr_str);
#ifdef ENABLE_68K_DEBUGGER
        } else if (cmd == "context") {
            cmd = "";
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
        } else if (cmd == "regions") {
            cmd = "";
            if (mem_ctrl_instance)
                mem_ctrl_instance->dump_regions();
        } else if (cmd == "printenv") {
            cmd = "";
            if (ofnvram->init())
                continue;
            ofnvram->printenv();
        } else if (cmd == "setenv") {
            cmd = "";
            string var_name, value;
            ss >> var_name;
            std::istream::sentry se(ss); // skip white space
            getline(ss, value); // get everything up to eol
            if (ofnvram->init()) {
                cout << " Cannot open NVRAM" << endl;
                continue;
            }
            if (!ofnvram->setenv(var_name, value)) {
                cout << " Please try again" << endl;
            } else {
                cout << " ok" << endl; // mimic Forth
            }
 #ifndef _WIN32
        } else if (cmd == "nvedit") {
            cmd = "";
            cout << "===== press CNTRL-C to save =====" << endl;

            // save original terminal state
            tcgetattr(STDIN_FILENO, &orig_termios);
            struct termios new_termios = orig_termios;

            new_termios.c_cflag &= ~(CSIZE | PARENB);
            new_termios.c_cflag |= CS8;

            new_termios.c_lflag &= ~(ISIG | NOFLSH | ICANON | ECHOCTL);
            new_termios.c_lflag |= NOFLSH | ECHONL;

            // new_termios.c_iflag &= ~(ICRNL | IGNCR);
            // new_termios.c_iflag |= INLCR;

            // new_termios.c_oflag &= ~(ONOCR | ONLCR);
            // new_termios.c_oflag |= OPOST | OCRNL | ONLRET;

            tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

            // save original signal handler for SIGINT
            // then redirect SIGINT to new handler
            memset(&new_act_sigint, 0, sizeof(new_act_sigint));
            new_act_sigint.sa_handler = mysig_handler;
            sigaction(SIGINT, &new_act_sigint, &old_act_sigint);

            // save original signal handler for SIGTERM
            // then redirect SIGTERM to new handler
            memset(&new_act_sigterm, 0, sizeof(new_act_sigterm));
            new_act_sigterm.sa_handler = mysig_handler;
            sigaction(SIGTERM, &new_act_sigterm, &old_act_sigterm);

            getline(cin, inp, '\x03');

            // restore original terminal state
            tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

            // restore original signal handler for SIGINT
            signal(SIGINT, old_act_sigint.sa_handler);

            // restore original signal handler for SIGTERM
            signal(SIGTERM, old_act_sigterm.sa_handler);

            if (ofnvram->init())
                continue;
            ofnvram->setenv("nvramrc", inp);
#endif
#ifdef DEBUG_CPU_INT
        } else if (cmd == "amicint") {
            cmd = "";
            string value;
            int irq_id;
            ss >> value;
            try {
                irq_id = str2num(value);
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
                continue;
            }
            InterruptCtrl* int_ctrl = dynamic_cast<InterruptCtrl*>(
                gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
            int_ctrl->ack_int(irq_id, 1);
        } else if (cmd == "viaint") {
            cmd = "";
            string value;
            int irq_bit;
            ss >> value;
            try {
                irq_bit = str2num(value);
            } catch (invalid_argument& exc) {
                cout << exc.what() << endl;
                continue;
            }
            ViaCuda* via_obj = dynamic_cast<ViaCuda*>(gMachineObj->get_comp_by_name("ViaCuda"));
            ppc_state.pc -= 4;
            via_obj->assert_int(irq_bit);
#endif
        } else {
            if (!cmd.empty()) {
                cout << "Unknown command: " << cmd << endl;
                cmd = "";
            }
        }
        last_cmd = cmd;
    }
}
