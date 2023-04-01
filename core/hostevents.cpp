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
                this->post_event(we);
            }
            break;

        case SDL_KEYDOWN:
            key_downs++;
            break;

        case SDL_KEYUP:
            key_ups++;
            break;

        case SDL_MOUSEMOTION:
            mouse_motions++;
            break;

        default:
            unhandled_events++;
        }
    }
}
