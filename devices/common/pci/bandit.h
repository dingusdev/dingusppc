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

/** Bandit/Chaos ARBus-to-PCI bridge definitions.

    Bandit is a custom ARBus-to-PCI bridge used in the second generation
    of the Power Macintosh computer equipped with the PCI bus.

    Chaos is a custom ARBus-to-PCI bridge that provides a specialized
    PCI-like bus for video called VCI. This 64-bit bus runs at the same
    frequency as the CPU bus (40-50 MHz) and connects video input/output
    devices with the CPU bus.

    Chaos seems to be a Bandit variant without PCI configuration space.
    It's assumed to be present in some PCI Power Macintosh models of the
    first generation.
*/

#ifndef BANDIT_PCI_H
#define BANDIT_PCI_H

#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <memory>
#include <string>

#define BANDIT_DEV          (11)       // Bandit's own device number
#define BANDIT_CAR_TYPE     (1 << 0)   // Bandit config address type bit

/* Convenient macros for parsing CONFIG_ADDR fields. */
#define BUS_NUM()   ((this->config_addr >> 16) & 0xFFU)
#define DEV_NUM()   ((this->config_addr >> 11) & 0x1FU)
#define FUN_NUM()   ((this->config_addr >>  8) & 0x07U)
#define REG_NUM()   ((this->config_addr      ) & 0xFCU)

/** Bandit specific configuration registers. */
enum {
    BANDIT_ADDR_MASK            = 0x48,
    BANDIT_MODE_SELECT          = 0x50,
    BANDIT_ARBUS_RD_HOLD_OFF    = 0x58,
    BANDIT_DELAYED_AACK         = 0x60,
};

/** checks if one bit is set at time, return 0 if not */
#define SINGLE_BIT_SET(val) ((val) && !((val) & ((val)-1)))

/*
    Common functionality for Bandit/Chaos PCI host bridge.
 */
class BanditHost : public PCIHost, public MMIODevice {
public:
    BanditHost(int bridge_num) { this->bridge_num = bridge_num; };

    // PCIHost methods
    virtual void pci_interrupt(uint8_t irq_line_state, PCIBase *dev) {}

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void    write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    int device_postinit();

protected:
    uint32_t    config_addr;
    int         bridge_num;

private:
    void cfg_setup(uint32_t offset, int size, int &bus_num, int &dev_num,
                   int &fun_num, uint8_t &reg_offs, AccessDetails &details,
                   PCIBase *&device);
};

/*
    PCI device for Bandit (but not for Chaos).
 */
class BanditPciDevice : public PCIDevice {
public:
    BanditPciDevice(int bridge_num, std::string name, int dev_id, int rev);
    ~BanditPciDevice() = default;

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

protected:
    void verbose_address_space();

private:
    uint32_t    addr_mask;
    uint32_t    mode_ctrl; // controls various chip modes/features
    uint32_t    rd_hold_off_cnt;
};

/*
    Bandit HLE emulation class.
 */
class Bandit : public BanditHost {
public:
    Bandit(int bridge_num, std::string name, int dev_id=1, int rev=3);
    ~Bandit() = default;

    static std::unique_ptr<HWComponent> create_first() {
        return std::unique_ptr<Bandit>(new Bandit(1, "Bandit-PCI1"));
    };

    static std::unique_ptr<HWComponent> create_second() {
        return std::unique_ptr<Bandit>(new Bandit(2, "Bandit-PCI2"));
    };

    static std::unique_ptr<HWComponent> create_psx_first() {
        return std::unique_ptr<Bandit>(new Bandit(1, "PSX-PCI1", 8, 0));
    };

private:
    uint32_t                    base_addr;
    std::unique_ptr<BanditPciDevice> my_pci_device;
};

/**
    Chaos HLE emulation class.
 */
class Chaos : public BanditHost {
public:
    Chaos(std::string name);
    ~Chaos() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Chaos>(new Chaos("VCI0"));
    };
};

#endif // BANDIT_PCI_H
