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

/** MacIO device family emulation

    Mac I/O (MIO) is a family of ASICs to bring support for Apple legacy
    I/O hardware to the PCI-based Power Macintosh. That legacy hardware has
    existed long before Power Macintosh was introduced. It includes:
    - versatile interface adapter (VIA)
    - Sander-Woz integrated machine (SWIM) that is a floppy disk controller
    - CUDA MCU for ADB, parameter RAM, realtime clock and power management support
    - serial communication controller (SCC)
    - Macintosh Enhanced SCSI Hardware (MESH)

    In the 68k Macintosh era, all this hardware was implemented using several
    custom chips. In a PCI-compatible Power Macintosh, the above devices are part
    of the MIO chip itself. MIO's functional blocks implementing virtual devices
    are called "cells", i.e. "VIA cell", "SWIM cell" etc.

    MIO itself is PCI compliant while the legacy hardware it emulates isn't.
    MIO occupies 512Kb of the PCI memory space divided into registers space and
    DMA space. Access to emulated legacy devices is accomplished by reading from/
    writing to MIO's PCI address space at predefined offsets.

    MIO includes a DMA controller that offers up to 12 DMA channels implementing
    Apple's own DMA protocol called descriptor-based DMA (DBDMA).

    Official documentation (that is somewhat incomplete and erroneous) can be
    found in the second chapter of the book "Macintosh Technology in the Common
    Hardware Reference Platform" by Apple Computer, Inc.
*/

#ifndef MACIO_H
#define MACIO_H

#include <devices/common/ata/idechannel.h>
#include <devices/common/dbdma.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/nvram.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/scsi/mesh.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/common/viacuda.h>
#include <devices/ethernet/bigmac.h>
#include <devices/ethernet/mace.h>
#include <devices/floppy/swim3.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/serial/escc.h>
#include <devices/sound/awacs.h>

#include <cinttypes>
#include <memory>
#include <string>

/** PCI device IDs for various MacIO ASICs. */
enum {
    MIO_DEV_ID_GRANDCENTRAL = 0x0002,
    MIO_DEV_ID_OHARE        = 0x0007,
    MIO_DEV_ID_HEATHROW     = 0x0010,
    MIO_DEV_ID_PADDINGTON   = 0x0017,
};

/** Interrupt related constants. */
#define MACIO_INT_CLR    0x80UL       // clears bits in the interrupt events registers
#define MACIO_INT_MODE   0x80000000UL // interrupt mode: 0 - native, 1 - 68k-style

#define INT_TO_IRQ_ID(intx) (1ULL << intx)

/** Offsets to common MacIO interrupt registers. */
enum {
    MIO_INT_EVENTS2 = 0x10,
    MIO_INT_MASK2   = 0x14,
    MIO_INT_CLEAR2  = 0x18,
    MIO_INT_LEVELS2 = 0x1C,
    MIO_INT_EVENTS1 = 0x20,
    MIO_INT_MASK1   = 0x24,
    MIO_INT_CLEAR1  = 0x28,
    MIO_INT_LEVELS1 = 0x2C
};

class IobusDevice {
public:
    virtual uint16_t iodev_read(uint32_t address) = 0;
    virtual void iodev_write(uint32_t address, uint16_t value) = 0;
};

/** GrandCentral DBDMA channels. */
enum : uint8_t {
    MIO_GC_DMA_SCSI_CURIO    = 0,
    MIO_GC_DMA_FLOPPY        = 1,
    MIO_GC_DMA_ETH_XMIT      = 2,
    MIO_GC_DMA_ETH_RCV       = 3,
    MIO_GC_DMA_ESCC_A_XMIT   = 4,
    MIO_GC_DMA_ESCC_A_RCV    = 5,
    MIO_GC_DMA_ESCC_B_XMIT   = 6,
    MIO_GC_DMA_ESCC_B_RCV    = 7,
    MIO_GC_DMA_AUDIO_OUT     = 8,
    MIO_GC_DMA_AUDIO_IN      = 9,
    MIO_GC_DMA_SCSI_MESH     = 0xA,
};

class NvramAddrHiDev: public IobusDevice {
public:
    // IobusDevice methods
    uint16_t iodev_read(uint32_t address);
    void iodev_write(uint32_t address, uint16_t value);

protected:
    uint16_t    nvram_addr_hi = 0;
};

