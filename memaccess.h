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
#define READ_WORD_BE_A(addr) (BYTESWAP_16(*((uint16_t*)((addr)))))

/* read an aligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_A(addr) (BYTESWAP_32(*((uint32_t*)((addr)))))

/* read an aligned big-endian QWORD (64bit) */
#define READ_QWORD_BE_A(addr) (BYTESWAP_64(*((uint64_t*)((addr)))))

/* read an aligned little-endian WORD (16bit) */
#define READ_WORD_LE_A(addr) (*(uint16_t*)((addr)))

/* read an aligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_A(addr) (*(uint32_t*)((addr)))

/* read an aligned little-endian QWORD (64bit) */
#define READ_QWORD_LE_A(addr) (*(uint64_t*)((addr)))

/* read an unaligned big-endian WORD (16bit) */
#define READ_WORD_BE_U(addr) (((addr)[0] << 8) | (addr)[1])

/* read an unaligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_U(addr)                                                  \
    (((addr)[0] << 24) | ((addr)[1] << 16) | ((addr)[2] << 8) | (addr)[3])

/* read an unaligned big-endian QWORD (32bit) */
#define READ_QWORD_BE_U(addr)                                                  \
    ((uint64_t((addr)[0]) << 56) | (uint64_t((addr)[1]) << 48) |               \
     (uint64_t((addr)[2]) << 40) | (uint64_t((addr)[3]) << 32) |               \
     ((addr)[4] << 24) | ((addr)[5] << 16) | ((addr)[6] << 8)  | (addr)[7])

/* read an unaligned little-endian WORD (16bit) */
#define READ_WORD_LE_U(addr) (((addr)[1] << 8) | (addr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_U(addr)                                                  \
    (((addr)[3] << 24) | ((addr)[2] << 16) | ((addr)[1] << 8) | (addr)[0])

/* read an unaligned little-endian DWORD (64bit) */
#define READ_QWORD_LE_U(addr)                                                  \
    ((uint64_t((addr)[7]) << 56) | (uint64_t((addr)[6]) << 48) |               \
     (uint64_t((addr)[5]) << 40) | (uint64_t((addr)[4]) << 32) |               \
     ((addr)[3] << 24) | ((addr)[2] << 16) | ((addr)[1] << 8) | (addr)[0])

/* write an aligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_A(addr, val) (*((uint16_t*)((addr))) = BYTESWAP_16(val))

/* write an aligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_A(addr, val) (*((uint32_t*)((addr))) = BYTESWAP_32(val))

/* write an aligned big-endian QWORD (64bit) */
#define WRITE_QWORD_BE_A(addr, val) (*((uint64_t*)((addr))) = BYTESWAP_64(val))

/* write an unaligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_U(addr, val)                                             \
    do {                                                                       \
        (addr)[0] = ((val) >> 8) & 0xFF;                                       \
        (addr)[1] = (val) & 0xFF;                                              \
    } while (0)

/* write an unaligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_U(addr, val)                                            \
    do {                                                                       \
        (addr)[0] = ((val) >> 24) & 0xFF;                                      \
        (addr)[1] = ((val) >> 16) & 0xFF;                                      \
        (addr)[2] = ((val) >> 8) & 0xFF;                                       \
        (addr)[3] = (val) & 0xFF;                                              \
    } while (0)

/* write an unaligned big-endian DWORD (64bit) */
#define WRITE_QWORD_BE_U(addr, val)                                            \
    do {                                                                       \
        (addr)[0] = ((uint64_t)(val) >> 56) & 0xFF;                            \
        (addr)[1] = ((uint64_t)(val) >> 48) & 0xFF;                            \
        (addr)[2] = ((uint64_t)(val) >> 40) & 0xFF;                            \
        (addr)[3] = ((uint64_t)(val) >> 32) & 0xFF;                            \
        (addr)[4] = ((val) >> 24) & 0xFF;                                      \
        (addr)[5] = ((val) >> 16) & 0xFF;                                      \
        (addr)[6] = ((val) >> 8) & 0xFF;                                       \
        (addr)[7] = (val) & 0xFF;                                              \
    } while (0)

/* write an aligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_A(addr, val) (*((uint16_t*)((addr))) = (val))

/* write an aligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_A(addr, val) (*((uint32_t*)((addr))) = (val))

/* write an aligned little-endian QWORD (64bit) */
#define WRITE_QWORD_LE_A(addr, val) (*((uint64_t*)((addr))) = (val))

/* write an unaligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_U(addr, val)                                             \
    do {                                                                       \
        (addr)[0] = (val)&0xFF;                                                \
        (addr)[1] = ((val) >> 8) & 0xFF;                                       \
    } while (0)

/* write an unaligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_U(addr, val)                                            \
    do {                                                                       \
        (addr)[0] = (val)&0xFF;                                                \
        (addr)[1] = ((val) >> 8) & 0xFF;                                       \
        (addr)[2] = ((val) >> 16) & 0xFF;                                      \
        (addr)[3] = ((val) >> 24) & 0xFF;                                      \
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

/* value is dword from PCI config. MSB..LSB of value is stored in PCI config as 0:LSB..3:MSB.
   result is part of value at byte offset from LSB and size bytes (with wrap around) and flipped as required for pci_cfg_read result. */
