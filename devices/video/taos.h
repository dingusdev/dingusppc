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

/** @file Taos video controller definitions. */

/**
    Kudos to Keith Kaisershot @ blitter.net for his precious technical help!
*/

#ifndef TAOS_VIDEO_H
#define TAOS_VIDEO_H

#include <devices/common/clockgen/athens.h>
#include <devices/common/i2c/i2c.h>
#include <devices/common/mmiodevice.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

#define TAOS_IOREG_BASE         0xF0800000
#define TAOS_VRAM_REGION_BASE   0xF0000000
#define TAOS_CLUT_BASE          0xF1000000
#define TAOS_CHIP_VERSION       0xA5000000

/** Taos register definitions. */
// The implemented bits always start with the most significant bit (bit 31).
// Unimplemented bits return zeroes.
enum {
    // framebuffer configuration and control
    FB_BASE         = 0x00, // 0x00, frame buffer base address,    19 bits, 32 byte aligned
    ROW_WORDS       = 0x01, // 0x04, frame buffer pitch            12 bits, 32 byte aligned
    COLOR_MODE      = 0x02, // 0x08, color mode                     1 bit (MSB)
    VIDEO_MODE      = 0x03, // 0x0C, current video mode,            2 bits (MSB)
    VEFS            = 0x04, // 0x10, vertical even field start,    12 bits
    VOFS            = 0x05, // 0x14, vertical odd field start,     12 bits
    HSTART          = 0x06, // 0x18, horizontal active start,      12 bits
    VCONV_END       = 0x07, // 0x1C, vertical convolution end,     12 bits
    CRT_CTRL        = 0x08, // 0x20, frame buffer control,         11 bits
    CRT_TEST        = 0x09, // 0x24, enables test features,        18 bits

    // timing generator parameters
    HEQ             = 0x0B, // 0x2C, horizontal equalization,       8 bits
    HBWAY           = 0x0C, // 0x30, horizontal breezeway,         12 bits
    HAL             = 0x0D, // 0x34, horizontal active line,       12 bits
    HSERR           = 0x0E, // 0x38, horizontal serration,         12 bits
    HFP             = 0x0F, // 0x3C, horizontal front porch,       12 bits
    HPIX            = 0x10, // 0x40, horizontal pixel count,       12 bits
    HSP             = 0x11, // 0x44, horizontal sync pulse,        12 bits
    HLFLN           = 0x12, // 0x48, horizontal half line,         12 bits
    VBPEQ           = 0x13, // 0x4C, vertical back porch with EQ,  12 bits
    VBP             = 0x14, // 0x50, vertical back porch,          12 bits
    VAL             = 0x15, // 0x54, vertical active line,         12 bits
    VFP             = 0x16, // 0x58, vertical front porch,         12 bits
    VFPEQ           = 0x17, // 0x5C, vertical front porch with EQ, 12 bits
    VSYNC           = 0x18, // 0x60, vertical sync starting point, 12 bits
    VHLINE          = 0x19, // 0x64, vertical half line,           12 bits

    // GPIO and interrupts
    GPIO_IN         = 0x1A, // 0x68, reads GPIO pins,               8 bits
    GPIO_CONFIG     = 0x1B, // 0x6C, configure GPIO pins,           8 bits
    GPIO_OUT        = 0x1C, // 0x70, outputs to GPIO pins,          8 bits
    GPIO_INT_CTRL   = 0x1D, // 0x74, controls GPIO interrupts,      8 bits
    INT_LEVELS      = 0x1E, // 0x78, interrupt levels,             12 bits (read-only)
    INT_ENABLES     = 0x1F, // 0x7C, enable/disable interrupts,    12 bits
    INT_CLEAR       = 0x20, // 0x80, clear pending interrupts,     12 bits (write-only)
    TAOS_VERSION    = 0x23, // 0x8C, chip version,                  8 bits (read-only)
    INT_SET         = 0x24, // 0x90, allow setting interrupts      12 bits (write-only)
    HEB             = 0x25, // 0x94, horizontal early blank,       12 bits
};

/** Taos video modes (see VIDEO_MODE register above). */
enum {
    Progressive_Scan = 0,  // Progressive scan
    Interlace,             // Interlaced scan, no convolution
    Interlace_Conv,        // Interlaced scan, convolution, no scaling
    Interlace_Conv_Scale   // Interlaced scan, convolution, and scaling
};

/** CRT_CTRL bit definitions. */
enum {
    ENABLE_VIDEO_OUT    = 24,
};

#define REG_TO_INDEX(reg) ((reg) - HEQ)

/** Monitor ID definitions used internally by the Taos driver. */
// Those values are obtained by reading GPIO 0...2 pins that are
// directly connected to the three-position mechanical switch
// on the backside of the console. All signals are active low.
enum {
    MON_ID_PAL  = 3, // GPIO_0 = "0", GPIO_1 = "1", GPIO_2 = "1"
    MON_ID_NTSC = 5, // GPIO_0 = "1", GPIO_1 = "0", GPIO_2 = "1"
    MON_ID_VGA  = 6  // GPIO_0 = "1", GPIO_1 = "1", GPIO_2 = "0"
};

/** Definitions for the GPIO pins. */
enum {
    GPIO_MONID      = 29, // GPIO 0..2 are monitor identification pins
    GPIO_CDTRAY     = 27, // GPIO 4: "1" - CD-ROM tray is closed
    GPIO_VSYNC      = 25, // GPIO 6: vertical synch input (active low)
};

/** Broktree Bt856 digital video encoder. */
class Bt856 : public I2CDevice, public HWComponent {
public:
    Bt856(uint8_t dev_addr) {
        supports_types(HWCompType::I2C_DEV);

        this->my_addr = dev_addr;
    };

    ~Bt856() = default;

    // I2CDevice methods
    void start_transaction() {
        this->pos = 0; // reset read/write position
    };

    bool send_subaddress(uint8_t sub_addr) {
        return true;
    };

    bool send_byte(uint8_t data) {
        switch (this->pos) {
        case 0:
            this->reg_num = data;
            this->pos++;
            break;
        case 1:
            LOG_F(INFO, "Bt856: reg 0x%X set to 0x%X", this->reg_num, data);
            break;
        default:
            LOG_F(WARNING, "Bt856: too much data received!");
            return false; // return NACK
        }
        return true;
    };

    bool receive_byte(uint8_t* p_data) {
        *p_data = 0x60; // return my device ID
        return true;
    };

private:
    uint8_t     my_addr = 0;
    uint8_t     reg_num = 0;
    int         pos = 0;
};


class TaosVideo : public VideoCtrlBase, public MMIODevice {
public:
    TaosVideo();
    ~TaosVideo() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<TaosVideo>(new TaosVideo());
    }

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

private:
    void enable_display();
    void disable_display();
    void convert_frame_15bpp_indexed(uint8_t *dst_buf, int dst_pitch);

    std::unique_ptr<AthensClocks>   clk_gen = nullptr;
    std::unique_ptr<Bt856>          vid_enc = nullptr;

    uint8_t     gpio_cfg        = 0;
    uint8_t     mon_id          = MON_ID_NTSC;
    uint8_t     vsync_active    = 0;
    uint8_t     video_mode      = 0;
    uint8_t     *vram_ptr       = nullptr;
    uint32_t    fb_base         = 0;
    uint32_t    row_words       = 0;
    uint32_t    color_mode      = 0;
    uint32_t    crt_ctrl        = 0;
    uint32_t    int_enables     = 0;
    uint32_t    swatch_regs[15] = {};
};

#endif // TAOS_VIDEO_H