class NvramDev: public IobusDevice {
public:
    NvramDev(NvramAddrHiDev *addr_hi);

    // IobusDevice methods
    uint16_t iodev_read(uint32_t address);
    void iodev_write(uint32_t address, uint16_t value);

protected:
    NVram* nvram;
    NvramAddrHiDev* addr_hi;
};

/** This class provides common building blocks for various MacIO ASICs. */
class MacIoBase : public PCIDevice, public InterruptCtrl {
public:
    MacIoBase(std::string name, uint16_t dev_id, uint8_t rev=1);
    ~MacIoBase() = default;

    uint64_t register_dma_int(IntSrc src_id) override;
    void ack_int(uint64_t irq_id, uint8_t irq_line_state) override;
    void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) override;

protected:
    void notify_bar_change(int bar_num);
    void ack_int_common(uint64_t irq_id, uint8_t irq_line_state);
    void signal_cpu_int();
    void clear_cpu_int();

    // PCI device state
    uint32_t    iomem_size = 0;
    uint32_t    base_addr  = 0;

    // interrupt state
    uint64_t int_mask      = 0;
    bool     cpu_int_latch = false;
    std::atomic<uint64_t> int_levels{0};
    std::atomic<uint64_t> int_events{0};

    // Subdevice objects
    ViaCuda*            viacuda;   // VIA cell with Cuda MCU attached to it
    Swim3::Swim3Ctrl*   swim3;     // floppy disk controller
    MacioSndCodec*      snd_codec; // audio codec instance
    EsccController*     escc;      // ESCC serial controller

    // DMA channels
    std::unique_ptr<DMAChannel>     floppy_dma;
    std::unique_ptr<DMAChannel>     snd_out_dma;
    std::unique_ptr<DMAChannel>     snd_in_dma;
    std::unique_ptr<DMAChannel>     escc_a_tx_dma;
    std::unique_ptr<DMAChannel>     escc_a_rx_dma;
    std::unique_ptr<DMAChannel>     escc_b_tx_dma;
    std::unique_ptr<DMAChannel>     escc_b_rx_dma;
};

class GrandCentral : public PCIDevice, public InterruptCtrl {
public:
    GrandCentral();
    ~GrandCentral() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<GrandCentral>(new GrandCentral());
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // InterruptCtrl methods
    void attach_iodevice(int dev_num, IobusDevice* dev_obj);

    uint64_t register_dev_int(IntSrc src_id) override;
    uint64_t register_dma_int(IntSrc src_id) override;
    void ack_int(uint64_t irq_id, uint8_t irq_line_state) override;
    void ack_dma_int(uint64_t irq_id, uint8_t irq_line_state) override;

protected:
    void notify_bar_change(int bar_num);
    void ack_int_common(uint64_t irq_id, uint8_t irq_line_state);
    void signal_cpu_int(uint64_t irq_id);
    void clear_cpu_int();

private:
    uint32_t    base_addr = 0;

    // interrupt state
    uint32_t    int_mask      = 0;
    std::atomic<uint32_t>    int_levels{0};
    std::atomic<uint32_t>    int_events{0};
    bool        cpu_int_latch = false;

    // IOBus devices
    IobusDevice*    iobus_devs[6] = { nullptr };
    std::unique_ptr<NvramAddrHiDev>  nvram_addr_hi_dev = nullptr;
    std::unique_ptr<NvramDev>        nvram_dev = nullptr;

    // subdevice objects
    std::unique_ptr<AwacsScreamer>      awacs;   // AWACS audio codec instance
    std::unique_ptr<MeshStub>           mesh_stub = nullptr;

    MaceController*     mace;       // Ethernet cell within Curio
    ViaCuda*            viacuda;    // VIA cell with Cuda MCU attached to it
    EsccController*     escc;       // ESCC serial controller cell within Curio
    MeshBase*           mesh;       // internal SCSI (fast)
    Sc53C94*            curio;      // external SCSI (slow)
    Swim3::Swim3Ctrl*   swim3;      // floppy disk controller

