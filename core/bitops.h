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

#include <cinttypes>

#if defined(__GNUG__) && !defined(__clang__)  && (defined(__x86_64__) || defined(__i386__)) // GCC, mybe ICC but not Clang

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

template <class T>
inline T extract_bits(T val, int pos, int len) {
    return (val >> pos) & ((len == sizeof(T) * 8) ? (T)-1 : ((T)1 << len) - 1);
}

template <class T>
inline void insert_bits(T &old_val, T new_val, int pos, int len) {
    T mask = ((len == sizeof(T) * 8) ? (T)-1 : ((T)1 << len) - 1) << pos;
    old_val = (old_val & ~mask) | ((new_val << pos) & mask);
}

/* Return true if the specified bit is different in the given numbers. */
static inline bool bit_changed(uint64_t old_val, uint64_t new_val, int bit_num) {
    return (old_val ^ new_val) & (1ULL << bit_num);
}

static inline bool bit_set(const uint64_t val, const int bit_num) {
    return !!(val & (1ULL << bit_num));
}

template <class T>
inline void clear_bit(T &val, const int bit_num) {
    val &= ~((T)1 << bit_num);
}

template <class T>
inline void set_bit(T &val, const int bit_num) {
    val |= ((T)1 << bit_num);
}

static inline uint32_t extract_with_wrap_around(uint32_t val, int pos, int size) {
    return (uint32_t)((((uint64_t)val << 32) | val) >> ((8 - (pos & 3) - size) << 3)) &
        ((1LL << (size << 3)) - 1);
}

#endif // BIT_OPS_H
