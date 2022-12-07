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

/** @file ATA hard disk emulation. */

#include <devices/common/ata/atahd.h>
#include <loguru.hpp>

AtaHardDisk::AtaHardDisk() : AtaBaseDevice("ATA-HD")
{
}

int AtaHardDisk::perform_command()
{
    LOG_F(INFO, "%s: command 0x%x requested", this->name.c_str(), this->r_command);
    return -1;
}
