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

/** MPC106 (Grackle) definitions.

    Grackle IC is a combined memory and PCI controller manufactured by Motorola.
    It's the central device in the Gossamer architecture.
    Manual: https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    This code emulates as much functionality as needed to run Power Mac Beige G3.
    This implies that
    - we only support address map B
    - our virtual device reports revision 4.0 as expected by machine firmware
*/

#ifndef MPC106_H
#define MPC106_H

#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>

#include <cinttypes>
#include <memory>

class InterruptCtrl;

/** Grackle configuration space registers. */
enum GrackleReg : uint32_t {
    CFG10   = 0x40, // bus # + subordinate bus # + disconnect counter
    PMCR1   = 0x70, // power management config 1
    MSAR1   = 0x80, // memory starting address 1
    MSAR2   = 0x84, // memory starting address 2
    EMSAR1  = 0x88, // extended memory starting address 1
    EMSAR2  = 0x8C, // extended memory starting address 2
    MEAR1   = 0x90, // memory ending address 1
    MEAR2   = 0x94, // memory ending address 2
    EMEAR1  = 0x98, // extended memory ending address 1
    EMEAR2  = 0x9C, // extended memory ending address 2
    MBER    = 0xA0, // memory bank enable
    PICR1   = 0xA8, // processor interface configuration 1
    PICR2   = 0xAC, // processor interface configuration 2
    MCCR1   = 0xF0, // memory control configuration 1
    MCCR2   = 0xF4, // memory control configuration 2
    MCCR3   = 0xF8, // memory control configuration 3
    MCCR4   = 0xFC  // memory control configuration 4
};

/* PICR1 bit definitions. */
enum {
    CF_L2_MP_SHIFT          = 0,
    CF_L2_MP_MASK           = 3 << CF_L2_MP_SHIFT,
    SPECULATIVE_PCI_READS   = 1 << 2,
    CF_APARK                = 1 << 3,
    CF_LOOP_SNOOP           = 1 << 4,
    LE_MODE                 = 1 << 5,
    ST_GATH_EN              = 1 << 6,
    NO_PORT_REGS            = 1 << 7,
    CF_EXTERNAL_L2          = 1 << 8,
    CF_DPARK                = 1 << 9,
    TEA_EN                  = 1 << 10, // enabled during ROM flashing
    MCP_EN                  = 1 << 11,
    FLASH_WR_EN             = 1 << 12, // enabled during ROM flashing, disabled after ROM flashing
    CF_LBA_EN               = 1 << 13,
    CF_MP_ID_SHIFT          = 14,
    CF_MP_ID_MASK           = 3 << CF_MP_ID_SHIFT,
    ADDRESS_MAP             = 1 << 16,
    PROC_TYPE_SHIFT         = 17,
    PROC_TYPE_MASK          = 3 << PROC_TYPE_SHIFT,
    XIO_MODE                = 1 << 19,
    RCS0                    = 1 << 20,
    CF_CACHE_1G             = 1 << 21,
    CF_BREAD_WS_SHIFT       = 22,
    CF_BREAD_WS_MASK        = 3 << CF_BREAD_WS_SHIFT,
    CF_CBA_MASK_SHIFT       = 24,
    CF_CBA_MASK_MASK        = 0xff << CF_CBA_MASK_SHIFT,
};

/* PICR2 bit definitions. */
enum {
    CF_WDATA                = 0,
    CF_DOE                  = 1,
    CF_APHASE_WS_SHIFT      = 2,
    CF_APHASE_WS_MASK       = 3 << CF_APHASE_WS_SHIFT,
    CF_L2_SIZE_SHIFT        = 4,
    CF_L2_SIZE_MASK         = 3 << CF_L2_SIZE_SHIFT,
    CF_TOE_WIDTH            = 6,
    CF_FAST_CASTOUT         = 7,
    CF_TWO_BANKS            = 8,
    CF_L2_HIT_DELAY_SHIFT   = 9,
    CF_L2_HIT_DELAY_MASK    = 3 << CF_L2_HIT_DELAY_SHIFT,
    CF_RWITM_FILL           = 11,
    CF_INV_MODE             = 12,
    CF_HOLD                 = 13,
    CF_ADDR_ONLY_DISABLE    = 14,
    // RESERVED             = 15,
    CF_HIT_HIGH             = 16,
    CF_MOD_HIGH             = 17,
    CF_SNOOP_WS_SHIFT       = 18,
    CF_SNOOP_WS_MASK        = 3 << CF_SNOOP_WS_SHIFT,
    CF_WMODE_SHIFT          = 20,
    CF_WMODE_MASK           = 3 << CF_WMODE_SHIFT,
    CF_DATA_RAM_TYPE_SHIFT  = 22,
    CF_DATA_RAM_TYPE_MASK   = 3 << CF_DATA_RAM_TYPE_SHIFT,
    CF_FAST_L2_MODE         = 24,
    FLASH_WR_LOCKOUT        = 25, // Programmer Mode changes this to 0 which allows flash writing.
    CF_FF0_LOCAL            = 26,
    NO_SNOOP_EN             = 27,
    CF_FLUSH_L2             = 28,
    NO_SERIAL_CFG           = 29,
    L2_EN                   = 30,
    L2_UPDATE_EN            = 31,
};

