/** MacIO device family emulation

    Mac I/O (MIO) is a family of ASICs to bring support for Apple legacy
    I/O hardware to PCI-based Power Macintosh. That legacy hardware has
    existed long before Power Macintosh was introduced. It includes:
    - versatile interface adapter (VIA)
    - Sander-Woz integrated machine (SWIM) that is a floppy disk controller
    - CUDA MCU for ADB, parameter RAM, realtime clock and power management support
    - serial communication controller (SCC)
    - Macintosh Enhanced SCSI Hardware (MESH)

    In the 68k Macintosh era, all this hardware was implemented using several
    custom chips. In PCI-compatible Power Macintosh, the above devices are part
    of the MIO chip itself. MIO's functional blocks implementing virtual devices
    are called "cells", i.e. "VIA cell", "SWIM cell" etc.

    MIO itself is PCI compliant while the legacy hardware it emulates isn't.
    MIO occupies 512Kb of PCI memory space divided into registers space and DMA
    space. Access to emulated legacy devices is accomplished by reading from/
    writing to MIO's PCI address space at predefined offsets.

    MIO includes a DMA controller that offers 15 DMA channels implementing
    Apple's own DMA protocol called descriptor-based DMA (DBDMA).

    Official documentation (that is somewhat incomplete and erroneous) can be
    found in the second chapter of the book "Macintosh Technology in the Common
    Hardware Reference Platform" by Apple Computer, Inc.
*/

#ifndef MACIO_H
#define MACIO_H

#include <cinttypes>
#include "pcidevice.h"
#include "memctrlbase.h"
#include "mmiodevice.h"
#include "pcihost.h"
#include "viacuda.h"

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

class HeathrowIC : public PCIDevice
{
public:
    using PCIDevice::name;

    HeathrowIC();
    ~HeathrowIC();

    void set_host(PCIHost *host_instance) {this->host_instance = host_instance;};

    /* PCI device methods */
    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void     pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    /* MMIO device methods */
    uint32_t read(uint32_t offset, int size);
    void write(uint32_t offset, uint32_t value, int size);

protected:
    uint32_t mio_ctrl_read(uint32_t offset, int size);
    void mio_ctrl_write(uint32_t offset, uint32_t value, int size);

private:
    uint8_t pci_cfg_hdr[256] = {
        0x6B, 0x10, // vendor ID: Apple Computer Inc.
        0x10, 0x00, // device ID: Heathrow Mac I/O
        0x00, 0x00, // PCI command (set to 0 at power-up?)
        0x00, 0x00, // PCI status (set to 0 at power-up?)
        0x01,       // revision ID
        // class code is reported in OF property "class-code" as 0xff0000
        0x00,       // standard programming
        0x00,       // subclass code
        0xFF,       // class code: unassigned
        0x00, 0x00, // unknown defaults
        0x00, 0x00  // unknown defaults
    };

    uint32_t int_mask1;
    uint32_t int_clear1;
    uint32_t feat_ctrl;  // features control register

    /* device cells */
    ViaCuda *viacuda; /* VIA cell with Cuda MCU attached to it */
};

#endif /* MACIO_H */
