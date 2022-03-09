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

/** @file MESH (Macintosh Enhanced SCSI Hardware) controller definitions. */

#ifndef MESH_H
#define MESH_H

#include <cinttypes>

// Chip ID returned by the MESH cell inside the Heathrow ASIC
#define HeathrowMESHID  4

namespace MeshScsi {

// MESH registers offsets
enum MeshReg : uint8_t {
    XferCount0 = 0,
    XferCount1 = 1,
    FIFO       = 2,
    Sequence   = 3,
    BusStatus0 = 4,
    BusStatus1 = 5,
    FIFOCount  = 6,
    Exception  = 7,
    Error      = 8,
    IntMask    = 9,
    Interrupt  = 0xA,
    SourceID   = 0xB,
    DestID     = 0xC,
    SyncParms  = 0xD,
    MeshID     = 0xE,
    SelTimeOut = 0xF
};

}; // namespace MeshScsi

class MESHController {
public:
    MESHController(uint8_t mesh_id) { this->chip_id = mesh_id; };
    ~MESHController() = default;

    // MESH registers access
    uint8_t read(uint8_t reg_offset);
    void   write(uint8_t reg_offset, uint8_t value);

private:
    uint8_t chip_id; // Chip ID for the MESH controller
};

#endif // MESH_H
