//DingusPPC - Prototype 5bf2
//Written by divingkatae
//(c)2018-20 (theweirdo)
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 on Discord)

/** MPC106 (Grackle) emulation

    Author: Max Poliakovski

    Grackle IC is a combined memory and PCI controller manufactored by Motorola.
    It's the central device in the Gossamer architecture.
    Manual: https://www.nxp.com/docs/en/reference-manual/MPC106UM.pdf

    This code emulate as much functionality as needed to run PowerMac Beige G3.
    This implies that
    - we only support address map B
    - our virtual device reports revision 4.0 as expected by machine firmware
*/

#ifndef MPC106_H_
#define MPC106_H_

#include <cinttypes>
#include "memctrlbase.h"
#include "mmiodevice.h"


class MPC106 : public MemCtrlBase, public MMIODevice
{
public:
    using MemCtrlBase::name;

    MPC106();
    ~MPC106();
    uint32_t read(uint32_t offset, int size);
    void write(uint32_t offset, uint32_t value, int size);

protected:
    /* PCI access */
    uint32_t pci_read(uint32_t size);
    void pci_write(uint32_t value, uint32_t size);

    /* my own registers access */
    uint32_t myself_read(int reg_num, uint32_t size);
    void myself_write(int reg_num, uint32_t value, uint32_t size);

private:
    uint8_t my_pci_cfg_hdr[256] = {
        0x57, 0x10, // vendor ID: Motorola
        0x02, 0x00, // device ID: MPC106
        0x06, 0x00, // PCI command
        0x80, 0x00, // PCI status
        0x40,       // revision ID: 4.0
        0x00,       // standard programming
        0x00,       // subclass code
        0x06,       // class code
        [0x73] = 0xCD, // default value for ODCR
        [0xA8] = 0x10, 0x00, 0x00, 0xFF, // PICR1
        [0xAC] = 0x0C, 0x06, 0x0C, 0x00, // PICR2
        [0xBA] = 0x04,
        [0xC0] = 0x01,
        [0xE0] = 0x42, 0x00, 0xFF, 0x0F,
        [0xE8] = 0x20,
        [0xF0] = 0x00, 0x00, 0x02, 0xFF,
        [0xF4] = 0x03,
        [0xFC] = 0x00, 0x00, 0x10, 0x00
    };

    uint32_t config_addr;
    //uint32_t config_data;
};

#endif
