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


def find_imm(line):
    pos = 12
    while pos < len(line):
        reg_id = line[pos:pos+4]
        if reg_id.startswith("rD") or reg_id.startswith("rA") or reg_id.startswith("rB"):
            pos += 16
        elif reg_id.startswith("XER:"):
            pos += 18
        elif reg_id.startswith("CR:"):
            pos += 17
        elif reg_id.startswith("imm"):
            return int(line[pos+4:pos+14], base=16)
    return 0


with open("instruction_tests_console.txt", "r") as in_file:
    with open("ppcinttests.csv", "w") as out_file:
        lineno = 0
        for line in in_file:
            line = line.strip()
            opcode = (line[0:8]).rstrip()
            out_file.write(opcode + ",")

            imm = find_imm(line)

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
                elif reg_id.startswith("rB"):
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
                else:
                    out_file.write("Unknown reg ID" + reg_id)
                    break

            out_file.write("\n")

            lineno += 1
            if lineno > 534:
                break
