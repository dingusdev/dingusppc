/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** Burgundy sound codec emulation. */

#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <devices/sound/burgundy.h>
#include <endianswap.h>
#include <loguru.hpp>

BurgundyCodec::BurgundyCodec(std::string name) : MacioSndCodec(name)
{
    supports_types(HWCompType::SND_CODEC);

    static int burgundy_sample_rates[1] = { 44100 };

    // Burgundy seems to supports only one sample rate
    this->sr_table  = burgundy_sample_rates;
    this->max_sr_id = 1;

    this->set_sample_rate(0); // set default sample rate
}

uint32_t BurgundyCodec::snd_ctrl_read(uint32_t offset, int size) {
    uint32_t result = 0;

    switch (offset) {
    case AWAC_CODEC_CTRL_REG:
        result = this->last_ctrl_data;
        break;
    case AWAC_CODEC_STATUS_REG:
        result = (this->first_valid << 23) | BURGUNDY_READY |
                 (this->byte_counter << 14) | (this->read_pos << 12) |
                 (this->data_byte << 4);
        break;
    default:
        LOG_F(ERROR, "%s: read from unsupported register 0x%X", this->name.c_str(),
              offset);
    }

    return BYTESWAP_32(result);
}

void BurgundyCodec::snd_ctrl_write(uint32_t offset, uint32_t value, int size) {
    uint8_t reg_addr;
    uint8_t cur_byte;
    //uint8_t last_byte;

    value = BYTESWAP_32(value);

    switch (offset) {
    case AWAC_CODEC_CTRL_REG:
        this->last_ctrl_data = value;
        reg_addr  = (value >> 12) & 0xFF;
        cur_byte  = (value >>  8) & 3;
        //last_byte = (value >> 10) & 3;
        if (value & BURGUNDY_REG_WR) {
            uint32_t mask = 0xFFU << (cur_byte * 8);
            this->reg_array[reg_addr] = (this->reg_array[reg_addr] & ~mask) |
                                        ((value & 0xFFU) << (cur_byte * 8));
        } else {
            this->reg_addr  = reg_addr;
            this->read_pos  = cur_byte;
            this->data_byte = (this->reg_array[reg_addr] >> (cur_byte * 8)) & 0xFFU;
            this->first_valid = 1;

            TimerManager::get_instance()->add_oneshot_timer(
                USECS_TO_NSECS(10), // not sure if this is the correct delay
                [this]() {
                    this->first_valid  = 0;
                    this->byte_counter = (this->byte_counter + 1) & 3;
            });
        }
        break;
    default:
        LOG_F(ERROR, "%s: write to unsupported register 0x%X", this->name.c_str(),
              offset);
    }
}

static const DeviceDescription Burgundy_Descriptor = {
    BurgundyCodec::create, {}, {}
};

REGISTER_DEVICE(BurgundySnd, Burgundy_Descriptor);
