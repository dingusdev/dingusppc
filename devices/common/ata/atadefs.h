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

namespace ata_interface {

/** Device IDs according with the Apple ATA zero/one specification. */
enum {
    DEVICE_ID_INVALID   = -1,
    DEVICE_ID_ZERO      = 0,
    DEVICE_ID_ONE       = 1
};

/** Device types. */
enum {
    DEVICE_TYPE_UNKNOWN = -1,
    DEVICE_TYPE_ATA     = 0,
    DEVICE_TYPE_ATAPI   = 1,
};

/** ATA register offsets. */
enum ATA_Reg : int {
    DATA        = 0x0,
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
    TIME_CONFIG = 0x20    // Apple ASIC specific timing configuration
};

/** Status register bits. */
enum ATA_Status : int {
    ERR  = 0x1,
    IDX  = 0x2,
    CORR = 0x4,
    DRQ  = 0x8,
    DSC  = 0x10,
    DWF  = 0x20,
    DRDY = 0x40,
    BSY  = 0x80
};

/** Error register bits. */
enum ATA_Error : int {
    ANMF   = 0x1,   //no address mark
    TK0NF  = 0x2,   //track 0 not found
    ABRT   = 0x4,   //abort command
    MCR    = 0x8,
    IDNF   = 0x10,  //id mark not found
    MC     = 0x20,  //media change request
    UNC    = 0x40,
    BBK    = 0x80  //bad block
};

/** Bit definition for the device control register. */
enum ATA_CTRL : int{
    IEN    = 0x2,
    SRST   = 0x4,
    HOB    = 0x80,
};

/* ATA commands. */
enum ATA_Cmd : int {
    NOP              = 0x00,
    RESET_ATAPI      = 0x08,
    RECALIBRATE      = 0x10,
    READ_SECTOR      = 0x20,
    READ_SECTOR_NR   = 0x21,
    READ_LONG        = 0x22,
    READ_SECTOR_EXT  = 0x24,
    WRITE_SECTOR     = 0x30,
    WRITE_SECTOR_NR  = 0x31,
    WRITE_LONG       = 0x32,
    READ_VERIFY      = 0x40,
    FORMAT_TRACKS    = 0x50,
    IDE_SEEK         = 0x70,
    DIAGNOSTICS      = 0x90,
    INIT_DEV_PARAM   = 0x91,
    PACKET           = 0xA0,
    IDFY_PKT_DEV     = 0xA1,
    READ_MULTIPLE    = 0xC4,
    WRITE_MULTIPLE   = 0xC5,
    READ_DMA         = 0xC8,
    WRITE_DMA        = 0xCA,
    WRITE_BUFFER_DMA = 0xE9,
    READ_BUFFER_DMA  = 0xEB,
    IDENTIFY_DEVICE  = 0xEC,
};

}; // namespace ata_interface

/** Interface for ATA devices. */
class AtaInterface {
public:
    AtaInterface() = default;
    virtual ~AtaInterface() = default;
    virtual uint16_t read(const uint8_t reg_addr) = 0;
    virtual void write(const uint8_t reg_addr, const uint16_t val) = 0;

    virtual int get_device_id() = 0;
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

    // invalid device ID means no real device is present
    int get_device_id() { return ata_interface::DEVICE_ID_INVALID; };
};

#endif // ATA_INTERFACE_H