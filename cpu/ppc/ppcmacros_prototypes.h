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

#ifndef PPCMACROS_PROTTYPES_H
#define PPCMACROS_PROTTYPES_H

#    define OPCODE(op, ...)                                                                        \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define POWEROPCODE(op, ...)                                                                   \
        void dppc_interpreter::power_##op(uint32_t instr)

#    define OPCODESHIFT(op, ...)                                                                   \
        template <field_shift shift>                                                               \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODESHIFTREC(op, ...)                                                                \
        template <field_direction isleft, field_rc rec>                                            \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODECARRY(op, ...)                                                                   \
        template <field_carry carry, field_rc rec, field_ov ov>                                    \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODEOVREC(op, ...)                                                                   \
        template <field_rc rec, field_ov ov>                                                       \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODEEXTSIGN(op, ...)                                                                 \
        template <class T, field_rc rec>                                                           \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define POWEROPCODEOVREC(op, ...)                                                              \
        template <field_rc rec, field_ov ov>                                                       \
        void dppc_interpreter::power_##op(uint32_t instr)

#    define OPCODEREC(op, ...)                                                                     \
        template <field_rc rec>                                                                    \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define POWEROPCODEREC(op, ...)                                                                \
        template <field_rc rec>                                                                    \
        void dppc_interpreter::power_##op(uint32_t instr)

#    define OPCODELOGIC(op, ...)                                                                   \
        template <logical_fun logical_op, field_rc rec>                                            \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODELKAA(op, ...)                                                                    \
        template <field_lk l, field_aa a>                                                          \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODEMEM(op, ...)                                                                     \
        template <class T>                                                                         \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODE601REC(op, ...)                                                                  \
        template <field_601 for601, field_rc rec>                                                  \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODE601L(op, ...)                                                                    \
        template <field_lk l, field_601 for601>                                                    \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#    define OPCODEL(op, ...)                                                                       \
        template <field_lk l>                                                                      \
        void dppc_interpreter::ppc_##op(uint32_t instr)

#endif /* PPCEMU_H */