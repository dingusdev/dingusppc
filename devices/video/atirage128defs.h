/*
DingusPPC - The Experimental PowerPC Macintosh emulator

Copyright (C) 2018-24 divingkatae and maximum
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

#ifndef ATI_RAGE128_DEFS_H
#define ATI_RAGE128_DEFS_H

/* ATI PCI device IDs. */
enum {
    ATI_RAGE_128_PRO_DEV_ID = 0x5046,
    ATI_RAGE_128_GL_DEV_ID  = 0x5245,
    ATI_RAGE_128_VR_DEV_ID  = 0x524B,
};

/** Rage 128 register offsets. */
enum {
    // MMR registers
    ATI_MM_INDEX                 = 0x000,    // 0x0000
    ATI_MM_DATA                  = 0x001,    // 0x0004
    ATI_CLOCK_CNTL_INDEX         = 0x002,    // 0x0008
    ATI_CLOCK_CNTL_DATA          = 0x003,    // 0x000C
    ATI_BIOS_0_SCRATCH           = 0x004,    // 0x0010
    ATI_BIOS_1_SCRATCH           = 0x005,    // 0x0014
    ATI_BIOS_2_SCRATCH           = 0x006,    // 0x0018
    ATI_BIOS_3_SCRATCH           = 0x007,    // 0x001C
    ATI_BUS_CNTL                 = 0x00C,    // 0x0030
    ATI_BUS_CNTL1                = 0x00D,    // 0x0034
    ATI_GEN_INT_CNTL             = 0x010,    // 0x0040
    ATI_GEN_INT_STATUS           = 0x011,    // 0x0044
    ATI_CRTC_GEN_CNTL            = 0x014,    // 0x0050
    ATI_CRTC_EXT_CNTL            = 0x015,    // 0x0054
    ATI_DAC_CNTL                 = 0x016,    // 0x0058
    ATI_CRTC_STATUS              = 0x017,    // 0x005C
    ATI_GPIO_MONID               = 0x01A,    // 0x0068
    ATI_SEPROM_CNTL              = 0x01B,    // 0x006C
    ATI_I2C_CNTL_1               = 0x025,    // 0x0094
    ATI_PALETTE_INDEX            = 0x02C,    // 0x00B0
    ATI_PALETTE_DATA             = 0x02D,    // 0x00B4
    ATI_CONFIG_CNTL              = 0x038,    // 0x00E0
    ATI_CONFIG_XSTRAP            = 0x039,    // 0x00E4
    ATI_CONFIG_BONDS             = 0x03A,    // 0x00E8
    ATI_GEN_RESET_CNTL           = 0x03C,    // 0x00F0
    ATI_GEN_STATUS               = 0x03D,    // 0x00F4
    ATI_CNFG_MEMSIZE             = 0x03E,    // 0x00F8
    ATI_CONFIG_APER_0_BASE       = 0x040,    // 0x0100
    ATI_CONFIG_APER_1_BASE       = 0x041,    // 0x0104
    ATI_CONFIG_APER_SIZE         = 0x042,    // 0x0108
    ATI_CONFIG_REG_1_BASE        = 0x043,    // 0x010C
    ATI_CONFIG_REG_APER_SIZE     = 0x044,    // 0x0110
    ATI_TEST_DEBUG_CNTL          = 0x048,    // 0x0120
    ATI_TEST_DEBUG_MUX           = 0x049,    // 0x0124
    ATI_HW_DEBUG                 = 0x04A,    // 0x0128
    ATI_HOST_PATH_CNTL           = 0x04C,    // 0x0130
    ATI_MEM_CNTL                 = 0x050,    // 0x0140
    ATI_EXT_MEM_CNTL             = 0x051,    // 0x0144
    ATI_MEM_ADDR_CONFIG          = 0x052,    // 0x0148
    ATI_MEM_INTF_CNTL            = 0x053,    // 0x014C
    ATI_MEM_STR_CNTL             = 0x054,    // 0x0150
    ATI_VIPH_CONTROL             = 0x074,    // 0x01D0
    ATI_CRTC_H_TOTAL_DISP        = 0x080,    // 0x0200
    ATI_CRTC_H_SYNC_STRT_WID     = 0x081,    // 0x0204
    ATI_CRTC_V_TOTAL_DISP        = 0x082,    // 0x0208
    ATI_CRTC_V_SYNC_STRT_WID     = 0x083,    // 0x020C
    ATI_CRTC_VLINE_CRNT_VLINE    = 0x084,    // 0x0210
    ATI_CRTC_CRNT_FRAME          = 0x085,    // 0x0214
    ATI_CRTC_GUI_TRIG_VLINE      = 0x086,    // 0x0218
    ATI_CRTC_DEBUG               = 0x087,    // 0x021C
    ATI_CRTC_OFFSET              = 0x089,    // 0x0224
    ATI_CRTC_OFFSET_CNTL         = 0x08A,    // 0x0228
    ATI_CRTC_PITCH               = 0x08B,    // 0x022C
    ATI_OVR_CLR                  = 0x08C,    // 0x0230
    ATI_OVR_WID_LEFT_RIGHT       = 0x08D,    // 0x0234
    ATI_OVR_WID_TOP_BOTTOM       = 0x08E,    // 0x0238
    ATI_SNAPSHOT_VH_COUNTS       = 0x090,    // 0x0240
    ATI_SNAPSHOT_F_COUNT         = 0x091,    // 0x0244
    ATI_N_VIF_COUNT              = 0x092,    // 0x0248
    ATI_SNAPSHOT_VIF_COUNT       = 0x093,    // 0x024C
    ATI_CUR_OFFSET               = 0x098,    // 0x0260
    ATI_CUR_HORZ_VERT_POSN       = 0x099,    // 0x0264
    ATI_CUR_HORZ_VERT_OFF        = 0x09A,    // 0x0268
    ATI_CUR_CLR0                 = 0x09B,    // 0x026C
    ATI_CUR_CLR1                 = 0x09C,    // 0x0270
    ATI_DAC_EXT_CNTL             = 0x0A0,    // 0x0280
    ATI_DAC_CRC_SIG              = 0x0B3,    // 0x02CC
    ATI_PM4_BUFFER_DL_WPTR_DELAY = 0x1C6,    // 0x0718
    ATI_DST_OFFSET               = 0x501,    // 0x1404
    ATI_DST_PITCH                = 0x502,    // 0x1408
    ATI_DST_WIDTH                = 0x503,    // 0x140C
    ATI_DST_HEIGHT               = 0x504,    // 0x1410
    ATI_SRC_X                    = 0x505,    // 0x1414
    ATI_SRC_Y                    = 0x506,    // 0x1418
    ATI_DST_X                    = 0x507,    // 0x141C
    ATI_DST_Y                    = 0x508,    // 0x1420
    ATI_SRC_PITCH_OFFSET         = 0x50A,    // 0x1428
    ATI_DST_PITCH_OFFSET         = 0x50B,    // 0x142C
    ATI_SRC_Y_X                  = 0x50D,    // 0x1434
    ATI_DST_Y_X                  = 0x50E,    // 0x1438
    ATI_DST_HEIGHT_WIDTH         = 0x50F,    // 0x143C
    ATI_DST_WIDTH_X              = 0x562,    // 0x1588
    ATI_DST_HEIGHT_WIDTH_8       = 0x563,    // 0x158C
    ATI_SRC_X_Y                  = 0x564,    // 0x1590
    ATI_DST_X_Y                  = 0x565,    // 0x1594
    ATI_DST_WIDTH_HEIGHT         = 0x566,    // 0x1598
    ATI_DST_WIDTH_X_INCY         = 0x567,    // 0x159C
    ATI_DST_HEIGHT_Y             = 0x568,    // 0x15A0
    ATI_DST_X_SUB                = 0x569,    // 0x15A4
    ATI_DST_Y_SUB                = 0x56A,    // 0x15A8
    ATI_SRC_OFFSET               = 0x56B,    // 0x15AC
    ATI_SRC_PITCH                = 0x56C,    // 0x15B0
    ATI_DST_WIDTH_BW             = 0x56D,    // 0x15B4
    ATI_GUI_SCRATCH_REG0         = 0x578,    // 0x15E0
    ATI_GUI_SCRATCH_REG1         = 0x579,    // 0x15E4
    ATI_GUI_SCRATCH_REG2         = 0x57A,    // 0x15E8
    ATI_GUI_SCRATCH_REG3         = 0x57B,    // 0x15EC
    ATI_GUI_SCRATCH_REG4         = 0x57C,    // 0x15F0
    ATI_GUI_SCRATCH_REG5         = 0x57D,    // 0x15F4
    ATI_DST_BRES_ERR             = 0x58A,    // 0x1628
    ATI_DST_BRES_INC             = 0x58B,    // 0x162C
    ATI_DST_BRES_DEC             = 0x58C,    // 0x1630
    ATI_DST_BRES_LNTH            = 0x58D,    // 0x1634
    ATI_DST_BRES_LNTH_SUB        = 0x58E,    // 0x1638
    ATI_SC_LEFT                  = 0x590,    // 0x1640
    ATI_SC_RIGHT                 = 0x591,    // 0x1644
    ATI_SC_TOP                   = 0x592,    // 0x1648
    ATI_SC_BOTTOM                = 0x593,    // 0x164C
    ATI_SC_CNTL                  = 0x598,    // 0x1660
    ATI_DEFAULT_OFFSET           = 0x5B8,    // 0x16E0
    ATI_DEFAULT_PITCH            = 0x5B9,    // 0x16E4
    ATI_DEFAULT_SC_BOTTOM_RIGHT  = 0x5BA,    // 0x16E8
    ATI_SC_TOP_LEFT              = 0x5BB,    // 0x16EC
    ATI_SC_BOTTOM_RIGHT          = 0x5BC,    // 0x16F0
    ATI_SRC_SC_BOTTOM_RIGHT      = 0x5BD,    // 0x16F4
    ATI_WAIT_UNTIL               = 0x5C8,    // 0x1720
    ATI_GUI_STAT                 = 0x5D0,    // 0x1740
    ATI_HOST_DATA0               = 0x5F0,    // 0x17C0
    ATI_HOST_DATA1               = 0x5F1,    // 0x17C4
    ATI_HOST_DATA2               = 0x5F2,    // 0x17C8
    ATI_HOST_DATA3               = 0x5F3,    // 0x17CC
    ATI_HOST_DATA4               = 0x5F4,    // 0x17D0
    ATI_HOST_DATA5               = 0x5F5,    // 0x17D4
    ATI_HOST_DATA6               = 0x5F6,    // 0x17D8
    ATI_HOST_DATA7               = 0x5F7,    // 0x17DC
    ATI_HOST_DATA_LAST           = 0x5F8,    // 0x17E0
    ATI_W_START                  = 0x633,    // 0x18CC
    ATI_CONSTANT_COLOR           = 0x68C,    // 0x1A30
    ATI_Z_VIS                    = 0x6B4,    // 0x1AD4
    ATI_WINDOW_XY_OFFSET         = 0x6F3,    // 0x1BCC
    ATI_DRAW_LINE_POINT          = 0x6F4,    // 0x1BD0
    ATI_DST_PITCH_OFFSET_C       = 0x720,    // 0x1C80
    ATI_SC_TOP_LEFT_C            = 0x722,    // 0x1C88