inline uint32_t pci_cfg_rev_read(uint32_t value, uint32_t offset, uint32_t size) {
    switch (size << 2 | offset) {
    case 0x04: return  value        & 0xff; // 0
    case 0x05: return (value >>  8) & 0xff; // 1
    case 0x06: return (value >> 16) & 0xff; // 2
    case 0x07: return (value >> 24) & 0xff; // 3

    case 0x08: return ((value & 0xff) << 8)    | ((value >>  8) & 0xff); // 0 1
    case 0x09: return ( value        & 0xff00) | ((value >> 16) & 0xff); // 1 2
    case 0x0a: return ((value >>  8) & 0xff00) | ((value >> 24) & 0xff); // 2 3
    case 0x0b: return ((value >> 16) & 0xff00) | ( value        & 0xff); // 3 0

    case 0x10: return ((value &       0xff) << 24) | ((value &  0xff00) <<  8) | ((value >>  8) & 0xff00) | ((value >> 24) & 0xff); // 0 1 2 3
    case 0x11: return ((value &     0xff00) << 16) | ( value       & 0xff0000) | ((value >> 16) & 0xff00) | ( value        & 0xff); // 1 2 3 0
    case 0x12: return ((value &   0xff0000) <<  8) | ((value >> 8) & 0xff0000) | ((value & 0xff) << 8)    | ((value >>  8) & 0xff); // 2 3 0 1
    case 0x13: return ( value & 0xff000000)        | ((value &    0xff) << 16) | ( value        & 0xff00) | ((value >> 16) & 0xff); // 3 0 1 2
    default: LOG_F(ERROR, "pci_cfg_rev: invalid offset %d for size %d!", offset, size); return 0xffffffff;
    }
}

/* value is dword from PCI config. MSB..LSB of value (3.2.1.0) is stored in PCI config as 0:LSB..3:MSB.
   data is flipped bytes (d0.d1.d2.d3, as passed to pci_cfg_write) to be merged into value. 
   result is part of value at byte offset from LSB and size bytes (with wrap around) modified by data. */
inline uint32_t pci_cfg_rev_write(uint32_t value, uint32_t offset, uint32_t size, uint32_t data) {
    switch (size << 2 | offset) {
    case 0x04: return (value & 0xffffff00) |  (data & 0xff);        //  3  2  1 d0
    case 0x05: return (value & 0xffff00ff) | ((data & 0xff) <<  8); //  3  2 d0  0
    case 0x06: return (value & 0xff00ffff) | ((data & 0xff) << 16); //  3 d0  1  0
    case 0x07: return (value & 0x00ffffff) | ((data & 0xff) << 24); // d0  2  1  0

    case 0x08: return (value & 0xffff0000) | ((data >> 8) & 0xff)    | ((data & 0xff) <<  8); //  3  2 d1 d0
    case 0x09: return (value & 0xff0000ff) |  (data & 0xff00)        | ((data & 0xff) << 16); //  3 d1 d0  0
    case 0x0a: return (value & 0x0000ffff) | ((data & 0xff00) <<  8) | ((data & 0xff) << 24); // d1 d0  1  0
    case 0x0b: return (value & 0x00ffff00) | ((data & 0xff00) << 16) |  (data & 0xff);        // d0  2  1 d1

    case 0x10: return ((data &       0xff) << 24) | ((data &   0xff00) <<  8) | ((data >>  8) & 0xff00) | ((data >> 24) & 0xff); // d3 d2 d1 d0
    case 0x11: return ((data &     0xff00) << 16) | ( data        & 0xff0000) | ((data >> 16) & 0xff00) | ( data        & 0xff); // d2 d1 d0 d3
    case 0x12: return ((data &   0xff0000) <<  8) | ((data >> 8)  & 0xff0000) | ((data & 0xff) << 8)    | ((data >>  8) & 0xff); // d1 d0 d3 d2
    case 0x13: return ( data & 0xff000000)        | ((data &     0xff) << 16) | ( data        & 0xff00) | ((data >> 16) & 0xff); // d0 d3 d2 d1
    default: LOG_F(ERROR, "pci_cfg_rev: invalid offset %d for size %d!", offset, size); return 0xffffffff;
    }
}

inline uint32_t flip_sized(uint32_t value, uint32_t size) {
    switch (size) {
    case 1: return value;
    case 2: return BYTESWAP_16(value);
    case 4: return BYTESWAP_32(value);
    default: LOG_F(ERROR, "flip_sized: invalid size %d!", size); return 0xffffffff;
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
