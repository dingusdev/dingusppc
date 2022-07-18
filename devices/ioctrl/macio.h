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

    MIO includes a DMA controller that offers 15 DMA channels implementing
    Apple's own DMA protocol called descriptor-based DMA (DBDMA).

    Official documentation (that is somewhat incomplete and erroneous) can be
    found in the second chapter of the book "Macintosh Technology in the Common
    Hardware Reference Platform" by Apple Computer, Inc.
*/

#ifndef MACIO_H
#define MACIO_H

#include <devices/common/dbdma.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/nvram.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/scsi/mesh.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/common/viacuda.h>
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

class GrandCentral : public PCIDevice, public InterruptCtrl {
public:
    GrandCentral();
    ~GrandCentral() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<GrandCentral>(new GrandCentral());
    }

    // MMIO device methods
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

    // InterruptCtrl methods
    uint32_t register_dev_int(IntSrc src_id);
    uint32_t register_dma_int(IntSrc src_id);
    void ack_int(uint32_t irq_id, uint8_t irq_line_state);
    void ack_dma_int(uint32_t irq_id, uint8_t irq_line_state);

protected:
    void notify_bar_change(int bar_num);

private:
    uint32_t    base_addr = 0;

    // interrupt state
    uint32_t    int_mask   = 0;
    uint32_t    int_levels = 0;
    uint32_t    int_events = 0;

    uint32_t    nvram_addr_hi;

    // subdevice objects
    std::unique_ptr<AwacsScreamer>      awacs;   // AWACS audio codec instance

    NVram*              nvram;   // NVRAM module
    MaceController*     mace;
    ViaCuda*            viacuda; // VIA cell with Cuda MCU attached to it
    EsccController*     escc;    // ESCC serial controller
    Sc53C94*            scsi_0;  // external SCSI
    Swim3::Swim3Ctrl*   swim3;   // floppy disk controller

    std::unique_ptr<DMAChannel>     snd_out_dma;
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

class HeathrowIC : public PCIDevice, public InterruptCtrl {
public:
    HeathrowIC();
    ~HeathrowIC() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<HeathrowIC>(new HeathrowIC());
    }

    // MMIO device methods
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

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

private:
    uint32_t base_addr   = 0;
    uint32_t int_events2 = 0;
    uint32_t int_mask2   = 0;
    uint32_t int_levels2 = 0;
    uint32_t int_events1 = 0;
    uint32_t int_mask1   = 0;
    uint32_t int_levels1 = 0;
    uint32_t macio_id    = 0xF0700008UL;
    uint32_t feat_ctrl   = 0;    // features control register
    uint32_t aux_ctrl    = 0;    // aux features control register

    // subdevice objects
    std::unique_ptr<AwacsScreamer>      screamer; // Screamer audio codec instance
    //std::unique_ptr<MESHController>     mesh;     // MESH SCSI cell instance

    NVram*              nvram;    // NVRAM
    ViaCuda*            viacuda;  // VIA cell with Cuda MCU attached to it
    MESHController*     mesh;     // MESH SCSI cell instance
    EsccController*     escc;     // ESCC serial controller
    Swim3::Swim3Ctrl*   swim3;    // floppy disk controller

    std::unique_ptr<DMAChannel>     snd_out_dma;
};

#endif /* MACIO_H */
