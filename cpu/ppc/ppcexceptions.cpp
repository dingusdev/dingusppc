/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** @file Handling of low-level PPC exceptions. */

#include <loguru.hpp>
#include "ppcemu.h"
#include "ppcmmu.h"

#include <setjmp.h>
#include <stdexcept>
#include <string>

jmp_buf exc_env; /* Global exception environment. */

void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits) {
#ifdef CPU_PROFILING
    exceptions_processed++;
#endif

    switch (exception_type) {
    case Except_Type::EXC_SYSTEM_RESET:
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0100;
        break;

    case Except_Type::EXC_MACHINE_CHECK:
        if (!(ppc_state.msr & 0x1000)) {
            /* TODO: handle internal checkstop */
        }
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0200;
        break;

    case Except_Type::EXC_DSI:
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0300;
        break;

    case Except_Type::EXC_ISI:
        if (exec_flags & ~EXEF_TIMER) {
            ppc_state.spr[SPR::SRR0] = ppc_next_instruction_address;
        } else {
            ppc_state.spr[SPR::SRR0] = ppc_state.pc & 0xFFFFFFFCUL;
        }
        ppc_next_instruction_address = 0x0400;
        break;

    case Except_Type::EXC_EXT_INT:
        if (exec_flags & ~EXEF_TIMER) {
            ppc_state.spr[SPR::SRR0] = ppc_next_instruction_address;
        } else {
            ppc_state.spr[SPR::SRR0] = (ppc_state.pc & 0xFFFFFFFCUL) + 4;
        }
        ppc_next_instruction_address = 0x0500;
        break;

    case Except_Type::EXC_ALIGNMENT:
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0600;
        break;

    case Except_Type::EXC_PROGRAM:
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0700;
        break;

    case Except_Type::EXC_NO_FPU:
        ppc_state.spr[SPR::SRR0]     = ppc_state.pc & 0xFFFFFFFC;
        ppc_next_instruction_address = 0x0800;
        break;

    case Except_Type::EXC_DECR:
        if (exec_flags & ~EXEF_TIMER) {
            ppc_state.spr[SPR::SRR0] = ppc_next_instruction_address;
        } else {
            ppc_state.spr[SPR::SRR0] = (ppc_state.pc & 0xFFFFFFFCUL) + 4;
        }
        ppc_next_instruction_address = 0x0900;
        break;

    case Except_Type::EXC_SYSCALL:
        ppc_state.spr[SPR::SRR0]     = (ppc_state.pc & 0xFFFFFFFC) + 4;
        ppc_next_instruction_address = 0x0C00;
        break;

    case Except_Type::EXC_TRACE:
        ppc_state.spr[SPR::SRR0]     = (ppc_state.pc & 0xFFFFFFFC) + 4;
        ppc_next_instruction_address = 0x0D00;
        break;

    default:
        ABORT_F("Unknown exception occurred: %X\n", (unsigned)exception_type);
        break;
    }

    ppc_state.spr[SPR::SRR1] = (ppc_state.msr & 0x0000FF73) | srr1_bits;
    ppc_state.msr &= 0xFFFB1041;
    /* copy MSR[ILE] to MSR[LE] */
    ppc_state.msr = (ppc_state.msr & 0xFFFFFFFE) | ((ppc_state.msr >> 16) & 1);

    if (ppc_state.msr & 0x40) {
        ppc_next_instruction_address |= 0xFFF00000;
    }

    exec_flags = EXEF_EXCEPTION;

    // perform context synchronization for recoverable exceptions
    if (exception_type != Except_Type::EXC_MACHINE_CHECK &&
        exception_type != Except_Type::EXC_SYSTEM_RESET) {
        do_ctx_sync();
    }

    mmu_change_mode();

    if (exception_type != Except_Type::EXC_EXT_INT && exception_type != Except_Type::EXC_DECR) {
        longjmp(exc_env, 2); /* return to the main execution loop. */
    }
}


[[noreturn]] void dbg_exception_handler(Except_Type exception_type, uint32_t srr1_bits) {
    std::string exc_descriptor;

    switch (exception_type) {
    case Except_Type::EXC_SYSTEM_RESET:
        exc_descriptor = "System reset exception occurred";
        break;

    case Except_Type::EXC_MACHINE_CHECK:
        exc_descriptor = "Machine check exception occurred";
        break;

    case Except_Type::EXC_DSI:
    case Except_Type::EXC_ISI:
        if (ppc_state.spr[SPR::DSISR] & 0x40000000)
            exc_descriptor = "DSI/ISI exception: unmapped memory access";
        else if (ppc_state.spr[SPR::DSISR] & 0x08000000)
            exc_descriptor = "DSI/ISI exception: access protection violation";
        else {
            if (exception_type == Except_Type::EXC_DSI)
                exc_descriptor = "DSI exception";
            else
                exc_descriptor = "ISI exception";
        }
        break;

    case Except_Type::EXC_EXT_INT:
        exc_descriptor = "External interrupt exception occurred";
        break;

    case Except_Type::EXC_ALIGNMENT:
        exc_descriptor = "Alignment exception occurred";
        break;

    case Except_Type::EXC_PROGRAM:
        exc_descriptor = "Program exception occurred";
        break;

    case Except_Type::EXC_NO_FPU:
        exc_descriptor = "Floating-Point unavailable exception occurred";
        break;

    case Except_Type::EXC_DECR:
        exc_descriptor = "Decrementer exception occurred";
        break;

    case Except_Type::EXC_SYSCALL:
        exc_descriptor = "Syscall exception occurred";
        break;

    case Except_Type::EXC_TRACE:
        exc_descriptor = "Trace exception occurred";
        break;
    }

    throw std::invalid_argument(exc_descriptor);
}
