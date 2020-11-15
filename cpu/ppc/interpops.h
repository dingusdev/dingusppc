GEN_OP(addi, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d2] + code->simm;
    NEXT;
})

GEN_OP(adde, {
    uint32_t xer_ca = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    uint32_t val_a  = ppc_state.gpr[code->d2];
    uint32_t val_b  = ppc_state.gpr[code->d3];

    uint32_t result = val_a + val_b + xer_ca;
    if ((result < val_a) || (xer_ca && (result == val_a))) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
    ppc_state.gpr[code->d1] = result;
    NEXT;
})

GEN_OP(addze, {
    uint32_t val_a  = ppc_state.gpr[code->d2];
    uint32_t xer_ca = !!(ppc_state.spr[SPR::XER] & 0x20000000);
    uint32_t result = val_a + xer_ca;
    if (result < val_a) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
    ppc_state.gpr[code->d1] = result;
    NEXT;
})

GEN_OP(andidot, {
    uint32_t result = ppc_state.gpr[code->d1] & code->uimm;
    ppc_state.gpr[code->d2] = result;
    ppc_changecrf0(result);
    NEXT;
})

GEN_OP(lwz, {
    ppc_state.gpr[code->d1] = (mem_grab_dword((
        (code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm));
    NEXT;
})

GEN_OP(lwzu, {
    uint32_t ea = ppc_state.gpr[code->d2] + code->simm;
    ppc_state.gpr[code->d1] = mem_grab_dword(ea);
    ppc_state.gpr[code->d2] = ea;
    NEXT;
})

GEN_OP(lbz, {
    ppc_state.gpr[code->d1] = (mem_grab_byte((
        (code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm));
    NEXT;
})

GEN_OP(lhz, {
    ppc_state.gpr[code->d1] = (mem_grab_word((
        (code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm));
    NEXT;
})

GEN_OP(rlwinm, {
    uint32_t val_s  = ppc_state.gpr[code->d1];
    uint32_t r      = ((val_s << code->d3) | (val_s >> (32 - code->d3)));
    uint32_t result = r & code->uimm;
    ppc_state.gpr[code->d2] = result;

    if (code->d4) {
        ppc_changecrf0(result);
    }
    NEXT;
})

GEN_OP(srawidot, {
    uint32_t val_s  = ppc_state.gpr[code->d1];
    uint32_t result = (int32_t)val_s >> code->d2;
    if ((val_s & 0x80000000UL) && (val_s & code->uimm)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
    ppc_state.gpr[code->d2] = result;
    ppc_changecrf0(result);
    NEXT;
})

GEN_OP(bc, {
    if (ppc_state.cr & code->uimm) {
        interp_tpc += code->bt;
    } else {
        NEXT;
    }
})

GEN_OP(mtspr, {
    ppc_state.spr[code->uimm] = ppc_state.gpr[code->d1];
    NEXT;
})

GEN_OP(bdnz, {
    if (--ppc_state.spr[SPR::CTR]) {
        interp_tpc += code->bt;
    } else {
        NEXT;
    }
})

GEN_OP(bdz, {
    if (!(--ppc_state.spr[SPR::CTR])) {
        interp_tpc += code->bt;
    } else {
        NEXT;
    }
})

GEN_OP(bclr, {
    // PLACEHOLDER
})

GEN_OP(bexit, {
    ppc_state.pc = code->uimm; // write source pc
    interp_running = false;
})
