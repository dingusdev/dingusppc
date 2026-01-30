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

#ifndef PPCDISASM_H
#define PPCDISASM_H

#include <cinttypes>
#include <string>
#include <vector>

typedef struct PPCDisasmContext {
    uint32_t instr_addr;
    uint32_t instr_code;
    std::string instr_str;
    bool simplified; /* true if we should output simplified mnemonics */
    std::vector<std::string> regs_in;
    std::vector<std::string> regs_out;
} PPCDisasmContext;

std::string disassemble_single(PPCDisasmContext* ctx);

int test_ppc_disasm(void);

/** sign-extend an integer. */
#define SIGNEXT(x, sb) ((x) | (((x) & (1 << (sb))) ? ~((1 << (sb)) - 1) : 0))

#endif /* PPCDISASM_H */
