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

/** @file Video Controller base class implementation. */

#include <core/timermanager.h>
#include <devices/common/hwinterrupt.h>
#include <devices/video/videoctrl.h>
#include <devices/memctrl/memctrlbase.h>
#include <memaccess.h>

#include <cinttypes>

VideoCtrlBase::VideoCtrlBase(int width, int height)
{
    EventManager::get_instance()->add_window_handler(this, &VideoCtrlBase::handle_events);

    this->create_display_window(width, height);
    this->display.set_video_ctrl(this);
}

VideoCtrlBase::~VideoCtrlBase()
{
    this->stop_refresh_task();
}

void VideoCtrlBase::handle_events(const WindowEvent& wnd_event) {
    this->display.handle_events(wnd_event);
}

// TODO: consider renaming, since it's not always a window
void VideoCtrlBase::create_display_window(int width, int height)
{
    bool is_initialization = this->display.configure(width, height);
    if (is_initialization) {
        this->blank_on = true; // TODO: should be true!
        this->blank_display();
    }

    this->active_width  = width;
    this->active_height = height;
}

void VideoCtrlBase::blank_display() {
    this->display.blank();
}

void VideoCtrlBase::update_screen()
{
    if (this->blank_on) {
        this->display.blank();
        return;
    }

    int cursor_x = 0;
    int cursor_y = 0;
    if (this->cursor_on) {
        this->get_cursor_position(cursor_x, cursor_y);
    }

    if (this->draw_fb) {
        if (this->cursor_dirty) {
            this->setup_hw_cursor();
            this->cursor_dirty = false;
        }
        this->display.update(
            this->convert_fb_cb, this->cursor_ovl_cb,
            this->cursor_on, cursor_x, cursor_y,
            this->draw_fb_is_dynamic);
    } else if (this->draw_fb_is_dynamic) {
        this->display.update_skipped();
    }
}

void VideoCtrlBase::set_draw_fb() {
    this->draw_fb = true;
}

void VideoCtrlBase::set_cursor_dirty() {
    this->cursor_dirty = true;
}

void VideoCtrlBase::start_refresh_task() {
    this->display.configure(this->active_width, this->active_height);

    uint64_t refresh_interval = static_cast<uint64_t>(1.0f / refresh_rate * NS_PER_SEC + 0.5);
    this->refresh_task_id = TimerManager::get_instance()->add_cyclic_timer(
        refresh_interval,
        [this]() {
            // assert VBL interrupt
            this->vbl_cb(1);
            this->update_screen();
        }
    );

    if (vert_blank == 0) {
        LOG_F(ERROR, "Vertical blank is 0. Using 25 instead.");
        vert_blank = 25;
    }

    uint64_t vbl_duration = static_cast<uint64_t>((double)hori_total * vert_blank / this->pixel_clock * NS_PER_SEC + 0.5);
    this->vbl_end_task_id = TimerManager::get_instance()->add_cyclic_timer(
        refresh_interval,
        refresh_interval + vbl_duration,
        [this]() {
            // deassert VBL interrupt
            this->vbl_cb(0);
        }
    );
}

void VideoCtrlBase::stop_refresh_task() {
    if (this->refresh_task_id) {
        TimerManager::get_instance()->cancel_timer(this->refresh_task_id);
        this->refresh_task_id = 0;
    }
    if (this->vbl_end_task_id) {
        TimerManager::get_instance()->cancel_timer(this->vbl_end_task_id);
        this->vbl_end_task_id = 0;
    }
}

