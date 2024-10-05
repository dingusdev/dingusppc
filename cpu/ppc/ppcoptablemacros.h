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

#ifndef PPCOPTABLEMACROS_H
#define PPCOPTABLEMACROS_H

#undef OPCODE
#define OPCODE(op, grabber, number, ...)                                                           \
    Opcode##grabber##Grabber[##number] = ppc_##op;

#undef OPCODE31
#define OPCODE31(op, grabber, number, ...) \
    Opcode##grabber##Grabber[##(number << 1)] = ppc_##op;

#undef POWEROPCODE
#define POWEROPCODE(op, grabber, number, ...) \
    if (is_601) \
    Opcode##grabber##Grabber[##number] = power_##op;

#undef POWEROPCODE31
#define POWEROPCODE31(op, grabber, number, ...) \
    if (is_601) \
        Opcode##grabber##Grabber[##(number << 1)] = power_##op;

#undef OPCODESHIFT
#define OPCODESHIFT(op, grabber, number, ...)                                                      \
    Opcode##grabber##Grabber[##number] = ppc_##op<SHFT0>;                                          \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<SHFT1>;

#undef OPCODESHIFTREC
#define OPCODESHIFTREC(op, grabber, number, ...)                                                   \
    Opcode##grabber##Grabber[##number] = ppc_##op<LEFT1, RC0>;                                     \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<LEFT1, RC1>;                               \
    Opcode##grabber##Grabber[##(number + (1 << 9))] = ppc_##op<RIGHT0, RC0>;                       \
    Opcode##grabber##Grabber[##(number + (1 << 9) + 1)] = ppc_##op<RIGHT0, RC1>;                   \

#undef OPCODECARRY
#define OPCODECARRY(op, grabber, number, ...)                                                 \
    Opcode##grabber##Grabber[##number] = ppc_##op<CARRY0, RC0, OV0>;                          \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<CARRY0, RC1, OV0>;                    \
    Opcode##grabber##Grabber[##(number + 2)] = ppc_##op<CARRY0, RC0, OV1>;                    \
    Opcode##grabber##Grabber[##(number + 3)] = ppc_##op<CARRY0, RC1, OV1>;                    \
    Opcode##grabber##Grabber[##(number + (1 << 10))] = ppc_##op<CARRY1, RC0, OV0>;            \
    Opcode##grabber##Grabber[##(number + (1 << 10) + 1)] = ppc_##op<CARRY1, RC1, OV0>;        \
    Opcode##grabber##Grabber[##(number + (1 << 10) + 2)] = ppc_##op<CARRY1, RC0, OV1>;        \
    Opcode##grabber##Grabber[##(number + (1 << 10) + 3)] = ppc_##op<CARRY1, RC1, OV1>;

#undef OPCODEOVREC
#define OPCODEOVREC(op, grabber, number, ...)                                                 \
    Opcode##grabber##Grabber[##number] = ppc_##op<RC0, OV0>;                                  \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<RC1, OV0>;                            \
    Opcode##grabber##Grabber[##(number + 2)] = ppc_##op<RC0, OV1>;                            \
    Opcode##grabber##Grabber[##(number + 3)] = ppc_##op<RC1, OV1>;

#undef OPCODEEXTSIGN
#define OPCODEEXTSIGN(op, grabber, number, ...)                                      \
    Opcode##grabber##Grabber[##(number << 1)] = ppc_##op<int16_t, RC0>;              \
    Opcode##grabber##Grabber[##((number << 1) + 1)] = ppc_##op<int16_t, RC1>;        \
    Opcode##grabber##Grabber[##((number + 32) << 1)] = ppc_##op<int8_t, RC0>;        \
    Opcode##grabber##Grabber[##((number + 32) << 1 + 1)] = ppc_##op<int8_t, RC1>;

#undef POWEROPCODEOVREC
#define POWEROPCODEOVREC(op, grabber, number, ...)                              \
    if (is_601) {                                                               \
        Opcode##grabber##Grabber[##(number << 1)] = power_##op<RC0, OV0>;       \
        Opcode##grabber##Grabber[##((number << 1) + 1)] = power_##op<RC1, OV0>; \
        Opcode##grabber##Grabber[##((number << 1) + 2)] = power_##op<RC0, OV1>; \
        Opcode##grabber##Grabber[##((number << 1) + 3)] = power_##op<RC1, OV1>; \
    }

#undef OPCODEREC
#define OPCODEREC(op, grabber, number, ...)                                                   \
    Opcode##grabber##Grabber[##(number << 1)]       = ppc_##op<RC0>;                          \
    Opcode##grabber##Grabber[##((number << 1) + 1)] = ppc_##op<RC1>;                          \

#undef OPCODERECF
#define OPCODERECF(op, grabber, number, ...)                            \
    Opcode##grabber##Grabber[##(number << 1)]        = ppc_##op<RC0>;   \
    Opcode##grabber##Grabber[##((number << 1) + 1)]   = ppc_##op<RC1>;  \

