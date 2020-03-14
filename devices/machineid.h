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

#ifndef MACHINE_ID_H
#define MACHINE_ID_H

#include <cinttypes>
#include "hwcomponent.h"
#include "mmiodevice.h"

/**
    @file Contains definitions for PowerMacintosh machine ID registers.

    The machine ID register is a memory-based register containing hardcoded
    values the system software can read to identify machine/board it's running on.

    Register location and value meaning are board-dependent.
 */

/**
    The machine ID for the Gossamer board is accesible at 0xFF000004 (phys).
    It contains a 16-bit value revealing machine's capabilities like bus speed,
    ROM speed, I/O configuration etc.
    Because the meaning of these bits is poorly documented, the code below
    simply return a raw value obtained from real hardware.
 */
class GossamerID : public MMIODevice {
public:
    GossamerID(const uint16_t id) { this->id = id, this->name = "Machine-id"; };
    ~GossamerID() = default;

    bool supports_type(HWCompType type) {
        return type == HWCompType::MMIO_DEV;
    };

    uint32_t read(uint32_t offset, int size) {
         return ((!offset && size == 2) ? this->id : 0); };

    void write(uint32_t offset, uint32_t value, int size) {}; /* not writable */

private:
    uint16_t id;
};

#endif /* MACHINE_ID_H */
