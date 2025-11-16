/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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

/** @file MacIO 2nd generation I/O controllers emulation. */

#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

MacIoTwo::MacIoTwo(std::string name, uint16_t dev_id) : MacIoBase(name, dev_id) {
    // NVRAM connection
    this->nvram = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));

    // connect SCSI controller cell and its DMA channel
    this->mesh = dynamic_cast<MeshController*>(gMachineObj->get_comp_by_type(HWCompType::SCSI_HOST));
    this->mesh_dma = std::unique_ptr<DMAChannel> (new DMAChannel("mesh"));
    this->mesh_dma->register_dma_int(this, this->register_dma_int(IntSrc::DMA_SCSI_MESH));
    this->mesh_dma->connect(this->mesh);
    this->mesh->connect(this->mesh_dma.get());

    // connect IDE HW
    this->ide_0 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide0"));
    this->ide_1 = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name("Ide1"));
    this->ide0_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Ide0-Dma"));
    this->ide1_dma = std::unique_ptr<DMAChannel> (new DMAChannel("Ide1-Dma"));

    // connect Ethernet HW (Heathrow and Paddington)
    if (this->device_id != MIO_DEV_ID_OHARE) {
        this->bmac = dynamic_cast<BigMac*>(gMachineObj->get_comp_by_type(HWCompType::ETHER_MAC));
        this->enet_xmit_dma = std::unique_ptr<DMAChannel> (new DMAChannel("BmacTx"));
        this->enet_rcv_dma  = std::unique_ptr<DMAChannel> (new DMAChannel("BmacRx"));
    }

    // set EMMO status (active low)
    this->emmo = GET_BIN_PROP("emmo") ^ 1;
}

uint32_t MacIoTwo::read(uint32_t rgn_start, uint32_t offset, int size) {
    uint32_t value;

    LOG_F(9, "%s: read @%x.%c", this->get_name().c_str(),
        offset, SIZE_ARG(size));

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        value = mio_ctrl_read(offset, size);
        break;
    case 8:
        value = dma_read(offset & 0x7FFF, size);
        break;
    case 0x10: // SCSI
        value = this->mesh->read((offset >> 4) & 0xF);
        break;
    case 0x11: // Ethernet
        value = BYTESWAP_SIZED(this->bmac->read(offset & 0xFFFU), size);
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            value = this->escc->read(compat_to_macrisc[(offset >> 1) & 0xF]);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            value = 0;
            LOG_F(ERROR, "%s: ESCC compatible read  @%x.%c", this->name.c_str(),
                offset, SIZE_ARG(size));
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        value = this->escc->read((offset >> 4) & 0xF);
        break;
    case 0x14:
        value = this->snd_codec->snd_ctrl_read(offset & 0xFF, size);
        break;
    case 0x15: // SWIM3
        value = this->swim3->read((offset >> 4) & 0xF);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        value = this->viacuda->read((offset >> 9) & 0xF);
        break;
    case 0x20: // IDE 0
        value = this->ide_0->read((offset >> 4) & 0x1F, size);
        break;
    case 0x21: // IDE 1
        value = this->ide_1->read((offset >> 4) & 0x1F, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            value = this->nvram->read_byte((offset >> 4) & 0x1FFF);
        } else {
            value = 0;
            LOG_F(WARNING, "%s: read @%x.%c", this->get_name().c_str(),
                offset, SIZE_ARG(size));
        }
    }

    return value;
}

void MacIoTwo::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) {
    LOG_F(9, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
        offset, SIZE_ARG(size), size * 2, value);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        this->mio_ctrl_write(offset, value, size);
        break;
    case 8:
        this->dma_write(offset & 0x7FFF, value, size);
        break;
    case 0x10: // SCSI
        this->mesh->write((offset >> 4) & 0xF, value);
        break;
    case 0x11: // Ethernet
        this->bmac->write(offset & 0xFFFU, BYTESWAP_SIZED(value, size));
        break;
    case 0x12: // ESCC compatible addressing
        if ((offset & 0xFF) < 0x0C) {
            this->escc->write(compat_to_macrisc[(offset >> 1) & 0xF], value);
            break;
        }
        if ((offset & 0xFF) < 0x60) {
            LOG_F(ERROR, "%s: SCC write @%x.%c = %0*x", this->name.c_str(),
                offset, SIZE_ARG(size), size * 2, value);
            break;
        }
        // fallthrough
    case 0x13: // ESCC MacRISC addressing
        this->escc->write((offset >> 4) & 0xF, value);
        break;
    case 0x14:
        this->snd_codec->snd_ctrl_write(offset & 0xFF, value, size);
        break;
    case 0x15: // SWIM3
        this->swim3->write((offset >> 4) & 0xF, value);
        break;
    case 0x16: // VIA-CUDA
    case 0x17:
        this->viacuda->write((offset >> 9) & 0xF, value);
        break;
    case 0x20: // IDE 0
        this->ide_0->write((offset >> 4) & 0x1F, value, size);
        break;
    case 0x21: // IDE 1
        this->ide_1->write((offset >> 4) & 0x1F, value, size);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        } else {
            LOG_F(WARNING, "%s: write @%x.%c = %0*x", this->get_name().c_str(),
                offset, SIZE_ARG(size), size * 2, value);
        }
    }
}

