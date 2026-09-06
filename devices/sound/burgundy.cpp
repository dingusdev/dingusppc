/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

/** Burgundy sound codec emulation. */

#include <core/endianswap.h>
#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <devices/sound/burgundy.h>
#include <loguru.hpp>

namespace SOUND_CONTROL { // 0x00
    enum {
        INSUBFRAME_MASK     = 0x0000000F, INSUBFRAME_POS        = 0,
            INSUBFRAME0         = 0x1,
            INSUBFRAME1         = 0x2,
            INSUBFRAME2         = 0x4,
            INSUBFRAME3         = 0x8,
        OUTSUBFRAME_MASK    = 0x000000F0, OUTSUBFRAME_POS       = 4,
            OUTSUBFRAME0        = 0x1,
            OUTSUBFRAME1        = 0x2,
            OUTSUBFRAME2        = 0x4,
            OUTSUBFRAME3        = 0x8,
        RATE_MASK           = 0x00000700, RATE_POS              = 8,
            RATE_44100          = 0x0,
        ERROR               = 0x00000800,
        PORTCHANGE          = 0x00001000,
        ERRORINT            = 0x00002000,
        STATUSSUBFRAME_MASK = 0x00018000, STATUSSUBFRAME_POS    = 15,
            STATUSSUBFRAME0     = 0x0,
            STATUSSUBFRAME1     = 0x1,
            STATUSSUBFRAME2     = 0x2,
            STATUSSUBFRAME3     = 0x3,
    };
}

namespace CODEC_CONTROL { // 0x10
    enum {
        DATA_MASK           = 0x000000FF, DATA_POS              = 0,
        CURRENTBYTE_MASK    = 0x00000300, CURRENTBYTE_POS       = 8,
        LASTBYTE_MASK       = 0x00000C00, LASTBYTE_POS          = 10,
        ADDR_MASK           = 0x000FF000, ADDR_POS              = 12,
        RESET               = 0x00100000, // should be set when current byte is 0
        WRITE               = 0x00200000, // READ = 0
        BUSY                = 0x01000000, // set when writing to CODEC_CONTROL, clear when done
    };
}

namespace CODEC_STATUS { // 0x20
    enum {
        SENSE_MASK              = 0x0000000F, SENSE_POS         = 0,
            SENSE_MIC               = 0x2,
            SENSE_HEADPHONES        = 0x4,
            SENSE_HEADPHONES2       = 0x8, // 1 = line level mic (default) 0 = powered mic
        DATA_MASK               = 0x00000FF0, DATA_POS          = 4,
        CURRENTBYTE_MASK        = 0x00003000, CURRENTBYTE_POS   = 12,
        BYTECOUNTER_MASK        = 0x0000C000, BYTECOUNTER_POS   = 14,
        INDICATOR_MASK          = 0x000F0000, INDICATOR_POS     = 16,
            INDICATOR_TONECONTROL   = 0x1,
            INDICATOR_OVERFLOW0     = 0x2,
            INDICATOR_OVERFLOW1     = 0x3,
            INDICATOR_OVERFLOW2     = 0x4,
            INDICATOR_INPUTLINECHG  = 0x6,
            INDICATOR_THRESHOLD0    = 0xB,
            INDICATOR_THRESHOLD1    = 0xC,
            INDICATOR_THRESHOLD2    = 0xD,
            INDICATOR_THRESHOLD3    = 0xE,
            INDICATOR_TWILIGHTCMP   = 0xF,
        READY                   = 0x00400000,
        FIRST_VALID_BYTE        = 0x00800000, // wait for set, then wait for clear before reading data
    };
}

BurgundyCodec::BurgundyCodec(std::string name) : MacioSndCodec(name) {
    supports_types(HWCompType::SND_CODEC);

    static int burgundy_sample_rates[1] = { 44100 };

    // Burgundy seems to supports only one sample rate
    this->sr_table  = burgundy_sample_rates;
    this->max_sr_id = 1;

    this->set_sample_rate(0); // set default sample rate

    this->reg_array[0x01] = 0x01010000; // ID 1 (Burgundy), Vendor 1 (Crystal), Version 0, Revision 0
}

