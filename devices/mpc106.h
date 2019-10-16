//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

/** MPC106 (Grackle) emulation

    Author: Max Poliakovski

    Grackle IC is a combined memory and PCI controller manufactured by Motorola.
    It's the central device in the Gossamer architecture.
    Manual: https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    This code emulates as much functionality as needed to run PowerMac Beige G3.
    This implies that
    - we only support address map B
    - our virtual device reports revision 4.0 as expected by machine firmware
*/

#ifndef MPC106_H_
#define MPC106_H_

#include <cinttypes>
#include <unordered_map>
#include "memctrlbase.h"
#include "mmiodevice.h"
#include "pcidevice.h"
#include "pcihost.h"


class MPC106 : public MemCtrlBase, public PCIDevice, public PCIHost
{
public:
    using MemCtrlBase::name;

    MPC106();
    ~MPC106();
    uint32_t read(uint32_t offset, int size);
    void write(uint32_t offset, uint32_t value, int size);

    /* PCI host bridge API */
    bool pci_register_device(int dev_num, PCIDevice *dev_instance);

protected:
    /* PCI access */
    uint32_t pci_read(uint32_t size);
    void pci_write(uint32_t value, uint32_t size);

    /* my own PCI configuration registers access */
    uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    void pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    void set_host(PCIHost *host_instance) {}; // unimplemented for now

    /* PCI host bridge API */
    //bool pci_register_device(int dev_num, PCIDevice *dev_instance);
    bool pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice *obj);

    void setup_ram(void);

private:
    uint8_t my_pci_cfg_hdr[256] = {
        0x57, 0x10, // vendor ID: Motorola
        0x02, 0x00, // device ID: MPC106
        0x06, 0x00, // PCI command
        0x80, 0x00, // PCI status
        0x40,       // revision ID: 4.0
        0x00,       // standard programming
        0x00,       // subclass code: host bridge
        0x06,       // class code: bridge device
        0x08,       // cache line size
        0x00,       // latency timer
        0x00,       // header type
        0x00,       // BIST Control
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00,  //Interrupt line
        0x00,  //Interrupt pin
        0x00,  //MIN GNT
        0x00,  //MAX LAT
        0x00,  //Bus number
        0x00,  //Subordinate bus number
        0x00,  //Discount counter
        0x00,  //MAX LAT
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, //Performance monitor command
        0x00, 0x00, //Performance monitor mode control
        0xFF, 0xFF,

        0x00, 0x00, 0x00, 0x00, //Performance monitor counter 0
        0x00, 0x00, 0x00, 0x00, //Performance monitor counter 1
        0x00, 0x00, 0x00, 0x00, //Performance monitor counter 2
        0x00, 0x00, 0x00, 0x00, //Performance monitor counter 3

        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,

        0x00, 0x00, //Power mgt config 1
        0x00,       //Power mgt config 2
        0xCD, //default value for ODCR
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Memory Starting Address
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Extended Memory Starting Address
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Memory Ending Address
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //Extended Memory Ending Address

        0x00, //Memory bank enable
        0xFF, 0xFF,
        0x00, //Memory page mode
        0xFF, 0xFF, 0xFF, 0xFF,

        0x10, 0x00, 0x00, 0xFF, // PICR1
        0x0C, 0x06, 0x0C, 0x00, // PICR2
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x00, //ECC single-bit error counter
        0x00, //ECC single-bit error trigger
        0x04,  //Alternate OS visible paramaters 1
        0x01,  //Alternate OS visible paramaters 2

        0xFF, 0xFF, 0xFF, 0xFF,

        0x01, //Error enabling 1
        0x00, //Error detection 1
        0xFF,
        0x00, //60x bus error status
        0x00, //Error enabling 2
        0x00, //Error detection 2
        0xFF,
        0x00, //PCI bus error status
        0x00, 0x00, 0x00, 0x00, //60x/PCI ERROR address

        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,

        0x42, 0x00, 0xFF, 0x0F, //Emulation support config 1
        0x00, 0x00, 0x00, 0x00, //Modified memory status (no clear)
        0x20, 0x00, 0x00, 0x00, //Emulation support config 2
        0x00, 0x00, 0x00, 0x00, //Modified memory status (clear)

        0x00, 0x00, 0x02, 0xFF, //Memory ctrl config 1
        0x03, 0x00, 0x00, 0x00, //Memory ctrl config 2
        0x00, 0x00, 0x00, 0x00, //Memory ctrl config 3
        0x00, 0x00, 0x10, 0x00  //Memory ctrl config 4
    };

    uint32_t config_addr;
    //uint32_t config_data;

    std::unordered_map<int, PCIDevice*> pci_0_bus;
};

#endif