    std::unique_ptr<DMAChannel>     curio_dma;
    std::unique_ptr<DMAChannel>     mesh_dma = nullptr;
    std::unique_ptr<DMAChannel>     snd_out_dma;
    std::unique_ptr<DMAChannel>     snd_in_dma;
    std::unique_ptr<DMAChannel>     floppy_dma;
    std::unique_ptr<DMAChannel>     enet_tx_dma;
    std::unique_ptr<DMAChannel>     enet_rx_dma;
    std::unique_ptr<DMAChannel>     escc_a_tx_dma;
    std::unique_ptr<DMAChannel>     escc_a_rx_dma;
    std::unique_ptr<DMAChannel>     escc_b_tx_dma;
    std::unique_ptr<DMAChannel>     escc_b_rx_dma;

    uint16_t unsupported_dma_channel_read = 0;
    uint16_t unsupported_dma_channel_write = 0;
};

/** Implementation class for O'Hare/Heathrow/Paddington devices. */
class MacIoTwo : public MacIoBase {
public:
    MacIoTwo(std::string name, uint16_t dev_id);
    ~MacIoTwo() = default;

    static std::unique_ptr<HWComponent> create_ohare() {
        return std::unique_ptr<MacIoTwo>(new MacIoTwo("ohare", MIO_DEV_ID_OHARE));
    }

    static std::unique_ptr<HWComponent> create_heathrow() {
        return std::unique_ptr<MacIoTwo>(new MacIoTwo("heathrow", MIO_DEV_ID_HEATHROW));
    }

    static std::unique_ptr<HWComponent> create_paddington() {
        return std::unique_ptr<MacIoTwo>(new MacIoTwo("paddington", MIO_DEV_ID_PADDINGTON));
    }

