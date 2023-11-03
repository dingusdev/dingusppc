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

/** @file Video Conroller base class implementation. */

#include <core/timermanager.h>
#include <devices/common/hwinterrupt.h>
#include <devices/video/videoctrl.h>
#include <memaccess.h>

#include <cinttypes>

VideoCtrlBase::VideoCtrlBase(int width, int height)
{
    EventManager::get_instance()->add_window_handler(this, &VideoCtrlBase::handle_events);

    this->create_display_window(width, height);
}

VideoCtrlBase::~VideoCtrlBase()
{
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

    this->display.update(
        this->convert_fb_cb,
        this->cursor_on, cursor_x, cursor_y);
}

void VideoCtrlBase::start_refresh_task() {
    uint64_t refresh_interval = static_cast<uint64_t>(1.0f / refresh_rate * NS_PER_SEC + 0.5);
    this->refresh_task_id = TimerManager::get_instance()->add_cyclic_timer(
        refresh_interval,
        [this]() {
            // assert VBL interrupt
            if (this->int_ctrl)
                this->int_ctrl->ack_int(this->irq_id, 1);
            this->update_screen();
        }
    );

    uint64_t vbl_duration = static_cast<uint64_t>(1.0f / ((double)(this->pixel_clock) /
        hori_total / vert_blank) * NS_PER_SEC + 0.5);
    this->vbl_end_task_id = TimerManager::get_instance()->add_cyclic_timer(
        refresh_interval,
        refresh_interval + vbl_duration,
        [this]() {
            // deassert VBL interrupt
            if (this->int_ctrl)
                this->int_ctrl->ack_int(this->irq_id, 0);
        }
    );
}

void VideoCtrlBase::stop_refresh_task() {
    if (this->refresh_task_id) {
        TimerManager::get_instance()->cancel_timer(this->refresh_task_id);
    }
    if (this->vbl_end_task_id) {
        TimerManager::get_instance()->cancel_timer(this->vbl_end_task_id);
    }
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

void VideoCtrlBase::setup_hw_cursor(int cursor_width, int cursor_height)
{
    this->display.setup_hw_cursor(
        [this](uint8_t *dst_buf, int dst_pitch) {
            this->draw_hw_cursor(dst_buf, dst_pitch);
        },
        cursor_width, cursor_height);
    this->cursor_on = true;
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
