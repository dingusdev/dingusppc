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

/* read an aligned little-endian WORD (16bit) */
#define READ_WORD_LE_A(addr)  (*(uint16_t *)((addr)))

/* read an aligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_A(addr) (*(uint32_t *)((addr)))


/* read an unaligned big-endian WORD (16bit) */
#define READ_WORD_BE_U(addr)  (((addr)[0] << 8) | (addr)[1])

/* read an unaligned big-endian DWORD (32bit) */
#define READ_DWORD_BE_U(addr) (((addr)[0] << 24) | ((addr)[1] << 16) | \
                              ((addr)[2] << 8)  |  (addr)[3])

/* read an unaligned little-endian WORD (16bit) */
#define READ_WORD_LE_U(addr)  (((addr)[1] << 8) | (ptr)[0])

/* read an unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE_U(addr) (((addr)[3] << 24) | ((addr)[2] << 16) | \
                              ((addr)[1] << 8)  |  (addr)[0])

#endif /* MEM_READ_WRITE_H */
