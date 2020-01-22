#ifndef PPCDISASM_H
#define PPCDISASM_H

#include <cinttypes>
#include <string>

typedef struct PPCDisasmContext {
    uint32_t    instr_addr;
    uint32_t    instr_code;
    std::string instr_str;
    bool        simplified; /* true if we should output simplified mnemonics */
} PPCDisasmContext;

std::string disassemble_single(PPCDisasmContext *ctx);

/** sign-extend an integer. */
#define SIGNEXT(x, sb) ((x) | (((x) & (1 << (sb))) ? ~((1 << (sb))-1) : 0))

#endif /* PPCDISASM_H */
