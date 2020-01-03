//DingusPPC - Prototype 5bf2
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

// The opcodes for the processor - ppcopcodes.cpp

#ifndef PPCMEMORY_H
#define PPCMEMORY_H

#include <vector>
#include <array>

/** generic PowerPC BAT descriptor (MMU internal state) */
typedef struct PPC_BAT_entry {
    uint8_t     access;   /* copy of Vs | Vp bits */
    uint8_t     prot;     /* copy of PP bits */
    uint32_t    phys_hi;  /* high-order bits for physical address generation */
    uint32_t    lo_mask;  /* mask for low-order logical address bits */
    uint32_t    bepi;     /* copy of Block effective page index */
} PPC_BAT_entry;

extern uint32_t bat_srch;
extern uint32_t bepi_chk;

extern unsigned char * grab_macmem_ptr;

extern void ibat_update(uint32_t bat_reg);
extern void dbat_update(uint32_t bat_reg);

extern void address_quickinsert_translate(uint32_t value_insert, uint32_t address_grab, uint8_t num_bytes);
extern void address_quickgrab_translate(uint32_t address_grab, uint8_t num_bytes);
extern uint8_t *quickinstruction_translate(uint32_t address_grab);

#endif // PPCMEMORY_H
