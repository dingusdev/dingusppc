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

#ifndef EVENT_MANAGER_H
#define EVENT_MANAGER_H

#include <core/coresignal.h>

#include <cinttypes>

class WindowEvent {
public:
    WindowEvent()  = default;
    ~WindowEvent() = default;

    uint16_t    sub_type;
    uint32_t    window_id;
};

enum : uint32_t {
    MOUSE_EVENT_MOTION = 1 << 0,
    MOUSE_EVENT_BUTTON = 1 << 1,
};

class MouseEvent {
public:
    MouseEvent()  = default;
    ~MouseEvent() = default;

    uint32_t    flags;
    uint32_t    xrel;
    uint32_t    yrel;
    uint8_t     buttons_state;
};

class EventManager {
public:
    static EventManager* get_instance() {
        if (!event_manager) {
            event_manager = new EventManager();
        }
        return event_manager;
    };

    void poll_events();

    template <typename T>
    void add_window_handler(T *inst, void (T::*func)(const WindowEvent&)) {
        _window_signal.connect_method(inst, func);
    }

    template <typename T>
    void add_mouse_handler(T *inst, void (T::*func)(const MouseEvent&)) {
        _mouse_signal.connect_method(inst, func);
    }

    template <typename T>
    void add_post_handler(T *inst, void (T::*func)()) {
        _post_signal.connect_method(inst, func);
    }

private:
    static EventManager* event_manager;
    EventManager() {}; // private constructor to implement a singleton

    CoreSignal<const WindowEvent&>  _window_signal;
    CoreSignal<const MouseEvent&>   _mouse_signal;
    CoreSignal<>                    _post_signal;

    uint64_t    events_captured = 0;
    uint64_t    unhandled_events = 0;
    uint64_t    key_downs = 0;
    uint64_t    key_ups = 0;
    uint64_t    mouse_motions = 0;
};

#endif // EVENT_MANAGER_H
