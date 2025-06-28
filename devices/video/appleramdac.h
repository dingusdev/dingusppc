/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

//#define CURSOR_LO_DELAY
/* Uncomment this to add a delay before allowing the low byte of the cursor
   position to be latched. In that case, a write to the high byte of the
   cursor position latches the low byte immediately since the high byte is
   expected to always be the second byte written. This change solves the issue
   described below.

   The horizontal position of the cursor may be incorrect between the times
   when the low byte and the high byte are written. This is true when both
   bytes change such as when the position changes from 0xFF to 0x100. This is
   usually not a problem since the position usually changes only during a VBL.

   In the case of Open Firmware, there is at least 2 ms between writes so
   the cursor position could be incorrect for a third of the refresh cycle.
   Open Firmware doesn't use VBLs. It also doesn't usually use a hardware
   cursor so this change isn't usually useful.
*/

/** Definitions for the Apple RAMDAC ASICs (RaDACal & DACula). */

#ifndef APPLE_RAMDAC_H
#define APPLE_RAMDAC_H

#include <devices/common/hwcomponent.h>
#include <devices/ioctrl/macio.h>

#include <cinttypes>
#include <functional>

enum DacFlavour {
    RADACAL,
    DACULA,
};

constexpr auto DACULA_VENDOR_SIERRA = 0x3C;    // 14.3 MHz
constexpr auto DACULA_VENDOR_ATT    = 0x84;    // 15 MHz

constexpr auto VIDEO_XTAL = 14318180.0f; // external crystal oscillator frequency

namespace RamdacRegs {

enum RamdacRegs : uint8_t {
    ADDRESS         = 0, // address register
    CURSOR_CLUT     = 1, // cursor palette data
    MULTI           = 2, // multipurpose section
    CLUT_DATA       = 3, // color palette data

    // multipurpose section registers (both RaDACal & DACula)
    CURSOR_POS_HI   = 0x10, // cursor position, high-order byte
    CURSOR_POS_LO   = 0x11, // cursor position, low-order byte
    MISC_CTRL       = 0x20, // miscellaneus control bits ; Dacula: read ; when 1MB VRAM then write (mode&15) else write (mode)
    DBL_BUF_CTRL    = 0x21, // double buffer control bits ;
                            // - Dacula: write 4;
                            // - Radical: set to 0 if using optional bank (bankb) in Open Firmware or for 2nd page in ndrv
    TEST_CTRL       = 0x22, // enable/disable DAC tests ; Dacula: write 0

    // multipurpose section registers (DACula only)
    PLL_CTRL        = 0x23, // phase locked loop control ; Dacula: if read 2 then write 3 else write 2 <-
    VIDCLK_M_SET_A  = 0x24, // M parameter for the set A ; Dacula: N2 for 2
    VIDCLK_PN_SET_A = 0x25, // P & N parameters for the set A ; Dacula: D2 for 2
    //              = 0x26, // Dacula: write 0xc6 before changing the clock
    VIDCLK_M_SET_B  = 0x27, // M parameter for the set B ; Dacula: N2 for 3 <-
    VIDCLK_PN_SET_B = 0x28, // P & N parameters for the set B ; Dacula: D2 for 3 <-
    DAC_29          = 0x29, // Dacula: linux writes 0xa6 after changing the clock ; macrom writes 0xc6
    VENDOR_ID       = 0x40, // Dacula: DAC_TYPE 0x3c or 0x84
};

} // namespace RamdacRegs

typedef std::function<void(uint8_t index, uint8_t *colors)> GetClutEntryCallback;
typedef std::function<void(uint8_t index, uint8_t *colors)> SetClutEntryCallback;
typedef std::function<void(bool cursor_on)> CursorCtrlCallback;

class AppleRamdac : public HWComponent, public IobusDevice {
public:
    AppleRamdac(DacFlavour flavour);
    ~AppleRamdac() = default;

    uint16_t    iodev_read(uint32_t address);
    void        iodev_write(uint32_t address, uint16_t value);

    int get_clock_div();
    int get_pix_width();
    int get_dot_freq();
    uint8_t get_dbl_buf_cr() { return dbl_buf_cr; }

    void set_fb_parameters(int width, int height, uint32_t pitch) {
        this->video_width  = width;
        this->video_height = height;
        this->fb_pitch     = pitch;
    }

    void measure_hw_cursor(uint8_t *fb_ptr);
    void draw_hw_cursor(uint8_t *src_buf, uint8_t *dst_buf, int dst_pitch);

    GetClutEntryCallback get_clut_entry_cb = nullptr;
    SetClutEntryCallback set_clut_entry_cb = nullptr;
    CursorCtrlCallback   cursor_ctrl_cb    = nullptr;

protected:
    DacFlavour  flavour;
    uint8_t     dac_addr        = 0;
    uint8_t     dac_cr          = 0;
    uint8_t     dbl_buf_cr      = 0;
    uint8_t     tst_cr          = 0;
    uint16_t    cursor_xpos     = 0; // horizontal position of the cursor region
    uint16_t    cursor_ypos     = 0;
    uint16_t    cursor_height   = 0;
    uint8_t     clk_m[2]        = {};
    uint8_t     clk_pn[2]       = {};
    uint8_t     pll_cr          = 0;
    uint8_t     dac_vendor      = DACULA_VENDOR_SIERRA;
    uint8_t     comp_index      = 0;
    uint8_t     clut_color[3]   = {};
    uint32_t    cursor_clut[8]  = {};
    int         video_width     = 0;
    int         video_height    = 0;
    uint32_t    fb_pitch        = 0;
#ifdef CURSOR_LO_DELAY
    uint8_t     cursor_pos_lo   = 0;
    uint32_t    cursor_timer_id = 0;
#endif
};

#endif // APPLE_RAMDAC_H