/* MCCR1 bit definitions. */
enum {
    BANK_0_ROW_SHIFT    = 0,
    BANK_0_ROW_MASK     = 3 << BANK_0_ROW_SHIFT,
    BANK_1_ROW_SHIFT    = 2,
    BANK_1_ROW_MASK     = 3 << BANK_1_ROW_SHIFT,
    BANK_2_ROW_SHIFT    = 4,
    BANK_2_ROW_MASK     = 3 << BANK_2_ROW_SHIFT,
    BANK_3_ROW_SHIFT    = 6,
    BANK_3_ROW_MASK     = 3 << BANK_3_ROW_SHIFT,
    BANK_4_ROW_SHIFT    = 8,
    BANK_4_ROW_MASK     = 3 << BANK_4_ROW_SHIFT,
    BANK_5_ROW_SHIFT    = 10,
    BANK_5_ROW_MASK     = 3 << BANK_5_ROW_SHIFT,
    BANK_6_ROW_SHIFT    = 12,
    BANK_6_ROW_MASK     = 3 << BANK_6_ROW_SHIFT,
    BANK_7_ROW_SHIFT    = 14,
    BANK_7_ROW_MASK     = 3 << BANK_7_ROW_SHIFT,
    PCKEN               = 1 << 16,
    RAM_TYPE            = 1 << 17,
    SREN                = 1 << 18,
    MEMGO               = 1 << 19,
    BURST               = 1 << 20,
    _8N64               = 1 << 21, // 1 = the MPC106 is configured for an 8-bit data path for ROM bank 0 rather than 64-bit
    _501_MODE           = 1 << 22,
    ROMFAL_SHIFT        = 23,
    ROMFAL_MASK         = 31 << ROMFAL_SHIFT,
    ROMNAL_SHIFT        = 28,
    ROMNAL_MASK         = 15 << ROMNAL_SHIFT,
};

class MPC106 : public MemCtrlBase, public PCIDevice, public PCIHost {
public:
    MPC106();
    ~MPC106() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<MPC106>(new MPC106());
    }

    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    bool needs_swap_endian(bool is_mmio) override;
#endif

    int device_postinit() override;

protected:
    /* my own PCI configuration registers access */
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details) override;
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details) override;

    void setup_ram(void);

private:
    inline void cfg_setup(uint32_t offset, int size, int &bus_num, int &dev_num,
                          int &fun_num, uint8_t &reg_offs, AccessDetails &details,
                          PCIBase *&device);

#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    inline bool needs_swap_endian_pci() {
        return (picr1 & LE_MODE) != 0;
    }
#endif

    uint32_t config_addr;

    uint16_t pmcr1 = 0;          // power management config 1
    uint8_t  pmcr2 = 0;          // power management config 2
    uint8_t  odcr  = 0xCD;       // output driver control
    uint32_t picr1 = 0xFF100010; // ROM on CPU bus, address map B, CPU type = MPC601
    uint32_t picr2 = 0x000C060C;
    uint32_t mccr1 = 0xFF820000; // 64bit ROM bus
    uint32_t mccr2 = 3;
    uint32_t mccr3 = 0;
    uint32_t mccr4 = 0x00100000;

    uint32_t mem_start[2]       = {};
    uint32_t ext_mem_start[2]   = {};
    uint32_t mem_end[2]         = {};
    uint32_t ext_mem_end[2]     = {};
    uint8_t  mem_bank_en        = 0;
};

#endif // MPC106_H
