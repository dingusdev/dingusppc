/** @file Set of macros for accessing integers of various sizes and endianness
          in host memory.
 */

#ifndef MEM_READ_WRITE_H
#define MEM_READ_WRITE_H

#include <cinttypes>
#include "endianswap.h"

/* read an aligned big-endian WORD (16bit) */
#define READ_WORD_BE_A(addr)  (BYTESWAP_16(*((uint16_t *)((addr)))))

/* read an aligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_A(addr) (BYTESWAP_32(*((uint32_t *)((addr)))))

/* read an aligned big-endian QWORD (64bit) */
#define READ_QWORD_BE_A(addr) (BYTESWAP_64(*((uint64_t *)((addr)))))

/* read an aligned little-endian WORD (16bit) */
#define READ_WORD_LE_A(addr)  (*(uint16_t *)((addr)))

/* read an aligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_A(addr) (*(uint32_t *)((addr)))

/* read an aligned little-endian QWORD (64bit) */
#define READ_QWORD_LE_A(addr) (*(uint64_t *)((addr)))

/* read an unaligned big-endian WORD (16bit) */
#define READ_WORD_BE_U(addr)  (((addr)[0] << 8) | (addr)[1])

/* read an unaligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_U(addr) (((addr)[0] << 24) | ((addr)[1] << 16) | \
                              ((addr)[2] << 8)  |  (addr)[3])

/* read an unaligned big-endian QWORD (32bit) */
#define READ_QWORD_BE_U(addr) (((addr)[0] << 56) | ((addr)[1] << 48) | \
                              ((addr)[2] << 40) | ((addr)[3] << 32) | \
                              ((addr)[4] << 24) | ((addr)[5] << 16) | \
                              ((addr)[6] << 8)  |  (addr)[7])

/* read an unaligned little-endian WORD (16bit) */
#define READ_WORD_LE_U(addr)  (((addr)[1] << 8) | (ptr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_U(addr) (((addr)[3] << 24) | ((addr)[2] << 16) | \
                              ((addr)[1] << 8)  |  (addr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_QWORD_LE_U(addr) (((addr)[7] << 56) | ((addr)[6] << 48) | \
                              ((addr)[5] << 40) | ((addr)[4] << 32) | \
                              ((addr)[3] << 24) | ((addr)[2] << 16) | \
                              ((addr)[1] << 8)  |  (addr)[0])


/* write an aligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_A(addr,val) (*((uint16_t *)((addr))) = BYTESWAP_16(val))

/* write an aligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_A(addr,val) (*((uint32_t *)((addr))) = BYTESWAP_32(val))

/* write an aligned big-endian QWORD (64bit) */
#define WRITE_QWORD_BE_A(addr,val) (*((uint64_t *)((addr))) = BYTESWAP_64(val))

/* write an unaligned big-endian WORD (16bit) */
#define WRITE_WORD_BE_U(addr, val)      \
do {                                    \
    (addr)[0] = ((val) >> 8) & 0xFF;    \
    (addr)[1] = (val) & 0xFF;           \
} while(0)

/* write an unaligned big-endian DWORD (32bit) */
#define WRITE_DWORD_BE_U(addr, val)     \
do {                                    \
    (addr)[0] = ((val) >> 24) & 0xFF;   \
    (addr)[1] = ((val) >> 16) & 0xFF;   \
    (addr)[2] = ((val) >> 8)  & 0xFF;   \
    (addr)[3] = (val) & 0xFF;           \
} while(0)

#endif /* MEM_READ_WRITE_H */
