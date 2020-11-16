#ifndef NU_INTERPRETER_H
#define NU_INTERPRETER_H

#include "ppcdefs.h"

#define USE_DTC

struct CachedInstr; // forward declaration

typedef void (*ImplSubr)(const CachedInstr *);

struct CachedInstr {
#if defined(USE_DTC)
    void*    call_me;
#else
    ImplSubr call_me;
#endif

    union {
        struct  {
            uint8_t     d1;
            uint8_t     d2;
            uint8_t     d3;
            uint8_t     d4;
        };
        int32_t    bt; // branch target
    };

    union {
        int32_t     simm;
        uint32_t    uimm;
    };
};

bool PreDecode(uint32_t next_pc, CachedInstr* c_instr);
void NuInterpExec(uint32_t start_addr);

#endif // NU_INTERPRETER_H
