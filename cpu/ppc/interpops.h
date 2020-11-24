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
    if ((code->d2 != code->d1) || code->d2 != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        ppc_state.gpr[code->d1] = mem_grab_dword(ea);
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
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
#if defined(USE_DTC)
        code += code->bt;
        goto *(code->call_me);
#else
        interp_tpc += code->bt;
#endif
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
#if defined(USE_DTC)
        code += code->bt;
        goto *(code->call_me);
#else
        interp_tpc += code->bt;
#endif
    } else {
        NEXT;
    }
})

GEN_OP(bdz, {
    if (!(--ppc_state.spr[SPR::CTR])) {
#if defined(USE_DTC)
        code += code->bt;
        goto *(code->call_me);
#else
        interp_tpc += code->bt;
#endif
    } else {
        NEXT;
    }
})

GEN_OP(bclr, {
    // PLACEHOLDER
})

GEN_OP(bexit, {
    ppc_state.pc = code->uimm; // write source pc
#if defined(USE_DTC)
    return;
#else
    interp_running = false;
#endif
})

GEN_OP(twi, {
    if ((((int32_t)ppc_state.gpr[code->d2] < code->simm) && (code->d1 & 0x10)) ||
        (((int32_t)ppc_state.gpr[code->d2] > code->simm) && (code->d1 & 0x08)) ||
        (((int32_t)ppc_state.gpr[code->d2] == code->simm) && (code->d1 & 0x04)) ||
        ((ppc_state.gpr[code->d2] < (uint32_t)code->simm) && (code->d1 & 0x02)) ||
        ((ppc_state.gpr[code->d2] > (uint32_t)code->simm) && (code->d1 & 0x01))) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(mulli, {
    int64_t product = (int64_t)(int32_t)ppc_state.gpr[code->d2] * (int64_t)(int32_t)code->simm;
    ppc_state.gpr[code->d1] = (uint32_t)product;
    NEXT;
})

GEN_OP(subfic, {
    ppc_state.gpr[code->d1] = code->simm - ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d1] < ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    NEXT;
})

GEN_OP(cmpli, {
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (ppc_state.gpr[code->d2] == uimm)
        ? 0x20000000UL
        : (ppc_state.gpr[code->d2] > uimm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> code->d1)) | ((cmp_c + xercon) >> code->d1));
    NEXT;
})

GEN_OP(cmpi, {
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (code->d2 == code->simm)
        ? 0x20000000UL
        : (ppc_state.gpr[code->d2] > uimm) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> code->d1)) | ((cmp_c + xercon) >> code->d1));

    NEXT;
})

GEN_OP(addic, {
    ppc_state.gpr[code->d1] = (ppc_state.gpr[code->d2] + code->simm);

    if (ppc_state.gpr[code->d1] < ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    NEXT;
})

GEN_OP(addicdot, {
    ppc_state.gpr[code->d1] = (ppc_state.gpr[code->d2] + code->simm);
    ppc_changecrf0(ppc_state.gpr[code->d1]);

    if (ppc_state.gpr[code->d1] < ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    NEXT;
})

GEN_OP(addis, {
    ppc_state.gpr[code->d1] = (code->d2 == 0) ? (code->simm << 16)
                                              : ppc_state.gpr[code->d2] + code->simm;
    NEXT;
})

GEN_OP(sc, { ppc_exception_handler(Except_Type::EXC_SYSCALL, 0x20000); })

GEN_OP(rlwimix, {
    uint32_t val_s          = ppc_state.gpr[code->d1];
    uint32_t rotator        = ((val_s << code->d3) | (val_s >> (32 - code->d3)));
    ppc_state.gpr[code->d2] = (ppc_state.gpr[code->d2] & ~code->uimm) | (rotator & code->uimm);
    if (code->d4) {
        ppc_changecrf0(ppc_state.gpr[code->d2]);
    }
})

GEN_OP(rlwnmx, {
    uint32_t val_s = ppc_state.gpr[code->d1];
    uint32_t r = ((val_s << ppc_state.gpr[code->d3]) | (val_s >> (32 - ppc_state.gpr[code->d3])));
    ppc_state.gpr[code->d2] = r & code->uimm;
    if (code->d4) {
        ppc_changecrf0(ppc_state.gpr[code->d2]);
    }
})

GEN_OP(ori, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] | code->uimm;
    NEXT;
})

