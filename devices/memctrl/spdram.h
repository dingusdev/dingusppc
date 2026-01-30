/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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

/** @file Serial presence detect (SPD) RAM module emulation.

    Author: Max Poliakovski

    @description
    Serial presence detect (SPD) is a standard that prescribes an automatic way
    to supply various information about a memory module.
    A SPD-compatible memory module contains a small EEPROM that stores useful
    information like memory size, configuration, speed, manufacturer etc.
    The content depends on the module type.

    This EEPROM is accessible via the I2C bus. The EEPROM I2C address will be
    configured depending on the physical RAM slot the module is inserted to.

    A 168-Pin SDRAM slot includes three specialized pins SA0-SA2 for setting
    an unique I2C address for each module EEPROM. The SA0-SA2 pins are hardwired
    differently for each RAM slot. Example:

    Slot    SA0-SA2     I2C address
    A       %000        0x50 + %000 = 0x50
    B       %001        0x50 + %001 = 0x51

    Further reading: https://en.wikipedia.org/wiki/Serial_presence_detect
 */

#ifndef SPD_EEPROM_H
#define SPD_EEPROM_H

#include <devices/common/hwcomponent.h>
#include <devices/common/i2c/i2c.h>
#include <loguru.hpp>

#include <cinttypes>
#include <stdexcept>
#include <string>

enum RAMType : int { SDRAM = 4 };


class SpdSdram168 : public I2CDevice {
public:
    SpdSdram168(uint8_t addr) : I2CDevice() {
        this->dev_addr = addr;
        this->pos      = 0;
        supports_types(HWCompType::I2C_DEV | HWCompType::RAM);
    }

    ~SpdSdram168() = default;

    void set_capacity(int capacity_megs) {
        switch (capacity_megs) {
        case 8:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0x6; /* 6  columns */
            this->eeprom_data[5] = 0x1; /* one bank   */
            break;
        case 16:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0x7; /* 7  columns */
            this->eeprom_data[5] = 0x1; /* one bank   */
            break;
        case 32:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0x8; /* 8  columns */
            this->eeprom_data[5] = 0x1; /* one bank   */
            break;
        case 64:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0x9; /* 9  columns */
            this->eeprom_data[5] = 0x1; /* one bank   */
            break;
        case 128:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0xA; /* 10 columns */
            this->eeprom_data[5] = 0x1; /* one bank   */
            break;
        case 256:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0xA; /* 10 columns */
            this->eeprom_data[5] = 0x2; /* two banks  */
            break;
        case 512:
            this->eeprom_data[3] = 0xC; /* 12 rows    */
            this->eeprom_data[4] = 0xB; /* 11 columns */
            this->eeprom_data[5] = 0x2; /* two banks  */
            break;
        default:
            throw std::invalid_argument(std::string("Unsupported capacity!"));
        }
        LOG_F(INFO, "SDRAM capacity set to %dMB, I2C addr = 0x%X", capacity_megs, this->dev_addr);
    }

    void start_transaction() {
        this->pos = 0;
    }

    bool send_subaddress(uint8_t sub_addr) {
        this->pos = sub_addr;
        LOG_F(9, "SDRAM subaddress set to 0x%X", sub_addr);
        return true;
    }

    bool send_byte(uint8_t data) {
        LOG_F(9, "SDRAM byte 0x%X received", data);
        return true;
    }

    bool receive_byte(uint8_t* p_data) {
        if (this->pos >= this->eeprom_data[0]) {
            this->pos = 0; /* attempt to read past SPD data should wrap around */
        }
        LOG_F(9, "SDRAM sending EEPROM byte 0x%X", this->eeprom_data[this->pos]);
        *p_data = this->eeprom_data[this->pos++];
        return true;
    }

private:
    uint8_t dev_addr; /* I2C address */
    int pos;          /* actual read position */

    /* EEPROM content */
    uint8_t eeprom_data[256] = {
        128,            /* number of bytes present */
        8,              /* log2(EEPROM size) */
        RAMType::SDRAM, /* memory type */

        /* the following fields will be set up in set_capacity() */
        0, /* number of row addresses */
        0, /* number of column addresses */
        0  /* number of banks */
    };
};

#endif /* SPD_EEPROM_H */
