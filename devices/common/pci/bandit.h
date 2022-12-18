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

/** Bandit ARBus-to-PCI bridge definitions.

    The Bandit ASIC is a custom ARBus-to-PCI bridge used in the second
    generation of the Power Macintosh computer equipped with the PCI bus.
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
#include <unordered_map>

#define BANDIT_ID_SEL       (1 << 0)   // Bandit's own IDSEL
#define BANDIT_CAR_TYPE     (1 << 0)   // Bandit config address type bit
#define BANDIT_CONFIG_SPACE 0x00800000 // Bandit Config Space bit

/** Bandit specific configuration registers. */
enum {
    BANDIT_ADDR_MASK = 0x48,
};

/** checks if one bit is set at time, return 0 if not */
#define SINGLE_BIT_SET(val) ((val) && !((val) & ((val)-1)))

class Bandit : public PCIHost, public PCIDevice {
public:
    Bandit(int bridge_num, std::string name, int dev_id=1, int rev=3);
    ~Bandit() = default;

    static std::unique_ptr<HWComponent> create_first() {
        return std::unique_ptr<Bandit>(new Bandit(1, "Bandit-PCI1"));
    };

    static std::unique_ptr<HWComponent> create_psx_first() {
        return std::unique_ptr<Bandit>(new Bandit(1, "PSX-PCI1", 8, 0));
    };

    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

protected:
    void verbose_address_space();

private:
    uint32_t    base_addr;
    uint32_t    config_addr;
    uint32_t    addr_mask;
};

/** Chaos is a custom ARBus-to-PCI bridge that provides a specialized
    PCI-like bus for video called VCI. This 64-bit bus runs at the same
    frequency as the CPU bus (40-50 MHz) and provides an interface
    between video input/output devices and the CPU bus.
 */
class Chaos : public PCIHost, public MMIODevice {
public:
    Chaos(std::string name);
    ~Chaos() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<Chaos>(new Chaos("VCI0"));
    };

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

private:
    std::string name;
    uint32_t    config_addr;
};

#endif // BANDIT_PCI_H
