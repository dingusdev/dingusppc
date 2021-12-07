/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** @file PDM on-board video emulation. */

#include <cpu/ppc/ppcmmu.h>
#include <devices/memctrl/hmc.h>
#include <devices/video/pdmonboard.h>
#include <devices/video/videoctrl.h>
#include <machines/machinebase.h>
#include <loguru.hpp>

#include <cinttypes>

PdmOnboardVideo::PdmOnboardVideo()
    : VideoCtrlBase()
{
    this->video_mode = PDM_VMODE_OFF;
    this->blanking   = 0x80;
    this->crtc_on    = false;
    this->vdac_mode  = 8;

    // get pointer to the Highspeed Memory controller
    this->hmc_obj = dynamic_cast<HMC*>(gMachineObj->get_comp_by_name("HMC"));
}

void PdmOnboardVideo::set_video_mode(uint8_t new_mode)
{
    switch(new_mode & 0x7F) {
    case PdmVideoMode::Portrait:
    case PdmVideoMode::Rgb12in:
    case PdmVideoMode::Rgb13in:
    case PdmVideoMode::Rgb16in:
    case PdmVideoMode::VGA:
        if (this->video_mode != (new_mode & 0x1F)) {
            this->video_mode = new_mode & 0x1F;
            LOG_F(INFO, "PDM-Video: video mode set to 0x%X", this->video_mode);
        }
        if (this->blanking != (new_mode & 0x80)) {
            this->blanking = new_mode & 0x80;
            if (this->blanking) {
                this->disable_video_internal();
            } else {
                this->enable_video_internal();
            }
        }
        break;
    default:
        LOG_F(9, "PDM-Video: video disabled, new mode = 0x%X", new_mode & 0x1F);
        this->video_mode = new_mode & 0x1F;
        this->blanking   = 0x80;
        this->crtc_on    = false;
    }
}

void PdmOnboardVideo::set_pixel_depth(uint8_t depth)
{
    static uint8_t pix_depths[8] = {1, 2, 4, 8, 16, 0xFF, 0xFF, 0xFF};

    this->pixel_depth = pix_depths[depth];
    if (this->pixel_depth == 0xFF) {
        ABORT_F("PDM-Video: invalid pixel depth code %d specified!", depth);
    }
}

void PdmOnboardVideo::vdac_config(uint8_t mode)
{
    if (mode != 8) {
        LOG_F(WARNING, "Unknown Ariel Config code 0x%X", mode);
    }
    this->vdac_mode = 8; // 1 bpp, master mode, no overlay
}

void PdmOnboardVideo::set_clut_index(uint8_t index)
{
    this->clut_index = index;
    this->comp_index = 0;
}

void PdmOnboardVideo::set_clut_color(uint8_t color)
{
    this->clut_color[this->comp_index++] = color;
    if (this->comp_index >= 3) {
        LOG_F(INFO, "PDM-Video: received color for index %d", this->clut_index);
        // TODO: combine separate components into a single ARGB value
        this->palette[this->clut_index][0] = this->clut_color[0];
        this->palette[this->clut_index][1] = this->clut_color[1];
        this->palette[this->clut_index][2] = this->clut_color[2];
        this->palette[this->clut_index][3] = 255;
        this->clut_index++;   // assume the HW works like that
        this->comp_index = 0; // assume the HW works like that
    }
}

void PdmOnboardVideo::enable_video_internal()
{
    int new_width, new_height;

    // ensure all video parameters contain safe values
    switch(this->video_mode) {
    case PdmVideoMode::Portrait:
    case PdmVideoMode::Rgb16in:
    case PdmVideoMode::VGA:
        if (this->pixel_depth > 8) {
            ABORT_F("PDM-Video: no 16bpp support in mode %d!", this->video_mode);
        }
    default:
        break;
    }

    // set video mode parameters
    switch(this->video_mode) {
    case PdmVideoMode::Portrait:
        new_width  = 640;
        new_height = 870;
        this->pixel_clock = 57283200;
        break;
    case PdmVideoMode::Rgb12in:
        new_width  = 512;
        new_height = 384;
        this->pixel_clock = 15667200;
        break;
    case PdmVideoMode::Rgb13in:
        new_width  = 640;
        new_height = 480;
        this->pixel_clock = 31334400;
        break;
    case PdmVideoMode::Rgb16in:
        new_width  = 832;
        new_height = 624;
        this->pixel_clock = 57283200;
        break;
    case PdmVideoMode::VGA:
        new_width  = 640;
        new_height = 480;
        this->pixel_clock = 25175000;
        break;
    default:
        ABORT_F("PDM-Video: invalid video mode %d", this->video_mode);
    }

    if (new_width != this->active_width || new_height != this->active_height) {
        LOG_F(WARNING, "Display window resizing not implemented yet!");
    }

    // figure out where our framebuffer is located
    uint64_t hmc_control = this->hmc_obj->get_control_reg();
    uint32_t fb_base_phys = ((hmc_control >> HMC_VBASE_BIT) & 1) ? 0 : 0x100000;
    LOG_F(INFO, "PDM-Video: framebuffer phys base addr = 0x%X", fb_base_phys);

    // set framebuffer address and pitch
    this->fb_ptr = mmu_get_dma_mem(fb_base_phys, PDM_FB_SIZE_MAX);
    this->active_width  = new_width;
    this->active_height = new_height;

    switch(this->pixel_depth) {
    case 1:
        this->convert_fb_cb = [this](uint8_t *dst_buf, int dst_pitch) {
            this->convert_frame_1bpp(dst_buf, dst_pitch);
        };
        this->fb_pitch = new_width >> 3; // one byte contains 8 pixels
        break;
    default:
        ABORT_F("PDM-Video: pixel depth %d not implemented yet!", this->pixel_depth);
    }

    this->update_screen();

    LOG_F(INFO, "PDM-Video: video enabled");
    this->crtc_on = true;
}

void PdmOnboardVideo::disable_video_internal()
{
    this->crtc_on = false;
    LOG_F(INFO, "PDM-Video: video disabled");
}

/** Ariel II has a weird 1bpp mode where a white pixel is mapped to
    CLUT entry #127 (%01111111) and a black pixel to #255 (%11111111).
    It requres a non-standard conversion routine implementend below.
 */
void PdmOnboardVideo::convert_frame_1bpp(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t  *src_buf, *src_row, pix;
    uint32_t *dst_row;
    int      src_pitch, width;
    uint32_t pixels[2];

    // prepare cached ARGB values for white & black pixels
    pixels[0] = (0xFF << 24) | (this->palette[127][0] << 16) |
                (this->palette[127][1] << 8) | this->palette[127][2];
    pixels[1] = (0xFF << 24) | (this->palette[255][0] << 16) |
                (this->palette[255][1] << 8) | this->palette[255][2];

    src_buf   = this->fb_ptr;
    src_pitch = this->fb_pitch;
    width     = this->active_width >> 3;

    for (int h = 0; h < this->active_height; h++) {
        src_row = &src_buf[h * src_pitch];
        dst_row = (uint32_t *)(&dst_buf[h * dst_pitch]);

        for (int x = 0; x < width; x++) {
            pix = src_row[x];

            for (int i = 0; i < 8; i++, pix <<= 1, dst_row++) {
                *dst_row = pixels[(pix >> 7) & 1];
            }
        }
    }
}
