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

/* PCI related definitions. */
enum {
    ATI_PCI_VENDOR_ID   = 0x1002,
    ATI_RAGE_PRO_DEV_ID = 0x4750,
    ATI_RAGE_GT_DEV_ID  = 0x4754,
};

/** Mach registers offsets. */
enum {
    ATI_CRTC_H_TOTAL_DISP     = 0x0000,
    ATI_CRTC_H_SYNC_STRT_WID  = 0x0004,
    ATI_CRTC_V_TOTAL_DISP     = 0x0008,
    ATI_CRTC_V_SYNC_STRT_WID  = 0x000C,
    ATI_CRTC_VLINE_CRNT_VLINE = 0x0010,
    ATI_CRTC_OFF_PITCH        = 0x0014,
    ATI_CRTC_INT_CNTL         = 0x0018,
    ATI_CRTC_GEN_CNTL         = 0x001C,
    ATI_DSP_CONFIG            = 0x0020,
    ATI_DSP_ON_OFF            = 0x0024,
    ATI_TIMER_CFG             = 0x0028,
    ATI_MEM_BUF_CNTL          = 0x002C,
    ATI_MEM_ADDR_CFG          = 0x0034,
    ATI_CRT_TRAP              = 0x0038,
    ATI_I2C_CNTL_0            = 0x003C,
    ATI_OVR_CLR               = 0x0040,
    ATI_OVR_WID_LEFT_RIGHT    = 0x0044,
    ATI_OVR_WID_TOP_BOTTOM    = 0x0048,
    ATI_VGA_DSP_CFG           = 0x004C,
    ATI_VGA_DSP_TGL           = 0x0050,
    ATI_DSP2_CONFIG           = 0x0054,
    ATI_DSP2_TOGGLE           = 0x0058,
    ATI_CRTC2_OFF_PITCH       = 0x005C,
    ATI_CUR_CLR0              = 0x0060,
    ATI_CUR_CLR1              = 0x0064,
    ATI_CUR_OFFSET            = 0x0068,
    ATI_CUR_HORZ_VERT_POSN    = 0x006C,
    ATI_CUR_HORZ_VERT_OFF     = 0x0070,
    ATI_GP_IO                 = 0x0078,
    ATI_HW_DEBUG              = 0x007C,
    ATI_SCRATCH_REG0          = 0x0080,
    ATI_SCRATCH_REG1          = 0x0084,
    ATI_SCRATCH_REG2          = 0x0088,
    ATI_SCRATCH_REG3          = 0x008C,
    ATI_CLOCK_CNTL            = 0x0090,
    ATI_CONFIG_STAT1          = 0x0094,
    ATI_CONFIG_STAT2          = 0x0098,
    ATI_BUS_CNTL              = 0x00A0,
    ATI_EXT_MEM_CNTL          = 0x00AC,
    ATI_MEM_CNTL              = 0x00B0,
    ATI_MEM_VGA_WP_SEL        = 0x00B4,
    ATI_MEM_VGA_RP_SEL        = 0x00B8,
    ATI_I2C_CNTL_1            = 0x00BC,
    ATI_DAC_REGS              = 0x00C0,
    ATI_DAC_W_INDEX           = 0x00C0,
    ATI_DAC_DATA              = 0x00C1,
    ATI_DAC_MASK              = 0x00C2,
    ATI_DAC_R_INDEX           = 0x00C3,
    ATI_DAC_CNTL              = 0x00C4,
    ATI_GEN_TEST_CNTL         = 0x00D0,
    ATI_CUSTOM_MACRO_CNTL     = 0x00D4,
    ATI_CONFIG_CNTL           = 0x00DC,
    ATI_CONFIG_CHIP_ID        = 0x00E0,
    ATI_CONFIG_STAT0          = 0x00E4,
    ATI_CRC_SIG               = 0x00E8,
    ATI_DST_Y_X_ALIAS1        = 0x00F4,
    ATI_DST_OFF_PITCH         = 0x0100,
    ATI_DST_X                 = 0x0104,
    ATI_DST_Y                 = 0x0108,
    ATI_DST_Y_X               = 0x010C,
    ATI_DST_WIDTH             = 0x0110,
    ATI_DST_HEIGHT            = 0x0114,
    ATI_DST_HEIGHT_WIDTH      = 0x0118,
    ATI_DST_X_WIDTH           = 0x011C,
    ATI_DST_BRES_LNTH         = 0x0120,
    ATI_DST_BRES_ERR          = 0x0124,
    ATI_DST_BRES_INC          = 0x0128,
    ATI_DST_BRES_DEC          = 0x012C,
    ATI_DST_CNTL              = 0x0130,
    ATI_DST_Y_X_ALIAS2        = 0x0134,
    ATI_TRAIL_BRES_ERR        = 0x0138,
    ATI_TRAIL_BRES_INC        = 0x013C,
    ATI_TRAIL_BRES_DEC        = 0x0140,
    ATI_DST_BRES_LNTH_ALIAS1  = 0x0144,
    ATI_Z_OFF_PITCH           = 0x0148,
    ATI_Z_CNTL                = 0x014C,
    ATI_ALPHA_TST_CNTL        = 0x0150,
    ATI_SRC_OFF_PITCH         = 0x0180,
    ATI_SRC_X                 = 0x0184,
    ATI_SRC_Y                 = 0x0188,
    ATI_SRC_Y_X               = 0x018C,
    ATI_SRC_WIDTH1            = 0x0190,
    ATI_SRC_HEIGHT1           = 0x0194,
    ATI_SRC_HEIGHT1_WIDTH1    = 0x0198,
    ATI_SRC_X_START           = 0x019C,
    ATI_SRC_Y_START           = 0x01A0,
    ATI_SRC_Y_X_START         = 0x01A4,
    ATI_SRC_WIDTH2            = 0x01A8,
    ATI_SRC_HEIGHT2           = 0x01AC,
    ATI_SRC_HEIGHT2_WIDTH2    = 0x01B0,
    ATI_SRC_CNTL              = 0x01B4,
    ATI_SCALE_OFF             = 0x01C0,
    ATI_SCALE_WIDTH           = 0x01DC,
    ATI_SCALE_HEIGHT          = 0x01E0,
    ATI_SCALE_PITCH           = 0x01EC,
    ATI_SCALE_X_INC           = 0x01F0,
    ATI_SCALE_Y_INC           = 0x01F4,
    ATI_SCALE_VACC            = 0x01F8,
    ATI_SCALE_3D_CNTL         = 0x01FC,
    ATI_HOST_CNTL             = 0x0240,
    ATI_PAT_REG0              = 0x0280,
    ATI_PAT_REG1              = 0x0284,
    ATI_PAT_CNTL              = 0x0288,
    ATI_SC_LEFT               = 0x02A0,
    ATI_SC_RIGHT              = 0x02A4,
    ATI_SC_LEFT_RIGHT         = 0x02A8,
    ATI_SC_TOP                = 0x02AC,
    ATI_SC_BOTTOM             = 0x02B0,
    ATI_SC_TOP_BOTTOM         = 0x02B4,
    ATI_DP_BKGD_CLR           = 0x02C0,
    ATI_DP_FOG_CLR            = 0x02C0,
    ATI_DP_WRITE_MSK          = 0x02C8,
    ATI_DP_PIX_WIDTH          = 0x02D0,
    ATI_DP_MIX                = 0x02D4,
    ATI_DP_SRC                = 0x02D8,
    ATI_FRGD_CLR_MIX          = 0x02DC,
    ATI_FRGD_BKGD_CLR         = 0x02E0,
    ATI_DST_X_Y               = 0x02E8,
    ATI_DST_WIDTH_HEIGHT      = 0x02EC,
    ATI_USR_DST_PITCH         = 0x02F0,
    ATI_DP_SET_GUI_ENGINE2    = 0x02F4,
    ATI_DP_SET_GUI_ENGINE     = 0x02F8,
    ATI_CLR_CMP_CLR           = 0x0300,
    ATI_CLR_CMP_MSK           = 0x0304,
    ATI_CLR_CMP_CNTL          = 0x0308,
    ATI_FIFO_STAT             = 0x0310,
    ATI_CONTEXT_MASK          = 0x0320,
    ATI_GUI_TRAJ_CNTL         = 0x0330,
    ATI_GUI_STAT              = 0x0338,
    ATI_MPP_CONFIG            = 0x04C0,
    ATI_MPP_STROBE_SEQ        = 0x04C4,
    ATI_MPP_ADDR              = 0x04C8,
    ATI_MPP_DATA              = 0x04CC,
    ATI_TVO_CNTL              = 0x0500,
    ATI_SETUP_CNTL            = 0x0704,
};

