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

/** @file Video Conroller base class implementation. */

#include "videoctrl.h"
#include <loguru.hpp>
#include <SDL.h>

#include <chrono>
#include <cinttypes>

VideoCtrlBase::VideoCtrlBase(int width, int height)
{
    LOG_F(INFO, "Create display window...");

    this->active_width  = width;
    this->active_height = height;

    // Create display window
    this->display_wnd = SDL_CreateWindow(
        "DingusPPC Display",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        this->active_width,
        this->active_height,
        SDL_WINDOW_OPENGL
    );

    if (this->display_wnd == NULL) {
        LOG_F(ERROR, "Display: SDL_CreateWindow failed with %s", SDL_GetError());
    }

    this->renderer = SDL_CreateRenderer(this->display_wnd, -1, SDL_RENDERER_ACCELERATED);
    if (this->renderer == NULL) {
        LOG_F(ERROR, "Display: SDL_CreateRenderer failed with %s", SDL_GetError());
    }

    SDL_SetRenderDrawColor(this->renderer, 0, 0, 0, 255);
    SDL_RenderClear(this->renderer);
    SDL_RenderPresent(this->renderer);

    // Stupidly poll for 10 events.
    // Otherwise no window will be shown on mac OS!
    SDL_Event e;
    for (int i = 0; i < 10; i++) {
        SDL_PollEvent(&e);
    }

    this->disp_texture = SDL_CreateTexture(
        this->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );

    if (this->disp_texture == NULL) {
        LOG_F(ERROR, "Display: SDL_CreateTexture failed with %s", SDL_GetError());
    }
}

VideoCtrlBase::~VideoCtrlBase()
{
    if (this->disp_texture) {
        SDL_DestroyTexture(this->disp_texture);
    }

    if (this->renderer) {
        SDL_DestroyRenderer(this->renderer);
    }

    if (this->display_wnd) {
        SDL_DestroyWindow(this->display_wnd);
    }
}

void VideoCtrlBase::update_screen()
{
    uint8_t*    dst_buf;
    int         dst_pitch;

    //auto start_time = std::chrono::steady_clock::now();

    SDL_LockTexture(this->disp_texture, NULL, (void **)&dst_buf, &dst_pitch);

    // texture update callback to get ARGB data from guest framebuffer
    this->convert_fb_cb(dst_buf, dst_pitch);

    SDL_UnlockTexture(this->disp_texture);
    SDL_RenderClear(this->renderer);
    SDL_RenderCopy(this->renderer, this->disp_texture, NULL, NULL);
    SDL_RenderPresent(this->renderer);

    // HW cursor data is stored at the beginning of the video memory
    // HACK: use src_offset to recognize cursor data being ready
    // Normally, we should check GEN_CUR_ENABLE bit in the GEN_TEST_CNTL register
    //if (src_offset > 0x400 && READ_DWORD_LE_A(&this->block_io_regs[ATI_CUR_OFFSET])) {
    //    this->draw_hw_cursor(dst_buf + dst_pitch * 20 + 120, dst_pitch);
    //}

    //auto end_time = std::chrono::steady_clock::now();
    //auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    //LOG_F(INFO, "Display uodate took: %lld ns", time_elapsed.count());
}

void VideoCtrlBase::get_palette_colors(uint8_t index, uint8_t& r, uint8_t& g,
                                       uint8_t& b, uint8_t& a)
{
    b =  this->palette[index] & 0xFFU;
    g = (this->palette[index] >>  8) & 0xFFU;
    r = (this->palette[index] >> 16) & 0xFFU;
    a = (this->palette[index] >> 24) & 0xFFU;
}

void VideoCtrlBase::set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    this->palette[index] = (a << 24) | (r << 16) | (g << 8) | b;
}

void VideoCtrlBase::convert_frame_1bpp(uint8_t *dst_buf, int dst_pitch)
{
    // TODO: implement me!
}

void VideoCtrlBase::convert_frame_8bpp(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_buf, *src_row, *dst_row;
    int     src_pitch;

    src_buf   = this->fb_ptr;
    src_pitch = this->fb_pitch;

    for (int h = 0; h < this->active_height; h++) {
        src_row = &src_buf[h * src_pitch];
        dst_row = &dst_buf[h * dst_pitch];

        for (int x = 0; x < this->active_width; x++) {
            WRITE_DWORD_LE_A(dst_row, this->palette[src_row[x]]);
            dst_row += 4;
        }
    }
}
