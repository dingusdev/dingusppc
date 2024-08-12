/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file CMD646U2 PCI Ultra ATA controller definitions. */

#ifndef CMD646_IDE_H
#define CMD646_IDE_H

#include <devices/common/hwcomponent.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/ata/idechannel.h>
#include <devices/common/pci/pcidevice.h>

#include <memory>

#define DEV_ID_CMD646   0x646
#define MY_DEV_CLASS    0x010180 // mass storage | IDE controller | IDE master
#define MY_REV_ID       7

// Offset for converting addresses of the device control block registers
// defined in the PCI IDE Controller specification, rev. 1.0 3/4/94
// to the addresses used in IdeChannel
#define DEV_CTRL_BLK_OFFSET 0x14

/** CMD646 control/status registers. */
enum {
    ARTTIM0 = 0x53,
    DRWTIM0 = 0x54,
    ARTTIM1 = 0x55,
    DRWTIM1 = 0x56,
};

/** CMD646 bus master registers. */
enum {
    MRDMODE     = 1, // misnomer, contains interrupt control/status bits (CMD646U2 specific)
    UDIDETCR0   = 3, // Ultra DMA timing control register (CMD646U2 specific)
};

/** Bit definitions for the MRDMODE register. */
enum {
    BM_CH0_INT       = 1 << 2,
    BM_CH1_INT       = 1 << 3,
    BM_BLOCK_CH0_INT = 1 << 4,
    BM_BLOCK_CH1_INT = 1 << 5,
};

class CmdIdeCtrl : public PCIDevice {
public:
    CmdIdeCtrl();
    ~CmdIdeCtrl() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<CmdIdeCtrl>(new CmdIdeCtrl());
    }

    // PCIDevice methods
    uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details) override;
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details) override;

    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) override;
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) override;

    int device_postinit() override {
        this->irq_info = this->host_instance->register_pci_int(this);
        return 0;
    };

private:
    void    notify_bar_change(int bar_num);
    uint8_t read_config_reg(uint32_t reg_offset);
    void    write_config_reg(uint32_t reg_offset, uint8_t val);
    uint8_t read_bus_master_reg(const uint8_t reg_offset);
    void    write_bus_master_reg(const uint8_t reg_offset, const uint8_t val);
    void    update_irq(const int ch_num, const uint8_t irq_level);

    // on reset, programming interface defaults to
    // "both channels operating in native mode"
    uint8_t     prog_if = 0x0F;

    uint32_t    io_bases[5] = {};

    IdeChannel *ch0 = nullptr;
    IdeChannel *ch1 = nullptr;

    // unknown default, set it to 2 clocks (60 ns)
    uint8_t     addr_setup_time_0 = 0x40; // address setup time for drive 0
    uint8_t     addr_setup_time_1 = 0x40; // address setup time for drive 1
    uint8_t     data_rw_time_0    = 0x00; // data read/write time for drive 0
    uint8_t     data_rw_time_1    = 0x00; // data read/write time for drive 1

    uint8_t     mrdmode = 0;
    uint8_t     udma_time_cr = 0;

    IntDetails  irq_info = {};
};

#endif // CMD646_IDE_H