void VideoCtrlBase::get_palette_color(uint8_t index, uint8_t& r, uint8_t& g,
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

void VideoCtrlBase::setup_hw_cursor(int cursor_width, int cursor_height)
{
    this->display.setup_hw_cursor(
        [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_hw_cursor(dst_buf, dst_pitch);
        },
        cursor_width, cursor_height);
    this->cursor_on = true;
}

void VideoCtrlBase::convert_frame_1bpp_indexed(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - ((this->active_width + 7) >> 3);
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr - 1;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        uint8_t bit = 0x00;
        uint8_t c;
        for (int x = this->active_width; x > 0; x--) {
            if (!bit) {
                src_row += 1;
                bit = 0x80;
                c = *src_row;
            }
            WRITE_DWORD_LE_A(dst_row, this->palette[!!(c & bit)]);
            bit >>= 1;
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

void VideoCtrlBase::convert_frame_2bpp_indexed(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - (this->active_width >> 2);
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        uint8_t c;
        for (int x = this->active_width >> 2; x > 0; x--) {
            c = *src_row;
            WRITE_DWORD_LE_A(dst_row, this->palette[c >> 6]);
            dst_row += 4;
            WRITE_DWORD_LE_A(dst_row, this->palette[(c >> 4) & 3]);
            dst_row += 4;
            WRITE_DWORD_LE_A(dst_row, this->palette[(c >> 2) & 3]);
            dst_row += 4;
            WRITE_DWORD_LE_A(dst_row, this->palette[c & 3]);
            dst_row += 4;
            src_row += 1;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

void VideoCtrlBase::convert_frame_4bpp_indexed(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - (this->active_width >> 1);
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        uint8_t c;
        for (int x = this->active_width >> 1; x > 0; x--) {
            c = *src_row;
            WRITE_DWORD_LE_A(dst_row, this->palette[c >> 4]);
            dst_row += 4;
            WRITE_DWORD_LE_A(dst_row, this->palette[c & 15]);
            dst_row += 4;
            src_row += 1;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

void VideoCtrlBase::convert_frame_8bpp_indexed(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            WRITE_DWORD_LE_A(dst_row, this->palette[*src_row++]);
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

#if 0
// alternative version to the above but only works for little endian host
void VideoCtrlBase::convert_frame_8bpp_32LE_indexed(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *dst_row;
    uint32_t *src_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = (uint32_t*)this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width >> 2; x > 0; x--) {
            uint32_t pixels = *src_row++;
            WRITE_DWORD_LE_A(dst_row     , this->palette[(uint8_t)(pixels      )]);
            WRITE_DWORD_LE_A(dst_row +  4, this->palette[(uint8_t)(pixels >>  8)]);
            WRITE_DWORD_LE_A(dst_row +  8, this->palette[(uint8_t)(pixels >> 16)]);
            WRITE_DWORD_LE_A(dst_row + 12, this->palette[(uint8_t)(pixels >> 24)]);
            dst_row += 16;
        }
        src_row = (uint32_t*)((uint8_t*)src_row + src_pitch);
        dst_row += dst_pitch;
    }
}
#endif

// RGB332
void VideoCtrlBase::convert_frame_8bpp(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            uint32_t c = *src_row++;
            uint32_t r = ((c << 16) & 0x00E00000) | ((c << 13) & 0x001C0000) | ((c << 10) & 0x00030000);
            uint32_t g = ((c << 11) & 0x0000E000) | ((c <<  8) & 0x00001C00) | ((c <<  5) & 0x00000300);
            uint32_t b = ((c <<  6) & 0x000000C0) | ((c <<  4) & 0x00000030) | ((c <<  2) & 0x0000000C) | (c & 0x00000003);
            WRITE_DWORD_LE_A(dst_row, r | g | b);
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
bool VideoCtrlBase::needs_swap_endian() {
    extern MemCtrlBase* mem_ctrl_instance;
    return mem_ctrl_instance->needs_swap_endian(!this->framebuffer_in_main_memory());
}
#endif

// RGB555
template <VideoCtrlBase::fb_endian endian>
void VideoCtrlBase::convert_frame_15bpp(uint8_t *dst_buf, int dst_pitch, bool swapper)
{
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    if (needs_swap_endian() && !swapper) {
        convert_frame_15bpp<endian == BE ? LE : BE>(dst_buf, dst_pitch, true);
        return;
    }
#endif

    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - 2 * this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            uint32_t c = (endian == BE) ? READ_WORD_BE_A(src_row) : READ_WORD_LE_A(src_row);
            uint32_t r = ((c << 9) & 0x00F80000) | ((c << 4) & 0x00070000);
            uint32_t g = ((c << 6) & 0x0000F800) | ((c << 1) & 0x00000700);
            uint32_t b = ((c << 3) & 0x000000F8) | ((c >> 2) & 0x00000007);
            WRITE_DWORD_LE_A(dst_row, r | g | b);
            src_row += 2;
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}
template void VideoCtrlBase::convert_frame_15bpp<VideoCtrlBase::BE>(uint8_t *dst_buf, int dst_pitch, bool swapper);
template void VideoCtrlBase::convert_frame_15bpp<VideoCtrlBase::LE>(uint8_t *dst_buf, int dst_pitch, bool swapper);

// RGB565
template <VideoCtrlBase::fb_endian endian>
void VideoCtrlBase::convert_frame_16bpp(uint8_t *dst_buf, int dst_pitch, bool swapper)
{
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    if (needs_swap_endian() && !swapper) {
        convert_frame_16bpp<endian == BE ? LE : BE>(dst_buf, dst_pitch, true);
        return;
    }
#endif

    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - 2 * this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            uint32_t c = (endian == BE) ? READ_WORD_BE_A(src_row) : READ_WORD_LE_A(src_row);
            uint32_t r = ((c << 8) & 0x00F80000) | ((c << 3) & 0x00070000);
            uint32_t g = ((c << 5) & 0x0000FC00) | ((c >> 1) & 0x00000300);
            uint32_t b = ((c << 3) & 0x000000F8) | ((c >> 2) & 0x00000007);
            WRITE_DWORD_LE_A(dst_row, r | g | b);
            src_row += 2;
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}
template void VideoCtrlBase::convert_frame_16bpp<VideoCtrlBase::BE>(uint8_t *dst_buf, int dst_pitch, bool swapper);
template void VideoCtrlBase::convert_frame_16bpp<VideoCtrlBase::LE>(uint8_t *dst_buf, int dst_pitch, bool swapper);


// RGB888
void VideoCtrlBase::convert_frame_24bpp(uint8_t *dst_buf, int dst_pitch)
{
    uint8_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - 3 * this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = this->fb_ptr;
    dst_row = dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            uint32_t c = (src_row[0] << 16) | (src_row[1] << 8) | src_row[2];
            WRITE_DWORD_LE_A(dst_row, c);
            src_row += 3;
            dst_row += 4;
        }
        src_row += src_pitch;
        dst_row += dst_pitch;
    }
}

// ARGB8888
template <VideoCtrlBase::fb_endian endian>
void VideoCtrlBase::convert_frame_32bpp(uint8_t *dst_buf, int dst_pitch, bool swapper)
{
#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    if (needs_swap_endian() && !swapper) {
        convert_frame_32bpp<endian == BE ? LE : BE>(dst_buf, dst_pitch, true);
        return;
    }
#endif

    uint32_t *src_row, *dst_row;
    int     src_pitch;

    src_pitch = this->fb_pitch - 4 * this->active_width;
    dst_pitch = dst_pitch - 4 * this->active_width;

    src_row = (uint32_t*)this->fb_ptr;
    dst_row = (uint32_t*)dst_buf;
    for (int h = this->active_height; h > 0; h--) {
        for (int x = this->active_width; x > 0; x--) {
            uint32_t c = (endian == BE) ? READ_DWORD_BE_A(src_row) : READ_DWORD_LE_A(src_row);
            WRITE_DWORD_LE_A(dst_row, c);
            src_row++;
            dst_row++;
        }
        src_row = (uint32_t*)((uint8_t*)src_row + src_pitch);
        dst_row = (uint32_t*)((uint8_t*)dst_row + dst_pitch);
    }
}
template void VideoCtrlBase::convert_frame_32bpp<VideoCtrlBase::BE>(uint8_t *dst_buf, int dst_pitch, bool swapper);
template void VideoCtrlBase::convert_frame_32bpp<VideoCtrlBase::LE>(uint8_t *dst_buf, int dst_pitch, bool swapper);
