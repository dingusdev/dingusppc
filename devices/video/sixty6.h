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

/** @file TNT on-board NTSC/PAL video output definitions. */

#ifndef SIXTY6_VIDEO_H
#define SIXTY6_VIDEO_H

#include <devices/ioctrl/macio.h>
#include <devices/video/saa7187.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

namespace Sixty6BaseReg {

enum Sixty6BaseReg : uint32_t {
    CLUT_DATA           = 0x00, // Open Firmware chaos reads PRSNT_BITS from here!?
    CLUT_ADDR           = 0x01,

    CONTROL_DATA        = 0x02,
    CONTROL_ADDR        = 0x03,
};

};

namespace Sixty6Reg {

enum Sixty6Reg : uint32_t {
    // registers 0x00..0x7f are writable; unused in sixty6; Hardware Cursor for ninety9
    REG_V1O_L   = 0x80, // 8 bits
    REG_V1O_H   = 0x81, // 2 bits

    REG_V2O_L   = 0x82, // 8 bits
    REG_V2O_H   = 0x83, // 2 bits

    REG_V1E_L   = 0x84, // 8 bits
    REG_V1E_H   = 0x85, // 2 bits

    REG_V2E_L   = 0x86, // 8 bits
    REG_V2E_H   = 0x87, // 2 bits

    REG_H1_L    = 0x88, // 8 bits
    REG_H1_H    = 0x89, // 2 bits

    REG_H2_L    = 0x8a, // 8 bits
    REG_H2_H    = 0x8b, // 2 bits

    BASE_ADDR_L = 0x8c, // 8 bits
    BASE_ADDR_M = 0x8d, // 8 bits
    BASE_ADDR_H = 0x8e, // 2 bits

    PITCH_L     = 0x8f, // 8 bits
    PITCH_H     = 0x90, // 3 bits

    CURSOR_X_L  = 0x91, // 8 bits
    CURSOR_X_H  = 0x92, // 2 bits

    CURSOR_Y_L  = 0x93, // 8 bits
    CURSOR_Y_H  = 0x94, // 2 bits

    INT_COUNT_L = 0x95, // 8 bits
    INT_COUNT_H = 0x96, // 2 bits

    CONTROL_1   = 0x97, // 8 bits // 0x00, 0x02, 0x40, 0x48, 0x49, 0x79
                            // 0x01: ?
                            // 0x02: hardware cursor enable (read-only as 0 for sixty6)
                            // 0x04: zoom
                            // 0x08: ?
                            // 0x30: pixel depth: 0 = 8-bit indexed, 2 = 16 bit, 3 = 32 bit
                            // 0x40: interrupt enable
                            // 0x80: interrupt
    CONTROL_2   = 0x98, // 8 bits // 0x02, 0x03
                            // 0x01: ?
                            // 0x02: ?
    // registers 0x99..0xff are read only and read from registers 0x19..0x7F.
};

};

class Saa7187VideoEncoder;
class ControlVideo;

class Sixty6Video: public VideoCtrlBase, public IobusDevice, public HWComponent {

public:
    Sixty6Video();

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Sixty6Video>(new Sixty6Video());
    }

protected:

    void enable_display();
    void disable_display();

    // HWComponent methods
    int device_postinit();

    // IobusDevice methods
    uint16_t iodev_read(uint32_t address);
    void iodev_write(uint32_t address, uint16_t value);

    uint8_t     control_addr = 0;
    uint8_t     clut_addr = 0;
    uint8_t     clut_color[3];
    uint8_t     comp_index = 0;

    uint16_t    v1_odd = 0;
    uint16_t    v2_odd = 0;
    uint16_t    v1_even = 0;
    uint16_t    v2_even = 0;
    uint16_t    h1 = 0;
    uint16_t    h2 = 0;
    uint32_t    base_addr = 0;
    uint16_t    pitch = 0;
    uint16_t    cursor_x = 0;
    uint16_t    cursor_y = 0;
    uint16_t    int_count = 0;
    uint8_t     control_1 = 0;
    bool        interrupt_enabled = false;
    uint8_t     control_2 = 0;
    uint8_t     last_control_1_value = 0;
    uint8_t     last_control_1_count = 0;

    bool        changed = false;

    std::unique_ptr<Saa7187VideoEncoder> saa7187;
    ControlVideo * control_video = nullptr;
};

#endif // SIXTY6_VIDEO_H
