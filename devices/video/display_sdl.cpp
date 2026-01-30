/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <cmath>
#include <string>

static const char * get_full_screen_mode_string(int scale_mode) {
#define onemode(x) case Display::x: return #x ;
    switch(scale_mode) {
        onemode(not_full_screen)
        onemode(full_screen_int)
        onemode(full_screen)
        onemode(full_screen_no_bars)
        default: return "unknown";
    }
}

class Display::Impl {
public:
    uint32_t        disp_wnd_id = 0;
    SDL_Window*     display_wnd = 0;
    SDL_Renderer*   renderer = 0;
    double          renderer_scale_x; // scaling factor from guest OS to host OS
    double          renderer_scale_y;
    double          default_scale_x;
    double          default_scale_y;
    SDL_Texture*    disp_texture = 0;
    SDL_Texture*    cursor_texture = 0;
    SDL_Rect        cursor_rect; // destination rectangle for cursor drawing
    int             display_w;
    int             display_h;
    double          drawable_w;
    double          drawable_h;
    SDL_Rect        dest_rect;
};

Display::Display(): impl(std::make_unique<Impl>()) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
}

Display::~Display() {
    if (impl->cursor_texture) {
        SDL_DestroyTexture(impl->cursor_texture);
        impl->cursor_texture = 0;
    }

    if (impl->disp_texture) {
        SDL_DestroyTexture(impl->disp_texture);
        impl->disp_texture = 0;
    }

    if (impl->renderer) {
        SDL_DestroyRenderer(impl->renderer);
        impl->renderer = 0;
    }

    if (impl->display_wnd) {
        SDL_DestroyWindow(impl->display_wnd);
        impl->display_wnd = 0;
    }
}

const double scale_step = std::pow(2.0, 1.0/8.0);

