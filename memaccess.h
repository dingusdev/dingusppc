/** @file Set of macros for accessing host memory units of various sizes
          and endianness.
 */

#ifndef MEM_ACCESS_H
#define MEM_ACCESS_H

#include "endianswap.h"
#include <cinttypes>
#include <thirdparty/loguru/loguru.hpp>

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
#define READ_DWORD_BE_U(addr) (((addr)[0] << 24) | ((addr)[1] << 16) | ((addr)[2] << 8) | (addr)[3])

/* read an unaligned big-endian QWORD (32bit) */
#define READ_QWORD_BE_U(addr)                                                  \
    ((uint64_t((addr)[0]) << 56) | (uint64_t((addr)[1]) << 48) |               \
     (uint64_t((addr)[2]) << 40) | (uint64_t((addr)[3]) << 32) |               \
     ((addr)[4] << 24) | ((addr)[5] << 16) | ((addr)[6] << 8)  | (addr)[7])

/* read an unaligned little-endian WORD (16bit) */
#define READ_WORD_LE_U(addr) (((addr)[1] << 8) | (addr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_U(addr) (((addr)[3] << 24) | ((addr)[2] << 16) | ((addr)[1] << 8) | (addr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_QWORD_LE_U(addr)                                                                      \
    (((addr)[7] << 56) | ((addr)[6] << 48) | ((addr)[5] << 40) | ((addr)[4] << 32) |               \
     ((addr)[3] << 24) | ((addr)[2] << 16) | ((addr)[1] << 8) | (addr)[0])


/* write an aligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_A(addr, val) (*((uint16_t*)((addr))) = BYTESWAP_16(val))

/* write an aligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_A(addr, val) (*((uint32_t*)((addr))) = BYTESWAP_32(val))

/* write an aligned big-endian QWORD (64bit) */
#define WRITE_QWORD_BE_A(addr, val) (*((uint64_t*)((addr))) = BYTESWAP_64(val))

/* write an unaligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_U(addr, val)                                                                 \
    do {                                                                                           \
        (addr)[0] = ((val) >> 8) & 0xFF;                                                           \
        (addr)[1] = (val)&0xFF;                                                                    \
    } while (0)

/* write an unaligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_U(addr, val)                                                                \
    do {                                                                                           \
        (addr)[0] = ((val) >> 24) & 0xFF;                                                          \
        (addr)[1] = ((val) >> 16) & 0xFF;                                                          \
        (addr)[2] = ((val) >> 8) & 0xFF;                                                           \
        (addr)[3] = (val)&0xFF;                                                                    \
    } while (0)

/* write an aligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_A(addr, val) (*((uint16_t*)((addr))) = (val))

/* write an aligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_A(addr, val) (*((uint32_t*)((addr))) = (val))

/* write an aligned little-endian QWORD (64bit) */
#define WRITE_QWORD_LE_A(addr, val) (*((uint64_t*)((addr))) = (val))

/* write an unaligned little-endian WORD (16bit) */
#define WRITE_WORD_LE_U(addr, val)                                                                 \
    do {                                                                                           \
        (addr)[0] = (val)&0xFF;                                                                    \
        (addr)[1] = ((val) >> 8) & 0xFF;                                                           \
    } while (0)

/* write an unaligned little-endian DWORD (32bit) */
#define WRITE_DWORD_LE_U(addr, val)                                                                \
    do {                                                                                           \
        (addr)[0] = (val)&0xFF;                                                                    \
        (addr)[1] = ((val) >> 8) & 0xFF;                                                           \
        (addr)[2] = ((val) >> 16) & 0xFF;                                                          \
        (addr)[3] = ((val) >> 24) & 0xFF;                                                          \
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
