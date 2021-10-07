def gen_ppc_opcode(opc_str, imm):
    if opc_str == "ADD":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x10A << 1)
    elif opc_str == "ADD.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x10A << 1) + 1
    elif opc_str == "ADDC":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xA << 1)
    elif opc_str == "ADDC.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xA << 1) + 1
    elif opc_str == "ADDCO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x20A << 1)
    elif opc_str == "ADDCO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x20A << 1) + 1
    elif opc_str == "ADDO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x30A << 1)
    elif opc_str == "ADDO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x30A << 1) + 1
    elif opc_str == "ADDE":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x8A << 1)
    elif opc_str == "ADDE.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x8A << 1) + 1
    elif opc_str == "ADDEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x28A << 1)
    elif opc_str == "ADDEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x28A << 1) + 1
    elif opc_str == "ADDI":
        return (0x0E << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ADDIC":
        return (0x0C << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ADDIC.":
        return (0x0D << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ADDIS":
        return (0x0F << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ADDME":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xEA << 1)
    elif opc_str == "ADDME.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xEA << 1) + 1
    elif opc_str == "ADDMEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2EA << 1)
    elif opc_str == "ADDMEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2EA << 1) + 1
    elif opc_str == "ADDZE":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xCA << 1)
    elif opc_str == "ADDZE.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xCA << 1) + 1
    elif opc_str == "ADDZEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2CA << 1)
    elif opc_str == "ADDZEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2CA << 1) + 1
    elif opc_str == "AND":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1C << 1)
    elif opc_str == "AND.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1C << 1) + 1
    elif opc_str == "ANDC":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3C << 1)
    elif opc_str == "ANDC.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3C << 1) + 1
    elif opc_str == "ANDI.":
        return (0x1C << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ANDIS.":
        return (0x1D << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "CMP":
        return (0x1F << 26) + (3 << 16) + (4 << 11)
    elif opc_str == "CMPI":
        return (0x0B << 26) + (0 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "CMPL":
        return (0x1F << 26) + (3 << 16) + (4 << 11) + (0x20 << 1)
    elif opc_str == "CMPLI":
        return (0x0A << 26) + (0 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "CNTLZW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x1A << 1)
    elif opc_str == "CNTLZW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x1A << 1) + 1
    elif opc_str == "DIVW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1EB << 1)
    elif opc_str == "DIVW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1EB << 1) + 1
    elif opc_str == "DIVWO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3EB << 1)
    elif opc_str == "DIVWO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3EB << 1) + 1
    elif opc_str == "DIVWU":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1CB << 1)
    elif opc_str == "DIVWU.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1CB << 1) + 1
    elif opc_str == "DIVWUO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3CB << 1)
    elif opc_str == "DIVWUO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x3CB << 1) + 1
    elif opc_str == "EQV":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x11C << 1)
    elif opc_str == "EQV.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x11C << 1) + 1
    elif opc_str == "EXTSB":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x3BA << 1)
    elif opc_str == "EXTSB.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x3BA << 1) + 1
    elif opc_str == "EXTSH":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x39A << 1)
    elif opc_str == "EXTSH.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x39A << 1) + 1
    elif opc_str == "FABS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x108 << 1)
    elif opc_str == "FABS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x108 << 1) + 1
    elif opc_str == "FADD":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1)
    elif opc_str == "FADD.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1) + 1
    elif opc_str == "FADDS":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1)
    elif opc_str == "FADDS.":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1) + 1
    elif opc_str == "FCMPO":
        return (0x3B << 26) + (3 << 16) + (4 << 11) + (0x20 << 1)
    elif opc_str == "FCMPU":
        return (0x3B << 26) + (3 << 16) + (4 << 11)
    elif opc_str == "FCTIW":
        return (0x3B << 26) + (3 << 16) + (4 << 11) + (0xE << 1)
    elif opc_str == "FCTIW.":
        return (0x3B << 26) + (3 << 16) + (4 << 11) + (0xE << 1) + 1
    elif opc_str == "FCTIWZ":
        return (0x3B << 26) + (3 << 16) + (4 << 11) + (0xF << 1)
    elif opc_str == "FCTIWZ.":
        return (0x3B << 26) + (3 << 16) + (4 << 11) + (0xF << 1) + 1
    elif opc_str == "FDIV":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x12 << 1)
    elif opc_str == "FDIV.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x12 << 1) + 1
    elif opc_str == "FMADD":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1D << 1)
    elif opc_str == "FMADD.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1D << 1) + 1
    elif opc_str == "FMADDS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1D << 1)
    elif opc_str == "FMADDS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1D << 1) + 1
    elif opc_str == "FMUL":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x19 << 1)
    elif opc_str == "FMUL.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x19 << 1) + 1
    elif opc_str == "FMULS":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x19 << 1)
    elif opc_str == "FMULS.":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x19 << 1) + 1
    elif opc_str == "FNABS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x88 << 1)
    elif opc_str == "FNABS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x88 << 1) + 1
    elif opc_str == "FNEG":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x28 << 1)
    elif opc_str == "FMULS.":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 6) + (0x28 << 1) + 1
    elif opc_str == "FMSUB":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1)
    elif opc_str == "FMSUB.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1) + 1
    elif opc_str == "FMSUBS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1)
    elif opc_str == "FMSUBS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1) + 1
    elif opc_str == "FNMADD":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1F << 1)
    elif opc_str == "FNMADD.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1F << 1) + 1
    elif opc_str == "FNMADDS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1F << 1)
    elif opc_str == "FNMADDS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1F << 1) + 1
    elif opc_str == "FNMSUB":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1)
    elif opc_str == "FNMSUB.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1) + 1
    elif opc_str == "FNMSUBS":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1)
    elif opc_str == "FNMSUBS.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (4 << 6) + (0x1C << 1) + 1
    elif opc_str == "FSUB":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1)
    elif opc_str == "FSUB.":
        return (0x3F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1) + 1
    elif opc_str == "FSUBS":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1)
    elif opc_str == "FSUBS.":
        return (0x3B << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x15 << 1) + 1
    elif opc_str == "MULHW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x4B << 1)
    elif opc_str == "MULHW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x4B << 1) + 1
    elif opc_str == "MULHWU":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xB << 1)
    elif opc_str == "MULHWU.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xB << 1) + 1
    elif opc_str == "MULLI":
        return (0x07 << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "MULLW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xEB << 1)
    elif opc_str == "MULLW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0xEB << 1) + 1
    elif opc_str == "MULLWO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x2EB << 1)
    elif opc_str == "MULLWO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x2EB << 1) + 1
    elif opc_str == "NAND":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1DC << 1)
    elif opc_str == "NAND.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1DC << 1) + 1
    elif opc_str == "NEG":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x68 << 1)
    elif opc_str == "NEG.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x68 << 1) + 1
    elif opc_str == "NEGO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x268 << 1)
    elif opc_str == "NEGO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x268 << 1) + 1
    elif opc_str == "NOR":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x7C << 1)
    elif opc_str == "NOR.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x7C << 1) + 1
    elif opc_str == "OR":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1BC << 1)
    elif opc_str == "OR.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x1BC << 1) + 1
    elif opc_str == "ORC":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x19C << 1)
    elif opc_str == "ORC.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x19C << 1) + 1
    elif opc_str == "ORI":
        return (0x18 << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "ORIS":
        return (0x19 << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "SLW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x18 << 1)
    elif opc_str == "SLW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x18 << 1) + 1
    elif opc_str == "SRAW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x318 << 1)
    elif opc_str == "SRAW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x318 << 1) + 1
    elif opc_str == "SRAWI":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + ((imm & 0x1F) << 11) + (0x338 << 1)
    elif opc_str == "SRAWI.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + ((imm & 0x1F) << 11) + (0x338 << 1) + 1
    elif opc_str == "SRW":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x218 << 1)
    elif opc_str == "SRW.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x218 << 1) + 1
    elif opc_str == "SUBF":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x28 << 1)
    elif opc_str == "SUBF.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x28 << 1) + 1
    elif opc_str == "SUBFO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x228 << 1)
    elif opc_str == "SUBFO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x228 << 1) + 1
    elif opc_str == "SUBFC":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x8 << 1)
    elif opc_str == "SUBFC.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x8 << 1) + 1
    elif opc_str == "SUBFCO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x208 << 1)
    elif opc_str == "SUBFCO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x208 << 1) + 1
    elif opc_str == "SUBFE":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x88 << 1)
    elif opc_str == "SUBFE.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x88 << 1) + 1
    elif opc_str == "SUBFEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x288 << 1)
    elif opc_str == "SUBFEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x288 << 1) + 1
    elif opc_str == "SUBFIC":
        return (0x08 << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "SUBFME":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xE8 << 1)
    elif opc_str == "SUBFME.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xE8 << 1) + 1
    elif opc_str == "SUBFMEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2E8 << 1)
    elif opc_str == "SUBFMEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2E8 << 1) + 1
    elif opc_str == "SUBFZE":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xC8 << 1)
    elif opc_str == "SUBFZE.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0xC8 << 1) + 1
    elif opc_str == "SUBFZEO":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2C8 << 1)
    elif opc_str == "SUBFZEO.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (0x2C8 << 1) + 1
    elif opc_str == "XOR":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x13C << 1)
    elif opc_str == "XOR.":
        return (0x1F << 26) + (3 << 21) + (3 << 16) + (4 << 11) + (0x13C << 1) + 1
    elif opc_str == "XORI":
        return (0x1A << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)
    elif opc_str == "XORIS":
        return (0x1B << 26) + (3 << 21) + (3 << 16) + (imm & 0xFFFF)