    void set_media_bay_id(uint8_t id) {
        this->mb_id = id;
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

    // InterruptCtrl methods
    uint64_t register_dev_int(IntSrc src_id) override;
    uint64_t register_dma_int(IntSrc src_id) override;

protected:
    uint32_t dma_read(uint32_t offset, int size);
    void dma_write(uint32_t offset, uint32_t value, int size);

    uint32_t mio_ctrl_read(uint32_t offset, int size);
    void mio_ctrl_write(uint32_t offset, uint32_t value, int size);

    void feature_control(uint32_t value);

private:
    uint32_t feat_ctrl     = 0;    // features control register
    uint32_t aux_ctrl      = 0;    // aux features control register

    uint8_t  cpu_id = 0xE0; // CPUID field (LSB of the MIO_HEAT_ID)
    uint8_t  mb_id  = 0x70; // Media Bay ID (bits 15:8 of the MIO_HEAT_ID)
    uint8_t  mon_id = 0x10; // Monitor ID (bits 23:16 of the MIO_HEAT_ID)
    uint8_t  fp_id  = 0x70; // Flat panel ID (MSB of the MIO_HEAT_ID)
    uint8_t  emmo   = 0x01; // factory tester status, active low

    // Subdevice object pointers
    NVram*              nvram;    // NVRAM
    MeshController*     mesh;     // MESH SCSI cell instance
    IdeChannel*         ide_0;    // Internal ATA
    IdeChannel*         ide_1;    // Media Bay ATA
    BigMac*             bmac;     // Ethernet MAC cell

    // DMA channels
    std::unique_ptr<DMAChannel>     mesh_dma;
    std::unique_ptr<DMAChannel>     enet_xmit_dma;
    std::unique_ptr<DMAChannel>     enet_rcv_dma;

    uint16_t unsupported_dma_channel_read = 0;
    uint16_t unsupported_dma_channel_write = 0;
};

/** O'Hare/Heathrow specific registers. */
enum {
    MIO_OHARE_ID        = 0x34, // IDs register
    MIO_OHARE_FEAT_CTRL = 0x38, // feature control register
    MIO_AUX_CTRL        = 0x3C,
};

/** MIO_OHARE_ID bits. */
enum {
    MIO_OH_ID_FP                       = 0x70000000, // Flat panel ID
    MIO_OH_ID_MON                      = 0x00300000, // Monitor ID (mon_id?=1, no s-video/composite=3 but only if fatman exists).
    MIO_OH_ID_MB                       = 0x0000F000, // Media Bay ID (0,1=swim, 3=ata, 5=pci, 7=?)
    MIO_OH_ID_CPU                      = 0x000000E0, // CPUID field
    MIO_OH_ID_DISABLE_SCSI_TERMINATORS = 0x00000010, // clear this bit to disable-scsi-terminators - read this bit for emmo_pin
    MIO_OH_ID_ENABLE_SCSI_TERMINATORS  = 0x00000008, // set this bit to enable-scsi-terminators
    MIO_OH_ID_DO_SCSI_TERMINATORS      = 0x00000001, // set this bit to enable or disable scsi-terminators
};

/** MIO_OHARE_FEAT_CTRL bits. */
enum {
    MIO_OH_FC_IN_USE_LED               = 1 <<  0, // modem serial port in use in Open Firmware
                                                  // controls display sense on Beige G3 desktop
    MIO_OH_FC_NOT_MB_PWR               = 1 <<  1,
    MIO_OH_FC_PCI_MB_EN                = 1 <<  2,
    MIO_OH_FC_IDE_MB_EN                = 1 <<  3,
    MIO_OH_FC_FLOPPY_EN                = 1 <<  4,
    MIO_OH_FC_IDE_INT_EN               = 1 <<  5,
    MIO_OH_FC_NOT_IDE0_RESET           = 1 <<  6,
    MIO_OH_FC_NOT_MB_RESET             = 1 <<  7,
    MIO_OH_FC_IOBUS_EN                 = 1 <<  8,
    MIO_OH_FC_SCC_CELL_EN              = 1 <<  9,
    MIO_OH_FC_SCSI_CELL_EN             = 1 << 10,
    MIO_OH_FC_SWIM_CELL_EN             = 1 << 11,
    MIO_OH_FC_SND_PWR                  = 1 << 12,
    MIO_OH_FC_SND_CLK_EN               = 1 << 13,
    MIO_OH_FC_SCC_A_ENABLE             = 1 << 14,
    MIO_OH_FC_SCC_B_ENABLE             = 1 << 15,
    MIO_OH_FC_NOT_PORT_VIA_DESKTOP_VIA = 1 << 16,
    MIO_OH_FC_NOT_PWM_MON_ID           = 1 << 17,
    MIO_OH_FC_NOT_HOOKPB_MB_CNT        = 1 << 18,
    MIO_OH_FC_NOT_SWIM3_CLONEFLOPPY    = 1 << 19,
    MIO_OH_FC_AUD22RUN                 = 1 << 20,
    MIO_OH_FC_SCSI_LINKMODE            = 1 << 21,
    MIO_OH_FC_ARB_BYPASS               = 1 << 22,
    MIO_OH_FC_NOT_IDE1_RESET           = 1 << 23,
    MIO_OH_FC_SLOW_SCC_PCLK            = 1 << 24,
    MIO_OH_FC_RESET_SCC                = 1 << 25,
    MIO_OH_FC_MFDC_CELL_EN             = 1 << 26,
    MIO_OH_FC_USE_MFDC                 = 1 << 27,
    MIO_OH_FC_RESVD28                  = 1 << 28,
    MIO_OH_FC_RESVD29                  = 1 << 29,
    MIO_OH_FC_RESVD30                  = 1 << 30,
    MIO_OH_FC_RESVD31                  = 1 << 31,
};

/** O'Hare/Heathrow DBDMA channels. */
enum : uint8_t {
    MIO_OHARE_DMA_MESH          = 0,
    MIO_OHARE_DMA_FLOPPY        = 1,
    MIO_OHARE_DMA_ETH_XMIT      = 2,
    MIO_OHARE_DMA_ETH_RCV       = 3,
    MIO_OHARE_DMA_ESCC_A_XMIT   = 4,
    MIO_OHARE_DMA_ESCC_A_RCV    = 5,
    MIO_OHARE_DMA_ESCC_B_XMIT   = 6,
    MIO_OHARE_DMA_ESCC_B_RCV    = 7,
    MIO_OHARE_DMA_AUDIO_OUT     = 8,
    MIO_OHARE_DMA_AUDIO_IN      = 9,
    MIO_OHARE_DMA_IDE0          = 0xB,
    MIO_OHARE_DMA_IDE1          = 0xC
};

#endif /* MACIO_H */
