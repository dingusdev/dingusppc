/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

/** @file Valkyrie video controller definitions. */

#ifndef VALKYRIE_VIDEO_H
#define VALKYRIE_VIDEO_H

#include <devices/common/clockgen/athens.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/mmiodevice.h>
#include <devices/video/displayid.h>
#include <devices/video/videoctrl.h>

#include <cinttypes>
#include <memory>

namespace Valkyrie {
    constexpr uint32_t REGBASE_CORDYCEPS  = 0x50F20000; // Performa 5200 etc.
    constexpr uint32_t REGBASE_ALCHEMY    = 0xF1300000; // Performa 6400 etc.
    constexpr uint32_t VRAM_BASE          = 0xF1000000;
    constexpr uint32_t CLUT_OFFSET        = 0x4000;
    constexpr uint32_t CONTROL_OFFSET     = 0xA000;

    /** Chip control/status register indexes. */
    enum {
        csr_mode        = 0x00, // video mode
        csr_depth       = 0x01, // framebuffer pixel depth
        csr_id_reset    = 0x02, // LinuxPPC driver source calls it csr_stat
        csr_subsys_cfg  = 0x03, // subsystem config
        csr_int_stat    = 0x04, // interrupt status
        csr_int_en      = 0x06, // interrupt enable
        csr_monid       = 0x07, // monitor sense
    };

    /** Color palette register indexes. */
    enum {
        clut_index = 0,
        clut_color = 1,
    };

    // Bit definitions for INT_EN & INT_STAT registers.
    enum {
        VBL_IRQ = 1 << 0,
    };
}; // namespace Valkyrie

class ValkyrieVideo : public VideoCtrlBase, public MMIODevice {
public:
    ValkyrieVideo(const uint32_t base_addr);
    ~ValkyrieVideo() = default;

    static std::unique_ptr<HWComponent> create_for_cordyceps() {
        return std::unique_ptr<ValkyrieVideo>(new ValkyrieVideo(Valkyrie::REGBASE_CORDYCEPS));
    }

    static std::unique_ptr<HWComponent> create_for_alchemy() {
        return std::unique_ptr<ValkyrieVideo>(new ValkyrieVideo(Valkyrie::REGBASE_ALCHEMY));
    }

    // HWComponent methods
    int device_postinit() override;

    // MMIODevice methods
    uint32_t read(uint32_t rgn_start, uint32_t offset, int size) override;
    void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) override;

private:
    uint8_t     mode    = 0x9F;
    uint8_t     depth   = 0;
    uint8_t     mon_id  = 0;
    uint8_t     chip_id = 0x10; // 7:4 - chip ID, 3:0 - revision (guessed)

    // interrupt state
    uint8_t     int_en      = 0;
    uint8_t     int_latch   = 0;

    // Color palette sequencer state
    uint8_t     clut_index = 0;
    uint8_t     comp_index = 0;
    uint8_t     clut_color[3] {};

    uint8_t     *vram_ptr   = nullptr;
    int         reg_shift   = 3;

    uint64_t    mode_timer_id = 0;

    void disable_video_internal();
    void enable_video_internal();
    void schedule_mode_switch();

    std::unique_ptr<DisplayID>      disp_id = nullptr;
    std::unique_ptr<AthensClocks>   clk_gen = nullptr;
};

#endif // VALKYRIE_VIDEO_H
