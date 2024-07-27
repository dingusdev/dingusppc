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

/** @file Display/screen abstraction (implemented on each platform). */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <core/hostevents.h>

#include <functional>
#include <memory>

class Display {
public:
    Display();
    ~Display();

    // Configures the display for the given width/height.
    // Returns true if this is the first time the screen has been configured.
    bool configure(int width, int height);

    // Clears the display
    void blank();


    // Update the host framebuffer display. If the display adapter does its own
    // dirty tracking, fb_known_to_be_changed will be set to true, so that the
    // implementation can take that into account.
    void update(std::function<void(uint8_t *dst_buf, int dst_pitch)> convert_fb_cb,
                std::function<void(uint8_t *dst_buf, int dst_pitch)> cursor_ovl_cb,
                bool draw_hw_cursor, int cursor_x, int cursor_y,
                bool fb_known_to_be_changed);

    // Called in cases where the framebuffer contents have not changed, so a
    // normal update() call is not happening. Allows implementations that need
    // to do per-frame bookkeeping to still do that.
    void update_skipped();

    void handle_events(const WindowEvent& wnd_event);
    void setup_hw_cursor(std::function<void(uint8_t *dst_buf, int dst_pitch)> draw_hw_cursor,
                         int cursor_width, int cursor_height);
private:
    class Impl; // Holds private fields
    std::unique_ptr<Impl> impl;
};

#endif // DISPLAY_H
