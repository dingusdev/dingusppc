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

/** @file ATA hard drive support */

#ifndef ATA_HD_H
#define ATA_HD_H

#include <cinttypes>
#include <stdio.h>
#include <string>

#define SEC_SIZE 512

/** Heath IDE register offsets. */
enum IDE_Reg : int {
    IDE_DATA    = 0x0,
    ERROR       = 0x1,    // error (read)
    FEATURES    = 0x1,    // features (write)
    SEC_COUNT   = 0x2,    // sector count
    SEC_NUM     = 0x3,    // sector number
    CYL_LOW     = 0x4,    // cylinder low
    CYL_HIGH    = 0x5,    // cylinder high
    DRIVE_HEAD  = 0x6,    // drive/head
    STATUS      = 0x7,    // status (read)
    CMD         = 0x7,    // command (write)
    ALT_STATUS  = 0x16,   // alt status (read)
    DEV_CTRL    = 0x16,   // device control (write)
    TIME_CONFIG = 0x20,
};

enum IDE_Status { 
    ERR  = 0x1, 
    IDX  = 0x2,
    CORR = 0x4,
    DRQ  = 0x8,
    DSC  = 0x10,
    DWF  = 0x20,
    DRDY = 0x40,
    BSY  = 0x80
};

/** Heath IDE commands. */
enum IDE_Cmd {
    RESET_ATAPI     = 0x08,
    RECALIBRATE     = 0x10,
    READ_SECTOR     = 0x20,
    WRITE_SECTOR    = 0x30,
    FORMAT_TRACKS   = 0x50,
    DIAGNOSTICS     = 0x90,
};

class HeathIDE : public HWComponent {
public:
    HeathIDE(std::string filename);
    uint32_t read(int reg);
    void write(int reg, uint32_t value);

    void perform_command(uint32_t command);

private:
    uint32_t regs[33];
    uint8_t buffer[SEC_SIZE];
};

#endif