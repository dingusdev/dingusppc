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

/** @file Video Conroller base class definitions. */

#ifndef VIDEO_CTRL_H
#define VIDEO_CTRL_H

#include <SDL.h>

#include <cinttypes>
#include <functional>

class VideoCtrlBase {
public:
    VideoCtrlBase(int width = 640, int height = 480);
    ~VideoCtrlBase();

    void create_display_window(int width, int height);
    void blank_display();
    void update_screen(void);

    void get_palette_colors(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b,
                            uint8_t& a);
    void set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // converters for various framebuffer pixel depths
    virtual void convert_frame_1bpp(uint8_t *dst_buf, int dst_pitch);
    virtual void convert_frame_8bpp(uint8_t *dst_buf, int dst_pitch);

protected:
    // CRT controller parameters
    bool        crtc_on = false;
    bool        blank_on = true;
    bool        resizing = false;
    int         active_width;   // width of the visible display area
    int         active_height;  // height of the visible display area
    int         hori_total;
    int         vert_total;
    int         pixel_depth;
    float       pixel_clock;
    float       refresh_rate;

    uint32_t    palette[256]; // internal DAC palette in RGBA format

    // Framebuffer parameters
    uint8_t*    fb_ptr;
    int         fb_pitch;
    uint32_t    refresh_task_id = 0;

    std::function<void(uint8_t *dst_buf, int dst_pitch)> convert_fb_cb;

private:
    uint32_t        disp_wnd_id = 0;
    SDL_Window*     display_wnd = 0;
    SDL_Renderer*   renderer = 0;
    SDL_Texture*    disp_texture = 0;
};

#endif // VIDEO_CTRL_H
