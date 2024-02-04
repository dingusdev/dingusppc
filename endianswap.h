/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** @file Macros for performing byte swapping for quick endian conversion.

    It will try to use the fast built-in machine instructions first
    and fall back to the slow conversion if none were found.
 */

#ifndef ENDIAN_SWAP_H
#define ENDIAN_SWAP_H

#ifdef __GNUG__ /* GCC, ICC and Clang */

#   ifdef __APPLE__
#       include <machine/endian.h>
#   else
#       include <endian.h>
#   endif

#   define BYTESWAP_16(x) (__builtin_bswap16 (x))
#   define BYTESWAP_32(x) (__builtin_bswap32 (x))
#   define BYTESWAP_64(x) (__builtin_bswap64 (x))

#elif _MSC_VER   /* MSVC */

#   include <stdlib.h>

#   define BYTESWAP_16(x) (_byteswap_ushort (x))
#   define BYTESWAP_32(x) (_byteswap_ulong (x))
#   define BYTESWAP_64(x) (_byteswap_uint64 (x))

#else

#   warning "Unknown byte swapping built-ins (do it the slow way)!"

#   define BYTESWAP_16(x) (((x) >> 8)  | (((x) & 0xFFUL) << 8))

#   define BYTESWAP_32(x) (((x) >> 24) | (((x) >> 8) & 0xFF00UL) | \
                         (((x) & 0xFF00UL) << 8) | ((x) << 24))

#   define BYTESWAP_64(x)                       \
        (((x) >> 56)                          | \
        (((x) & 0x00FF000000000000ULL) >> 48) | \
        (((x) & 0x0000FF0000000000ULL) >> 40) | \
        (((x) & 0x000000FF00000000ULL) >> 32) | \
        (((x) & 0x00000000FF000000ULL) << 32) | \
        (((x) & 0x0000000000FF0000ULL) << 40) | \
        (((x) & 0x000000000000FF00ULL) << 48) | \
        (((x) & 0x00000000000000FFULL) << 56))

#endif

#define BYTESWAP_SIZED(val, size) \
    ((size) == 2 ? BYTESWAP_16(val) : (size) == 4 ? BYTESWAP_32(val) : (val))

#endif /* ENDIAN_SWAP_H */
