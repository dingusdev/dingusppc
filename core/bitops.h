/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** Non-standard low-level bitwise operations. */

#ifndef BIT_OPS_H
#define BIT_OPS_H

#if defined(__GNUG__) && !defined(__clang__) // GCC, mybe ICC but not Clang

#   include <x86intrin.h>

#   define ROTL_32(x, n)    (_rotl((x), (n)))
#   define ROTR_32(x, n)    (_rotr((x), (n)))

#elif defined(_MSC_VER)   // MSVC

#   include <intrin.h>

#   define ROTL_32(x, n)    (_rotl((x), (n)))
#   define ROTR_32(x, n)    (_rotr((x), (n)))

#else

    // cyclic rotation idioms that modern compilers will
    // recognize and generate very compact code for
    // evolving specific machine instructions

    inline unsigned ROTL_32(unsigned x, unsigned n) {
        n &= 0x1F;
        return (x << n) | (x >> (32 - n));
    }

    inline unsigned ROTR_32(unsigned x, unsigned n) {
        n &= 0x1F;
        return (x >> n) | (x << (32 - n));
    }

#endif

#endif // BIT_OPS_H