uint32_t BurgundyCodec::snd_ctrl_read(uint32_t offset, int size) {
    uint32_t value = 0;

    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        value = this->snd_ctrl_reg;
        break;
    case AWAC_CODEC_CTRL_REG:
        value = this->last_ctrl_data;
        break;
    case AWAC_CODEC_STATUS_REG:
        value =
            (this->data_byte << CODEC_STATUS::DATA_POS) |
            (this->read_pos << CODEC_STATUS::CURRENTBYTE_POS) |
            (this->byte_counter << CODEC_STATUS::BYTECOUNTER_POS) |
            (0 << CODEC_STATUS::INDICATOR_POS) |
            CODEC_STATUS::READY |
            (this->first_valid ? CODEC_STATUS::FIRST_VALID_BYTE : 0);
        break;
    case AWAC_FRAME_COUNT:
        value = (uint32_t)(
            (
                (TimerManager::get_instance()->current_time_ns() - frame_count_start_time) * this->sr_table[0]
                + 500000000
            )  / 1000000000 + this->frame_count
        );
        break;
    default:
        LOG_F(ERROR, "%s: read from unsupported register 0x%X", this->name.c_str(),
              offset);
    }

    return BYTESWAP_32(value);
}

void BurgundyCodec::snd_ctrl_write(uint32_t offset, uint32_t value, int size) {
    value = BYTESWAP_32(value);

    switch (offset) {
    case AWAC_SOUND_CTRL_REG:
        this->snd_ctrl_reg = value;
        //this->set_sample_rate((this->snd_ctrl_reg & SOUND_CONTROL::RATE_MASK) >> SOUND_CONTROL::RATE_POS);
        break;
    case AWAC_CODEC_CTRL_REG:
    {
        this->last_ctrl_data = value & ~CODEC_CONTROL::BUSY;
        uint8_t write_byte = (value & CODEC_CONTROL::DATA_MASK) >> CODEC_CONTROL::DATA_POS;
        uint8_t reg_addr = (value & CODEC_CONTROL::ADDR_MASK) >> CODEC_CONTROL::ADDR_POS;
        uint8_t cur_byte = (value & CODEC_CONTROL::CURRENTBYTE_MASK) >> CODEC_CONTROL::CURRENTBYTE_POS;
        uint8_t last_byte = (value & CODEC_CONTROL::LASTBYTE_MASK) >> CODEC_CONTROL::LASTBYTE_POS;
        bool reset = value & CODEC_CONTROL::RESET;
        bool write = value & CODEC_CONTROL::WRITE;
        if (write) {
            if (reg_addr < BURGUNDY_NUM_REGS) {
                uint32_t mask = 0xFFU << (cur_byte * 8);
                this->reg_array[reg_addr] = (this->reg_array[reg_addr] & ~mask) |
                                            (write_byte << (cur_byte * 8));
            }
        } else {
            this->reg_addr = reg_addr;
            this->read_pos = cur_byte;
            this->data_byte = ((reg_addr < BURGUNDY_NUM_REGS ? this->reg_array[reg_addr] : 0) >> (cur_byte * 8)) & 0xFFU;
            this->first_valid = true;

            TimerManager::get_instance()->add_oneshot_timer(
                USECS_TO_NSECS(22), // average is approximately 22.6 µs on a real B&W G3
                [this]() {
                    this->first_valid  = false;
                    this->byte_counter = (this->byte_counter + 1) & 3;
            });
        }
        break;
    }
    case AWAC_FRAME_COUNT:
        this->frame_count = value;
        this->frame_count_start_time = TimerManager::get_instance()->current_time_ns();
        break;
    default:
        LOG_F(ERROR, "%s: write to unsupported register 0x%X", this->name.c_str(),
              offset);
    }
}

static const DeviceDescription Burgundy_Descriptor = {
    BurgundyCodec::create, {}, {}, HWCompType::SND_CODEC
};

REGISTER_DEVICE(BurgundySnd, Burgundy_Descriptor);
