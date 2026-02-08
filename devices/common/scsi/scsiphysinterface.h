/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#ifndef SCSI_PHYS_INTERFACE_H
#define SCSI_PHYS_INTERFACE_H

#include <cinttypes>

/** Physical layer IDs. */
enum : int {
    PHY_ID_UNKNOWN  = 0,
    PHY_ID_SCSI     = 1, // parallel SCSI
    PHY_ID_ATAPI    = 2, // parallel ATA with packet interface
};

/** Interface for the physical layer of a SCSI/ATAPI device. */
class ScsiPhysInterface {
public:
    ScsiPhysInterface(int id = PHY_ID_UNKNOWN) { this->phy_id = id; }
    ~ScsiPhysInterface() = default;

    virtual int get_phy_id() { return this->phy_id; };

    virtual bool    is_device_ready() { return true; }
    virtual uint8_t not_ready_reason() { return 0; }
    virtual bool    last_sel_has_attention() { return false; }
    virtual uint8_t last_sel_msg() { return 0; }
    virtual void    set_xfer_len(uint64_t len) = 0;
    virtual void    set_status(uint8_t status_code) = 0;
    virtual void    switch_phase(const int new_phase) = 0;

protected:
    int phy_id = PHY_ID_UNKNOWN;
};

#endif // SCSI_PHYS_INTERFACE_H
