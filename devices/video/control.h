/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** @file TNT on-board video output definitions. */

#ifndef CONTROL_VIDEO_H
#define CONTROL_VIDEO_H

#include <devices/common/i2c/athens.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/ioctrl/macio.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

#define FB_BLANK_VSYNC  (3 <<  0) // FB_BLANK_VSYNC_SUSPEND = &=  ~0x03
#define FB_BLANK_HSYNC  (3 <<  4) // FB_BLANK_HSYNC_SUSPEND = &=  ~0x30
                                  // FB_BLANK_POWERDOWN     = &=  ~0x33
#define FB_BLANK_NORMAL (1 << 10) //                        = |=  0x400
#define TEST_STROBE     (1 <<  3) // strobe bit
#define FB_DISABLE      (1 << 10) // disable bit            = &= ~0x400

// Memory-mapped registers.
namespace ControlRegs {

enum ControlRegs : int {
    // 512 bytes, repeats 8 times for 4K total.
    // A register every 16 bytes, little endian, 32 bits, repeats 4 times.
    CUR_LINE        = 0x00, // (ro)         ; current active video line
    VFPEQ           = 0x01, // (rw) 12 bits ;
    VFP             = 0x02, // (rw) 12 bits ;
    VAL             = 0x03, // (rw) 12 bits ; vertical active line
    VBP             = 0x04, // (rw) 12 bits ;
    VBPEQ           = 0x05, // (rw) 12 bits ; vertical back porch with equalization
    VSYNC           = 0x06, // (rw) 12 bits ;
    VHLINE          = 0x07, // (rw) 12 bits ;
    PIPED           = 0x08, // (rw) 10 bits ;
    HPIX            = 0x09, // (rw) 12 bits ;
    HFP             = 0x0A, // (rw) 12 bits ;
    HAL             = 0x0B, // (rw) 12 bits ; horizontal active line
    HBWAY           = 0x0C, // (rw) 12 bits ;
    HSP             = 0x0D, // (rw) 12 bits ;
    HEQ             = 0x0E, // (rw)  8 bits ;
    HLFLN           = 0x0F, // (rw) 12 bits ;
    HSERR           = 0x10, // (rw) 12 bits ;
    CNTTST          = 0x11, // (rw) 12 bits ; Swatch counter test
    TEST            = 0x12, // (rw) 11 bits ; ctrl
    GBASE           = 0x13, // (rw) 22 bits, 32 byte aligned ; Graphics base address
    ROW_WORDS       = 0x14, // (rw) 15 bits, 32 byte aligned ;
    MON_SENSE       = 0x15, // (rw)  9 bits ; Monitor sense control & status
    ENABLE          = 0x16, // (rw) 12 bits ; bit 7 does something
    GSC_DIVIDE      = 0x17, // (rw)  2 bits ; graphics clock divide count
    REFRESH_COUNT   = 0x18, // (rw) 10 bits ;
    INT_ENABLE      = 0x19, // (rw)  4 bits ;
    INT_STATUS      = 0x1A, // (ro)  ? bits ;
};

}; // namespace ControlRegs

namespace RadacalRegs {

enum RadacalRegs : uint8_t {
    ADDRESS         = 0, // address register
    CURSOR_DATA     = 1, // cursor data
    MULTI           = 2, // multipurpose section
    CLUT_DATA       = 3, // color palette data

    // multipurpose section registers
    CURSOR_POS_HI   = 0x10, // cursor position, high-order byte
    CURSOR_POS_LO   = 0x11, // cursor position, low-order byte
    MISC_CTRL       = 0x20, // miscellaneus control bits
    DBL_BUF_CTRL    = 0x21, // double buffer control bits
    //              = 0x22, // ? = 0
};

}; // namespace RadacalRegs

class ControlVideo : public PCIDevice, public VideoCtrlBase, public IobusDevice {
public:
    ControlVideo();
    ~ControlVideo() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<ControlVideo>(new ControlVideo());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

protected:
    void notify_bar_change(int bar_num);

    void enable_display();
    void disable_display();

    // HWComponent methods
    int device_postinit();

    // IobusDevice methods for RaDACal
    uint16_t iodev_read(uint32_t address);
    void iodev_write(uint32_t address, uint16_t value);

    // HW cursor support
    void draw_hw_cursor(uint8_t *dst_buf, int dst_pitch);

private:
    std::unique_ptr<DisplayID>      display_id;
    std::unique_ptr<AthensClocks>   clk_gen;

    std::unique_ptr<uint8_t[]>  vram_ptr;

    uint32_t    vram_size;
    uint32_t    io_base = 0;
    uint32_t    vram_base = 0;
    uint32_t    regs_base = 0;
    uint32_t    prev_test = 0x433;
    uint32_t    test = 0;
    bool        display_enabled = false;
    uint32_t    clock_divider = 0;
    uint32_t    row_words = 0;
    uint32_t    fb_base = 0;
    uint16_t    swatch_params[16];
    int         test_shift = 0;
    uint8_t     cur_mon_id = 0;
    uint8_t     flags = 0;
    uint8_t     int_enable = 0;
    uint8_t     int_status = 0;
    uint8_t     last_int_status = -1;
    int         last_int_status_read_count = 0;

    // RaDACal internal state
    uint8_t     rad_addr = 0;
    uint8_t     rad_cr = 0;
    uint8_t     rad_dbl_buf_cr = 0;
    uint16_t    rad_cur_pos = 0;
    uint8_t     comp_index;
    uint8_t     clut_color[3];
    uint8_t     cursor_data[24] = { 0 };
};

#endif // CONTROL_VIDEO_H
