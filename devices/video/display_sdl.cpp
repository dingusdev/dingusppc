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
#include <devices/video/videoctrl.h>
#include <SDL.h>
#include <loguru.hpp>
#include <string>

class Display::Impl {
public:
    bool            resizing = false;
    uint32_t        disp_wnd_id = 0;
    SDL_Window*     display_wnd = 0;
    SDL_Renderer*   renderer = 0;
    double          renderer_scale_x; // scaling factor from guest OS to host OS
    double          renderer_scale_y;
    SDL_Texture*    disp_texture = 0;
    SDL_Texture*    cursor_texture = 0;
    SDL_Rect        cursor_rect; // destination rectangle for cursor drawing
    int             display_width;
    int             display_height;
    SDL_Rect        dest_rect;
};

Display::Display(): impl(std::make_unique<Impl>()) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
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

    impl->display_width = width;
    impl->display_height = height;

    if (!impl->display_wnd) { // create display window
        impl->display_wnd = SDL_CreateWindow(
            "",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
        );

        impl->disp_wnd_id = SDL_GetWindowID(impl->display_wnd);
        if (impl->display_wnd == NULL)
            ABORT_F("Display: SDL_CreateWindow failed with %s", SDL_GetError());

        impl->renderer = SDL_CreateRenderer(impl->display_wnd, -1, SDL_RENDERER_ACCELERATED);
        if (impl->renderer == NULL)
            ABORT_F("Display: SDL_CreateRenderer failed with %s", SDL_GetError());

        SDL_RendererInfo info;
        SDL_GetRendererInfo(impl->renderer, &info);
        LOG_F(INFO, "Renderer \"%s\" max size: %d x %d", info.name, info.max_texture_width, info.max_texture_height);

        this->configure_dest();

        update_window_title();
        is_initialization = true;
    } else { // resize display window
        SDL_SetWindowSize(impl->display_wnd, width, height);
        this->configure_dest();
    }

    configure_texture();

    return is_initialization;
}

void Display::configure_dest() {
    int drawable_width, drawable_height;
    SDL_GetRendererOutputSize(impl->renderer, &drawable_width, &drawable_height);

    double scale = std::min(
        static_cast<double>(drawable_width) / impl->display_width,
        static_cast<double>(drawable_height) / impl->display_height
    );

    if (this->full_screen_mode == full_screen_int) {
        double new_scale = floor(scale);
        if (scale == new_scale)
            this->full_screen_mode = full_screen;
        else
            scale = floor(scale);
    }

    LOG_F(INFO, "drawable: %d x %d  display: %d x %d  scale: %f",
        drawable_width, drawable_height, impl->display_width, impl->display_height, scale);

    impl->dest_rect.w = scale * impl->display_width;
    impl->dest_rect.h = scale * impl->display_height;
    impl->dest_rect.x = (drawable_width  - impl->dest_rect.w) / 2,
    impl->dest_rect.y = (drawable_height - impl->dest_rect.h) / 2,
    impl->renderer_scale_x = scale;
    impl->renderer_scale_y = scale;
}

void Display::configure_texture() {
    if (impl->disp_texture)
        SDL_DestroyTexture(impl->disp_texture);

    impl->disp_texture = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        impl->display_width, impl->display_height
    );

    if (impl->disp_texture == NULL)
        ABORT_F("Display: SDL_CreateTexture failed with %s", SDL_GetError());
}

void Display::handle_events(const WindowEvent& wnd_event) {
    switch (wnd_event.sub_type) {

    case SDL_WINDOWEVENT_SIZE_CHANGED:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            this->update_window_title();
            impl->resizing = false;
            video_ctrl->set_draw_fb();
        }
        break;

    case SDL_WINDOWEVENT_EXPOSED:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            SDL_RenderPresent(impl->renderer);
        }
        break;

    case DPPC_WINDOWEVENT_WINDOW_SCALE_QUALITY_TOGGLE:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            auto current_quality = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
            auto new_quality = current_quality == NULL || strcmp(current_quality, "nearest") == 0 ? "best" : "nearest";
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, new_quality);
            // We need the window/texture to be recreated to pick up the hint change.

            this->configure_texture();
            video_ctrl->set_draw_fb();
            video_ctrl->set_cursor_dirty();
        }
        break;

    case DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            switch (this->full_screen_mode) {
            case not_full_screen:
                SDL_SetWindowFullscreen(impl->display_wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
                this->full_screen_mode = full_screen_int;
                break;
            case full_screen_int:
                SDL_SetWindowFullscreen(impl->display_wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
                this->full_screen_mode = full_screen;
                break;
            case full_screen:
                SDL_SetWindowFullscreen(impl->display_wnd, 0);
                this->full_screen_mode = not_full_screen;
                SDL_SetWindowSize(impl->display_wnd, impl->display_width, impl->display_height);
                break;
            }

            this->configure_dest();
            video_ctrl->set_draw_fb();
            video_ctrl->set_cursor_dirty();
        }
        break;

    case DPPC_WINDOWEVENT_MOUSE_GRAB_CHANGED:
        this->update_window_title();
        break;

    }
}

void Display::update_window_title()
{
    std::string old_window_title = SDL_GetWindowTitle(impl->display_wnd);

    int width, height;
    SDL_GetWindowSize(impl->display_wnd, &width, &height);
    bool is_relative = SDL_GetRelativeMouseMode();

    std::string new_window_title = "DingusPPC Display " + std::to_string(width) + "x" + std::to_string(height);
    if (is_relative)
        new_window_title += " (🖱 Grabbed)";

    if (new_window_title != old_window_title)
        SDL_SetWindowTitle(impl->display_wnd, new_window_title.c_str());
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
    SDL_RenderCopy(impl->renderer, impl->disp_texture, NULL, &impl->dest_rect);

    // draw HW cursor if enabled
    if (draw_hw_cursor) {
        impl->cursor_rect.x = cursor_x * impl->renderer_scale_x + impl->dest_rect.x;
        impl->cursor_rect.y = cursor_y * impl->renderer_scale_y + impl->dest_rect.y;
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
    impl->cursor_rect.w = cursor_width * impl->renderer_scale_x;
    impl->cursor_rect.h = cursor_height * impl->renderer_scale_y;
}
