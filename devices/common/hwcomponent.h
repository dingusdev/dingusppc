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

#ifndef HW_COMPONENT_H
#define HW_COMPONENT_H

#include <cinttypes>
#include <string>

/** types of different HW components */
enum HWCompType {
    UNKNOWN     = 0ULL,       /* unknown component type */
    MEM_CTRL    = 1ULL << 0,  /* memory controller */
    ROM         = 1ULL << 2,  /* read-only memory */
    RAM         = 1ULL << 3,  /* random access memory */
    MMIO_DEV    = 1ULL << 4,  /* memory mapped I/O device */
    PCI_HOST    = 1ULL << 5,  /* PCI host   */
    PCI_DEV     = 1ULL << 6,  /* PCI device */
    I2C_HOST    = 1ULL << 8,  /* I2C host   */
    I2C_DEV     = 1ULL << 9,  /* I2C device */
    ADB_HOST    = 1ULL << 12, /* ADB host   */
    ADB_DEV     = 1ULL << 13, /* ADB device */
    INT_CTRL    = 1ULL << 16, /* interrupt controller */
    SCSI_BUS    = 1ULL << 20, /* SCSI bus */
    SCSI_HOST   = 1ULL << 21, /* SCSI host adapter */
    SCSI_DEV    = 1ULL << 22, /* SCSI device */
    SND_SERVER  = 1ULL << 31, /* host sound server */
};

/** Abstract base class for HW components. */
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

#endif /* HW_COMPONENT_H */
