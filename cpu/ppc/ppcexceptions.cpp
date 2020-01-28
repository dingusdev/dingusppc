/** @file Handling of low-level PPC exceptions. */

#include <setjmp.h>
#include "ppcemu.h"

jmp_buf exc_env; /* Global exception environment. */

[[noreturn]] void ppc_exception_handler(Except_Type exception_type,
                                        uint32_t srr1_bits)
{
    grab_exception = true;
    #ifdef PROFILER
    exceptions_performed++;
    #endif
    bb_kind = BB_end_kind::BB_EXCEPTION;

    switch(exception_type) {
        case Except_Type::EXC_SYSTEM_RESET:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0100;
            break;

        case Except_Type::EXC_MACHINE_CHECK:
            if (!(ppc_state.ppc_msr & 0x1000)) {
                /* TODO: handle internal checkstop */
            }
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0200;
            break;

        case Except_Type::EXC_DSI:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0300;
            break;

        case Except_Type::EXC_ISI:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_next_instruction_address;
            ppc_next_instruction_address = 0x0400;
            break;

        case Except_Type::EXC_EXT_INT:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_next_instruction_address;
            ppc_next_instruction_address = 0x0500;
            break;

        case Except_Type::EXC_ALIGNMENT:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0600;
            break;

        case Except_Type::EXC_PROGRAM:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0700;
            break;

        case Except_Type::EXC_NO_FPU:
            ppc_state.ppc_spr[SPR::SRR0] = ppc_cur_instruction & 0xFFFFFFFC;
            ppc_next_instruction_address = 0x0800;
            break;

        case Except_Type::EXC_DECR:
            ppc_state.ppc_spr[SPR::SRR0] = (ppc_cur_instruction & 0xFFFFFFFC) + 4;
            ppc_next_instruction_address = 0x0900;
            break;

        case Except_Type::EXC_SYSCALL:
            ppc_state.ppc_spr[SPR::SRR0] = (ppc_cur_instruction & 0xFFFFFFFC) + 4;
            ppc_next_instruction_address = 0x0C00;
            break;

        case Except_Type::EXC_TRACE:
            ppc_state.ppc_spr[SPR::SRR0] = (ppc_cur_instruction & 0xFFFFFFFC) + 4;
            ppc_next_instruction_address = 0x0D00;
            break;

        default:
            //printf("Unknown exception occured: %X\n", exception_type);
            //exit(-1);
            break;
    }

    ppc_state.ppc_spr[SPR::SRR1] = (ppc_state.ppc_msr & 0x0000FF73) | srr1_bits;
    ppc_state.ppc_msr &= 0xFFFB1041;
    /* copy MSR[ILE] to MSR[LE] */
    ppc_state.ppc_msr = (ppc_state.ppc_msr & 0xFFFFFFFE) |
                       ((ppc_state.ppc_msr >> 16) & 1);

    if (ppc_state.ppc_msr & 0x40) {
        ppc_next_instruction_address |= 0xFFF00000;
    }

    longjmp(exc_env, 2); /* return to the main execution loop. */
}
