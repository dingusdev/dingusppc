#ifndef MEM_READ_WRITE_H
#define MEM_READ_WRITE_H

/** @file Set of macros for accessing integers of various sizes and endianness
          in host memory.
 */

/* read a unaligned big-endian WORD (16bit) */
#define READ_WORD_BE(addr)  (((addr)[0] << 16) | (addr)[1])

/* read a unaligned big-endian DWORD (32bit) */
#define READ_DWORD_BE(addr) (((addr)[0] << 24) | ((addr)[1] << 16) | \
        ((addr)[2] << 8) | (addr)[3])

/* read a unaligned little-endian DWORD (32bit) */
#define READ_DWORD_LE(addr) (((addr)[3] << 24) | ((addr)[2] << 16) | \
        ((addr)[1] << 8) | (addr)[0])

#endif /* MEM_READ_WRITE_H */
