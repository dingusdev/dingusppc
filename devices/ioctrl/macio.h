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

/** Interrupt related constants. */
#define MACIO_INT_CLR    0x80UL       // clears bits in the interrupt events registers
#define MACIO_INT_MODE   0x80000000UL // interrupt mode: 0 - native, 1 - 68k-style

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

class GrandCentral : public PCIDevice, public InterruptCtrl {
public:
    GrandCentral();
    ~GrandCentral() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<GrandCentral>(new GrandCentral());
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // InterruptCtrl methods
    uint32_t register_dev_int(IntSrc src_id);
    uint32_t register_dma_int(IntSrc src_id);
    void ack_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state);

    void attach_iodevice(int dev_num, IobusDevice* dev_obj);

protected:
    void notify_bar_change(int bar_num);
    void ack_int_common(uint32_t irq_id, uint8_t irq_line_state);
    void signal_cpu_int(uint32_t irq_id);
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

class OHare : public PCIDevice, public InterruptCtrl {
public:
    OHare();
    ~OHare() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<OHare>(new OHare());
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // InterruptCtrl methods
    uint32_t register_dev_int(IntSrc src_id);
    uint32_t register_dma_int(IntSrc src_id);
    void ack_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state);

protected:
    void notify_bar_change(int bar_num);
    uint32_t read_ctrl(uint32_t offset, int size);
    void write_ctrl(uint32_t offset, uint32_t value, int size);
    uint32_t dma_read(uint32_t offset, int size);
    void dma_write(uint32_t offset, uint32_t value, int size);

    void signal_cpu_int();
    void clear_cpu_int();

private:
    uint32_t    base_addr = 0;

    // interrupt state
    uint32_t    int_mask      = 0;
    std::atomic<uint32_t>    int_levels{0};
    std::atomic<uint32_t>    int_events{0};
    bool        cpu_int_latch = false;

    std::unique_ptr<AwacsScreamer>  awacs; // AWACS audio codec instance
    std::unique_ptr<DMAChannel>     snd_out_dma;

    NVram*              nvram;   // NVRAM module
    ViaCuda*            viacuda; // VIA cell with Cuda MCU attached to it
    EsccController*     escc;    // ESCC serial controller
};

/**
    Heathrow ASIC emulation

    Heathrow is a MIO-compliant ASIC used in the Gossamer architecture. It's
    hard-wired to PCI device number 16. Its I/O memory (512Kb) will be configured
    by the Macintosh firmware to live at 0xF3000000.

    Emulated subdevices and their offsets within Heathrow I/O space:
    ----------------------------------------------------------------
    mesh(SCSI)     register space: 0x00010000, DMA space: 0x00008000
    bmac(ethernet) register space: 0x00011000, DMA space: 0x00008200, 0x00008300
    escc(compat)   register space: 0x00012000, size: 0x00001000
                        DMA space: 0x00008400, size: 0x00000400
    escc(MacRISC)  register space: 0x00013000, size: 0x00001000
                        DMA space: 0x00008400, size: 0x00000400
    escc:ch-a      register space: 0x00013020, DMA space: 0x00008400, 0x00008500
    escc:ch-b      register space: 0x00013000, DMA space: 0x00008600, 0x00008700
    davbus(sound)  register space: 0x00014000, DMA space: 0x00008800, 0x00008900
    SWIM3(floppy)  register space: 0x00015000, DMA space: 0x00008100
    NVRAM          register space: 0x00060000, size: 0x00020000
    IDE            register space: 0x00020000, DMA space: 0x00008b00
    VIA-CUDA       register space: 0x00016000, size: 0x00002000
*/

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
    MIO_OH_FC_IN_USE_LED               = 1 <<  0,
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

class HeathrowIC : public PCIDevice, public InterruptCtrl {
public:
    HeathrowIC();
    ~HeathrowIC() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<HeathrowIC>(new HeathrowIC());
    }

    // MMIO device methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

    // InterruptCtrl methods
    uint32_t register_dev_int(IntSrc src_id);
    uint32_t register_dma_int(IntSrc src_id);
    void ack_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state);

protected:
    uint32_t dma_read(uint32_t offset, int size);
    void dma_write(uint32_t offset, uint32_t value, int size);

    uint32_t mio_ctrl_read(uint32_t offset, int size);
    void mio_ctrl_write(uint32_t offset, uint32_t value, int size);

    void notify_bar_change(int bar_num);

    void feature_control(const uint32_t value);

    void signal_cpu_int();
    void clear_cpu_int();

private:
    uint32_t base_addr     = 0;
    std::atomic<uint32_t> int_events2{0};
    uint32_t int_mask2     = 0;
    std::atomic<uint32_t> int_levels2{0};
    std::atomic<uint32_t> int_events1{0};
    uint32_t int_mask1     = 0;
    std::atomic<uint32_t> int_levels1{0};
    uint32_t feat_ctrl     = 0;    // features control register
    uint32_t aux_ctrl      = 0;    // aux features control register
    bool     cpu_int_latch = false;

    uint8_t  cpu_id = 0xE0; // CPUID field (LSB of the MIO_HEAT_ID)
    uint8_t  mb_id  = 0x70; // Media Bay ID (bits 15:8 of the MIO_HEAT_ID)
    uint8_t  mon_id = 0x10; // Monitor ID (bits 23:16 of the MIO_HEAT_ID)
    uint8_t  fp_id  = 0x70; // Flat panel ID (MSB of the MIO_HEAT_ID)
    uint8_t  emmo_pin;      // factory tester status, active low

    // subdevice objects
    MacioSndCodec*      snd_codec; // audio codec instance

    NVram*              nvram;    // NVRAM
    ViaCuda*            viacuda;  // VIA cell with Cuda MCU attached to it
    MeshController*     mesh;     // MESH SCSI cell instance
    EsccController*     escc;     // ESCC serial controller
    IdeChannel*         ide_0;    // Internal ATA
    IdeChannel*         ide_1;    // Media Bay ATA
    Swim3::Swim3Ctrl*   swim3;    // floppy disk controller
    BigMac*             bmac;     // Ethernet MAC cell

    // DMA channels
    std::unique_ptr<DMAChannel>     mesh_dma;
    std::unique_ptr<DMAChannel>     floppy_dma;
    std::unique_ptr<DMAChannel>     enet_xmit_dma;
    std::unique_ptr<DMAChannel>     enet_rcv_dma;
    std::unique_ptr<DMAChannel>     escc_b_rcv_dma;
    std::unique_ptr<DMAChannel>     snd_out_dma;
};

#endif /* MACIO_H */