bool Display::configure(int width, int height) {
    bool is_initialization = false;

    impl->display_w = width;
    impl->display_h = height;

    if (!impl->display_wnd) { // create display window
        impl->display_wnd = SDL_CreateWindow(
            "",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            impl->display_w, impl->display_h,
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

        int w, h;
        double scale = 1.0;
        while (1) {
            SDL_GetRendererOutputSize(impl->renderer, &w, &h);
            if (w == 0 || h == 0) {
                scale /= scale_step;
                LOG_F(INFO, "Invalid renderer size. Reducing scale to %.3f.", scale);
                SDL_SetWindowSize(impl->display_wnd,
                    std::round(scale * impl->display_w),
                    std::round(scale * impl->display_h));
            } else
                break;
        }
        impl->default_scale_x = std::round(double(w) / std::round(scale * impl->display_w));
        impl->default_scale_y = std::round(double(h) / std::round(scale * impl->display_h));
        impl->drawable_w = scale * impl->display_w * impl->default_scale_x;
        impl->drawable_h = scale * impl->display_h * impl->default_scale_y;
        this->scale_window = scale * impl->default_scale_x;
        this->configure_dest();

        update_window_title();
        is_initialization = true;
    } else { // resize display window
        this->update_window_size();
        this->configure_dest();
    }

    configure_texture();

    return is_initialization;
}

void Display::update_window_size() {
    if (this->full_screen_mode != not_full_screen)
        return;

    int w, h;
    double w_err, h_err;
    while (1) {
        impl->drawable_w = this->scale_window * impl->display_w;
        impl->drawable_h = this->scale_window * impl->display_h;

        SDL_SetWindowSize(impl->display_wnd,
            std::round(impl->drawable_w / impl->default_scale_x),
            std::round(impl->drawable_h / impl->default_scale_y));

        SDL_GetRendererOutputSize(impl->renderer, &w, &h);

        w_err = std::abs(impl->drawable_w - w);
        h_err = std::abs(impl->drawable_h - h);

        LOG_F(INFO, "update_window_size try drawable: %4.0f x %-4.0f  display: %4d x %-4d  scale: %.3f  err: %4.0f x %-4.0f",
            impl->drawable_w, impl->drawable_h, impl->display_w, impl->display_h, this->scale_window, w_err, h_err);

        if (w_err < 4 && h_err < 4)
            break;

        this->scale_window /= scale_step;
    }

    LOG_F(INFO, "update_window_size     drawable: %4.0f x %-4.0f  display: %4d x %-4d  scale: %.3f",
        impl->drawable_w, impl->drawable_h, impl->display_w, impl->display_h, this->scale_window);

    this->update_mouse_grab(false); // make sure the mouse is still inside the window
}

void Display::configure_dest() {
    bool should_set_full_screen = this->full_screen_mode > not_full_screen;
    if (this->is_set_full_screen != should_set_full_screen) {
        if (should_set_full_screen) {
            int w, h;
            SDL_SetWindowFullscreen(impl->display_wnd, SDL_WINDOW_FULLSCREEN_DESKTOP);
            SDL_GetRendererOutputSize(impl->renderer, &w, &h);
            impl->drawable_w = w;
            impl->drawable_h = h;
        } else {
            SDL_SetWindowFullscreen(impl->display_wnd, 0);
            this->update_window_size();
        }
        this->is_set_full_screen = should_set_full_screen;
    }

    double scalex = impl->drawable_w / impl->display_w;
    double scaley = impl->drawable_h / impl->display_h;
    double scale_full = std::min(scalex, scaley);
    double scale_full_int = floor(scale_full);
    double scale_full_no_bars = std::max(scalex, scaley);
    double scale;

    if (this->full_screen_mode == not_full_screen) {
        scale = this->scale_window = scale_full;
        this->full_screen_mode_forward = full_screen_int;
        this->full_screen_mode_reverse = full_screen_no_bars;
    } else {
        if (!use_scale_full_screen || this->scale_full_screen == 0.0) {
            switch (this->full_screen_mode) {
                case full_screen_int:
                    this->scale_full_screen = scale_full_int;
                    break;
                case full_screen:
                    this->scale_full_screen = scale_full;
                    break;
                case full_screen_no_bars:
                    this->scale_full_screen = scale_full_no_bars;
                    break;
            }
        }
        scale = this->scale_full_screen;

        if (scale == scale_full) {
            // scale_full is preferable to scale_full_int
            this->full_screen_mode = full_screen;
        }

        if (scale >= scale_full_no_bars)
            this->full_screen_mode_forward = not_full_screen;
        else if (scale >= scale_full)
            this->full_screen_mode_forward = full_screen_no_bars;
        else if (scale >= scale_full_int)
            this->full_screen_mode_forward = full_screen;
        else
            this->full_screen_mode_forward = full_screen_int;

        if (scale <= scale_full_int)
            this->full_screen_mode_reverse = not_full_screen;
        else if (scale <= scale_full)
            this->full_screen_mode_reverse = full_screen_int;
        else if (scale <= scale_full_no_bars)
            this->full_screen_mode_reverse = full_screen;
        else
            this->full_screen_mode_reverse = full_screen_no_bars;
    }

    impl->dest_rect.w = std::round(scale * impl->display_w);
    impl->dest_rect.h = std::round(scale * impl->display_h);
    impl->dest_rect.x = std::round((impl->drawable_w - impl->dest_rect.w) / 2.0),
    impl->dest_rect.y = std::round((impl->drawable_h - impl->dest_rect.h) / 2.0),
    impl->renderer_scale_x = scale;
    impl->renderer_scale_y = scale;

    LOG_F(INFO, "configure_dest         drawable: %4.0f x %-4.0f  display: %4d x %-4d  scale: %.3f"
        " (int: %.3f  full: %.3f  nobars: %.3f)  mode: %s  forward: %s  reverse: %s",
        impl->drawable_w, impl->drawable_h, impl->display_w, impl->display_h, impl->renderer_scale_x,
        scale_full_int, scale_full, scale_full_no_bars,
        get_full_screen_mode_string(this->full_screen_mode),
        get_full_screen_mode_string(this->full_screen_mode_forward),
        get_full_screen_mode_string(this->full_screen_mode_reverse)
    );
}

void Display::configure_texture() {
    if (impl->disp_texture)
        SDL_DestroyTexture(impl->disp_texture);

    impl->disp_texture = SDL_CreateTexture(
        impl->renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        impl->display_w, impl->display_h
    );

    if (impl->disp_texture == NULL)
        ABORT_F("Display: SDL_CreateTexture failed with %s", SDL_GetError());
}

void Display::handle_events(const WindowEvent& wnd_event) {
    switch (wnd_event.sub_type) {

    case SDL_WINDOWEVENT_SIZE_CHANGED:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            int ww, wh;
            SDL_GetWindowSize(impl->display_wnd, &ww, &wh);
            int w, h;
            SDL_GetRendererOutputSize(impl->renderer, &w, &h);
            double new_default_scale_x = w / ww;
            double new_default_scale_y = w / ww;
            if (new_default_scale_x != impl->default_scale_x || new_default_scale_y != impl->default_scale_y) {
                // recalculate scale when switching between Retina and Low Resolution modes
                impl->drawable_w = impl->drawable_w * new_default_scale_x / impl->default_scale_x;
                impl->drawable_h = impl->drawable_h * new_default_scale_y / impl->default_scale_y;
                this->scale_window = this->scale_window * new_default_scale_x / impl->default_scale_x;
                impl->default_scale_x = new_default_scale_x;
                impl->default_scale_y = new_default_scale_y;
                this->configure_dest();
            }

            this->update_window_title();
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
    case DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE_REVERSE:
        {
            if (wnd_event.window_id != impl->disp_wnd_id)
                break;

            if (wnd_event.sub_type == DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE)
                this->full_screen_mode = this->full_screen_mode_forward;
            else
                this->full_screen_mode = this->full_screen_mode_reverse;

            if (this->full_screen_mode > not_full_screen)
                this->use_scale_full_screen = false;

            this->configure_dest();
            video_ctrl->set_draw_fb();
            video_ctrl->set_cursor_dirty();
        }
        break;

    case DPPC_WINDOWEVENT_WINDOW_BIGGER:
    case DPPC_WINDOWEVENT_WINDOW_SMALLER:
        if (wnd_event.window_id == impl->disp_wnd_id) {
            if (this->full_screen_mode == not_full_screen) {
                if (wnd_event.sub_type == DPPC_WINDOWEVENT_WINDOW_BIGGER)
                    this->scale_window *= scale_step;
                else
                    this->scale_window /= scale_step;
                this->update_window_size();
            } else {
                this->use_scale_full_screen = true;
                if (wnd_event.sub_type == DPPC_WINDOWEVENT_WINDOW_BIGGER)
                    this->scale_full_screen *= scale_step;
                else
                    this->scale_full_screen /= scale_step;
            }

            this->configure_dest();
            video_ctrl->set_draw_fb();
            video_ctrl->set_cursor_dirty();
        }
        break;

    case DPPC_WINDOWEVENT_MOUSE_GRAB_TOGGLE:
        if (wnd_event.window_id == impl->disp_wnd_id)
            this->toggle_mouse_grab();
        break;

    case DPPC_WINDOWEVENT_MOUSE_GRAB_CHANGED:
        this->update_window_title();
        break;

    }
}

void Display::toggle_mouse_grab()
{
    if (SDL_GetRelativeMouseMode()) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
    } else {
        this->update_mouse_grab(true);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
}

void Display::update_mouse_grab(bool will_be_grabbed)
{
    bool is_grabbed = SDL_GetRelativeMouseMode();
    if (will_be_grabbed || is_grabbed) {
        // If the mouse is initially outside the window, move it to the middle,
        // so that clicks are handled by the window (instead making it lose
        // focus, at least with macOS hosts).
        int window_x, window_y, window_width, window_height, mouse_x, mouse_y;
        SDL_GetWindowPosition(impl->display_wnd, &window_x, &window_y);
        SDL_GetWindowSize(impl->display_wnd, &window_width, &window_height);
        SDL_GetGlobalMouseState(&mouse_x, &mouse_y);
        if (mouse_x < window_x || mouse_x >= window_x + window_width ||
                mouse_y < window_y || mouse_y >= window_y + window_height) {
            if (is_grabbed)
                SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_WarpMouseInWindow(impl->display_wnd, window_width / 2, window_height / 2);
            if (is_grabbed)
                SDL_SetRelativeMouseMode(SDL_TRUE);
        }
    }
}

void Display::update_window_title()
{
    std::string old_window_title = SDL_GetWindowTitle(impl->display_wnd);

    int width, height;
    SDL_GetWindowSize(impl->display_wnd, &width, &height);
    bool is_grabbed = SDL_GetRelativeMouseMode();

    std::string new_window_title = "DingusPPC Display " +
        std::to_string(impl->display_w) + "x" + std::to_string(impl->display_h)
        + " " + std::to_string(int(std::round(impl->renderer_scale_x * 100))) + "%";
    if (is_grabbed)
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
    uint8_t*    dst_buf = nullptr;
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
    uint8_t*    dst_buf = nullptr;
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
