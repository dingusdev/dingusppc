/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file Apple Desktop Bus AppleJack controller definitions. */

#ifndef ADB_APPLEJACK_H
#define ADB_APPLEJACK_H

#include <devices/common/adb/adbmouse.h>
#include <devices/common/hwcomponent.h>

#include <memory>
#include <string>

enum : uint8_t {
    APPLEJACK_HANDLER_ID = 0x46,
};

class GamepadEvent;

class AdbAppleJack : public AdbMouse {
public:
    AdbAppleJack(std::string name);
    ~AdbAppleJack() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AdbAppleJack>(new AdbAppleJack("ADB-APPLEJACK"));
    }

    void reset() override;
    void event_handler(const GamepadEvent& event);

    bool get_register_0() override;
    void set_register_3() override;

private:
    uint32_t buttons_state = 0;
    bool triggers_changed = false;
    bool buttons_changed = false;
};

#endif // ADB_APPLEJACK_H
