/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

#include "ppcemu.h"

#ifndef PPCOPMACROS_H
#define PPCOPMACROS_H

#undef OPCODE
#define OPCODE(op, ...)                                                                            \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }

#undef POWEROPCODE
#define POWEROPCODE(op, ...)                                                                       \
    void dppc_interpreter::power_##op(uint32_t instr) {                                            \
        __VA_ARGS__                                                                                \
    }

#undef OPCODESHIFT
#define OPCODESHIFT(op, ...)                                                                       \
    template <field_shift shift>                                                                   \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<SHFT0>(uint32_t instr);                               \
    template void dppc_interpreter::ppc_##op<SHFT1>(uint32_t instr);

#undef OPCODESHIFTREC
#define OPCODESHIFTREC(op, ...)                                                                    \
    template <field_direction isleft, field_rc rec>                                                \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<RIGHT0, RC0>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<LEFT1, RC0>(uint32_t instr);                          \
    template void dppc_interpreter::ppc_##op<RIGHT0, RC1>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<LEFT1, RC1>(uint32_t instr);                      

#undef OPCODECARRY
#define OPCODECARRY(op, ...)                                                                       \
    template <field_carry carry, field_rc rec, field_ov ov>                                        \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<CARRY0, RC0, OV0>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY0, RC1, OV0>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY0, RC0, OV1>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY0, RC1, OV1>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY1, RC0, OV0>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY1, RC1, OV0>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY1, RC0, OV1>(uint32_t instr);                    \
    template void dppc_interpreter::ppc_##op<CARRY1, RC1, OV1>(uint32_t instr);

#undef OPCODEOVREC
#define OPCODEOVREC(op, ...)                                                                       \
    template <field_rc rec, field_ov ov>                                                           \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<RC0, OV0>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<RC1, OV0>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<RC0, OV1>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<RC1, OV1>(uint32_t instr);

#undef OPCODEEXTSIGN
#define OPCODEEXTSIGN(op, ...)                                                                     \
    template <class T, field_rc rec>                                                               \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<int8_t, RC0>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<int16_t, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<int8_t, RC1>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<int16_t, RC1>(uint32_t instr);                        

#undef POWEROPCODEOVREC
#define POWEROPCODEOVREC(op, ...)                                                                  \
    template <field_rc rec, field_ov ov>                                                           \
    void dppc_interpreter::power_##op(uint32_t instr) {                                            \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::power_##op<RC0, OV0>(uint32_t instr);                          \
    template void dppc_interpreter::power_##op<RC1, OV0>(uint32_t instr);                          \
    template void dppc_interpreter::power_##op<RC0, OV1>(uint32_t instr);                          \
    template void dppc_interpreter::power_##op<RC1, OV1>(uint32_t instr);

#undef OPCODEREC
#define OPCODEREC(op, ...)                                                                         \
    template <field_rc rec>                                                                        \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<RC0>(uint32_t instr);                                 \
    template void dppc_interpreter::ppc_##op<RC1>(uint32_t instr);                                 \

#undef POWEROPCODEREC
#define POWEROPCODEREC(op, ...)                                                                    \
    template <field_rc rec>                                                                        \
    void dppc_interpreter::power_##op(uint32_t instr) {                                            \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::power_##op<RC0>(uint32_t instr);                               \
    template void dppc_interpreter::power_##op<RC1>(uint32_t instr);                             

#undef OPCODELOGIC
#define OPCODELOGIC(op, ...)                                                                       \
    template <logical_fun logical_op, field_rc rec>                                                \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<ppc_and, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_andc, RC0>(uint32_t instr);                       \
    template void dppc_interpreter::ppc_##op<ppc_eqv, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_nand, RC0>(uint32_t instr);                       \
    template void dppc_interpreter::ppc_##op<ppc_nor, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_or, RC0>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<ppc_orc, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_xor, RC0>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_and, RC1>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_andc, RC1>(uint32_t instr);                       \
    template void dppc_interpreter::ppc_##op<ppc_eqv, RC1>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_nand, RC1>(uint32_t instr);                       \
    template void dppc_interpreter::ppc_##op<ppc_nor, RC1>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_or, RC1>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<ppc_orc, RC1>(uint32_t instr);                        \
    template void dppc_interpreter::ppc_##op<ppc_xor, RC1>(uint32_t instr);

#undef OPCODELKAA
#define OPCODELKAA(op, ...)                                                                        \
    template <field_lk l, field_aa a>                                                              \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<LK0, AA0>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<LK0, AA1>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<LK1, AA0>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<LK1, AA1>(uint32_t instr);

#undef OPCODEMEM
#define OPCODEMEM(op, ...)                                                                         \
    template <class T>                                                                             \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<uint8_t>(uint32_t instr);                             \
    template void dppc_interpreter::ppc_##op<uint16_t>(uint32_t instr);                            \
    template void dppc_interpreter::ppc_##op<uint32_t>(uint32_t instr);

#undef OPCODE601REC
#define OPCODE601REC(op, ...)                                                                      \
    template <field_601 for601, field_rc rec>                                                      \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<NOT601, RC0>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<IS601, RC0>(uint32_t instr);                          \
    template void dppc_interpreter::ppc_##op<NOT601, RC1>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<IS601, RC1>(uint32_t instr);                          \
                             

#undef OPCODE601L
#define OPCODE601L(op, ...)                                                                        \
    template <field_lk l, field_601 for601>                                                        \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<LK0, NOT601>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<LK0, IS601>(uint32_t instr);                          \
    template void dppc_interpreter::ppc_##op<LK1, NOT601>(uint32_t instr);                         \
    template void dppc_interpreter::ppc_##op<LK1, IS601>(uint32_t instr);                          \
                             

#undef OPCODEL
#define OPCODEL(op, ...)                                                                           \
    template <field_lk l>                                                                          \
    void dppc_interpreter::ppc_##op(uint32_t instr) {                                              \
        __VA_ARGS__                                                                                \
    }                                                                                              \
    template void dppc_interpreter::ppc_##op<LK0>(uint32_t instr);                                 \
    template void dppc_interpreter::ppc_##op<LK1>(uint32_t instr);

#endif /* PPCEMU_H */