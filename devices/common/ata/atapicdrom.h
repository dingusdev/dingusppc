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

/** @file ATAPI CDROM definitions. */

#ifndef ATAPI_CDROM_H
#define ATAPI_CDROM_H

#include <devices/common/ata/atapibasedevice.h>
#include <devices/storage/cdromdrive.h>

#include <string>

class AtapiCdrom : public AtapiBaseDevice, public CdromDrive {
public:
    AtapiCdrom(std::string name);
    ~AtapiCdrom() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AtapiCdrom>(new AtapiCdrom("ATAPI-CDROM"));
    }

    int device_postinit() override;

    void perform_packet_command() override;

    int request_data() override;

    bool data_available() override {
        return this->data_left() != 0;
    }

    void status_good();
    void status_error(uint8_t sense_key, uint8_t asc);

    uint16_t get_data();
private:
    uint8_t sense_key = 0;
    uint8_t asc = 0;
    uint8_t ascq = 0;

    bool        doing_sector_areas = false;
    uint8_t     sector_areas;
    uint32_t    current_block;
    uint16_t    current_block_byte;
};

#endif // ATAPI_CDROM_H