static const char *get_name_dma(unsigned dma_channel) {
    switch (dma_channel) {
        case MIO_OHARE_DMA_MESH        : return "DMA_MESH"       ;
        case MIO_OHARE_DMA_FLOPPY      : return "DMA_FLOPPY"     ;
        case MIO_OHARE_DMA_ETH_XMIT    : return "DMA_ETH_XMIT"   ;
        case MIO_OHARE_DMA_ETH_RCV     : return "DMA_ETH_RCV"    ;
        case MIO_OHARE_DMA_ESCC_A_XMIT : return "DMA_ESCC_A_XMIT";
        case MIO_OHARE_DMA_ESCC_A_RCV  : return "DMA_ESCC_A_RCV" ;
        case MIO_OHARE_DMA_ESCC_B_XMIT : return "DMA_ESCC_B_XMIT";
        case MIO_OHARE_DMA_ESCC_B_RCV  : return "DMA_ESCC_B_RCV" ;
        case MIO_OHARE_DMA_AUDIO_OUT   : return "DMA_AUDIO_OUT"  ;
        case MIO_OHARE_DMA_AUDIO_IN    : return "DMA_AUDIO_IN"   ;
        case MIO_OHARE_DMA_IDE0        : return "DMA_IDE0"       ;
        case MIO_OHARE_DMA_IDE1        : return "DMA_IDE1"       ;
        default                        : return "unknown"        ;
    }
}

uint32_t MacIoTwo::dma_read(uint32_t offset, int size) {
    int dma_channel = offset >> 8;
    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        return this->mesh_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_FLOPPY:
        return this->floppy_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ETH_XMIT:
        return this->enet_xmit_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ETH_RCV:
        return this->enet_rcv_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_ESCC_B_RCV:
        return 0;
        //return this->escc_b_rx_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_AUDIO_OUT:
        return this->snd_out_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_IDE0:
        return this->ide0_dma->reg_read(offset & 0xFF, size);
    case MIO_OHARE_DMA_IDE1:
        return this->ide1_dma->reg_read(offset & 0xFF, size);
    default:
        if (!(unsupported_dma_channel_read & (1 << dma_channel))) {
            unsupported_dma_channel_read |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s read  @%02x.%c",
                this->name.c_str(), dma_channel, get_name_dma(dma_channel),
                offset & 0xFF, SIZE_ARG(size));
        }
    }

    return 0;
}

void MacIoTwo::dma_write(uint32_t offset, uint32_t value, int size) {
    int dma_channel = offset >> 8;
    switch (dma_channel) {
    case MIO_OHARE_DMA_MESH:
        this->mesh_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_FLOPPY:
        this->floppy_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_XMIT:
        this->enet_xmit_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ETH_RCV:
        this->enet_rcv_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_ESCC_B_RCV:
        this->escc_b_rx_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_AUDIO_OUT:
        this->snd_out_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_IDE0:
        this->ide0_dma->reg_write(offset & 0xFF, value, size);
        break;
    case MIO_OHARE_DMA_IDE1:
        this->ide1_dma->reg_write(offset & 0xFF, value, size);
        break;
    default:
        if (!(unsupported_dma_channel_write & (1 << dma_channel))) {
            unsupported_dma_channel_write |= (1 << dma_channel);
            LOG_F(WARNING, "%s: Unsupported DMA channel %d %s write @%02x.%c = %0*x",
                this->name.c_str(), dma_channel, get_name_dma(dma_channel),
                offset & 0xFF, SIZE_ARG(size), size * 2, value);
        }
    }
}

