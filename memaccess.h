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

/** @file Set of macros for accessing host memory in units of various sizes
          and endianness.
 */

#ifndef MEM_ACCESS_H
#define MEM_ACCESS_H

#include "endianswap.h"
#include <cinttypes>
#include <loguru.hpp>

/* read an aligned big-endian WORD (16bit) */
#define READ_WORD_BE_A( addr) (BYTESWAP_16(*((uint16_t*)(addr))))

/* read an aligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_A(addr) (BYTESWAP_32(*((uint32_t*)(addr))))

/* read an aligned big-endian QWORD (64bit) */
#define READ_QWORD_BE_A(addr) (BYTESWAP_64(*((uint64_t*)(addr))))

/* read an aligned little-endian WORD (16bit) */
#define READ_WORD_LE_A( addr) (*(uint16_t*)(addr))

/* read an aligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_A(addr) (*(uint32_t*)(addr))

/* read an aligned little-endian QWORD (64bit) */
#define READ_QWORD_LE_A(addr) (*(uint64_t*)(addr))

/* read an unaligned big-endian WORD (16bit) */
#define READ_WORD_BE_U( addr) ((((uint8_t*)(addr))[0] << 8) | ((uint8_t*)(addr))[1])

/* read an unaligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_U(addr)                                                  \
    ((((uint8_t*)(addr))[0] << 24) | (((uint8_t*)(addr))[1] << 16) |           \
     (((uint8_t*)(addr))[2] <<  8) |  ((uint8_t*)(addr))[3]         )

/* read an unaligned big-endian QWORD (32bit) */
#define READ_QWORD_BE_U(addr)                                                  \
    ((uint64_t(((uint8_t*)(addr))[0]) << 56) | (uint64_t(((uint8_t*)(addr))[1]) << 48) | \
     (uint64_t(((uint8_t*)(addr))[2]) << 40) | (uint64_t(((uint8_t*)(addr))[3]) << 32) | \
     (uint64_t(((uint8_t*)(addr))[4]) << 24) | (         ((uint8_t*)(addr))[5]  << 16) | \
     (         ((uint8_t*)(addr))[6]  <<  8) |           ((uint8_t*)(addr))[7]         )

/* read an unaligned little-endian WORD (16bit) */
#define READ_WORD_LE_U( addr) ((((uint8_t*)(addr))[1] << 8) | ((uint8_t*)(addr))[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_U(addr)                                                  \
    ((((uint8_t*)(addr))[3] << 24) | (((uint8_t*)(addr))[2] << 16) |           \
     (((uint8_t*)(addr))[1] <<  8) |  ((uint8_t*)(addr))[0]         )

/* read an unaligned little-endian DWORD (64bit) */
#define READ_QWORD_LE_U(addr)                                                  \
    ((uint64_t(((uint8_t*)(addr))[7]) << 56) | (uint64_t(((uint8_t*)(addr))[6]) << 48) | \
     (uint64_t(((uint8_t*)(addr))[5]) << 40) | (uint64_t(((uint8_t*)(addr))[4]) << 32) | \
     (uint64_t(((uint8_t*)(addr))[3]) << 24) | (         ((uint8_t*)(addr))[2]  << 16) | \
     (         ((uint8_t*)(addr))[1]  <<  8) |           ((uint8_t*)(addr))[0]          )

/* write an aligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_A( addr, val) (*((uint16_t*)(addr)) = BYTESWAP_16(val))

/* write an aligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_A(addr, val) (*((uint32_t*)(addr)) = BYTESWAP_32(val))

/* write an aligned big-endian QWORD (64bit) */
#define WRITE_QWORD_BE_A(addr, val) (*((uint64_t*)(addr)) = BYTESWAP_64(val))

/* write an unaligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_U(addr, val)                                             \
    do {                                                                       \
        ((uint8_t*)(addr))[0] = ((val) >> 8);                                  \
        ((uint8_t*)(addr))[1] = (uint8_t)(val);                                \
    } while (0)

/* write an unaligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_U(addr, val)                                            \
    do {                                                                       \
        ((uint8_t*)(addr))[0] = ((val) >> 24);                                 \
        ((uint8_t*)(addr))[1] = ((val) >> 16);                                 \
        ((uint8_t*)(addr))[2] = ((val) >>  8);                                 \
        ((uint8_t*)(addr))[3] = (uint8_t)(val);                                \
    } while (0)

/* write an unaligned big-endian DWORD (64bit) */
#define WRITE_QWORD_BE_U(addr, val)                                            \
    do {                                                                       \
        ((uint8_t*)(addr))[0] = ((uint64_t)(val) >> 56);                       \
        ((uint8_t*)(addr))[1] = ((uint64_t)(val) >> 48);                       \
        ((uint8_t*)(addr))[2] = ((uint64_t)(val) >> 40);                       \
        ((uint8_t*)(addr))[3] = ((uint64_t)(val) >> 32);                       \
        ((uint8_t*)(addr))[4] = (          (val) >> 24);                       \
        ((uint8_t*)(addr))[5] = (          (val) >> 16);                       \
        ((uint8_t*)(addr))[6] = (          (val) >>  8);                       \
        ((uint8_t*)(addr))[7] =   (uint8_t)(val)       ;                       \
    } while (0)

