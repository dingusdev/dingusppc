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

/** ATI Mach64 GX definitions. */

#ifndef ATI_MACH64_GX_H
#define ATI_MACH64_GX_H

#include <devices/common/pci/pcidevice.h>
#include <devices/video/atimach64defs.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

class AtiMach64Gx : public PCIDevice, public VideoCtrlBase {
public:
    AtiMach64Gx();
    ~AtiMach64Gx() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AtiMach64Gx>(new AtiMach64Gx());
    }

    /* PCI device methods */
    bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

    /* MMIODevice methods */
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size);
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size);

protected:
    void notify_bar_change(int bar_num);
    uint32_t read_reg(uint32_t offset, uint32_t size);
    void write_reg(uint32_t offset, uint32_t value, uint32_t size);
    void enable_crtc_internal();
    void disable_crtc_internal();

    void rgb514_write_reg(uint8_t reg_addr, uint8_t value);
    void rgb514_write_ind_reg(uint8_t reg_addr, uint8_t value);

private:
    uint8_t mm_regs[2048] = { 0 }; // internal registers

    int         vram_size;
    uint32_t    aperture_base;
    uint32_t    config_cntl;
    uint32_t    mm_regs_offset;
    int         crtc_enable = 0;

    // RGB514 RAMDAC state
    uint8_t     dac_idx_lo;
    uint8_t     dac_idx_hi;
    uint8_t     clut_index;
    uint8_t     comp_index;
    uint8_t     clut_color[3];
    uint8_t     dac_regs[256];

    std::unique_ptr<DisplayID>  disp_id;
    std::unique_ptr<uint8_t[]>  vram_ptr;
};

#endif /* ATI_MACH64_GX_H */
