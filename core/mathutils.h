/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

inline void _u32xu64(uint32_t a, uint64_t b, uint32_t &hi, uint64_t &lo)
{
    uint64_t p0 = (b & 0xffffffff) * a;
    uint64_t p1 = (b >> 32) * a;
    lo = p0 + (p1 << 32);
    hi = (p1 >> 32) + (lo < p0);
}

inline void _u32xu64(uint32_t a, uint64_t b, uint64_t &hi, uint32_t &lo)
{
    uint64_t p0 = (b & 0xffffffff) * a;
    uint64_t p1 = (b >> 32) * a;
    lo = (uint32_t)p0;
    hi = (p0 >> 32) + p1;
}

inline void _u64xu64(uint64_t a, uint64_t b, uint64_t &hi, uint64_t &lo)
{
    uint32_t p0h; uint64_t p0l; _u32xu64((uint32_t)b, a, p0h, p0l);
    uint64_t p1h; uint32_t p1l; _u32xu64(b >> 32, a, p1h, p1l);
    lo = p0l + ((uint64_t)p1l << 32);
    hi = p0h + p1h + (lo < p0l);
}

#endif // MATH_UTILS_H