def gen_rot_opcode(opc_str, sh, mb, me):
    if opc_str == "RLWIMI":
        return (0x14 << 26) + (4 << 21) + (3 << 16) + (sh << 11) + (mb << 6) + (me << 1)
    elif opc_str == "RLWIMI.":
        return (0x14 << 26) + (4 << 21) + (3 << 16) + (sh << 11) + (mb << 6) + (me << 1) + 1
    elif opc_str == "RLWINM":
        return (0x15 << 26) + (4 << 21) + (3 << 16) + (sh << 11) + (mb << 6) + (me << 1)
    elif opc_str == "RLWINM.":
        return (0x15 << 26) + (4 << 21) + (3 << 16) + (sh << 11) + (mb << 6) + (me << 1) + 1

def extract_imm(line):
    pos = 12
    while pos < len(line):
        reg_id = line[pos:pos+4]
        if reg_id.startswith("rD") or reg_id.startswith("rA"):
            pos += 16
        elif reg_id.startswith("rB"):
            pos += 16
        elif reg_id.startswith("XER:"):
            pos += 18
        elif reg_id.startswith("CR:"):
            pos += 17
        elif reg_id.startswith("imm"):
            return int(line[pos+4:pos+14], base=16)
    return 0

def extract_rot_params(line):
    pos = 12
    sh = 0
    mb = 0
    me = 0
    while pos < len(line):
        reg_id = line[pos:pos+4]
        if reg_id.startswith("rD") or reg_id.startswith("rA") or reg_id.startswith("rB") or reg_id.startswith("rS"):
            pos += 16
        elif reg_id.startswith("XER:"):
            pos += 18
        elif reg_id.startswith("CR:"):
            pos += 17
        elif reg_id.startswith("SH"):
            sh = int(line[pos+3:pos+13], base=16)
            pos += 16
        elif reg_id.startswith("MB"):
            mb = int(line[pos+3:pos+13], base=16)
            pos += 16
        elif reg_id.startswith("ME"):
            me = int(line[pos+3:pos+13], base=16)
            pos += 16
    return (sh, mb, me)


