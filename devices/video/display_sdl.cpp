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

#include <devices/video/display.h>
#include <SDL.h>
#include <loguru.hpp>

class Display::Impl {
public:
    bool            resizing = false;
    uint32_t        disp_wnd_id = 0;
    SDL_Window*     display_wnd = 0;
    SDL_Renderer*   renderer = 0;
    SDL_Texture*    disp_texture = 0;
    SDL_Texture*    cursor_texture = 0;
    SDL_Rect        cursor_rect; // destination rectangle for cursor drawing
};

Display::Display(): impl(std::make_unique<Impl>()) {
}

Display::~Display() {
    if (impl->cursor_texture)
        SDL_DestroyTexture(impl->cursor_texture);

    if (impl->disp_texture)
        SDL_DestroyTexture(impl->disp_texture);

    if (impl->renderer)
        SDL_DestroyRenderer(impl->renderer);

    if (impl->display_wnd)
        SDL_DestroyWindow(impl->display_wnd);
}

bool Display::configure(int width, int height) {
    bool is_initialization = false;

    if (!impl->display_wnd) { // create display window
        impl->display_wnd = SDL_CreateWindow(
            SDL_GetRelativeMouseMode() ?
                "DingusPPC Display (Mouse Grabbed)" : "DingusPPC Display",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width, height,
            SDL_WINDOW_OPENGL
        );

        impl->disp_wnd_id = SDL_GetWindowID(impl->display_wnd);
        if (impl->display_wnd == NULL)
            ABORT_F("Display: SDL_CreateWindow failed with %s", SDL_GetError());

        impl->renderer = SDL_CreateRenderer(impl->display_wnd, -1, SDL_RENDERER_ACCELERATED);
        if (impl->renderer == NULL)
            ABORT_F("Display: SDL_CreateRenderer failed with %s", SDL_GetError());

        is_initialization = true;
    } else { // resize display window
        SDL_SetWindowSize(impl->display_wnd, width, height);
    }

    if (impl->disp_texture)
        SDL_DestroyTexture(impl->disp_texture);

    impl->disp_texture = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width, height
    );

    if (impl->disp_texture == NULL)
        ABORT_F("Display: SDL_CreateTexture failed with %s", SDL_GetError());

    return is_initialization;
}

void Display::handle_events(const WindowEvent& wnd_event) {
    if (wnd_event.sub_type == SDL_WINDOWEVENT_SIZE_CHANGED &&
        wnd_event.window_id == impl->disp_wnd_id)
        impl->resizing = false;
    if (wnd_event.sub_type == SDL_WINDOWEVENT_EXPOSED &&
        wnd_event.window_id == impl->disp_wnd_id)
        SDL_RenderPresent(impl->renderer);
}

void Display::blank() {
    SDL_SetRenderDrawColor(impl->renderer, 0, 0, 0, 255);
    SDL_RenderClear(impl->renderer);
    SDL_RenderPresent(impl->renderer);
}

void Display::update(std::function<void(uint8_t *dst_buf, int dst_pitch)> convert_fb_cb,
                     std::function<void(uint8_t *dst_buf, int dst_pitch)> cursor_ovl_cb,
                     bool draw_hw_cursor, int cursor_x, int cursor_y,
                     bool fb_known_to_be_changed) {
    if (impl->resizing)
        return;

    uint8_t*    dst_buf;
    int         dst_pitch;

    SDL_LockTexture(impl->disp_texture, NULL, (void **)&dst_buf, &dst_pitch);

    // texture update callback to get ARGB data from guest framebuffer
    convert_fb_cb(dst_buf, dst_pitch);

    // overlay cursor data if requested
    if (cursor_ovl_cb != nullptr)
        cursor_ovl_cb(dst_buf, dst_pitch);

    SDL_UnlockTexture(impl->disp_texture);
    SDL_RenderClear(impl->renderer);
    SDL_RenderCopy(impl->renderer, impl->disp_texture, NULL, NULL);

    // draw HW cursor if enabled
    if (draw_hw_cursor) {
        impl->cursor_rect.x = cursor_x;
        impl->cursor_rect.y = cursor_y;
        SDL_RenderCopy(impl->renderer, impl->cursor_texture, NULL, &impl->cursor_rect);
    }

    SDL_RenderPresent(impl->renderer);
}

void Display::update_skipped() {
    // SDL implementation does not care about skipped updates.
}

void Display::setup_hw_cursor(std::function<void(uint8_t *dst_buf, int dst_pitch)> draw_hw_cursor,
                              int cursor_width, int cursor_height) {
    uint8_t*    dst_buf;
    int         dst_pitch;

    if (impl->cursor_texture)
        SDL_DestroyTexture(impl->cursor_texture);

    impl->cursor_texture = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        cursor_width, cursor_height
    );

    if (impl->cursor_texture == NULL)
        ABORT_F("SDL_CreateTexture for HW cursor failed with %s", SDL_GetError());

    SDL_LockTexture(impl->cursor_texture, NULL, (void **)&dst_buf, &dst_pitch);
    SDL_SetTextureBlendMode(impl->cursor_texture, SDL_BLENDMODE_BLEND);
    draw_hw_cursor(dst_buf, dst_pitch);
    SDL_UnlockTexture(impl->cursor_texture);

    impl->cursor_rect.x = 0;
    impl->cursor_rect.y = 0;
    impl->cursor_rect.w = cursor_width;
    impl->cursor_rect.h = cursor_height;
}
