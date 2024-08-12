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

/** @file CMD646U2 PCI Ultra ATA controller emulation. */

#include <devices/common/ata/cmd646.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

CmdIdeCtrl::CmdIdeCtrl() : PCIDevice("cmd-ide") {
    this->supports_types(HWCompType::PCI_DEV | HWCompType::IDE_HOST);

    // set up PCI configuration space header
    this->vendor_id = this->subsys_vndr = PCI_VENDOR_SILICON_IMAGE;
    this->device_id = this->subsys_id   = DEV_ID_CMD646;
    this->class_rev = ((MY_DEV_CLASS | this->prog_if) << 8) | MY_REV_ID;
    this->max_lat   = 4;
    this->min_gnt   = 2;
    this->irq_pin   = 1;

    this->bars_cfg[0] = 0xFFFFFFF9; // Command block I/O space, primary channel
    this->bars_cfg[1] = 0xFFFFFFFD; // Control block I/O space, primary channel
    this->bars_cfg[2] = 0xFFFFFFF9; // Command block I/O space, secondary channel
    this->bars_cfg[3] = 0xFFFFFFFD; // Control block I/O space, secondary channel
    this->bars_cfg[4] = 0xFFFFFFF1; // Bus master I/O space, both channels

    this->finish_config_bars();

    this->pci_notify_bar_change = [this](int bar_num) {
        this->notify_bar_change(bar_num);
    };

    gMachineObj->add_device("CmdAta0", std::unique_ptr<IdeChannel>(new IdeChannel("CmdAta0")));
    gMachineObj->add_device("CmdAta1", std::unique_ptr<IdeChannel>(new IdeChannel("CmdAta1")));

    this->ch0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("CmdAta0"));
    this->ch1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("CmdAta1"));

    this->ch0->set_irq_callback([this](const uint8_t intrq_state) {
        LOG_F(INFO, "CmdAta0 INTRQ updated to %d", intrq_state);
        this->update_irq(0, intrq_state);
    });

    this->ch1->set_irq_callback([this](const uint8_t intrq_state) {
        LOG_F(INFO, "CmdAta1 INTRQ updated to %d", intrq_state);
        this->update_irq(1, intrq_state);
    });
}

uint32_t CmdIdeCtrl::pci_cfg_read(uint32_t reg_offs, AccessDetails &details) {
    if (reg_offs < 64)
        return PCIDevice::pci_cfg_read(reg_offs, details);

    if (details.size != 1)
        ABORT_F("%s: non-byte read from PCI config reg 0x%X", this->name.c_str(),
                reg_offs + details.offset);

    if (reg_offs < 112)
        return this->read_config_reg(reg_offs + details.offset);

    LOG_F(WARNING, "%s: reading config reg at 0x%X", this->name.c_str(), reg_offs);

    return 0;
}

void CmdIdeCtrl::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details) {
    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }

    if (details.size != 1)
        ABORT_F("%s: non-byte write to PCI config reg 0x%X", this->name.c_str(),
                reg_offs + details.offset);

    if (reg_offs < 112)
        this->write_config_reg(reg_offs + details.offset, value);
    else
        LOG_F(WARNING, "%s: writing config reg at 0x%X", this->name.c_str(), reg_offs);
}

void CmdIdeCtrl::notify_bar_change(int bar_num) {
    if (bar_num >= 0 && bar_num <= 4)
        this->io_bases[bar_num] = this->bars[bar_num] & ~3;
}

bool CmdIdeCtrl::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    *res = 0;

    if ((offset & ~7) == this->io_bases[0]) {
        *res = this->ch0->read(offset & 7, size);
    } else if ((offset & ~7) == this->io_bases[2]) {
        *res = this->ch1->read(offset & 7, size);
    } else if ((offset & ~3) == this->io_bases[1]) {
        *res = this->ch0->read((offset & 3) + DEV_CTRL_BLK_OFFSET, size);
    } else if ((offset & ~3) == this->io_bases[3]) {
        *res = this->ch1->read((offset & 3) + DEV_CTRL_BLK_OFFSET, size);
    } else if ((offset & ~0xF) == this->io_bases[4]) {
        *res = this->read_bus_master_reg(offset & 0xF);
    } else {
        *res = 0xFFFFFFFFUL;
        return false;
    }

    return true;
}

