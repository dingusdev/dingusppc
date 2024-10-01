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

// The Power-specific opcodes for the processor - ppcopcodes.cpp
// Any shared opcodes are in ppcopcodes.cpp

#include "ppcemu.h"
#include "ppcdecodemacros.h"
#include "ppcmmu.h"
#include <stdint.h>

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t power_rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFU >> rot_mb;
    uint32_t m2 = 0xFFFFFFFFU << (31 - rot_me);
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

#include "ppcopmacros.h"
#include "poweropcodes.include"