/* Mach64 PLL register indices. */
enum {
    PLL_REF_DIV     =  2, // reference divider, same for all Mach64 clocks
    PLL_VCLK_CNTL   =  5,
    VCLK_POST_DIV   =  6,
    VCLK0_FB_DIV    =  7, // feedback divider for VCLK0
    VCLK1_FB_DIV    =  8, // feedback divider for VCLK1
    VCLK2_FB_DIV    =  9, // feedback divider for VCLK2
    VCLK3_FB_DIV    = 10, // feedback divider for VCLK3
    PLL_EXT_CNTL    = 11,
};

constexpr auto APERTURE_SIZE = 0x01000000UL; /* Mach64 aperture size */
constexpr auto MM_REGS_0_OFF = 0x007FFC00UL; /* offset to memory mapped registers, block 0 */
constexpr auto MM_REGS_1_OFF = 0x007FF800UL; /* offset to memory mapped registers, block 1 */
constexpr auto BE_FB_OFFSET  = 0x00800000UL; /* Offset to the big-endian frame buffer */

constexpr auto ATI_XTAL = 14318180.0f; // external crystal oscillator frequency

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
    const char* get_reg_name(uint32_t reg_offset);
    bool io_access_allowed(uint32_t offset, uint32_t* p_io_base);
    uint32_t read_reg(uint32_t offset, uint32_t size);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);
    float calc_pll_freq(int scale, int fb_div);
    void verbose_pixel_format(int crtc_index);
    void crtc_enable();
    void draw_hw_cursor(uint8_t *dst_buf, int dst_pitch);
    void update_screen();

private:
    uint8_t block_io_regs[2048] = {0};

    uint8_t pci_cfg[256] = {0}; /* PCI configuration space */

    uint8_t plls[64] = {0}; // internal PLL registers

    /* CRT controller parameters */
    bool        crtc_on = false;
    int         active_width;   // width of the visible display area
    int         active_height;  // height of the visible display area
    float       pixel_clock;
    float       refresh_rate;

    /* Video RAM variables */
    uint32_t    vram_size;
    uint8_t*    vram_ptr;

    uint32_t    aperture_base;

    DisplayID*  disp_id;

    uint8_t palette[256][4]; /* internal DAC palette in RGBA format */
    int comp_index;          /* color component index for DAC palette access */
    uint8_t *surface;
};
#endif /* ATI_RAGE_H */
