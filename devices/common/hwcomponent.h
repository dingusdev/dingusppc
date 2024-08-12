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

#ifndef HW_COMPONENT_H
#define HW_COMPONENT_H

#include <cinttypes>
#include <string>

/** types of different HW components */
enum HWCompType : uint64_t {
    UNKNOWN     = 0ULL,       // unknown component type
    MEM_CTRL    = 1ULL << 0,  // memory controller
    NVRAM       = 1ULL << 1,  // non-volatile random access memory
    ROM         = 1ULL << 2,  // read-only memory
    RAM         = 1ULL << 3,  // random access memory
    MMIO_DEV    = 1ULL << 4,  // memory mapped I/O device
    PCI_HOST    = 1ULL << 5,  // PCI host
    PCI_DEV     = 1ULL << 6,  // PCI device
    I2C_HOST    = 1ULL << 8,  // I2C host
    I2C_DEV     = 1ULL << 9,  // I2C device
    ADB_HOST    = 1ULL << 12, // ADB host
    ADB_DEV     = 1ULL << 13, // ADB device
    IOBUS_HOST  = 1ULL << 14, // IOBus host
    IOBUS_DEV   = 1ULL << 15, // IOBus device
    INT_CTRL    = 1ULL << 16, // interrupt controller
    SCSI_BUS    = 1ULL << 20, // SCSI bus
    SCSI_HOST   = 1ULL << 21, // SCSI host adapter
    SCSI_DEV    = 1ULL << 22, // SCSI device
    IDE_BUS     = 1ULL << 23, // IDE bus
    IDE_HOST    = 1ULL << 24, // IDE host controller
    IDE_DEV     = 1ULL << 25, // IDE device
    SND_CODEC   = 1ULL << 30, // sound codec
    SND_SERVER  = 1ULL << 31, // host sound server
    FLOPPY_CTRL = 1ULL << 32, // floppy disk controller
    FLOPPY_DRV  = 1ULL << 33, // floppy disk drive
    ETHER_MAC   = 1ULL << 40, // Ethernet media access controller
};

/** Base class for HW components. */
class HWComponent {
public:
    HWComponent()          = default;
    virtual ~HWComponent() = default;

    virtual std::string get_name(void) {
        return this->name;
    };
    virtual void set_name(std::string name) {
        this->name = name;
    };

    virtual bool supports_type(HWCompType type) {
        return !!(this->supported_types & type);
    };

    virtual void supports_types(uint64_t types) {
        this->supported_types = types;
    };

    virtual int device_postinit() {
        return 0;
    };

protected:
    std::string name;
    uint64_t    supported_types = HWCompType::UNKNOWN;
};

#endif // HW_COMPONENT_H
