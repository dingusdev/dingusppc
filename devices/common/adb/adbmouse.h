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

/** @file Apple Desktop Bus Mouse definitions. */

#ifndef ADB_MOUSE_H
#define ADB_MOUSE_H

#include <devices/common/adb/adbdevice.h>
#include <devices/common/hwcomponent.h>

#include <memory>
#include <string>

//#define ABSOLUTE

class MouseEvent;

class AdbMouse : public AdbDevice {
public:
enum DeviceClass {
    TABLET      = 0,
    MOUSE       = 1,
    TRACKBALL   = 2,
};

    AdbMouse(
        std::string name, uint8_t device_class, int num_buttons, int num_bits, uint16_t resolution);
    ~AdbMouse() = default;

    static std::unique_ptr<HWComponent> create() {
#ifdef ABSOLUTE
        uint8_t  device_class = TABLET;
        int      num_buttons = 3;
        int      num_bits = 16;
        uint16_t resolution = 72;
#else
        uint8_t  device_class = MOUSE;
        int      num_buttons = 3;
        int      num_bits = 10;
        uint16_t resolution = 300;
#endif
        return std::unique_ptr<AdbMouse>(
            new AdbMouse("ADB-MOUSE", device_class, num_buttons, num_bits, resolution));
    }

    void reset() override;
    void event_handler(const MouseEvent& event);

    bool get_register_0() override;
    bool get_register_1() override;
    void set_register_3() override;

protected:
    bool get_register_0(uint8_t buttons_state, bool force);
    uint8_t get_buttons_state() const;

private:
    int32_t  x_rel = 0;
    int32_t  y_rel = 0;
    int32_t  x_abs = 0;
    int32_t  y_abs = 0;
    uint8_t  buttons_state = 0;
    bool     changed = false;
    uint8_t  device_class;
    int      num_buttons;
    int      num_bits;
    uint16_t resolution;
};

#endif // ADB_MOUSE_H
