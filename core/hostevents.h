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
    KEYBOARD_EVENT_DOWN = 1 << 0,
    KEYBOARD_EVENT_UP = 1 << 1,
    GAMEPAD_EVENT_DOWN = 1 << 0,
    GAMEPAD_EVENT_UP = 1 << 1,
};

class MouseEvent {
public:
    MouseEvent()  = default;
    ~MouseEvent() = default;

    uint32_t    flags;
    uint32_t    xrel;
    uint32_t    yrel;
    uint32_t    xabs;
    uint32_t    yabs;
    uint8_t     buttons_state;
};

class KeyboardEvent {
public:
    KeyboardEvent() = default;
    ~KeyboardEvent() = default;

    uint32_t flags;
    uint32_t key;
    uint16_t keys_state;
};

/* AppleJack bits 3-7 are supported but unused */
enum GamepadButton : uint8_t {
    Red =          14,
    Green =        15,
    Yellow =       9,
    Blue =         8,

    FrontLeft =    0,
    FrontMiddle =  1,
    FrontRight =   2,

    LeftTrigger =  17,
    RightTrigger = 16,

    Up =           10,
    Down =         13,
    Left =         11,
    Right =        12,
};

class GamepadEvent {
public:
    GamepadEvent() = default;
    ~GamepadEvent() = default;

    uint32_t gamepad_id;
    uint32_t flags;
    uint8_t button;
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
    void add_keyboard_handler(T* inst, void (T::*func)(const KeyboardEvent&)) {
        _keyboard_signal.connect_method(inst, func);
    }

    template <typename T>
    void add_gamepad_handler(T* inst, void (T::*func)(const GamepadEvent&)) {
        _gamepad_signal.connect_method(inst, func);
    }

    template <typename T>
    void add_post_handler(T *inst, void (T::*func)()) {
        _post_signal.connect_method(inst, func);
    }

    void disconnect_handlers() {
        _window_signal.disconnect_all();
        _mouse_signal.disconnect_all();
        _keyboard_signal.disconnect_all();
        _post_signal.disconnect_all();
    }

private:
    static EventManager* event_manager;
    EventManager() {}; // private constructor to implement a singleton

    CoreSignal<const WindowEvent&>     _window_signal;
    CoreSignal<const MouseEvent&>      _mouse_signal;
    CoreSignal<const KeyboardEvent&>   _keyboard_signal;
    CoreSignal<const GamepadEvent&>    _gamepad_signal;
    CoreSignal<>                       _post_signal;

    uint64_t    events_captured = 0;
    uint64_t    unhandled_events = 0;
    uint64_t    key_downs = 0;
    uint64_t    key_ups = 0;
    uint8_t     buttons_state = 0;
};

#endif // EVENT_MANAGER_H