GEN_OP(oris, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] | (code->uimm << 16);
    NEXT;
})

GEN_OP(xori, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] ^ code->uimm;
    NEXT;
})

GEN_OP(xoris, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] ^ (code->uimm << 16);
    NEXT;
})

GEN_OP(andisdot, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] & (code->uimm << 16);
    ppc_changecrf0(ppc_state.gpr[code->d2]);
    NEXT;
})

GEN_OP(lbzu, {
    if ((code->d2 != code->d1) || code->d2 != 0) {
        uint32_t ea             = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        ppc_state.gpr[code->d1] = mem_grab_byte(ea);
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(stw, {
    mem_write_dword((((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm), 
            ppc_state.gpr[code->d1]);
    NEXT;
})


GEN_OP(stwu, {
    if (reg_a != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        mem_write_dword(ea, ppc_state.gpr[code->d1]);
        ppc_state.gpr[code->d2] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})


GEN_OP(stb, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
    mem_write_byte(ea, ppc_state.gpr[code->d1]);
    NEXT;
})


GEN_OP(stbu, {
    if (reg_a != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        mem_write_byte(ea, ppc_state.gpr[code->d1]);
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(lhzu, {
    if ((code->d2 != code->d1) || code->d2 != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        ppc_state.gpr[code->d1] = mem_grab_word(ea);
        ppc_state.gpr[code->d2] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(lha, {
    if ((code->d2 != code->d1) || code->d2 != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        ppc_state.gpr[code->d1] = (uint32_t)(uint16_t)(mem_grab_word(ea));
        ppc_state.gpr[code->d2] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(lhau, {
    if ((code->d2 != code->d1) || code->d2 != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        ppc_state.gpr[code->d1] = (uint32_t)(uint16_t)(mem_grab_word(ea));
        ppc_state.gpr[code->d2] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(sth, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
    mem_write_word(ea, ppc_state.gpr[code->d1]);
    NEXT;
})

GEN_OP(sthu, {
    if (reg_a != 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        mem_write_word(ea, ppc_state.gpr[code->d1]);
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(lmw, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
    do {
        ppc_state.gpr[(code->d1)] = mem_grab_dword(ea);
        ea += 4;
        (code->d1)++;
    } while (code->d1 < 32);
    NEXT;
})

GEN_OP(stmw, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;


    if (ea & 3) {
        ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x00000);
    }

    for (; code->d1 <= 31; code->d1++) {
        mem_write_dword(ea, ppc_state.gpr[code->d1]);
        ea += 4;
    }

    NEXT;
})

GEN_OP(lfs, {
    uint64_t ppc_rep = (uint64_t)(
        mem_grab_dword(((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm));

    ppc_state.fpr[code->d1].int64_r = ppc_rep;
    ppc_state.fpr[code->d1].dbl64_r = *(double*)&ppc_rep;
    NEXT;
})


GEN_OP(lfsu, {
    if ((code->d2) == 0) {
        uint32_t ea             = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        uint64_t ppc_rep        = (double)((uint64_t)mem_grab_dword(ea));

        ppc_state.fpr[code->d1].int64_r = ppc_rep;
        ppc_state.fpr[code->d1].dbl64_r = *(double*)&ppc_rep;
        ppc_state.gpr[code->d2] = ea;
    } 
    else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(lfd, {
    uint64_t ppc_rep = 
        (double)(mem_grab_qword(((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm));

    ppc_state.fpr[code->d1].int64_r = ppc_rep;
    ppc_state.fpr[code->d1].dbl64_r = *(double*)&ppc_rep;

    NEXT;
})


GEN_OP(lfdu, {
    if ((code->d2) == 0) {
        uint32_t ea             = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        uint64_t ppc_rep        = (double)((uint64_t)mem_grab_qword(ea));

        ppc_state.fpr[code->d1].int64_r = ppc_rep;
        ppc_state.fpr[code->d1].dbl64_r = *(double*)&ppc_rep;
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(stfs, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
    mem_write_dword(ea, uint32_t(ppc_state.fpr[code->d1].int64_r));
    NEXT;
})

GEN_OP(stfsu, {
    if (code->d2 == 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        mem_write_dword(ea, uint32_t(ppc_state.fpr[code->d1].int64_r));
        ppc_state.gpr[code->d2] = ea;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
    NEXT;
})

GEN_OP(stfd, {
    uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
    mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
})

GEN_OP(stfdu, {
    ppc_grab_regsfpsia(true);
    if (reg_a == 0) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + code->simm;
        mem_write_qword(ppc_effective_address, ppc_state.fpr[reg_s].int64_r);
        ppc_state.gpr[code->d2] = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
})

// placeholders until I can figure out how to refactor these fully
GEN_OP(bcl, {})
GEN_OP(bca, {})
GEN_OP(bcla, {})
GEN_OP(b, {})
GEN_OP(bl, {})
GEN_OP(ba, {})
GEN_OP(bla, {})
GEN_OP(bclrl, {})

GEN_OP(crnor, {})
GEN_OP(rfi, {})
GEN_OP(crandc, {})
GEN_OP(isync, {})
GEN_OP(crxor, {})
GEN_OP(crnand, {})
GEN_OP(crand, {})
GEN_OP(creqv, {})
GEN_OP(crorc, {})
GEN_OP(cror, {})
GEN_OP(bcctr, {})
GEN_OP(bcctrl, {})

// back to real code!

GEN_OP(cmp, {
    uint32_t xer_in = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    uint32_t cmp_in = (((int32_t)ppc_state.gpr[code->d2]) == ((int32_t)ppc_state.gpr[code->d3]))
        ? 0x20000000UL
        : (((int32_t)ppc_state.gpr[code->d2]) > ((int32_t)ppc_state.gpr[code->d3])) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr =
        ((ppc_state.cr & ~(0xf0000000UL >> (code->d1 & 0x1C))) |
         ((cmp_in + xer_in) >> (code->d1 & 0x1C)));
})

GEN_OP(tw, {})

GEN_OP(subfc, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d1] >= ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
})

GEN_OP(subfcdot, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d1] >= ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    ppc_changecrf0(ppc_state.gpr[code->d1]);
})

GEN_OP(addc, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] + ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d2] < ppc_state.gpr[code->d1]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
})

GEN_OP(addcdot, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] + ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d2] < ppc_state.gpr[code->d1]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    ppc_changecrf0(ppc_state.gpr[code->d1]);
})

GEN_OP(mulhwu, {
    uint64_t product        = (uint64_t)ppc_state.gpr[code->d3] * (uint64_t)ppc_state.gpr[code->d2];
    ppc_state.gpr[code->d1] = (uint32_t)(product >> 32);

    if (ppc_state.gpr[code->d2] < ppc_state.gpr[code->d1]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
})

GEN_OP(mulhwudot, {
    uint64_t product        = (uint64_t)ppc_state.gpr[code->d3] * (uint64_t)ppc_state.gpr[code->d2];
    ppc_state.gpr[code->d1] = (uint32_t)(product >> 32);

    if (ppc_state.gpr[code->d2] < ppc_state.gpr[code->d1]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    ppc_changecrf0(ppc_state.gpr[code->d1]);
})

GEN_OP(neg,  { 
    ppc_state.gpr[code->d1] = ~(ppc_state.gpr[code->d2]) + 1; 
})

GEN_OP(negdot, {
    ppc_state.gpr[code->d1] = ~(ppc_state.gpr[code->d2]) + 1;
    ppc_changecrf0(ppc_result_d);
})

GEN_OP(nego, {
    ppc_result_d = ~(ppc_result_a) + 1;

    if (ppc_state.gpr[code->d2] == 0x80000000)
        ppc_state.spr[SPR::XER] |= 0xC0000000;
    else
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFF;
})

GEN_OP(negodot, {
    ppc_result_d = ~(ppc_result_a) + 1;

    if (oe_flag) {
        if (ppc_result_a == 0x80000000)
            ppc_state.spr[SPR::XER] |= 0xC0000000;
        else
            ppc_state.spr[SPR::XER] &= 0xBFFFFFFF;
    }

    ppc_changecrf0(ppc_result_d);
})

GEN_OP(dcbf, {})

GEN_OP(lbzx, {
    uint32_t ea  = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + ppc_state.gpr[code->d3];
    ppc_state.gpr[code->d1] = mem_grab_byte(ea);
})

GEN_OP(mfmsr, {
    if (ppc_state.msr & 0x4000) {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x00040000);
    }

    ppc_state.gpr[code->d1] = ppc_state.msr;
})

GEN_OP(mfcr, { 
    ppc_state.gpr[code->d1] = ppc_state.cr; 
})

GEN_OP(lwarx, {
    ppc_state.reserve = true;
    ppc_state.gpr[code->d1] =
        (mem_grab_dword(((code->d2) ? ppc_state.gpr[code->d2] : 0) + ppc_state.gpr[code->d3]));
})

GEN_OP(lwzx, {
    ppc_state.gpr[code->d1] =
        (mem_grab_dword(((code->d2) ? ppc_state.gpr[code->d2] : 0) + ppc_state.gpr[code->d3]));
})

GEN_OP(slw, {
    ppc_state.gpr[code->d2] = 
            ((ppc_state.gpr[code->d3] > 0x1F) ? 0 : ppc_state.gpr[code->d1] << (ppc_state.gpr[code->d3] & 0x1F));
})

GEN_OP(slwdot, {
    ppc_state.gpr[code->d2] =
            ((ppc_state.gpr[code->d3] > 0x1F) ? 0 : ppc_state.gpr[code->d1] << (ppc_state.gpr[code->d3] & 0x1F));

    ppc_changecrf0(ppc_state.gpr[code->d2]);
})

GEN_OP(cntlzw, {
    uint32_t lead      = 0;
    uint32_t bit_check = ppc_state.gpr[code->d1];

    #ifdef USE_GCC_BUILTINS
        lead = __builtin_clz(bit_check);
    #elif defined USE_VS_BUILTINS
        lead = __lzcnt(bit_check);
    #else
        for (uint32_t mask = 0x80000000UL; mask; lead++, mask >>= 1) {
            if (bit_check & mask)
                break;
        }
    #endif

    ppc_state.gpr[code->d2] = lead;
})

GEN_OP(cntlzwdot, {
    uint32_t lead      = 0;
    uint32_t bit_check = ppc_state.gpr[code->d1];

    #ifdef USE_GCC_BUILTINS
        lead = __builtin_clz(bit_check);
    #elif defined USE_VS_BUILTINS
        lead = __lzcnt(bit_check);
    #else
        for (uint32_t mask = 0x80000000UL; mask; lead++, mask >>= 1) {
            if (bit_check & mask)
                break;
        }
    #endif

    ppc_state.gpr[code->d2] = lead;

    ppc_changecrf0(ppc_state.gpr[code->d2]);
})

GEN_OP(ppc_and, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] & ppc_state.gpr[code->d3];
    ppc_changecrf0(ppc_state.gpr[code->d2]);
})

GEN_OP(anddot, {
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] & ppc_state.gpr[code->d3];
    ppc_changecrf0(ppc_state.gpr[code->d2]);
})

GEN_OP(cmpl, {
    xercon = (ppc_state.spr[SPR::XER] & 0x80000000UL) >> 3;
    cmp_c  = (ppc_state.gpr[code->d2] == ppc_state.gpr[code->d3])
        ? 0x20000000UL
        : (ppc_state.gpr[code->d2] > ppc_state.gpr[code->d3]) ? 0x40000000UL : 0x80000000UL;
    ppc_state.cr = ((ppc_state.cr & ~(0xf0000000UL >> code->d1)) | ((cmp_c + xercon) >> code->d1));
})

GEN_OP(subf, { 
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];
})

GEN_OP(subfdot, { 
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];
    ppc_changecrf0(ppc_state.gpr[code->d1]);
})

GEN_OP(dcbst, {
})

GEN_OP(lwzux, {
    if ((code->d2 != code->d1) || code->d2 != 0) {
        ppc_result_d          = mem_grab_dword((ppc_state.gpr[code->d2] + ppc_state.gpr[code->d3]));
        ppc_result_a          = ppc_effective_address;
    } else {
        ppc_exception_handler(Except_Type::EXC_PROGRAM, 0x20000);
    }
})

GEN_OP(andc, { 
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] & ~(ppc_state.gpr[code->d3]);
})

GEN_OP(andcdot, { 
    ppc_state.gpr[code->d2] = ppc_state.gpr[code->d1] & ~(ppc_state.gpr[code->d3]);
    ppc_changecrf0(ppc_state.gpr[code->d2]);
})

GEN_OP(subfco, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d1] >= ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if ((ppc_state.gpr[code->d2] ^ ppc_state.gpr[code->d3]) &
        (ppc_state.gpr[code->d2] ^ ppc_state.gpr[code->d1]) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= 0xC0000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
})

GEN_OP(subfcodot, {
    ppc_state.gpr[code->d1] = ppc_state.gpr[code->d3] - ppc_state.gpr[code->d2];

    if (ppc_state.gpr[code->d1] >= ppc_state.gpr[code->d2]) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }

    if ((ppc_state.gpr[code->d2] ^ ppc_state.gpr[code->d3]) &
        (ppc_state.gpr[code->d2] ^ ppc_state.gpr[code->d1]) & 0x80000000UL) {
        ppc_state.spr[SPR::XER] |= 0xC0000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }

    ppc_changecrf0(ppc_state.gpr[code->d1]);
})

GEN_OP(lhbrx, {
    uint32_t ea = ((code->d1 == 0) ? ppc_state.gpr[code->d3] : 0) + ppc_state.gpr[code->d3];
    ppc_state.gpr[code->d1] = (uint32_t)(BYTESWAP_16(mem_grab_word(ppc_effective_address)));
})

GEN_OP(srawi, {
    uint32_t val_s  = ppc_state.gpr[code->d1];
    uint32_t result = (int32_t)val_s >> code->d2;
    if ((val_s & 0x80000000UL) && (val_s & code->uimm)) {
        ppc_state.spr[SPR::XER] |= 0x20000000UL;
    } else {
        ppc_state.spr[SPR::XER] &= 0xDFFFFFFFUL;
    }
    ppc_state.gpr[code->d2] = result;
    NEXT;
})

GEN_OP(eieio, {})

GEN_OP(sthbrx, {
    uint32_t ea = (code->d2 == 0) ? ppc_state.gpr[code->d3]
                                  : (ppc_state.gpr[code->d2] + ppc_state.gpr[code->d3]);
    uint32_t val1 = (uint32_t)(BYTESWAP_16((uint16_t)ppc_state.gpr[code->d1]));
    mem_write_word(ea, val1);
})

GEN_OP(extsb, {
    ppc_state.gpr[code->d2] = (int32_t)(int8_t)ppc_state.gpr[code->d1];
})
     
GEN_OP(extsbdot, {
    ppc_state.gpr[code->d2] = (int32_t)(int8_t)ppc_state.gpr[code->d1];
    ppc_changecrf0(ppc_state.gpr[code->d2]);
})
  
GEN_OP(extsh, { 
    ppc_state.gpr[code->d2] = (int32_t)(int16_t)ppc_state.gpr[code->d1];
})                  
     
GEN_OP(extshdot, {
    ppc_state.gpr[code->d2] = (int32_t)(int16_t)ppc_state.gpr[code->d1];
    ppc_changecrf0(ppc_state.gpr[code->d2]);
}) 
           
GEN_OP(divwu, {
    if (!ppc_state.gpr[code->d3]) { /* division by zero */
        ppc_result_d = 0;
        ppc_state.spr[SPR::XER] |= 0xC0000000;
    } else {
        ppc_state.gpr[code->d1] = ppc_state.gpr[code->d2] / ppc_state.gpr[code->d3];
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
})

GEN_OP(divwudot, {
    if (!ppc_state.gpr[code->d3]) { /* division by zero */
        ppc_result_d = 0;
        ppc_state.spr[SPR::XER] |= 0xC0000000;
        ppc_state.cr |= 0x20000000;
    } else {
        ppc_state.gpr[code->d1] = ppc_state.gpr[code->d2] / ppc_state.gpr[code->d3];
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
        ppc_changecrf0(ppc_result_d);
    }
})
    
GEN_OP(tlbld, {})

GEN_OP(icbi, {})

GEN_OP(stfiwx, {
    uint32_t ea = (code->d2 == 0) ? ppc_state.gpr[code->d3]
                                  : ppc_state.gpr[code->d2] + ppc_state.gpr[code->d3];
    mem_write_dword(ea, (uint32_t)(ppc_state.fpr[code->d1].int64_r));
})
    
GEN_OP(divwo, {
    if (!ppc_state.gpr[code->d3]) { /* handle the "anything / 0" case */
        ppc_state.gpr[code->d1] = (ppc_state.gpr[code->d2] & 0x80000000) ? -1
                                                                         : 0; /* UNDOCUMENTED! */

        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else if (ppc_state.gpr[code->d2] == 0x80000000UL && ppc_state.gpr[code->d3] == 0xFFFFFFFFUL) {
        ppc_state.gpr[code->d1] = 0xFFFFFFFF;
        ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else { /* normal signed devision */
        ppc_state.gpr[code->d1] = (int32_t)ppc_state.gpr[code->d2] / (int32_t)ppc_state.gpr[code->d3];
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }
})

GEN_OP(divwodot, {
    if (!ppc_state.gpr[code->d3]) { /* handle the "anything / 0" case */
        ppc_state.gpr[code->d1] = (ppc_state.gpr[code->d2] & 0x80000000) ? -1
                                                                         : 0; /* UNDOCUMENTED! */

        if (oe_flag)
            ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else if (ppc_state.gpr[code->d2] == 0x80000000UL && ppc_state.gpr[code->d3] == 0xFFFFFFFFUL) {
        ppc_state.gpr[code->d1] = 0xFFFFFFFF;
        ppc_state.spr[SPR::XER] |= 0xC0000000;

    } else { /* normal signed devision */
        ppc_state.gpr[code->d1] = (int32_t)ppc_state.gpr[code->d2] / (int32_t)ppc_state.gpr[code->d3];
        ppc_state.spr[SPR::XER] &= 0xBFFFFFFFUL;
    }

    ppc_changecrf0(ppc_result_d);
})

GEN_OP(tlbli, {})

GEN_OP(dcbz, {
    if (!(ppc_state.pc & 32) && (ppc_state.pc < 0xFFFFFFE0UL)) {
        uint32_t ea = ((code->d2) ? ppc_state.gpr[code->d2] : 0) + ppc_state.gpr[code->d3];
        mem_write_qword(ea, 0);
        mem_write_qword((ea + 8), 0);
        mem_write_qword((ea + 16), 0);
        mem_write_qword((ea + 24), 0);
    } else {
        ppc_exception_handler(Except_Type::EXC_ALIGNMENT, 0x00000);
    }
})