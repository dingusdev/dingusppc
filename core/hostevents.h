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

#include <cinttypes>
#include <functional>
#include <list>
#include <map>
#include <utility>

enum EventType : uint16_t {
    EVENT_UNKNOWN   = 0,
    EVENT_WINDOW    = 100,
    EVENT_KEYBOARD  = 200,
    EVENT_MOUSE     = 300,
};

class BaseEvent {
public:
    BaseEvent(EventType type) { this->type = type; };
    EventType   type;
};

class WindowEvent : public BaseEvent {
public:
    WindowEvent() : BaseEvent(EventType::EVENT_WINDOW) {};

    uint16_t    sub_type;
    uint32_t    window_id;
};

class EventManager {
public:
    static EventManager* get_instance() {
        if (!event_manager) {
            event_manager = new EventManager();
        }
        return event_manager;
    };

    using EventHandler = std::function<void(const BaseEvent&)>;

    void poll_events();

    void register_handler(const EventType event_type, EventHandler&& eh) {
        this->_handlers[event_type].emplace_back(std::move(eh));
    };

    void post_event(const BaseEvent& event) {
        auto type = event.type;

        if( _handlers.find(type) == _handlers.end() )
            return;

        auto&& handlers = _handlers.at(type);

        for(auto&& handler : handlers)
            handler(event);
    }

private:
    static EventManager* event_manager;
    EventManager() {}; // private constructor to implement a singleton

    std::map<EventType, std::list<EventHandler>> _handlers;

    uint64_t    events_captured = 0;
    uint64_t    unhandled_events = 0;
    uint64_t    key_downs = 0;
    uint64_t    key_ups = 0;
    uint64_t    mouse_motions = 0;
};

#endif // EVENT_MANAGER_H
