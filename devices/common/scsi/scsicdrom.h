/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file SCSI CD-ROM definitions. */

#ifndef SCSI_CDROM_H
#define SCSI_CDROM_H

#include <devices/common/scsi/scsi.h>
#include <devices/storage/cdromdrive.h>
#include <utils/imgfile.h>

#include <cinttypes>
#include <memory>
#include <string>

class ScsiCdrom : public CdromDrive, public ScsiDevice {
public:
    ScsiCdrom(std::string name, int my_id);
    ~ScsiCdrom() = default;

    virtual void process_command() override;
    virtual bool prepare_data() override;
    virtual bool get_more_data() override;

protected:
    int     test_unit_ready();
    void    read(uint32_t lba, uint16_t nblocks, uint8_t cmd_len);
    uint32_t inquiry(uint8_t *cmd_ptr, uint8_t *data_ptr) override;
    void    mode_select_6(uint8_t param_len);

    void    mode_sense_6();
    void    read_capacity_10();

private:
    bool    eject_allowed = true;
    int     bytes_out = 0;
    uint8_t data_buf[2048] = {};

    //inquiry info
    char vendor_info[9] = "SONY    ";
    char prod_info[17]  = "CD-ROM CDU-8003A";
    char rev_info[5]    = "1.9a";
};

#endif // SCSI_CDROM_H
