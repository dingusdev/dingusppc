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

/** @file IDE Channel (aka IDE port) definitions. */

#ifndef IDE_CHANNEL_H
#define IDE_CHANNEL_H

#include <devices/common/ata/atadefs.h>
#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>
#include <string>

class IdeChannel : public HWComponent
{
public:
    IdeChannel(const std::string name);
    ~IdeChannel() = default;

    static std::unique_ptr<HWComponent> create_first() {
        return std::unique_ptr<IdeChannel>(new IdeChannel("IDEO"));
    }

    static std::unique_ptr<HWComponent> create_second() {
        return std::unique_ptr<IdeChannel>(new IdeChannel("IDE1"));
    }

    uint32_t read(const uint8_t reg_addr, const int size);
    void write(const uint8_t reg_addr, const uint32_t val, const int size);

    void assert_pdiag() {
        this->devices[0]->pdiag_callback();
    };

    bool is_device1_present() {
        return this->devices[1]->get_device_id() != ata_interface::DEVICE_ID_INVALID;
    }

private:
    int         cur_dev = 0;
    uint32_t    ch_config = 0; // timing configuration for this channel

    std::unique_ptr<AtaInterface>   devices[2];
};

#endif // IDE_CHANNEL_H