/* write an aligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_A( addr, val) (*((uint16_t*)(addr)) = (val))

/* write an aligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_A(addr, val) (*((uint32_t*)(addr)) = (val))

/* write an aligned little-endian QWORD (64bit) */
#define WRITE_QWORD_LE_A(addr, val) (*((uint64_t*)(addr)) = (val))

/* write an unaligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_U(addr, val)                                             \
    do {                                                                       \
        ((uint8_t*)(addr))[0] = (uint8_t)(val);                                \
        ((uint8_t*)(addr))[1] = ((val) >> 8);                                  \
    } while (0)

/* write an unaligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_U(addr, val)                                            \
    do {                                                                       \
        ((uint8_t*)(addr))[0] = (uint8_t)(val);                                \
        ((uint8_t*)(addr))[1] = ((val) >>  8);                                 \
        ((uint8_t*)(addr))[2] = ((val) >> 16);                                 \
        ((uint8_t*)(addr))[3] = ((val) >> 24);                                 \
    } while (0)

/* write an unaligned little-endian DWORD (64bit) */
#define WRITE_QWORD_LE_U(addr, val)                                            \
    do {                                                                       \
        ((uint8_t*)(addr))[0] =   (uint8_t)(val)       ;                       \
        ((uint8_t*)(addr))[1] = (          (val) >>  8);                       \
        ((uint8_t*)(addr))[2] = (          (val) >> 16);                       \
        ((uint8_t*)(addr))[3] = (          (val) >> 24);                       \
        ((uint8_t*)(addr))[4] = ((uint64_t)(val) >> 32);                       \
        ((uint8_t*)(addr))[5] = ((uint64_t)(val) >> 40);                       \
        ((uint8_t*)(addr))[6] = ((uint64_t)(val) >> 48);                       \
        ((uint8_t*)(addr))[7] = ((uint64_t)(val) >> 56);                       \
    } while (0)

/* read value of the specified size from memory starting at addr,
   perform byte swapping when necessary so that the source
   byte order remains unchanged. */
inline uint32_t read_mem(const uint8_t* buf, uint32_t size) {
   switch (size) {
   case 4:
       return READ_DWORD_BE_A(buf);
       break;
   case 2:
       return READ_WORD_BE_A(buf);
       break;
   case 1:
       return *buf;
       break;
   default:
       LOG_F(WARNING, "READ_MEM: invalid size %d!", size);
       return 0;
   }
}

/* read value of the specified size from memory starting at addr,
   perform byte swapping when necessary so that the destination data
   will be in the reversed byte order. */
inline uint32_t read_mem_rev(const uint8_t* buf, uint32_t size) {
  switch (size) {
  case 4:
      return READ_DWORD_LE_A(buf);
      break;
  case 2:
      return READ_WORD_LE_A(buf);
      break;
  case 1:
      return *buf;
      break;
  default:
      LOG_F(WARNING, "READ_MEM_REV: invalid size %d!", size);
      return 0;
  }
}

/* write the specified value of the specified size to memory pointed
   to by addr, perform necessary byte swapping so that the byte order
   of the destination remains unchanged. */
inline void write_mem(uint8_t* buf, uint32_t value, uint32_t size) {
    switch (size) {
    case 4:
        WRITE_DWORD_BE_A(buf, value);
        break;
    case 2:
        WRITE_WORD_BE_A(buf, value & 0xFFFFU);
        break;
    case 1:
        *buf = value & 0xFF;
        break;
    default:
        LOG_F(WARNING, "WRITE_MEM: invalid size %d!", size);
    }
}

/* write the specified value of the specified size to memory pointed
   to by addr, perform necessary byte swapping so that the destination
   data in memory will be in the reversed byte order. */
inline void write_mem_rev(uint8_t* buf, uint32_t value, uint32_t size) {
    switch (size) {
    case 4:
        WRITE_DWORD_LE_A(buf, value);
        break;
    case 2:
        WRITE_WORD_LE_A(buf, value & 0xFFFFU);
        break;
    case 1:
        *buf = value & 0xFF;
        break;
    default:
        LOG_F(WARNING, "WRITE_MEM_REV: invalid size %d!", size);
    }
}

#endif /* MEM_ACCESS_H */
