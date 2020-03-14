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

/** @file I2C bus emulation.

    Author: Max Poliakovski
 */

#ifndef I2C_H
#define I2C_H

#include <vector>

/** Base class for I2C devices */
class I2CDevice {
public:
    virtual void start_transaction() = 0;
    virtual bool send_byte(uint8_t data) = 0;
    virtual bool receive_byte(uint8_t *p_data) = 0;
};

/** Base class for I2C hosts */
class I2CBus {
public:
    I2CBus()  { this->dev_list.clear(); };
    ~I2CBus() { this->dev_list.clear(); };

    virtual bool register_device(uint8_t dev_addr, I2CDevice *dev_obj) {
        if (!this->dev_list[dev_addr]) {
            return false; /* device address already taken */
        }
        this->dev_list[dev_addr] = dev_obj;
        return true;
    };

    virtual void start_transaction(uint8_t dev_addr) {
        if (this->dev_list[dev_addr]) {
            this->dev_list[dev_addr]->start_transaction();
        }
    };

    virtual bool send_byte(uint8_t dev_addr, uint8_t data) {
        if (!this->dev_list[dev_addr]) {
            return false; /* no device -> no acknowledge */
        }
        return this->dev_list[dev_addr]->send_byte(data);
    };

    virtual bool receive_byte(uint8_t dev_addr, uint8_t *p_data) {
        if (!this->dev_list[dev_addr]) {
            return false; /* no device -> no acknowledge */
        }
        return this->dev_list[dev_addr]->receive_byte(p_data);
    };

protected:
    std::vector<I2CDevice *> dev_list; /* list of registered I2C devices */
};

#endif /* I2C_H */
