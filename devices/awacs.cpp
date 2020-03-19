/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

/** AWAC sound device emulation.

    Author: Max Poliakovski 2019-20
*/

#include <thirdparty/loguru.hpp>
#include "awacs.h"
#include "machines/machinebase.h"

AWACDevice::AWACDevice()
{
    this->audio_proc = new AudioProcessor();

    /* register audio processor chip with the I2C bus */
    I2CBus *i2c_bus = dynamic_cast<I2CBus *>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x45, this->audio_proc);
}

AWACDevice::~AWACDevice()
{
    delete this->audio_proc;
}

uint32_t AWACDevice::snd_ctrl_read(uint32_t offset, int size)
{
    switch(offset) {
    case AWAC_SOUND_CTRL_REG:
        return this->snd_ctrl_reg;
    case AWAC_CODEC_CTRL_REG:
        return this->is_busy;
    case AWAC_CODEC_STATUS_REG:
        return (AWAC_AVAILABLE << 8) | (AWAC_MAKER_CRYSTAL << 16) |
               (AWAC_REV_SCREAMER << 20);
        break;
    default:
        LOG_F(ERROR, "AWAC: unsupported register at offset 0x%X", offset);
    }

    return 0;
}

void AWACDevice::snd_ctrl_write(uint32_t offset, uint32_t value, int size)
{
    int subframe, reg_num;
    uint16_t data;

    switch(offset) {
    case AWAC_SOUND_CTRL_REG:
        this->snd_ctrl_reg = value;
        LOG_F(INFO, "New sound control value = 0x%X", this->snd_ctrl_reg);
        break;
    case AWAC_CODEC_CTRL_REG:
        subframe = (value >> 14) & 3;
        reg_num  = (value >> 20) & 7;
        data = ((value >> 8) & 0xF00) | ((value >> 24) & 0xFF);
        LOG_F(INFO, "AWAC subframe = %d, reg = %d, data = %08X\n", subframe, reg_num, data);
        if (!subframe)
            this->control_regs[reg_num] = data;
        break;
    default:
        LOG_F(ERROR, "AWAC: unsupported register at offset 0x%X", offset);
    }
}
