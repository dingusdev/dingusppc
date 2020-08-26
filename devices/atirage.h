/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#ifndef ATI_RAGE_H
#define ATI_RAGE_H

#include "displayid.h"
#include "pcidevice.h"
#include <cinttypes>

using namespace std;

/* PCI related definitions. */
enum {
    ATI_PCI_VENDOR_ID   = 0x1002,
    ATI_RAGE_PRO_DEV_ID = 0x4750,
    ATI_RAGE_GT_DEV_ID  = 0x4754,
};

/** Mach registers offsets. */
enum {
    ATI_CRTC_H_TOTAL_DISP    = 0x0000,
    ATI_CRTC_H_SYNC_STRT_WID = 0x0004,
    ATI_CRTC_V_TOTAL_DISP    = 0x0008,
    ATI_CRTC_V_SYNC_STRT_WID = 0x000C,
    ATI_CRTC_OFF_PITCH       = 0x0014,
    ATI_CRTC_INT_CNTL        = 0x0018,
    ATI_CRTC_GEN_CNTL        = 0x001C,
    ATI_DSP_CONFIG           = 0x0020,
    ATI_DSP_ON_OFF           = 0x0024,
    ATI_TIMER_CFG            = 0x0028,
    ATI_MEM_BUF_CNTL         = 0x002C,
    ATI_MEM_ADDR_CFG         = 0x0034,
    ATI_CRT_TRAP             = 0x0038,
    ATI_I2C_CNTL_0           = 0x003C,
    ATI_OVR_CLR              = 0x0040,
    ATI_OVR_WID_LEFT_RIGHT   = 0x0044,
    ATI_OVR_WID_TOP_BOTTOM   = 0x0048,
    ATI_VGA_DSP_CFG          = 0x004C,
    ATI_VGA_DSP_TGL          = 0x0050,
    ATI_DSP2_CONFIG          = 0x0054,
    ATI_DSP2_TOGGLE          = 0x0058,
    ATI_CRTC2_OFF_PITCH      = 0x005C,
    ATI_CUR_CLR0             = 0x0060,
    ATI_CUR_CLR1             = 0x0064,
    ATI_CUR_OFFSET           = 0x0068,
    ATI_CUR_HORZ_VERT_POSN   = 0x006C,
    ATI_CUR_HORZ_VERT_OFF    = 0x0070,
    ATI_GP_IO                = 0x0078,
    ATI_HW_DEBUG             = 0x007C,
    ATI_SCRATCH_REG0         = 0x0080,
    ATI_SCRATCH_REG1         = 0x0084,
    ATI_SCRATCH_REG2         = 0x0088,
    ATI_SCRATCH_REG3         = 0x008C,
    ATI_CLOCK_CNTL           = 0x0090,
    ATI_CLONFIG_STAT1        = 0x0094,
    ATI_CLONFIG_STAT2        = 0x0098,
    ATI_BUS_CNTL             = 0x00A0,
    ATI_EXT_MEM_CNTL         = 0x00AC,
    ATI_MEM_CNTL             = 0x00B0,
    ATI_VGA_WP_SEL           = 0x00B4,
    ATI_VGA_RP_SEL           = 0x00B8,
    ATI_I2C_CNTL_1           = 0x00BC,
    ATI_DAC_REGS             = 0x00C0,
    ATI_DAC_W_INDEX          = 0x00C0,
    ATI_DAC_DATA             = 0x00C1,
    ATI_DAC_MASK             = 0x00C2,
    ATI_DAC_R_INDEX          = 0x00C3,
    ATI_DAC_CNTL             = 0x00C4,
    ATI_GEN_TEST_CNTL        = 0x00D0,
    ATI_CUSTOM_MACRO_CNTL    = 0x00D4,
    ATI_CONFIG_CNTL          = 0x00DC,
    ATI_CFG_CHIP_ID          = 0x00E0,
    ATI_CFG_STAT0            = 0x00E4,
    ATI_CRC_SIG              = 0x00E8,
    ATI_DST_OFF_PITCH        = 0x0100,
    ATI_SRC_OFF_PITCH        = 0x0180,
    ATI_HOST_CNTL            = 0x0240,
    ATI_DP_WRITE_MSK         = 0x02C8,
    ATI_DP_PIX_WIDTH         = 0x02D0,
    ATI_DST_X_Y              = 0x02E8,
    ATI_DST_WIDTH_HEIGHT     = 0x02EC,
    ATI_CONTEXT_MASK         = 0x0320,
    ATI_MPP_CONFIG           = 0x04C0,
    ATI_MPP_STROBE_SEQ       = 0x04C4,
    ATI_MPP_ADDR             = 0x04C8,
    ATI_MPP_DATA             = 0x04CC,
    ATI_TVO_CNTL             = 0x0500,
};

constexpr auto APERTURE_SIZE = 0x01000000UL; /* Mach64 aperture size */
constexpr auto MEMMAP_OFFSET = 0x007FFC00UL; /* offset to memory mapped registers */

class ATIRage : public PCIDevice {
public:
    ATIRage(uint16_t dev_id, uint32_t mem_amount);
    ~ATIRage();

    /* MMIODevice methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

    bool supports_type(HWCompType type) {
        return type == HWCompType::MMIO_DEV;
    };

    /* PCI device methods */
    bool supports_io_space(void) {
        return true;
    };

    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    /* I/O space access methods */
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

protected:
    uint32_t size_dep_read(uint8_t* buf, uint32_t size);
    void size_dep_write(uint8_t* buf, uint32_t val, uint32_t size);
    const char* get_reg_name(uint32_t reg_offset);
    bool io_access_allowed(uint32_t offset, uint32_t* p_io_base);
    uint32_t read_reg(uint32_t offset, uint32_t size);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);

private:
    // uint32_t atirage_membuf_regs[9];    /* ATI Rage Memory Buffer Registers */
    // uint32_t atirage_scratch_regs[4];   /* ATI Rage Scratch Registers */
    // uint32_t atirage_cmdfifo_regs[3];   /* ATI Rage Command FIFO Registers */
    // uint32_t atirage_datapath_regs[12]; /* ATI Rage Data Path Registers*/

    uint8_t block_io_regs[256] = {0};

    uint8_t pci_cfg[256] = {0}; /* PCI configuration space */

    /* Video RAM variables */
    uint32_t    vram_size;
    uint8_t*    vram_ptr;

    uint32_t    aperture_base;

    DisplayID*  disp_id;

    uint8_t palette[256][4]; /* internal DAC palette in RGBA format */
    int comp_index;          /* color component index for DAC palette access */
};
#endif /* ATI_RAGE_H */