if __name__ == "__main__":
    with open("ppcinttest.txt", "r") as in_file:
        with open("ppcinttests.csv", "w") as out_file:
            lineno = 0
            for line in in_file:
                lineno += 1

                line = line.strip()
                opcode = (line[0:8]).rstrip().upper()
                out_file.write(opcode + ",")

                if opcode.startswith("RLWI"):
                    sh, mb, me = extract_rot_params(line)
                    out_file.write("0x{:X}".format(gen_rot_opcode(opcode, sh, mb, me)))
                else:
                    imm = extract_imm(line)
                    out_file.write("0x{:X}".format(gen_ppc_opcode(opcode, imm)))

                pos = 12

                while pos < len(line):
                    reg_id = line[pos:pos+4]
                    if reg_id.startswith("rD"):
                        out_file.write(",rD=" + line[pos+3:pos+13])
                        pos += 16
                    elif reg_id.startswith("rA"):
                        out_file.write(",rA=" + line[pos+3:pos+13])
                        pos += 16
                    elif reg_id.startswith("rB") or reg_id.startswith("rS"):
                        out_file.write(",rB=" + line[pos+3:pos+13])
                        pos += 16
                    elif reg_id.startswith("XER:"):
                        out_file.write(",XER=" + line[pos+5:pos+15])
                        pos += 18
                    elif reg_id.startswith("CR:"):
                        out_file.write(",CR=" + line[pos+4:pos+14])
                        pos += 17
                    elif reg_id.startswith("imm"):
                        pos += 17 # ignore immediate operands
                    elif reg_id.startswith("SH"):
                        pos += 16
                    elif reg_id.startswith("MB") or reg_id.startswith("ME"):
                        pos += 16
                    else:
                        out_file.write("Unknown reg ID" + reg_id)
                        break

                out_file.write("\n")

    with open("ppcfloattest.txt", "r") as in_file:
        with open("ppcfloattests.csv", "w") as out_file:
            lineno = 0
            for line in in_file:
                lineno += 1

                line = line.strip()
                opcode = (line[0:8]).rstrip().upper()
                out_file.write(opcode + ",")

                if opcode.startswith("RLWI"):
                    sh, mb, me = extract_rot_params(line)
                    out_file.write("0x{:X}".format(gen_rot_opcode(opcode, sh, mb, me)))
                else:
                    imm = extract_imm(line)
                    out_file.write("0x{:X}".format(gen_ppc_opcode(opcode, imm)))

                pos = 12

                while pos < len(line):
                    reg_id = line[pos:pos+4]
                    if reg_id.startswith("frD"):
                        out_file.write(",frD=" + line[pos+4:pos+14])
                        pos += 16
                    elif reg_id.startswith("frA"):
                        out_file.write(",frA=" + line[pos+4:pos+14])
                        pos += 16
                    elif reg_id.startswith("frB"):
                        out_file.write(",frB=" + line[pos+4:pos+14])
                        pos += 16
                    elif reg_id.startswith("frC"):
                        out_file.write(",frC=" + line[pos+4:pos+14])
                        pos += 16
                    elif reg_id.startswith("FPSCR:"):
                        out_file.write(",FPSCR=" + line[pos+7:pos+17])
                        pos += 19
                    elif reg_id.startswith("CR:"):
                        out_file.write(",CR=" + line[pos+4:pos+14])
                        pos += 17
                    elif reg_id.startswith("imm"):
                        pos += 17 # ignore immediate operands
                    else:
                        out_file.write("Unknown reg ID" + reg_id)
                        break

                out_file.write("\n")