#undef POWEROPCODEREC
#    define POWEROPCODEREC(op, grabber, number, ...) \
    if (is_601) { \
        Opcode##grabber##Grabber[##(number << 1)]       = power_##op<RC0>; \
        Opcode##grabber##Grabber[##((number << 1) + 1)] = power_##op<RC1>; \
    } 

#undef OPCODELOGIC
#define OPCODELOGIC(op, grabber, number, ...)                                         \
    Opcode##grabber##Grabber[##number]                    = ppc_##op<ppc_and, RC0>;   \
    Opcode##grabber##Grabber[##((number + 1) << 1)]       = ppc_##op<ppc_and, RC1>;   \
    Opcode##grabber##Grabber[##((number + 32) << 1)]      = ppc_##op<ppc_andc, RC0>;  \
    Opcode##grabber##Grabber[##((number + 32) << 1 + 1)]  = ppc_##op<ppc_andc, RC1>;  \
    Opcode##grabber##Grabber[##((number + 96) << 1)]      = ppc_##op<ppc_nor, RC0>;   \
    Opcode##grabber##Grabber[##((number + 96) << 1 + 1)]  = ppc_##op<ppc_nor, RC1>;   \
    Opcode##grabber##Grabber[##((number + 256) << 1)]     = ppc_##op<ppc_eqv, RC0>;   \
    Opcode##grabber##Grabber[##((number + 256) << 1 + 1)] = ppc_##op<ppc_eqv, RC1>;   \
    Opcode##grabber##Grabber[##((number + 288) << 1)]     = ppc_##op<ppc_xor, RC0>;   \
    Opcode##grabber##Grabber[##((number + 288) << 1 + 1)] = ppc_##op<ppc_xor, RC1>;   \
    Opcode##grabber##Grabber[##((number + 384) << 1)]     = ppc_##op<ppc_orc, RC0>;   \
    Opcode##grabber##Grabber[##((number + 384) << 1 + 1)] = ppc_##op<ppc_orc, RC1>;   \
    Opcode##grabber##Grabber[##((number + 416) << 1)]     = ppc_##op<ppc_or, RC0>;    \
    Opcode##grabber##Grabber[##((number + 416) << 1 + 1)] = ppc_##op<ppc_or, RC1>;    \
    Opcode##grabber##Grabber[##((number + 448) << 1)]     = ppc_##op<ppc_nand, RC0>;  \
    Opcode##grabber##Grabber[##((number + 448) << 1 + 1)] = ppc_##op<ppc_nand, RC1>;  \

#undef OPCODELKAA
#define OPCODELKAA(op, grabber, number, ...)                                            \
    Opcode##grabber##Grabber[##number] = ppc_##op<LK0, AA0>;                            \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<LK1, AA0>;                      \
    Opcode##grabber##Grabber[##(number + 2)] = ppc_##op<LK0, AA1>;                      \
    Opcode##grabber##Grabber[##(number + 3)] = ppc_##op<LK1, AA1>;

#undef OPCODEMEM
#define OPCODEMEM(op, grabber, number, ...)                        \
    Opcode##grabber##Grabber[##(number)] = ppc_##op<uint32_t>;     \
    Opcode##grabber##Grabber[##(number + 2)] = ppc_##op<uint8_t>;  \
    Opcode##grabber##Grabber[##(number + 8)] = ppc_##op<uint16_t>;

#undef OPCODEMEMINDEXED
#define OPCODEMEMINDEXED(op, grabber, number, ...)                                      \
        Opcode##grabber##Grabber[##((number << 1))]            = ppc_##op<uint32_t>;    \
        Opcode##grabber##Grabber[##((number << 1) + (1 << 6))] = ppc_##op<uint8_t>;      \
        Opcode##grabber##Grabber[##((number << 1) + (1 << 8))] = ppc_##op<uint16_t>;                       

#undef OPCODE601REC
#define OPCODE601REC(op, grabber, number, ...)                                                \
        if (is_601) {                                                                         \
            Opcode##grabber##Grabber[##number]       = ppc_##op<IS601, RC0>;                  \
            Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<IS601, RC1>;                  \
        } else {                                                                              \
            Opcode##grabber##Grabber[##number]       = ppc_##op<NOT601, RC0>;                 \
            Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<NOT601, RC1>;                 \
        }

#undef OPCODE601L
#define OPCODE601L(op, grabber, number, ...)                              \
    if (is_601) {                                                         \
        Opcode##grabber##Grabber[##number]       = ppc_##op<LK0, IS601>;  \
        Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<LK1, IS601>;  \
        }                                                                 \
    else {                                                                \
        Opcode##grabber##Grabber[##number]       = ppc_##op<LK0, NOT601>; \
        Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<LK1, NOT601>; \
    }

#undef OPCODEL
#define OPCODEL(op, grabber, number, ...)                \
    Opcode##grabber##Grabber[##number] = ppc_##op<LK0>;  \
    Opcode##grabber##Grabber[##(number + 1)] = ppc_##op<LK1>;

#endif /* PPCEMU_H */