uint32_t MacIoTwo::mio_ctrl_read(uint32_t offset, int size) {
    uint32_t value;

    switch (offset & 0xFC) {
    case MIO_INT_EVENTS2:
        value = this->int_events >> 32;
        break;
    case MIO_INT_MASK2:
        value = this->int_mask >> 32;
        break;
    case MIO_INT_LEVELS2:
        value = this->int_levels >> 32;
        break;
    case MIO_INT_EVENTS1:
        value = uint32_t(this->int_events);
        break;
    case MIO_INT_MASK1:
        value = uint32_t(this->int_mask);
        break;
    case MIO_INT_LEVELS1:
        value = uint32_t(int_levels);
        break;
    case MIO_INT_CLEAR1:
    case MIO_INT_CLEAR2:
        // some Mac OS drivers read from these write-only registers
        // so we return zero here as real HW does
        value = 0;
        break;
    case MIO_OHARE_ID:
        value = (this->fp_id << 24) | (this->mon_id << 16) | (this->mb_id << 8) |
            (this->cpu_id | (this->emmo << 4));
        LOG_F(9, "%s: read OHARE_ID @%02x = %08x",
            this->get_name().c_str(), offset, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        value = this->feat_ctrl;
        LOG_F(9, "%s: read  FEAT_CTRL @%02x = %08x",
            this->get_name().c_str(), offset, value);
        break;
    default:
        value = 0;
        LOG_F(WARNING, "%s: read @%02x",
            this->get_name().c_str(), offset);
    }

    return BYTESWAP_32(value);
}

void MacIoTwo::mio_ctrl_write(uint32_t offset, uint32_t value, int size) {
    switch (offset & 0xFC) {
    case MIO_INT_MASK2:
        this->int_mask |= uint64_t(BYTESWAP_32(value) & ~MACIO_INT_MODE) << 32;
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR2:
        this->int_events &= ~(uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL) << 32);
        clear_cpu_int();
        break;
    case MIO_INT_MASK1:
        this->int_mask = (this->int_mask & 0x7FFFFFFF00000000ULL) | BYTESWAP_32(value);
        // copy IntMode bit to InterruptMask2 register
        this->int_mask |= uint64_t(this->int_mask & MACIO_INT_MODE) << 32;
        this->signal_cpu_int();
        break;
    case MIO_INT_CLEAR1:
        if ((this->int_mask & MACIO_INT_MODE) && (value & MACIO_INT_CLR)) {
            this->int_events = 0;
        } else {
            this->int_events &= ~uint64_t(BYTESWAP_32(value) & 0x7FFFFFFFUL);
        }
        clear_cpu_int();
        break;
    case MIO_OHARE_ID:
        LOG_F(ERROR, "%s: write OHARE_ID @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        break;
    case MIO_OHARE_FEAT_CTRL:
        LOG_F(WARNING, "%s: write FEAT_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->feature_control(BYTESWAP_32(value));
        break;
    case MIO_AUX_CTRL:
        LOG_F(9, "%s: write AUX_CTRL @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
        this->aux_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "%s: write @%x.%c = %0*x",
            this->get_name().c_str(), offset, SIZE_ARG(size), size * 2, value);
    }
}

void MacIoTwo::feature_control(uint32_t value) {
    this->feat_ctrl = value;

    if (!(this->feat_ctrl & 1)) {
        LOG_F(9, "%s: monitor sense enabled", this->name.c_str());
    } else {
        LOG_F(9, "%s: monitor sense disabled", this->name.c_str());
    }
}

uint64_t MacIoTwo::register_dev_int(IntSrc src_id) {
    if (this->device_id == MIO_DEV_ID_OHARE && src_id == ETHERNET) {
        ABORT_F("%s: attempt to register non-existing Ethernet device int",
                this->name.c_str());
    }

    switch (src_id) {
    case IntSrc::SCSI_MESH  : return INT_TO_IRQ_ID(0x0C);
    case IntSrc::IDE0       : return INT_TO_IRQ_ID(0x0D); // Beige G3 first IDE controller, or Yosemite ata-3
    case IntSrc::IDE1       : return INT_TO_IRQ_ID(0x0E);
    case IntSrc::SCCA       : return INT_TO_IRQ_ID(0x0F);
    case IntSrc::SCCB       : return INT_TO_IRQ_ID(0x10);
    case IntSrc::DAVBUS     : return INT_TO_IRQ_ID(0x11);
    case IntSrc::VIA_CUDA   : return INT_TO_IRQ_ID(0x12);
    case IntSrc::SWIM3      : return INT_TO_IRQ_ID(0x13);
    case IntSrc::NMI        : return INT_TO_IRQ_ID(0x14); // nmiSource in AppleHeathrow/Heathrow.cpp; programmer-switch in B&W G3

    case IntSrc::BANDIT1    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PCI_E      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x16) // same interrupt as bandit
                                   : INT_TO_IRQ_ID(0x18); // Lombard GPU
    case IntSrc::PCI_F      : return INT_TO_IRQ_ID(0x18);
    case IntSrc::PCI_A      : return INT_TO_IRQ_ID(0x17);
    case IntSrc::PCI_B      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x19)
                                   : INT_TO_IRQ_ID(0x18);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1A);
//  case IntSrc::???        : return INT_TO_IRQ_ID(0x1B);
    case IntSrc::PCI_C      : return (this->device_id == MIO_DEV_ID_OHARE)
                                   ? INT_TO_IRQ_ID(0x1C)
                                   : INT_TO_IRQ_ID(0x19);

    case IntSrc::PERCH2     : return INT_TO_IRQ_ID(0x15);
    case IntSrc::PCI_GPU    : return INT_TO_IRQ_ID(0x16);
    case IntSrc::PERCH1     : return INT_TO_IRQ_ID(0x1A);
    case IntSrc::PCI_PERCH  : return INT_TO_IRQ_ID(0x1C);

    case IntSrc::FIREWIRE   : return INT_TO_IRQ_ID(0x15); // Yosemite built-in PCI FireWire
    case IntSrc::PCI_J12    : return INT_TO_IRQ_ID(0x16); // Yosemite 32-bit 66MHz slot for GPU
    case IntSrc::PCI_J11    : return INT_TO_IRQ_ID(0x17); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J10    : return INT_TO_IRQ_ID(0x18); // Yosemite 64-bit 33MHz slot
    case IntSrc::PCI_J9     : return INT_TO_IRQ_ID(0x19); // Yosemite 64-bit 33MHz slot
    case IntSrc::ATA        : return INT_TO_IRQ_ID(0x1A); // Yosemite PCI pci-ata
    case IntSrc::USB        : return INT_TO_IRQ_ID(0x1C); // Yosemite PCI USB

    case IntSrc::ETHERNET   : return INT_TO_IRQ_ID(0x2A);

    default:
        ABORT_F("%s: unknown interrupt source %d", this->name.c_str(), src_id);
    }

    return 0;
}

