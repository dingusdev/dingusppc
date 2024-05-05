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

/** @file DisplayID class implementation. */

#include <devices/video/displayid.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <map>
#include <string>

typedef struct {
    uint16_t      h;
    uint16_t      v;
    float         pixel_clock; // MHz
    float         h_freq;      // kHz
    float         refresh;     // Hz
} MonitorRes;

typedef struct {
    uint8_t       std_sense_code;
    uint8_t       ext_sense_code;
    const char *  apple_enum;
    const char *  name;
    const char *  description;
    MonitorRes    resolutions[10];
} MonitorInfo;

/** Mapping between monitor IDs and their sense codes. */
static const std::map<std::string, MonitorInfo> MonitorIdToCode = {
    { "MacColor21in", {
        0, 0x00,
        "kESCZero21Inch",
        "21\" RGB",
        "RGB 21\", 21\" Color, Apple 21S Color", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "PortraitGS", {
        1, 0x14,
        "kESCOnePortraitMono",
        "Portrait Monochrome",
        "B&W 15\", Apple Portrait", {
            { 640, 870,  57.2832, 68.9  , 75   }
        }
    }},
    { "MacRGB12in", {
        2, 0x21,
        "kESCTwo12Inch",
        "12\" RGB",
        "12\" Apple RGB", {
            { 512, 384,  15.6672, 24.48 , 60.15}
        }
    }},
    { "Radius21in", {
        3, 0x31,
        "kESCThree21InchRadius",
        "21\" RGB (Radius)",
        "", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "Radius21inGS", {
        3, 0x34,
        "kESCThree21InchMonoRadius",
        "21\" Monochrome (Radius)",
        "", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "TwoPageGS", {
        3, 0x35,
        "kESCThree21InchMono",
        "21\" Monochrome",
        "B&W 21\", Apple 2 Page Mono", {
            {1152, 870, 100     , 68.7  , 75   }
        }
    }},
    { "NTSC", {
        4, 0x0A,
        "kESCFourNTSC",
        "NTSC",
        "", {
            { 512, 384,  12.2727, 15.7  , 59.94},
            { 640, 480,  12.2727, 15.7  , 59.94}
        }
    }},
    { "MacRGB15in", {
        5, 0x1E,
        "kESCFivePortrait",
        "Portrait RGB",
        "RGB 15\", 15\" Tilt", {
            { 640, 870,  57.2834,  0    , 75   }
        }
    }},
    { "Multiscan15in", {
        6, 0x03,
        "kESCSixMSB1",
        "MultiScan Band-1 (12\" thru 16\")",
        "Multiple Scan 13, 14\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 832, 624,   0     ,  0    , 75   }
        }
    }},
    { "Multiscan17in", {
        6, 0x0B,
        "kESCSixMSB2",
        "MultiScan Band-2 (13\" thru 19\")",
        "Multiple Scan 16, 17\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 832, 624,   0     ,  0    , 75   },
            {1024, 768,   0     ,  0    , 75   }
        }
    }},
    { "Multiscan20in", {
        6, 0x23,
        "kESCSixMSB3",
        "MultiScan Band-3 (13\" thru 21\")",
        "Multiple Scan 20, 21\"", {
            { 640, 480,   0     ,  0    , 67   },
            { 640, 480,   0     ,  0    ,120   }, // control; not platinum
            { 832, 624,   0     ,  0    , 75   },
            {1024, 768,   0     ,  0    , 74.9 },
            {1152, 870,   0     ,  0    , 75   },
            {1280, 960,   0     ,  0    , 75   },
            {1280,1024,   0     ,  0    , 75   }
        }
    }},
    { "HiRes12-14in", {
        6, 0x2B,
        "kESCSixStandard",
        "13\"/14\" RGB or 12\" Monochrome",
        "B&W 12\", 12\" Apple Monochrome, 13\" Apple RGB, Hi-Res 12-14\"", {
            { 640, 480,  30.24  , 35.0  , 66.7 },
        }
    }},
    { "PALEncoder", {
        7, 0x00,
        "kESCSevenPAL",
        "PAL",
        "PAL, NTSC/PAL (Option 1)", {
            { 640, 480,  14.75  , 15.625, 50   },
            { 768, 576,  14.75  , 15.625, 50   }
        }
    }},
    { "NTSCEncoder", {
        7, 0x14,
        "kESCSevenNTSC",
        "NTSC",
        "NTSC w/convolution (Alternate)", {
            { 512, 384,  12.2727,  0    ,  60  },
            { 640, 480,  12.2727,  0    ,  60  }
        }
    }},
    { "VGA-SVGA", {
        7, 0x17,
        "kESCSevenVGA",
        "VGA",
        "VGA", {
            { 640, 480,  0      , 0     , 60   },
            { 640, 480,  0      , 0     ,120   }, // control; not platinum
            { 800, 600,  0      , 0     , 60   },
            { 800, 600,  0      , 0     , 72   },
            { 800, 600,  0      , 0     , 75   },
            {1024, 768,  0      , 0     , 60   },
            {1024, 768,  0      , 0     , 70   },
            {1024, 768,  0      , 0     , 75   },
            {1280, 960,  0      , 0     , 75   },
            {1280,1024,  0      , 0     , 75   }
        }
    }},
    { "MacRGB16in", {
        7, 0x2D,
        "kESCSeven16Inch",
        "16\" RGB (GoldFish)",
        "RGB 16\", 16\" Color", {
            { 832, 624,  57.2832, 49.7  , 75   }
        }
    }},
    { "PAL", {
        7, 0x30,
        "kESCSevenPALAlternate",
        "PAL (Alternate)",
        "PAL w/convolution (Alternate) (Option 2)", {
            { 640, 480,  14.75  , 15.625, 50   },
            { 768, 576,  14.75  , 15.625, 50   }
        }
    }},
    { "MacRGB19in", {
        7, 0x3A,
        "kESCSeven19Inch",
        "Third-Party 19",
        "RGB 19\", 19\" Color", {
            {1024, 768,  80      , 0    , 74.9 }
        }
    }},
    { "DDC", {
        7, 0x3E,
        "kESCSevenDDC",
        "DDC display",
        "EDID", {
            {1024, 768,  80      , 0    ,  0   }
        }
    }},
    { "NotConnected", {
        7, 0x3F,
        "kESCSevenNoDisplay",
        "No display connected",
        "no-connect"
    }},
};

static const std::map<std::string, std::string> MonitorAliasToId = {
    { "AppleVision1710", "HiRes12-14in" }
};

DisplayID::DisplayID()
{
    // assume a DDC monitor is connected by default
    this->id_kind = Disp_Id_Kind::DDC2B;

    std::string mon_id = GET_STR_PROP("mon_id");
    if (!mon_id.empty()) {
        if (MonitorAliasToId.count(mon_id)) {
            mon_id = MonitorAliasToId.at(mon_id);
        }
        if (MonitorIdToCode.count(mon_id)) {
            auto monitor = MonitorIdToCode.at(mon_id);
            this->std_sense_code = monitor.std_sense_code;
            this->ext_sense_code = monitor.ext_sense_code;
            this->id_kind = Disp_Id_Kind::AppleSense;
            LOG_F(INFO, "DisplayID mode set to AppleSense");
            LOG_F(INFO, "Standard sense code: %d", this->std_sense_code);
            LOG_F(INFO, "Extended sense code: 0x%02X", this->ext_sense_code);
        }
    }

    /* Initialize DDC I2C bus */
    this->next_state = I2CState::STOP;
    this->prev_state = I2CState::STOP;
    this->last_sda   = 1;
    this->last_scl   = 1;
    this->data_ptr   = 0;
    this->data_pos   = 0;
}

DisplayID::DisplayID(uint8_t std_code, uint8_t ext_code)
{
    this->id_kind = Disp_Id_Kind::AppleSense;

    this->std_sense_code = std_code;
    this->ext_sense_code = ext_code;
}

uint8_t DisplayID::read_monitor_sense(uint8_t levels, uint8_t dirs)
{
    uint8_t scl, sda;

    switch(this->id_kind) {
    case Disp_Id_Kind::DDC2B:
        /* If GPIO pins are in the output mode, pick up their levels.
           In the input mode, GPIO pins will be read "high" */
        scl = (dirs & 2) ? !!(levels & 2) : 1;
        sda = (dirs & 4) ? !!(levels & 4) : 1;

        return update_ddc_i2c(sda, scl);
    case Disp_Id_Kind::AppleSense:
        switch ((dirs << 3) | levels) {
        case 0b0'100'011: // Sense line 2 pulled low; get sense line 1 and 0
            return  ((this->ext_sense_code & 0b0'11'00'00) >> 4);               // -> 0__
        case 0b0'010'101: // Sense line 1 pulled low; get sense line 2 and 0
            return (((this->ext_sense_code & 0b0'00'10'00) >> 1) |  // -> _00
                    ((this->ext_sense_code & 0b0'00'01'00) >> 2));  // -> 00_   // -> _0_
        case 0b0'001'110: // Sense line 0 pulled low; get sense line 2 and 1
            return  ((this->ext_sense_code & 0b0'00'00'11) << 1);               // -> __0
        default:
            return this->std_sense_code;
        }
    }
}

uint8_t DisplayID::set_result(uint8_t sda, uint8_t scl)
{
    uint16_t data_out;

    this->last_sda = sda;
    this->last_scl = scl;

    data_out = 0;

    if (scl) {
        data_out |= 2;
    }

    if (sda) {
        data_out |= 4;
    }

    return data_out;
}

uint8_t DisplayID::update_ddc_i2c(uint8_t sda, uint8_t scl)
{
    bool clk_gone_high = false;

    if (scl != this->last_scl) {
        this->last_scl = scl;
        if (scl) {
            clk_gone_high = true;
        }
    }

    if (sda != this->last_sda) {
        /* START = SDA goes high to low while SCL is high */
        /* STOP  = SDA goes low to high while SCL is high */
        if (this->last_scl) {
            if (!sda) {
                LOG_F(9, "DDC-I2C: START condition detected!");
                this->next_state = I2CState::DEV_ADDR;
                this->bit_count  = 0;
            } else {
                LOG_F(9, "DDC-I2C: STOP condition detected!");
                this->next_state = I2CState::STOP;
            }
        }
        return set_result(sda, scl);
    }

    if (!clk_gone_high) {
        return set_result(sda, scl);
    }

    switch (this->next_state) {
    case I2CState::STOP:
        break;

    case I2CState::ACK:
        this->bit_count = 0;
        this->byte      = 0;
        switch (this->prev_state) {
        case I2CState::DEV_ADDR:
            if ((dev_addr & 0xFE) == 0xA0) {
                sda = 0; /* send ACK */
            } else {
                LOG_F(ERROR, "DDC-I2C: unknown device address 0x%X", this->dev_addr);
                sda = 1; /* send NACK */
            }
            if (this->dev_addr & 1) {
                this->next_state = I2CState::DATA;
                this->data_ptr   = this->edid;
                this->data_pos   = 0;
                this->byte       = this->data_ptr[this->data_pos++];
            } else {
                this->next_state = I2CState::REG_ADDR;
            }
            break;
        case I2CState::REG_ADDR:
            this->next_state = I2CState::DATA;
            if (!this->reg_addr) {
                sda = 0; /* send ACK */
            } else {
                LOG_F(ERROR, "DDC-I2C: unknown register address 0x%X", this->reg_addr);
                sda = 1; /* send NACK */
            }
            break;
        case I2CState::DATA:
            this->next_state = I2CState::DATA;
            if (dev_addr & 1) {
                if (!sda) {
                    /* load next data byte */
                    if (data_pos < 128)
                        this->byte = this->data_ptr[this->data_pos++];
                    else
                        this->byte = 0;
                } else {
                    LOG_F(ERROR, "DDC-I2C: Oops! NACK received");
                }
            } else {
                sda = 0; /* send ACK */
            }
            break;
        }
        break;

    case I2CState::DEV_ADDR:
    case I2CState::REG_ADDR:
        this->byte = (this->byte << 1) | this->last_sda;
        if (this->bit_count++ >= 7) {
            this->bit_count  = 0;
            this->prev_state = this->next_state;
            this->next_state = I2CState::ACK;
            if (this->prev_state == I2CState::DEV_ADDR) {
                LOG_F(9, "DDC-I2C: device address received, addr=0x%X", this->byte);
                this->dev_addr = this->byte;
            } else {
                LOG_F(9, "DDC-I2C: register address received, addr=0x%X", this->byte);
                this->reg_addr = this->byte;
            }
        }
        break;

    case I2CState::DATA:
        sda = (this->byte >> (7 - this->bit_count)) & 1;
        if (this->bit_count++ >= 7) {
            this->bit_count  = 0;
            this->prev_state = this->next_state;
            this->next_state = I2CState::ACK;
        }
        break;
    }

    return set_result(sda, scl);
}