    // CFG registers

    ATI_VENDOR_ID       = 0x000,
    ATI_DEVICE_ID       = 0x002,
    ATI_COMMAND         = 0x004,
    ATI_STATUS          = 0x006,
    ATI_REVISION_ID     = 0x008,
    ATI_REGPROG_INF     = 0x009,
    ATI_SUB_CLASS       = 0x00A,
    ATI_BASE_CODE       = 0x00B,
    ATI_CACHE_LINE      = 0x00C,
    ATI_LATENCY         = 0x00D,
    ATI_HEADER          = 0x00E,
    ATI_BIST            = 0x00F,
    ATI_MEM_BASE        = 0x010,
    ATI_IO_BASE         = 0x014,
    ATI_REG_BASE        = 0x018,
    ATI_ADAPTER_ID      = 0x02C,
    ATI_BIOS_ROM        = 0x030,
    ATI_CAPABILITES_PTR = 0x034,
    ATI_INTERRUPT_LINE  = 0x03C,
    ATI_INTERRUPT_PIN   = 0x03D,
    ATI_MIN_GRANT       = 0x03E,
    ATI_MAX_LATENCY     = 0x03F,
    ATI_ADAPTER_ID_W    = 0x04C,
    ATI_CAPABILITIES_ID = 0x050,

    // byte offsets for the built-in DAC registers
    ATI_DAC_W_INDEX = 0x00C0,
    ATI_DAC_DATA    = 0x00C1,
    ATI_DAC_MASK    = 0x00C2,
    ATI_DAC_R_INDEX = 0x00C3,

    ATI_INVALID = 0xFFFF
};

constexpr auto APERTURE_SIZE = 0x02000000UL;    // Rage 128 aperture size
constexpr auto MM_REGS_0_OFF = 0x007FFC00UL;    // offset to memory mapped registers, block 0
constexpr auto MM_REGS_1_OFF = 0x007FF800UL;    // offset to memory mapped registers, block 1
constexpr auto MM_REGS_2_OFF = 0x003FFC00UL;    // offset to memory mapped registers, 4MB aperture
constexpr auto BE_FB_OFFSET  = 0x01000000UL;    // Offset to the big-endian frame buffer

constexpr auto ATI_XTAL = 14318180.0f;    // external crystal oscillator frequency

#endif    // ATI_RAGE128_DEFS_H