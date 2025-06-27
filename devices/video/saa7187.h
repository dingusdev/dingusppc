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

/** @file SAA7187 Digital video encoder definitions. */

#ifndef SAA7187_H
#define SAA7187_H

#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>

namespace Saa7187Regs {

enum Saa7187Regs : uint8_t { // i2c address is 0x44; fatman is at 0x46?
    IPC             = 0x3a, // Input port control
        FMT0            = 0x01,
        FMT1            = 0x02,
        VUV2C           = 0x04,
        VY2C            = 0x08,
        CBENB           = 0x80,
    OSDY0           = 0x42, // OSD LUT, 8 colors
    OSDU0           ,
    OSDV0           ,
    CHPS            = 0x5a, // Chrominance phase
    GAINU           = 0x5b, // Gain U
    GAINV           = 0x5c, // Gain V
    BLCKL           = 0x5d, // Gain U MSB, black level
        BLCKL0_5        = 0x3f,
        GAINU8          = 0x80,
    BLNNL           = 0x5e, // Gain V MSB, blanking level
        BLNNL0_5        = 0x3f,
        GAINV8          = 0x80,
    CCRS            = 0x60, // Cross-colour select
        CCRS0           = 0x40,
        CCRS1           = 0x80,
    SCTRL           = 0x61, // Standard control
        FISE            = 0x01,
        PAL             = 0x02,
        SCBW            = 0x04,
        RTCE            = 0x08,
        YGS             = 0x10,
        INPI1           = 0x20,
        DOWN            = 0x40,
    BSTA            = 0x62, // Burst amplitude
        BSTA0_6         = 0x7f,
        SQP             = 0x80,
    FSC0            = 0x63, // Subcarrier 0
    FSC8            = 0x64, // Subcarrier 1
    FSC16           = 0x65, // Subcarrier 2
    FSC24           = 0x66, // Subcarrier 3
        // 0x25555555 = NTSC
        // 0x26798c0c = PAL
    L21O0           = 0x67, // Line 21 odd 0
    L21O1           = 0x68, // Line 21 odd 1
    L21E0           = 0x69, // Line 21 even 0
    L21E1           = 0x6a, // Line 21 even 1
    SCCLN           = 0x6b, // CC line
        SCCLN0_4        = 0x1f,
    RCTRL           = 0x6c, // RCV port control
        PRCV2           = 0x01,
        ORCV2           = 0x02,
        CBLF            = 0x04,
        PRCV1           = 0x08,
        ORCV1           = 0x10,
        TRCV2           = 0x20,
        SRCV10          = 0x40,
        RCV11           = 0x80,
    RCM_CC          = 0x6d, // RCM, CC mode
        CCEN0           = 0x01,
        CCEN1           = 0x02,
        SRCM10          = 0x04,
        SRCM11          = 0x08,
    HTRIG0          = 0x6e, // Horizontal trigger
    HTRIG8          = 0x6f, // Horizontal trigger
        HTRIG8_10       = 0x07,
    RES_VTRIG       = 0x70, // fsc reset mode, Vertical trigger
        VTRIG0_4        = 0x1f,
        SBLBN           = 0x20,
        PHRES0          = 0x40,
        HRES1           = 0x80,
    BMRQ0           = 0x71, // Begin master request
    EMRQ0           = 0x72, // End master request
    BMRQ8_EMRQ8     = 0x73, // MSBs master request
        BMRQ08_10       = 0x07,
        EMRQ08_10       = 0x70,
    BRCV0           = 0x77, // Begin RCV2 output
    ERCV0           = 0x78, // End RCV2 output
    BRCV8_ERCV8     = 0x79, // MSBs RCV2 output
        BRCV08_10       = 0x07,
        ERCV08_10       = 0x70,
    FLEN            = 0x7a, // Field length
    FAL             = 0x7b, // First active line
    LAL             = 0x7c, // Last active line
    FLEN8_FAL8_LAL8 = 0x7d, // MSBs field control
        FLEN8_9         = 0x03,
        FAL8            = 0x10,
        LAL8            = 0x20,
    last_reg = 0x7e,
};

} // namespace Saa7187Regs

class Saa7187VideoEncoder : public I2CDevice, public HWComponent
{
public:
    Saa7187VideoEncoder(uint8_t dev_addr);
    ~Saa7187VideoEncoder() = default;

    // I2CDevice methods
    void start_transaction();
    bool send_subaddress(uint8_t sub_addr);
    bool send_byte(uint8_t data);
    bool receive_byte(uint8_t* p_data);

private:
    uint8_t     my_addr = 0;
    uint8_t     reg_num = 0;
    int         pos = 0;

    uint8_t     regs[Saa7187Regs::last_reg] = {0};
};

#endif // SAA7187_H
