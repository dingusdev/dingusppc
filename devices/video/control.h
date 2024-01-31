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

/** @file TNT on-board video output definitions. */

#ifndef CONTROL_VIDEO_H
#define CONTROL_VIDEO_H

#include <devices/common/i2c/athens.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/video/appleramdac.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

// Memory-mapped registers.
namespace ControlRegs {

enum ControlRegs : int {
    // 512 bytes, repeats 8 times for 4K total.
    // A register every 16 bytes, little endian, 32 bits, repeats 4 times.
    CUR_LINE        = 0x00, // current active video line        (ro) 12 bits
    VFPEQ           = 0x01, // vertical front porch with EQ     (rw) 12 bits
    VFP             = 0x02, // vertical front porch             (rw) 12 bits
    VAL             = 0x03, // vertical active line             (rw) 12 bits
    VBP             = 0x04, // vertical back porch              (rw) 12 bits
    VBPEQ           = 0x05, // vertical back porch with EQ      (rw) 12 bits
    VSYNC           = 0x06, // vertical sync starting point     (rw) 12 bits
    VHLINE          = 0x07, // vertical half line               (rw) 12 bits
    PIPE_DELAY      = 0x08, // controls pixel pipe delay        (rw) 10 bits, ndrv says 12 bits but only 10 are writable
    HPIX            = 0x09, // horizontal pixel count           (rw) 12 bits
    HFP             = 0x0A, // horizontal front porch           (rw) 12 bits
    HAL             = 0x0B, // horizontal active line           (rw) 12 bits
    HBWAY           = 0x0C, // horizontal breezeway             (rw) 12 bits
    HSP             = 0x0D, // horizontal sync starting point   (rw) 12 bits
    HEQ             = 0x0E, // horizontal equalization          (rw)  8 bits, ndrv says 12 bits but only 8 are writable
    HLFLN           = 0x0F, // horizontal half line             (rw) 12 bits
    HSERR           = 0x10, // horizontal serration             (rw) 12 bits
    CNTTST          = 0x11, // Swatch counter test value        (rw) 12 bits
    SWATCH_CTRL     = 0x12, // Swatch timing generator control  (rw) 11 bits
    GBASE           = 0x13, // graphics base address            (rw) 22 bits, 32 byte aligned
    ROW_WORDS       = 0x14, // framebuffer pitch                (rw) 15 bits, 32 byte aligned
    MON_SENSE       = 0x15, // Monitor sense control & status   (rw)  9 bits (three groups of 3-bits; the two LSB groups are writable)
    MISC_ENABLES    = 0x16, // controls chip's features         (rw) 12 bits
    GSC_DIVIDE      = 0x17, // graphics clock divide count      (rw)  2 bits
    REFRESH_COUNT   = 0x18, // VRAM refresh counter             (rw) 10 bits
    INT_ENABLE      = 0x19, // interrupt enable bits            (rw)  4 bits
    INT_STATUS      = 0x1A, // interrupt status bits            (ro)  3 bits
};

}; // namespace ControlRegs

// Bit definitions for the video timing generator (Swatch) control register.
enum {
//  1               = 1 <<  0, //
//  1               = 1 <<  1, //
    VSYNC_POLARITY  = 1 <<  2, // 0 - negative, 1 - positive
    RESET_TIMING    = 1 <<  3, // toggle this bit to change timing parameters
//  1               = 1 <<  4, //
//  1               = 1 <<  5, //
    HSYNC_POLARITY  = 1 <<  6, // 0 - negative, 1 - positive
//  0               = 1 <<  7, //
    INTERLACED      = 1 <<  8, // 0 - progressive, 1 = interlaced
//  0=unused        = 1 <<  9,
    DISABLE_TIMING  = 1 << 10, // 1 - disable video timing, 0 - enable it
};

// Bit definitions for MISC_ENABLES register.
enum {
    SCAN_CONTROL          = 1 <<  0, // 0 - interlaced, 1 - progressive; opposite of INTERLACED above
    FB_ENDIAN_LITTLE      = 1 <<  1, // framebuffer endianness: 0 - big, 1 - little // 1 also makes ControlRegs big endian
    DOUBLE_BUFFERING      = 1 <<  2, // the same data transfers are generated for both the standard bank of VRAM and the optional bank
    STANDARD_BANK_DISABLE = 1 <<  3, // 0 - data transfers are to be performed for the standard bank of VRAM
    SHIFT_CLOCK           = 1 <<  4, // shift clock is to be generated
    DETECT_PAGE_HITS      = 1 <<  5, // VRAM state machines detect page hits on the system bus to frame buffer single beat writes
    VRAM_WIDE_MODE        = 1 <<  6, // VRAM bus width: 1 - 128bit, 0 - 64bit
    MHZ_30_50             = 1 <<  7, // 0 - 50 MHz, 1 - 33 MHz // setting this to 33 MHz causes system to hang
    VSYNC_DISABLE         = 1 <<  8, // 0 - enable vertical sync, 1 - disable it
    HSYNC_DISABLE         = 1 <<  9, // 0 - enable horizontal sync, 1 - disable it
    CSYNC_DISABLE         = 1 << 10, // 0 - enable composite sync, 1 - disable it
    BLANK_DISABLE         = 1 << 11, // 0 - enable blanking, 1 - disable it
};

// Bit definitions for INT_ENABLE & INT_STATUS registers.
enum {
    VBL_IRQ_CLR  = 1 << 3, // VBL interrupt clear  bit (INT_ENABLE)
    VBL_IRQ_EN   = 1 << 2, // VBL interrupt enable bit (INT_ENABLE)
    VBL_IRQ_STAT = 1 << 2, // VBL interrupt status bit (INT_STATUS)
};

class ControlVideo : public PCIDevice, public VideoCtrlBase {
public:
    ControlVideo();
    ~ControlVideo() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<ControlVideo>(new ControlVideo());
    }

    uint8_t* GetVram();

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

protected:
    void change_one_bar(uint32_t &aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num);
    void notify_bar_change(int bar_num);

    void enable_display();
    void disable_display();

    // HWComponent methods
    int device_postinit();

private:
    std::unique_ptr<DisplayID>      display_id;
    std::unique_ptr<AthensClocks>   clk_gen;
    std::unique_ptr<AppleRamdac>    radacal = nullptr;

    std::unique_ptr<uint8_t[]>  vram_ptr;

    uint32_t    vram_size;
    uint32_t    io_base = 0;
    uint32_t    vram_base = 0;
    uint32_t    regs_base = 0;
    uint32_t    swatch_ctrl = 0;
    bool        display_enabled = false;
    uint32_t    clock_divider = 0;
    uint32_t    row_words = 0;
    uint32_t    fb_base = 0;
    uint16_t    swatch_params[16];
    uint16_t    cnt_tst = 0;
    int         strobe_counter = 0;
    uint8_t     vram_banks = 0;
    uint8_t     cur_mon_id = 0;
    uint8_t     mon_sense = 0;
    uint16_t    enables = 0;
    uint8_t     int_enable = 0;
    uint8_t     int_status = 0;
    uint8_t     last_int_status = -1;
    int         last_int_status_read_count = 0;
};

#endif // CONTROL_VIDEO_H
