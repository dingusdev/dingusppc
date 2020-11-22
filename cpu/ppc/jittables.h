#ifndef JIT_TABLES_H
#define JIT_TABLES_H

#include <cinttypes>
#include "ppcemu.h"

/** Instruction operands formats. */
enum class InstrOps {
    opNone,
    opDA,
    opDAB,
    opDASimm,
    opSAUimm,
    opSASh,
    opRot,
    opSSpr,
    opBrRel,
    opBrLink,
    opD,
    opTOASimm,
    opTOB,
    opCrfDASimm,
    opCrfDAUimm,
    opDSR,
    opDB,
    opSC, //for System Calls
    opSASimm,
};

/* Control flow kind. */
enum class CFlowType {
    CFL_NONE,
    CFL_COND_BRANCH,
    CFL_UNCOND_BRANCH,
    CFL_TRAP,
};

struct InstrInfo {
    InstrOps    ops_fmt;    // describes operands format
    CFlowType   cflow_type; // control flow type
    int         num_cycles;

    // Required by JIT
    //int       flags;      // flags updated by this instruction
};

extern uint16_t main_index_tab[];
extern uint16_t subgrp16_index_tab[];
extern uint16_t subgrp18_index_tab[];
extern uint16_t subgrp19_index_tab[];
extern uint16_t subgrp31_index_tab[];
extern uint16_t subgrp59_index_tab[];
extern uint16_t subgrp63_index_tab[];

void init_jit_tables(void);

#endif /* JIT_TABLES_H */
