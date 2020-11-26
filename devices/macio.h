/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#include "awacs.h"
#include "dbdma.h"
#include "hwcomponent.h"
#include "memctrlbase.h"
#include "mmiodevice.h"
#include "nvram.h"
#include "pcidevice.h"
#include "pcihost.h"
#include "viacuda.h"
#include <cinttypes>

/**
    Heathrow ASIC emulation

    Author: Max Poliakovski

    Heathrow is a MIO-compliant ASIC used in the Gossamer architecture. It's
    hard-wired to PCI device number 16. Its I/O memory (512Kb) will be configured
    by the Macintosh firmware to live at 0xF3000000.

    Emulated subdevices and their offsets within Heathrow I/O space:
    ----------------------------------------------------------------
    mesh(SCSI)     register space: 0x00010000, DMA space: 0x00008000
    bmac(ethernet) register space: 0x00011000, DMA space: 0x00008200, 0x00008300
    escc(serial)   register space: 0x00013000, size: 0x00001000
                        DMA space: 0x00008400, size: 0x00000400
    escc:ch-a      register space: 0x00013020, DMA space: 0x00008400, 0x00008500
    escc:ch-b      register space: 0x00013000, DMA space: 0x00008600, 0x00008700
    davbus(sound)  register space: 0x00014000, DMA space: 0x00008800, 0x00008900
    SWIM3(floppy)  register space: 0x00015000, DMA space: 0x00008100
    NVRAM          register space: 0x00060000, size: 0x00020000
    IDE            register space: 0x00020000, DMA space: 0x00008b00
    VIA-CUDA       register space: 0x00016000, size: 0x00002000
*/

class HeathrowIC : public PCIDevice {
public:
    HeathrowIC();
    ~HeathrowIC();

    bool supports_type(HWCompType type) {
        return type == HWCompType::MMIO_DEV;
    };

    /* PCI device methods */
    bool supports_io_space(void) {
        return false;
    };

    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    /* MMIO device methods */
    uint32_t read(uint32_t reg_start, uint32_t offset, int size);
    void write(uint32_t reg_start, uint32_t offset, uint32_t value, int size);

protected:
    uint32_t dma_read(uint32_t offset, int size);
    void dma_write(uint32_t offset, uint32_t value, int size);

    uint32_t mio_ctrl_read(uint32_t offset, int size);
    void mio_ctrl_write(uint32_t offset, uint32_t value, int size);

private:
    uint8_t pci_cfg_hdr[256] = {
        0x6B,
        0x10,    // vendor ID: Apple Computer Inc.
        0x10,
        0x00,    // device ID: Heathrow Mac I/O
        0x00,
        0x00,    // PCI command (set to 0 at power-up?)
        0x00,
        0x00,    // PCI status (set to 0 at power-up?)
        0x01,    // revision ID
        // class code is reported in OF property "class-code" as 0xff0000
        0x00,    // standard programming
        0x00,    // subclass code
        0xFF,    // class code: unassigned
        0x00,
        0x00,    // unknown defaults
        0x00,
        0x00    // unknown defaults
    };

    uint32_t int_mask2   = 0;
    uint32_t int_clear2  = 0;
    uint32_t int_levels2 = 0;
    uint32_t int_mask1   = 0;
    uint32_t int_clear1  = 0;
    uint32_t int_levels1 = 0;
    uint32_t feat_ctrl   = 0;    // features control register

    /* device cells */
    ViaCuda* viacuda;     /* VIA cell with Cuda MCU attached to it */
    NVram* nvram;         /* NVRAM cell */
    AWACDevice* screamer; /* Screamer audio codec instance */

    DMAChannel* snd_out_dma;
};

#endif /* MACIO_H */
