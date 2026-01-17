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

/** @file Base class for Apple Desktop Bus devices. */

#ifndef ADB_DEVICE_H
#define ADB_DEVICE_H

#include <devices/common/hwcomponent.h>

#include <string>

/** Common ADB device addresses/types. */
enum {
    ADB_ADDR_KBD    = 2, // keyboards
    ADB_ADDR_RELPOS = 3, // relative position devices (mouse)
    ADB_ADDR_ABSPOS = 4, // absolute position devices (graphic tablets)
};

class AdbBus; // forward declaration to prevent compiler errors

class AdbDevice : public HWComponent {
public:
    AdbDevice(std::string name);
    ~AdbDevice() = default;

    int device_postinit() override;

    virtual void reset() = 0;
    virtual bool talk(const uint8_t dev_addr, const uint8_t reg_num);
    virtual void listen(const uint8_t dev_addr, const uint8_t reg_num);
    virtual uint8_t get_address() { return this->my_addr; }

    // Attempts to poll the device. Returns the talk command corresponding to
    // the device if it has data, 0 otherwise.
    uint8_t poll();

protected:
    uint8_t gen_random_address();

    virtual bool get_register_0() { return false; }
    virtual bool get_register_1() { return false; }
    virtual bool get_register_2() { return false; }
    virtual bool get_register_3();

    virtual void set_register_0() {}
    virtual void set_register_1() {}
    virtual void set_register_2() {}
    virtual void set_register_3() {}

    uint8_t exc_event_flag = 0;
    uint8_t srq_flag       = 0;
    uint8_t led_state      = 0;
    uint8_t my_addr = 0;
    uint8_t dev_handler_id = 0;
    bool    got_collision = false;

    AdbBus* host_obj = nullptr;
};

#endif // ADB_DEVICE_H
