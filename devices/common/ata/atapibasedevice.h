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

/** @file Base class for ATAPI devices. */

#ifndef ATAPI_BASE_DEVICE_H
#define ATAPI_BASE_DEVICE_H

#include <devices/common/ata/atabasedevice.h>

#include <cinttypes>
#include <string>

class AtapiBaseDevice : public AtaBaseDevice
{
public:
    AtapiBaseDevice(const std::string name);
    ~AtapiBaseDevice() = default;

    void device_set_signature() override;

    uint16_t read(const uint8_t reg_addr) override;
    void write(const uint8_t reg_addr, const uint16_t value) override;

    // methods to be implemented in the particular device
    int  perform_command() override;
    virtual void perform_packet_command() = 0;
    virtual int request_data() = 0;
    virtual bool data_available() = 0;

    // methods with default implementation
    virtual void data_out_phase();
    virtual void present_status();

protected:
    uint8_t     r_int_reason;
    uint16_t    r_byte_count;
    bool        status_expected = false;

    alignas(uint16_t)
    uint8_t     cmd_pkt[12] = {};
};

#endif // ATAPI_BASE_DEVICE_H
