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
        if (!(ppc_state.msr & MSR::ME)) {
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
    ppc_state.msr = (ppc_state.msr & ~MSR::LE) | !!(ppc_state.msr & MSR::ILE);

    if (ppc_state.msr & MSR::IP) {
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

void ppc_floating_point_exception() {
    LOG_F(ERROR, "Floating point exception at 0x%08x for instruction 0x%08x",
          ppc_state.pc, ppc_cur_instruction);
    // mmu_exception_handler(Except_Type::EXC_PROGRAM, Exc_Cause::FPU_EXCEPTION);
}

void ppc_alignment_exception(uint32_t ea)
{
    uint32_t dsisr;

    switch (ppc_cur_instruction & 0xfc000000) {
        case 0x80000000: // lwz
        case 0x90000000: // stw
        case 0xa0000000: // lhz
        case 0xa8000000: // lha
        case 0xb0000000: // sth
        case 0xb8000000: // lmw
        case 0xc0000000: // lfs
        case 0xc8000000: // lfd
        case 0xd0000000: // stfs
        case 0xd8000000: // stfd
        case 0x84000000: // lwzu
        case 0x94000000: // stwu
        case 0xa4000000: // lhzu
        case 0xac000000: // lhau
        case 0xb4000000: // sthu
        case 0xbc000000: // stmw
        case 0xc4000000: // lfsu
        case 0xcc000000: // lfdu
        case 0xd4000000: // stfsu
        case 0xdc000000: // stfdu
indirect_with_immediate_index:
            dsisr  = ((ppc_cur_instruction >> 12) & 0x00004000)  // bit  17    — Set to bit   5    of the instruction.
                  |  ((ppc_cur_instruction >> 17) & 0x00003c00); // bits 18–21 - set to bits  1–4  of the instruction.
            break;
        case 0x7c000000:
            switch (ppc_cur_instruction & 0xfc0007ff) {
                case 0x7c000028: // lwarx (invalid form - bits 15-21 of DSISR are identical to those of lwz)
                case 0x7c0002aa: // lwax (64-bit only)
                case 0x7c00042a: // lswx
                case 0x7c0004aa: // lswi
                case 0x7c00052a: // stswx
                case 0x7c0005aa: // stswi
                case 0x7c0002ea: // lwaux (64 bit only)
                case 0x7c00012c: // stwcx
                case 0x7c00042c: // lwbrx
                case 0x7c00052c: // stwbrx
                case 0x7c00062c: // lhbrx
                case 0x7c00072c: // sthbrx
                case 0x7c00026c: // eciwx // MPC7451
                case 0x7c00036c: // ecowx // MPC7451
                case 0x7c00002e: // lwzx
                case 0x7c00012e: // stwx
                case 0x7c00022e: // lhzx
                case 0x7c0002ae: // lhax
                case 0x7c00032e: // sthx
                case 0x7c00042e: // lfsx
                case 0x7c0004ae: // lfdx
                case 0x7c00052e: // stfsx
                case 0x7c0005ae: // stfdx
                case 0x7c00006e: // lwzux
                case 0x7c00016e: // stwux
                case 0x7c00026e: // lhzux
                case 0x7c0002ee: // lhaux
                case 0x7c00036e: // sthux
                case 0x7c00046e: // lfsux
                case 0x7c0004ee: // lfdux
                case 0x7c00056e: // stfsux
                case 0x7c0005ee: // stfdux
indirect_with_index:
                    dsisr  = ((ppc_cur_instruction << 14) & 0x00018000)  // bits 15–16 - set to bits 29–30 of the instruction.
                          |  ((ppc_cur_instruction <<  8) & 0x00004000)  // bit  17    - set to bit  25    of the instruction.
                          |  ((ppc_cur_instruction <<  3) & 0x00003c00); // bits 18–21 - set to bits 21–24 of the instruction.
                    break;
                case 0x7c0007ec:
                    if ((ppc_cur_instruction & 0xffe007ff) == 0x7c0007ec) // dcbz
                        goto indirect_with_index;
                    /* fallthrough */
                default:
                    goto unexpected_instruction;
            }
            break;
        default:
unexpected_instruction:
            dsisr = 0;
            LOG_F(ERROR, "Alignment exception from unexpected instruction 0x%08x",
                  ppc_cur_instruction);
    }

    // bits 22–26 - Set to bits  6–10 (source or destination) of the instruction.
    // Undefined for dcbz.
    dsisr |= ((ppc_cur_instruction >> 16) & 0x000003e0);

    if ((ppc_cur_instruction & 0xfc000000) == 0xb8000000) { // lmw
        LOG_F(ERROR, "Alignment exception from instruction 0x%08x (lmw). "
              "What to set DSISR bits 27-31?", ppc_cur_instruction);
        // dsisr |= ((ppc_cur_instruction >> ?) & 0x0000001f); // bits 27–31
    }
    else if ((ppc_cur_instruction & 0xfc0007ff) == 0x7c0004aa) { // lswi
        LOG_F(ERROR, "Alignment exception from instruction 0x%08x (lswi). "
              "What to set DSISR bits 27-31?", ppc_cur_instruction);
        // dsisr |= ((ppc_cur_instruction >> ?) & 0x0000001f); // bits 27–31
    }
    else if ((ppc_cur_instruction & 0xfc0007ff) == 0x7c00042a) { // lswx
        LOG_F(ERROR, "Alignment exception from instruction 0x%08x (lswx). "
              "What to set DSISR bits 27-31?", ppc_cur_instruction);
        // dsisr |= ((ppc_cur_instruction >> ?) & 0x0000001f); // bits 27–31
    }
    else {
        // bits 27–31 - Set to bits 11–15 of the instruction (rA)
        dsisr |= ((ppc_cur_instruction >> 16) & 0x0000001f);
    }

    ppc_state.spr[SPR::DSISR] = dsisr;
    ppc_state.spr[SPR::DAR] = ea;
    ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x0);
}
