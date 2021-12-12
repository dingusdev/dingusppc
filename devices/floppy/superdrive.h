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

/** @file Macintosh Superdrive definitions. */

#ifndef MAC_SUPERDRIVE_H
#define MAC_SUPERDRIVE_H

#include <cinttypes>

namespace MacSuperdrive {

/** Apple Drive status request addresses. */
enum StatusAddr : uint8_t {
    MFM_Support  = 5,
    Double_Sided = 6,
    Drive_Exists = 7,
    Media_Kind   = 0xF
};

/** Apple Drive command addresses. */
enum CommandAddr : uint8_t {
    Motor_On_Off = 2,
};

/** Type of media currently in the drive. */
enum MediaKind : uint8_t {
    high_density = 0, // 1 or 2 MB disk
    low_density  = 1
};

class MacSuperDrive {
public:
    MacSuperDrive();
    ~MacSuperDrive() = default;

    void command(uint8_t addr, uint8_t value);
    uint8_t status(uint8_t addr);

private:
    uint8_t media_kind;
};

}; // namespace MacSuperdrive

#endif // MAC_SUPERDRIVE_H
