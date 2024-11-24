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
    DATA        = 0x00, // 16-bit data (read & write)
    ERROR       = 0x01, // error (read)
    FEATURES    = 0x01, // features (write)
    SEC_COUNT   = 0x02, // sector count
    SEC_NUM     = 0x03, // sector number
    CYL_LOW     = 0x04, // cylinder low
    CYL_HIGH    = 0x05, // cylinder high
    DEVICE_HEAD = 0x06, // device/head
    STATUS      = 0x07, // status (read)
    COMMAND     = 0x07, // command (write)
    ALT_STATUS  = 0x16, // alt status (read)
    DEV_CTRL    = 0x16, // device control (write)
    TIME_CONFIG = 0x20  // Apple ASIC specific timing configuration
};

/** ATAPI specific register offsets. */
enum ATAPI_Reg : int {
    INT_REASON      = 0x2,  // interrupt reason (read-only)
    BYTE_COUNT_LO   = 0x4,  // byte count (bits 0-7)
    BYTE_COUNT_HI   = 0x5,  // byte count (bits 8-15)
};

/** ATAPI Interrupt Reason bits. */
enum ATAPI_Int_Reason : uint8_t {
    CoD         = 1 << 0,
    IO          = 1 << 1,
    RELEASE     = 1 << 2,
};

/** ATAPI Features bits. */
enum ATAPI_Features : uint8_t {
    DMA     = 1 << 0,
    OVERLAP = 1 << 1
};

/** Device/Head register bits. */
enum ATA_Dev_Head : uint8_t {
    LBA     = 1 << 6,
};

/** Status register bits. */
enum ATA_Status : uint8_t {
    ERR  = 0x01,
    IDX  = 0x02,
    CORR = 0x04,
    DRQ  = 0x08,
    DSC  = 0x10,
    DWF  = 0x20,
    DRDY = 0x40,
    BSY  = 0x80
};

/** Error register bits. */
enum ATA_Error : uint8_t {
    ANMF   = 0x01,  //no address mark
    TK0NF  = 0x02,  //track 0 not found
    ABRT   = 0x04,  //abort command
    MCR    = 0x08,
    IDNF   = 0x10,  //id mark not found
    MC     = 0x20,  //media change request
    UNC    = 0x40,
    BBK    = 0x80,  //bad block
};

/** Bit definition for the device control register. */
enum ATA_CTRL : uint8_t {
    IEN    = 0x02,
    SRST   = 0x04,
    HOB    = 0x80,
};

/* ATA commands. */
enum ATA_Cmd : uint8_t {
    NOP                                  = 0x00,
    ATAPI_SOFT_RESET                     = 0x08,
    RECALIBRATE                          = 0x10,
    ATAPI_COMMAND_MODE_SENSE6            = 0x1A,
    READ_SECTOR                          = 0x20,
    READ_SECTOR_NR                       = 0x21,
    READ_LONG                            = 0x22,
    READ_SECTOR_EXT                      = 0x24,
    WRITE_SECTOR                         = 0x30,
    WRITE_SECTOR_NR                      = 0x31,
    WRITE_LONG                           = 0x32,
    READ_VERIFY                          = 0x40,
    ATAPI_COMMAND_GET_CONFIG             = 0x46,
    FORMAT_TRACKS                        = 0x50,
    IDE_SEEK                             = 0x70,
    DIAGNOSTICS                          = 0x90,
    INIT_DEV_PARAM                       = 0x91,
    ATAPI_PACKET                         = 0xA0,
    ATAPI_IDFY_DEV                       = 0xA1,
    ATAPI_SERVICE                        = 0xA2,
    ATAPI_READ_DVD_STRUCTURE             = 0xAD,
    READ_MULTIPLE                        = 0xC4,
    WRITE_MULTIPLE                       = 0xC5,
    SET_MULTIPLE_MODE                    = 0xC6,
    READ_DMA                             = 0xC8,
    WRITE_DMA                            = 0xCA,
    STANDBY_IMMEDIATE_E0                 = 0xE0,
    FLUSH_CACHE                          = 0xE7, // ATA-5
    WRITE_BUFFER_DMA                     = 0xE9,
    READ_BUFFER_DMA                      = 0xEB,
    IDENTIFY_DEVICE                      = 0xEC,
    SET_FEATURES                         = 0xEF,
};

}; // namespace ata_interface

/** Interface for ATA devices. */
class AtaInterface {
public:
    AtaInterface() = default;
    virtual ~AtaInterface() = default;
    virtual uint16_t read(const uint8_t reg_addr) = 0;
    virtual void write(const uint8_t reg_addr, const uint16_t val) = 0;

    virtual int  get_device_id() = 0;
    virtual void pdiag_callback() {};
};

/** Dummy ATA device. */
class AtaNullDevice : public AtaInterface {
public:
    AtaNullDevice() = default;
    ~AtaNullDevice() = default;

    uint16_t read(const uint8_t reg_addr) override {
        // return all one's except DD7 if no device is present
        // DD7 corresponds to the BSY bit of the status register
        // The host should have a pull-down resistor on DD7
        // to prevent the software from waiting for a long time
        // for empty slots
        return 0xFF7FU;
    };

    void write(const uint8_t reg_addr, const uint16_t val) override {};

    // invalid device ID means no real device is present
    int get_device_id() override { return ata_interface::DEVICE_ID_INVALID; };
};

#endif // ATA_INTERFACE_H
