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

/** @file Video Conroller base class definitions. */

#ifndef VIDEO_CTRL_H
#define VIDEO_CTRL_H

#include <devices/common/hwinterrupt.h>
#include <devices/video/display.h>

#include <cinttypes>
#include <functional>

class WindowEvent;

class VideoCtrlBase {
public:
    typedef enum {
        BE = 0,
        LE = 1,
    } fb_endian;

    VideoCtrlBase(int width = 640, int height = 480);
    ~VideoCtrlBase();

    void handle_events(const WindowEvent& wnd_event);
    void create_display_window(int width, int height);
    void blank_display();
    void update_screen(void);
    void set_draw_fb();
    void set_cursor_dirty();

    void start_refresh_task();
    void stop_refresh_task();

    void get_palette_color(uint8_t index, uint8_t& r, uint8_t& g, uint8_t& b,
                           uint8_t& a);
    void set_palette_color(uint8_t index, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    // HW cursor support
    void setup_hw_cursor(int cursor_width=64, int cur_height=64);
    virtual void draw_hw_cursor(uint8_t *dst_buf, int dst_pitch) {}
    virtual void get_cursor_position(int& x, int& y) { x = 0; y = 0; }

    // converters for various framebuffer pixel depths
    void convert_frame_1bpp_indexed(uint8_t *dst_buf, int dst_pitch);
    void convert_frame_2bpp_indexed(uint8_t *dst_buf, int dst_pitch);
    void convert_frame_4bpp_indexed(uint8_t *dst_buf, int dst_pitch);
    void convert_frame_8bpp_indexed(uint8_t *dst_buf, int dst_pitch);
#if 0
    void convert_frame_8bpp_32LE_indexed(uint8_t *dst_buf, int dst_pitch);
#endif
    void convert_frame_8bpp(uint8_t *dst_buf, int dst_pitch);
    template <VideoCtrlBase::fb_endian endian>
    void convert_frame_15bpp(uint8_t *dst_buf, int dst_pitch);
    template <VideoCtrlBase::fb_endian endian>
    void convert_frame_16bpp(uint8_t *dst_buf, int dst_pitch);
    void convert_frame_24bpp(uint8_t *dst_buf, int dst_pitch);
    template <VideoCtrlBase::fb_endian endian>
    void convert_frame_32bpp(uint8_t *dst_buf, int dst_pitch);

protected:
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    virtual bool framebuffer_in_main_memory(void) {
        return false;
    }
    virtual bool needs_swap_endian();
#endif

    // CRT controller parameters
    bool        crtc_on = false;
    bool        blank_on = true;
    bool        cursor_on = false;
    bool        cursor_dirty = false;
    int         active_width;   // width of the visible display area
    int         active_height;  // height of the visible display area
    int         hori_total = 0;
    int         vert_total = 0;
    int         hori_blank = 0;
    int         vert_blank = 0;
    int         pixel_depth;
    int         pixel_format;
    float       pixel_clock;
    float       refresh_rate;

    // Implementations may choose to track framebuffer writes and set draw_fb
    // to false if updates can be skipped. If they do this, they should set
    // draw_fb_is_dynamic at initialization time.
    bool        draw_fb = true;
    bool        draw_fb_is_dynamic = false;

    uint32_t    palette[256] = {0}; // internal DAC palette in RGBA format

    // Framebuffer parameters
    uint8_t*    fb_ptr = nullptr;
    int         fb_pitch = 0;
    uint32_t    refresh_task_id = 0;
    uint32_t    vbl_end_task_id = 0;

    // interrupt suff
    InterruptCtrl* int_ctrl = nullptr;
    uint64_t       irq_id   = 0;
    std::function<void(uint8_t irq_line_state)> vbl_cb = [this](uint8_t irq_line_state) {
        if (this->int_ctrl)
            this->int_ctrl->ack_int(this->irq_id, irq_line_state);
    };

    std::function<void(uint8_t *dst_buf, int dst_pitch)> convert_fb_cb = nullptr;
    std::function<void(uint8_t *dst_buf, int dst_pitch)> cursor_ovl_cb = nullptr;

private:
    Display display;
};

#endif // VIDEO_CTRL_H
