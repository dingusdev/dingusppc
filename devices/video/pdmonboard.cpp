/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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
    this->video_mode  = PDM_VMODE_OFF;
    this->blanking    = 0x80;
    this->crtc_on     = false;
    this->vdac_mode   = 8;
    this->pixel_depth = 0;

    // get pointer to the Highspeed Memory controller
    this->hmc_obj = dynamic_cast<HMC*>(gMachineObj->get_comp_by_name("HMC"));
}

void PdmOnboardVideo::set_video_mode(uint8_t new_mode)
{
    if (this->blanking != (new_mode & 0x80)) {
        if (new_mode & 0x80) {
            this->disable_video_internal();
        } else {
            this->blank_on = false;
            this->blanking = 0;
        }
    }

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
        if (!this->blanking) {
            this->enable_video_internal();
        }
        break;
    default:
        LOG_F(9, "PDM-Video: video disabled, new mode = 0x%X", new_mode & 0x1F);
        this->video_mode = new_mode & 0x1F;
        this->disable_video_internal();
    }
}

void PdmOnboardVideo::set_pixel_depth(uint8_t depth)
{
    static uint8_t pix_depths[8] = {1, 2, 4, 8, 16, 0xFF, 0xFF, 0xFF};

    uint8_t new_pix_depth = pix_depths[depth];
    if (new_pix_depth == 0xFF) {
        ABORT_F("PDM-Video: invalid pixel depth code %d specified!", depth);
    }

    if (new_pix_depth != this->pixel_depth) {
        this->pixel_depth = new_pix_depth;
        this->set_depth_internal(this->active_width);
        LOG_F(INFO, "PDM-Video: pixel depth changed to %dbpp", new_pix_depth);
    }
}

void PdmOnboardVideo::set_vdac_config(uint8_t mode)
{
    this->vdac_mode = mode;
    if ((mode & 0xF8) != 8) {
        LOG_F(WARNING, "Ariel control changed to unknown value 0x%X", mode);
    }
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
        this->set_palette_color(this->clut_index, clut_color[0],
                                clut_color[1], clut_color[2], 0xFF);
        this->clut_index++;
        this->comp_index = 0;
    }
}

void PdmOnboardVideo::set_depth_internal(int pitch)
{
    switch (this->pixel_depth) {
    case 1:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_1bpp(dst_buf, dst_pitch);
        };
        this->fb_pitch = pitch >> 3;    // one byte contains 8 pixels
        break;
    case 8:
        this->convert_fb_cb = [this](uint8_t* dst_buf, int dst_pitch) {
            this->convert_frame_8bpp(dst_buf, dst_pitch);
        };
        this->fb_pitch = pitch; // one byte contains 1 pixel
        break;
    default:
        ABORT_F("PDM-Video: pixel depth %d not implemented yet!", this->pixel_depth);
    }
}

void PdmOnboardVideo::enable_video_internal()
{
    int new_width, new_height, hori_blank, vert_blank;

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
        hori_blank = 192;
        vert_blank =  48;
        this->pixel_clock = 57283200;
        break;
    case PdmVideoMode::Rgb12in:
        new_width  = 512;
        new_height = 384;
        hori_blank = 128;
        vert_blank =  23;
        this->pixel_clock = 15667200;
        break;
    case PdmVideoMode::Rgb13in:
        new_width  = 640;
        new_height = 480;
        hori_blank = 256;
        vert_blank =  45;
        this->pixel_clock = 31334400;
        break;
    case PdmVideoMode::Rgb16in:
        new_width  = 832;
        new_height = 624;
        hori_blank = 320;
        vert_blank =  43;
        this->pixel_clock = 57283200;
        break;
    case PdmVideoMode::VGA:
        new_width  = 640;
        new_height = 480;
        hori_blank = 160;
        vert_blank =  45;
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

    // set CRTC parameters
    MapDmaResult res = mmu_map_dma_mem(fb_base_phys, PDM_FB_SIZE_MAX, false);
    this->fb_ptr = res.host_va;
    this->active_width  = new_width;
    this->active_height = new_height;
    this->hori_blank    = hori_blank;
    this->vert_blank    = vert_blank;
    this->hori_total    = new_width  + hori_blank;
    this->vert_total    = new_height + vert_blank;

    this->set_depth_internal(new_width);

    this->stop_refresh_task();

    // set up video refresh timer
    this->refresh_rate = (double)(this->pixel_clock) / hori_total / vert_total;
    LOG_F(INFO, "PDM-Video: refresh rate set to %f Hz", this->refresh_rate);
    this->start_refresh_task();

    LOG_F(9, "PDM-Video: video enabled");
    this->crtc_on = true;
}

void PdmOnboardVideo::disable_video_internal()
{
    this->stop_refresh_task();
    this->blank_on = true;
    this->blank_display();
    this->blanking = 0x80;
    this->crtc_on = false;
    LOG_F(9, "PDM-Video: video disabled");
}

/** Ariel II has a weird 1bpp mode where a white pixel is mapped to
    CLUT entry #127 (%01111111) and a black pixel to #255 (%11111111).
    It requres a non-standard conversion routine implemented below.
 */
void PdmOnboardVideo::convert_frame_1bpp(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t  *src_buf, *src_row, pix;
    uint32_t *dst_row;
    int      src_pitch, width;
    uint32_t pixels[2];

    // prepare cached ARGB values for white & black pixels
    pixels[0] = this->palette[127];
    pixels[1] = this->palette[255];

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
