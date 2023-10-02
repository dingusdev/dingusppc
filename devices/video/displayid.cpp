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

/** @file DisplayID class implementation. */

#include <devices/video/displayid.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <map>
#include <string>

/** Mapping between monitor IDs and their sense codes. */
static const std::map<std::string, uint16_t> MonitorIdToCode = {
    { "MacColor21in"    , 0x00FF },
    { "PortraitGS"      , 0x01FF },
    { "MacRGB12in"      , 0x02FF },
    { "TwoPageGS"       , 0x03FF },
    { "NTSC"            , 0x04FF },
    { "MacRGB15in"      , 0x05FF },
    { "HiRes12-14in"    , 0x06FF },
    { "Multiscan15in"   , 0x0603 },
    { "Multiscan17in"   , 0x060B },
    { "Multiscan20in"   , 0x0623 },
    { "AppleVision1710" , 0x062B }, // this code is assigned to several different monitors!
    { "PALEncoder"      , 0x0700 }, // no clue what it means
    { "NTSCEncoder"     , 0x0714 }, // no clue what it means
    { "VGA-SVGA"        , 0x0717 },
    { "MacRGB16in"      , 0x072D },
    { "MacRGB19in"      , 0x073A },
    { "PAL"             , 0x0730 },
    { "NotConnected"    , 0x07FF }
};

DisplayID::DisplayID()
{
    // assume a DDC monitor is connected by default
    this->id_kind = Disp_Id_Kind::DDC2B;

    std::string mon_id = GET_STR_PROP("mon_id");
    if (!mon_id.empty()) {
        if (MonitorIdToCode.count(mon_id)) {
            auto sense_code = MonitorIdToCode.at(mon_id);
            this->std_sense_code = (sense_code >> 8) & 0xFFU;
            this->ext_sense_code = (sense_code >> 0) & 0xFFU;
            this->id_kind = Disp_Id_Kind::AppleSense;
            LOG_F(INFO, "DisplayID mode set to AppleSense");
            LOG_F(INFO, "Standard sense code: 0x%d", this->std_sense_code);
            LOG_F(INFO, "Extended sense code: 0x%X", this->ext_sense_code);
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
        case 0x23: // Sense line 2 pulled low
            return ((this->ext_sense_code >> 4) & 3);
        case 0x15: // Sense line 1 pulled low
            return (((this->ext_sense_code & 8) >> 1) |
                    ((this->ext_sense_code & 4) >> 2));
        case 0xE: // Sense line 0 pulled low
            return ((this->ext_sense_code & 3) << 1);
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
