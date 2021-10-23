/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** @file I2C bus emulation.

    Author: Max Poliakovski
 */

#ifndef I2C_H
#define I2C_H

#include <stdexcept>
#include <cstring>
#include <string>
#include <loguru.hpp>

/** Base class for I2C devices */
class I2CDevice {
public:
    virtual void start_transaction()               = 0;
    virtual bool send_subaddress(uint8_t sub_addr) = 0;
    virtual bool send_byte(uint8_t data)           = 0;
    virtual bool receive_byte(uint8_t* p_data)     = 0;
};

/** Base class for I2C hosts */
class I2CBus {
public:
    I2CBus() {
        std::memset(this->dev_list, 0, sizeof(this->dev_list));
    };
    ~I2CBus() {
        std::memset(this->dev_list, 0, sizeof(this->dev_list));
    };

    virtual void register_device(uint8_t dev_addr, I2CDevice* dev_obj) {
        if (this->dev_list[dev_addr]) {
            throw std::invalid_argument(std::string("I2C address already taken!"));
        }
        this->dev_list[dev_addr] = dev_obj;
        LOG_F(INFO, "New I2C device, address = 0x%X", dev_addr);
    };

    virtual bool start_transaction(uint8_t dev_addr) {
        if (this->dev_list[dev_addr]) {
            this->dev_list[dev_addr]->start_transaction();
            return true;
        } else {
            return false;
        }
    };

    virtual bool send_subaddress(uint8_t dev_addr, uint8_t sub_addr) {
        if (!this->dev_list[dev_addr]) {
            return false; /* no device -> no acknowledge */
        }
        return this->dev_list[dev_addr]->send_subaddress(sub_addr);
    };

    virtual bool send_byte(uint8_t dev_addr, uint8_t data) {
        if (!this->dev_list[dev_addr]) {
            return false; /* no device -> no acknowledge */
        }
        return this->dev_list[dev_addr]->send_byte(data);
    };

    virtual bool receive_byte(uint8_t dev_addr, uint8_t* p_data) {
        if (!this->dev_list[dev_addr]) {
            return false; /* no device -> no acknowledge */
        }
        return this->dev_list[dev_addr]->receive_byte(p_data);
    };

protected:
    I2CDevice* dev_list[128]; /* list of registered I2C devices */
};

#endif /* I2C_H */
