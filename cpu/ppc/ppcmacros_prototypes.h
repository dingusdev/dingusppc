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

#    undef OPCODE
#    define OPCODE(op, ...)                                                                        \
        void ppc_##op(uint32_t instr);

#    undef POWEROPCODE
#    define POWEROPCODE(op, ...)                                                                   \
        void power_##op(uint32_t instr);

#    undef OPCODESHIFT
#    define OPCODESHIFT(op, ...)                                                                   \
        template <field_shift shift>                                                               \
        void ppc_##op(uint32_t instr);

#    undef OPCODESHIFTREC
#    define OPCODESHIFTREC(op, ...)                                                                \
        template <field_direction isleft, field_rc rec>                                            \
        void ppc_##op(uint32_t instr);

#    undef OPCODECARRY
#    define OPCODECARRY(op, ...)                                                                   \
        template <field_carry carry, field_rc rec, field_ov ov>                                    \
        void ppc_##op(uint32_t instr);

#    undef OPCODEOVREC
#    define OPCODEOVREC(op, ...)                                                                   \
        template <field_rc rec, field_ov ov>                                                       \
        void ppc_##op(uint32_t instr);

#    undef OPCODEEXTSIGN
#    define OPCODEEXTSIGN(op, ...)                                                                 \
        template <class T, field_rc rec>                                                           \
        void ppc_##op(uint32_t instr);

#    undef POWEROPCODEOVREC
#    define POWEROPCODEOVREC(op, ...)                                                              \
        template <field_rc rec, field_ov ov>                                                       \
        void power_##op(uint32_t instr);

#    undef OPCODEREC
#    define OPCODEREC(op, ...)                                                                     \
        template <field_rc rec>                                                                    \
        void ppc_##op(uint32_t instr);

#    undef POWEROPCODEREC
#    define POWEROPCODEREC(op, ...)                                                                \
        template <field_rc rec>                                                                    \
        void power_##op(uint32_t instr);

#    undef OPCODELOGIC
#    define OPCODELOGIC(op, ...)                                                                   \
        template <logical_fun logical_op, field_rc rec>                                            \
        void ppc_##op(uint32_t instr);

#    undef OPCODELKAA
#    define OPCODELKAA(op, ...)                                                                    \
        template <field_lk l, field_aa a>                                                          \
        void ppc_##op(uint32_t instr);

#    undef OPCODEMEM
#    define OPCODEMEM(op, ...)                                                                     \
        template <class T>                                                                         \
        void ppc_##op(uint32_t instr);

#    undef OPCODE601REC
#    define OPCODE601REC(op, ...)                                                                  \
        template <field_601 for601, field_rc rec>                                                  \
        void ppc_##op(uint32_t instr);

#    undef OPCODE601L
#    define OPCODE601L(op, ...)                                                                    \
        template <field_lk l, field_601 for601>                                                    \
        void ppc_##op(uint32_t instr);

#    undef OPCODEL
#    define OPCODEL(op, ...)                                                                       \
        template <field_lk l>                                                                      \
        void ppc_##op(uint32_t instr);

#endif /* PPCEMU_H */
