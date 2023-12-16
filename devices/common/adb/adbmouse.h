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

class MouseEvent;

class AdbMouse : public AdbDevice {
public:
    AdbMouse(std::string name);
    ~AdbMouse() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AdbMouse>(new AdbMouse("ADB-MOUSE"));
    }

    void reset() override;
    void event_handler(const MouseEvent& event);

    bool get_register_0() override;
    bool get_register_1() override;
    void set_register_3() override;

private:
    int32_t x_rel = 0;
    int32_t y_rel = 0;
    uint8_t buttons_state = 0;
    bool    changed = false;
};

#endif // ADB_MOUSE_H
