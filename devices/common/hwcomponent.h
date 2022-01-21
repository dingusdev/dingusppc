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

#include <string>

/** types of different HW components */
enum HWCompType : int {
    UNKNOWN     =   0, /* unknown component type */
    MEM_CTRL    =  10, /* memory controller */
    ROM         =  20, /* read-only memory */
    RAM         =  30, /* random access memory */
    MMIO_DEV    =  40, /* memory mapped I/O device */
    PCI_HOST    =  50, /* PCI host   */
    PCI_DEV     =  51, /* PCI device */
    I2C_HOST    =  60, /* I2C host   */
    I2C_DEV     =  61, /* I2C device */
    ADB_HOST    =  70, /* ADB host   */
    ADB_DEV     =  71, /* ADB device */
    INT_CTRL    =  80, /* interrupt controller */
    SND_SERVER  = 100, /* host sound server */
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

    virtual bool supports_type(HWCompType type) = 0;

    virtual int device_postinit() {
        return 0;
    };

protected:
    std::string name;
};

#endif /* HW_COMPONENT_H */
