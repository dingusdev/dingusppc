/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** @file Base class for ATA devices. */

#ifndef ATA_BASE_DEVICE_H
#define ATA_BASE_DEVICE_H

#include <devices/common/ata/atadefs.h>
#include <devices/common/hwcomponent.h>

#include <cinttypes>

class AtaBaseDevice : public HWComponent, public AtaInterface
{
public:
    AtaBaseDevice(const std::string name);
    ~AtaBaseDevice() = default;

    uint16_t read(const uint8_t reg_addr);
    void write(const uint8_t reg_addr, const uint16_t value);

    virtual int perform_command() = 0;

private:
    uint8_t regs[33] = {};
};

#endif // ATA_BASE_DEVICE_H
