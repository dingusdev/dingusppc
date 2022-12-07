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

/** @file ATA interface definitions. */

#ifndef ATA_INTERFACE_H
#define ATA_INTERFACE_H

#include <cinttypes>

/** IDE register offsets. */
enum IDE_Reg : int {
    IDE_DATA    = 0x0,
    ERROR       = 0x1,    // error (read)
    FEATURES    = 0x1,    // features (write)
    SEC_COUNT   = 0x2,    // sector count
    SEC_NUM     = 0x3,    // sector number
    CYL_LOW     = 0x4,    // cylinder low
    CYL_HIGH    = 0x5,    // cylinder high
    DEVICE_HEAD = 0x6,    // device/head
    STATUS      = 0x7,    // status (read)
    COMMAND     = 0x7,    // command (write)
    ALT_STATUS  = 0x16,   // alt status (read)
    DEV_CTRL    = 0x16,   // device control (write)
    TIME_CONFIG = 0x20
};

/** Status Register Bits */
enum IDE_Status : int {
    ERR  = 0x1,
    IDX  = 0x2,
    CORR = 0x4,
    DRQ  = 0x8,
    DSC  = 0x10,
    DWF  = 0x20,
    DRDY = 0x40,
    BSY  = 0x80
};

/** Error Register Bits */
enum IDE_Error : int {
    ANMF   = 0x1,
    TK0NF  = 0x2,
    ABRT   = 0x4,
    MCR    = 0x8,
    IDNF   = 0x10,
    MC     = 0x20,
    UNC    = 0x40,
    BBK    = 0x80
};

/** Heath IDE commands. */
enum IDE_Cmd : int {
    IDE_NOP         = 0x00,
    RESET_ATAPI     = 0x08,
    RECALIBRATE     = 0x10,
    READ_SECTOR     = 0x20,
    READ_LONG       = 0x22,
    WRITE_SECTOR    = 0x30,
    WRITE_LONG      = 0x32,
    WRITE_VERIFY    = 0x40,
    FORMAT_TRACKS   = 0x50,
    DIAGNOSTICS     = 0x90,
    READ_DMA        = 0xC8,
    WRITE_DMA       = 0xCA,
};

/** Interface for ATA devices. */
class AtaInterface {
public:
    AtaInterface() = default;
    virtual ~AtaInterface() = default;
    virtual uint16_t read(const uint8_t reg_addr) = 0;
    virtual void write(const uint8_t reg_addr, const uint16_t val) = 0;
};

/** Dummy ATA device. */
class AtaNullDevice : public AtaInterface {
public:
    AtaNullDevice() = default;
    ~AtaNullDevice() = default;

    uint16_t read(const uint8_t reg_addr) {
        // return all one's except DD7 if no device is present
        // DD7 corresponds to the BSY bit of the status register
        // The host should have a pull-down resistor on DD7
        // to prevent the software from waiting for a long time
        // for empty slots
        return 0xFF7FU;
    };

    void write(const uint8_t reg_addr, const uint16_t val) {};
};

#endif // ATA_INTERFACE_H
