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

/** @file Definitions for the PDM on-board video. */

#ifndef PDM_ONBOARD_H
#define PDM_ONBOARD_H

#include <devices/video/videoctrl.h>

#include <cinttypes>

class HMC;
class InterruptCtrl;

constexpr auto PDM_VMODE_OFF = 0x1F;

/** Max. size of our framebuffer in bytes (RGB 13-inch, 16 bpp) */
constexpr auto PDM_FB_SIZE_MAX = (640 * 480 * 2);

/** Fixed video modes supported by the PDM on-board video. */
enum PdmVideoMode : uint8_t {
    Portrait = 1,
    Rgb12in  = 2,
    Rgb13in  = 6,
    Rgb16in  = 9,
    VGA      = 0xB
};

class PdmOnboardVideo : public VideoCtrlBase {
public:
    PdmOnboardVideo();
    ~PdmOnboardVideo() = default;

    uint8_t get_video_mode() const {
        return ((this->video_mode & 0x1F) | this->blanking);
    }

    void set_video_mode(uint8_t new_mode);
    void set_pixel_depth(uint8_t depth);
    void set_vdac_config(uint8_t config);
    uint8_t get_vdac_config() const {
        return this->vdac_mode;
    }
    void set_clut_index(uint8_t index);
    void set_clut_color(uint8_t color);

    void init_interrupts(InterruptCtrl *int_ctrl, uint32_t vbl_irq_id) {
        this->int_ctrl = int_ctrl;
        this->irq_id = vbl_irq_id;
    }

protected:
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    bool framebuffer_in_main_memory(void) override {
        return true;
    }
#endif
    void    set_depth_internal(int width);
    void    enable_video_internal();
    void    disable_video_internal();
    void    set_fb_base();

    void pdm_convert_frame_1bpp_indexed(uint8_t *dst_buf, int dst_pitch);
    void pdm_convert_frame_2bpp_indexed(uint8_t *dst_buf, int dst_pitch);
    void pdm_convert_frame_4bpp_indexed(uint8_t *dst_buf, int dst_pitch);

private:
    uint8_t     video_mode;
    uint8_t     blanking;
    uint8_t     vdac_mode;
    uint8_t     clut_index;
    uint8_t     comp_index;
    uint8_t     fb_loc = 0;
    uint8_t     clut_color[3];

    HMC*        hmc_obj;
};

#endif // PDM_ONBOARD_H