bool CmdIdeCtrl::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    if ((offset & ~7) == this->io_bases[0]) {
        this->ch0->write(offset & 7, value, size);
    } else if ((offset & ~7) == this->io_bases[2]) {
        this->ch1->write(offset & 7, value, size);
    } else if ((offset & ~3) == this->io_bases[1]) {
        this->ch0->write((offset & 3) + DEV_CTRL_BLK_OFFSET, value, size);
    } else if ((offset & ~3) == this->io_bases[3]) {
        this->ch1->write((offset & 3) + DEV_CTRL_BLK_OFFSET, value, size);
    } else if ((offset & ~0xF) == this->io_bases[4]) {
        this->write_bus_master_reg(offset & 0xF, value & 0xFFU);
    } else
        return false;

    return true;
};

uint8_t CmdIdeCtrl::read_config_reg(uint32_t reg_offset) {
    switch(reg_offset) {
    case ARTTIM0:
        return this->addr_setup_time_0;
    case DRWTIM0:
        return this->data_rw_time_0;
    case ARTTIM1:
        return this->addr_setup_time_1;
    case DRWTIM1:
        return this->data_rw_time_1;
    default:
        LOG_F(ERROR, "%s: unimplemented config reg at 0x%X", this->name.c_str(),
              reg_offset);
    }

    return 0;
}

void CmdIdeCtrl::write_config_reg(uint32_t reg_offset, uint8_t val) {
    switch(reg_offset) {
    case ARTTIM0:
        this->addr_setup_time_0 = val;
        LOG_F(9, "%s: ARTTIM0 set to 0x%X", this->name.c_str(), val);
        break;
    case DRWTIM0:
        this->data_rw_time_0 = val;
        LOG_F(9, "%s: DRWTIM0 set to 0x%X", this->name.c_str(), val);
        break;
    case ARTTIM1:
        this->addr_setup_time_1 = val;
        LOG_F(9, "%s: ARTTIM1 set to 0x%X", this->name.c_str(), val);
        break;
    case DRWTIM1:
        this->data_rw_time_1 = val;
        LOG_F(9, "%s: DRWTIM1 set to 0x%X", this->name.c_str(), val);
        break;
    default:
        LOG_F(ERROR, "%s: unimplemented config reg at 0x%X", this->name.c_str(),
              reg_offset);
    }
}

uint8_t CmdIdeCtrl::read_bus_master_reg(const uint8_t reg_offset) {
    switch(reg_offset) {
    case MRDMODE:
        return this->mrdmode;
    default:
        LOG_F(ERROR, "%s: unimplemented bus master reg at 0x%X", this->name.c_str(),
              reg_offset);
    }

    return 0;
}

void CmdIdeCtrl::write_bus_master_reg(const uint8_t reg_offset, const uint8_t val) {
    switch(reg_offset) {
    case MRDMODE:
        if (val & BM_CH0_INT)
            this->mrdmode &= ~BM_CH0_INT;
        if (val & BM_CH1_INT)
            this->mrdmode &= ~BM_CH1_INT;
        this->mrdmode = (this->mrdmode & ~(BM_BLOCK_CH0_INT | BM_BLOCK_CH1_INT)) |
                        (val & (BM_BLOCK_CH0_INT | BM_BLOCK_CH1_INT));
        LOG_F(INFO, "%s: MRDMODE set to 0x%X", this->name.c_str(), val);
        if ((this->mrdmode & BM_CH0_INT) && !(this->mrdmode & BM_BLOCK_CH0_INT))
            this->update_irq(0, 1);
        if ((this->mrdmode & BM_CH1_INT) && !(this->mrdmode & BM_BLOCK_CH1_INT))
            this->update_irq(1, 1);
        break;
    case UDIDETCR0:
        this->udma_time_cr = val;
        break;
    default:
        LOG_F(ERROR, "%s: unimplemented bus master reg at 0x%X", this->name.c_str(),
              reg_offset);
    }
}

void CmdIdeCtrl::update_irq(const int ch_num, const uint8_t irq_level) {
    bool forward_irq = !(this->mrdmode & (ch_num ? BM_BLOCK_CH1_INT : BM_BLOCK_CH0_INT));

    if (irq_level)
        this->mrdmode |= ch_num ? BM_CH1_INT : BM_CH0_INT;
    else
        this->mrdmode &= ~(ch_num ? BM_CH1_INT : BM_CH0_INT);

    if (!irq_level || forward_irq)
        this->irq_info.int_ctrl_obj->ack_int(this->irq_info.irq_id, irq_level);
}

static const DeviceDescription CmdIde_Descriptor = {
    CmdIdeCtrl::create, {}, {}
};

REGISTER_DEVICE(CmdAta, CmdIde_Descriptor);
