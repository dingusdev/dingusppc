/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#   define BYTESWAP_16(x) (__builtin_bswap16 (x))
#   define BYTESWAP_32(x) (__builtin_bswap32 (x))
#   define BYTESWAP_64(x) (__builtin_bswap64 (x))

#elif _MSC_VER   /* MSVC */

#   include <stdlib.h>

#   define BYTESWAP_16(x) (_byteswap_ushort (x))
#   define BYTESWAP_32(x) (_byteswap_ulong (x))
#   define BYTESWAP_64(x) (_byteswap_uint64 (x))

#elif __has_include(<byteswap.h>) /* Many unixes */

#   include <byteswap.h>

#   define BYTESWAP_16(x) (bswap_16(x))
#   define BYTESWAP_32(x) (bswap_32(x))
#   define BYTESWAP_64(x) (bswap_64(x))

// TODO: C++23 has std::byteswap

#else

// #warning is not standard until C++23:
//
// #   warning "Unknown byte swapping built-ins (do it the slow way)!"

#   include <cstdint>

// Most optimizing compilers will translate these functions directly
// into their native instructions (e.g. bswap on x86).
inline std::uint16_t byteswap_16_impl(std::uint16_t x)
{
    return (x >> 8) | (x << 8);
}

inline std::uint32_t byteswap_32_impl(std::uint32_t x)
{
    return (
          ((x & UINT32_C(0x000000FF)) << 24)
        | ((x & UINT32_C(0x0000FF00)) << 8)
        | ((x & UINT32_C(0x00FF0000)) >> 8)
        | ((x & UINT32_C(0xFF000000)) >> 24)
        );
}

inline std::uint64_t byteswap_64_impl(std::uint64_t x)
{
    return (
          ((x & UINT64_C(0x00000000000000FF)) << 56)
        | ((x & UINT64_C(0x000000000000FF00)) << 40)
        | ((x & UINT64_C(0x0000000000FF0000)) << 24)
        | ((x & UINT64_C(0x00000000FF000000)) << 8)
        | ((x & UINT64_C(0x000000FF00000000)) >> 8)
        | ((x & UINT64_C(0x0000FF0000000000)) >> 24)
        | ((x & UINT64_C(0x00FF000000000000)) >> 40)
        | ((x & UINT64_C(0xFF00000000000000)) >> 56)
        );
}

#   define BYTESWAP_16(x) (byteswap_16_impl(x))
#   define BYTESWAP_32(x) (byteswap_32_impl(x))
#   define BYTESWAP_64(x) (byteswap_64_impl(x))

#endif

#define BYTESWAP_SIZED(val, size) \
    ((size) == 2 ? BYTESWAP_16(val) : (size) == 4 ? BYTESWAP_32(val) : (val))

#endif /* ENDIAN_SWAP_H */
