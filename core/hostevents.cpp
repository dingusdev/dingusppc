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

#include <core/hostevents.h>
#include <cpu/ppc/ppcemu.h>
#include <loguru.hpp>
#include <SDL.h>

EventManager* EventManager::event_manager;

void EventManager::poll_events()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        events_captured++;

        switch (event.type) {
        case SDL_QUIT:
            power_on = false;
            break;

        case SDL_WINDOWEVENT: {
                WindowEvent we;
                we.sub_type  = event.window.event;
                we.window_id = event.window.windowID;
                this->_window_signal.emit(we);
            }
            break;

        case SDL_KEYDOWN: {
                KeyboardEvent ke;
                ke.key_code = event.key.keysym.sym;
                ke.flags    = KEYBOARD_EVENT_DOWN;
                this->_keyboard_signal.emit(ke);
                key_downs++;
            }
            break;

        case SDL_KEYUP: {
                KeyboardEvent ke;
                ke.flags = KEYBOARD_EVENT_UP;
                this->_keyboard_signal.emit(ke);
                key_ups++;
            }
            break;

        case SDL_MOUSEMOTION: {
                MouseEvent me;
                me.xrel  = event.motion.xrel;
                me.yrel  = event.motion.yrel;
                me.flags = MOUSE_EVENT_MOTION;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_MOUSEBUTTONDOWN: {
                MouseEvent me;
                me.buttons_state = 1;
                me.flags = MOUSE_EVENT_BUTTON;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_MOUSEBUTTONUP: {
                MouseEvent me;
                me.buttons_state = 0;
                me.flags = MOUSE_EVENT_BUTTON;
                this->_mouse_signal.emit(me);
            }
            break;

        default:
            unhandled_events++;
        }
    }

    // perform post-processing
    this->_post_signal.emit();
}