uint64_t MacIoTwo::register_dma_int(IntSrc src_id) {
    if (this->device_id == MIO_DEV_ID_OHARE &&
        (src_id == IntSrc::DMA_ETHERNET_Tx || src_id == IntSrc::DMA_ETHERNET_Rx)) {
        ABORT_F("%s: attempt to register non-existing Ethernet DMA int", this->name.c_str());
    }

    switch (src_id) {
    case IntSrc::DMA_SCSI_MESH      : return INT_TO_IRQ_ID(0x00);
    case IntSrc::DMA_IDE0           : return INT_TO_IRQ_ID(0x02);
    case IntSrc::DMA_IDE1           : return INT_TO_IRQ_ID(0x03);
    case IntSrc::DMA_ETHERNET_Tx    : return INT_TO_IRQ_ID(0x20);
    case IntSrc::DMA_ETHERNET_Rx    : return INT_TO_IRQ_ID(0x21);
    default:
        return MacIoBase::register_dma_int(src_id);
    }

    return 0;
}

//===========================================================================
static const std::vector<std::string> OHare_Subdevices = {
    "NVRAM", "ViaCuda", "MeshTnt", "Escc", "Swim3", "Ide0", "Ide1"
};

static const std::vector<std::string> Heathrow_Subdevices = {
    "NVRAM", "ViaCuda", "MeshHeathrow", "Escc", "Swim3", "Ide0", "Ide1",
    "BigMacHeathrow"
};

static const std::vector<std::string> Paddington_Subdevices = {
    "NVRAM", "ViaCuda", "MeshHeathrow", "Escc", "Swim3", "Ide0", "Ide1",
    "BigMacPaddington"
};

static const DeviceDescription OHare_Descriptor = {
    MacIoTwo::create_ohare, OHare_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

static const DeviceDescription Heathrow_Descriptor = {
    MacIoTwo::create_heathrow, Heathrow_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

static const DeviceDescription Paddington_Descriptor = {
    MacIoTwo::create_paddington, Paddington_Subdevices, {},
    HWCompType::MMIO_DEV | HWCompType::PCI_DEV | HWCompType::INT_CTRL
};

REGISTER_DEVICE(OHare, OHare_Descriptor);
REGISTER_DEVICE(Heathrow, Heathrow_Descriptor);
REGISTER_DEVICE(Paddington, Paddington_Descriptor);
