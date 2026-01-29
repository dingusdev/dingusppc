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

#include <core/hostevents.h>
#include <core/coresignal.h>
#include <cpu/ppc/ppcemu.h>
#include <devices/common/adb/adbkeyboard.h>
#include <loguru.hpp>
#include <SDL.h>

EventManager* EventManager::event_manager;

static int get_sdl_event_key_code(const SDL_KeyboardEvent& event, uint32_t kbd_locale);

constexpr int KMOD_ALL = KMOD_LSHIFT | KMOD_RSHIFT | KMOD_LCTRL | KMOD_RCTRL | KMOD_LALT | KMOD_RALT | KMOD_LGUI | KMOD_RGUI;

void EventManager::set_keyboard_locale(uint32_t keyboard_id) {
    this->kbd_locale = keyboard_id;
}

void EventManager::poll_events() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        events_captured++;

        switch (event.type) {
        case SDL_QUIT:
            power_on = false;
            power_off_reason = po_quit;
            break;

        case SDL_WINDOWEVENT: {
                WindowEvent we{};
                we.sub_type  = event.window.event;
                we.window_id = event.window.windowID;
                this->_window_signal.emit(we);
            }
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP: {
                if (event.key.repeat)
                    break;

                // Internal shortcuts, intentionally not sent to the host.
                // Control-G: mouse grab
                if (event.key.keysym.sym == SDLK_g && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we;
                        we.sub_type  = DPPC_WINDOWEVENT_MOUSE_GRAB_TOGGLE;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                        we.sub_type  = DPPC_WINDOWEVENT_MOUSE_GRAB_CHANGED;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-S: scale quality
                if (event.key.keysym.sym == SDLK_s && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_SCALE_QUALITY_TOGGLE;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-F: fullscreen
                if (event.key.keysym.sym == SDLK_f && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-Shift-F: fullscreen reverse
                if (event.key.keysym.sym == SDLK_f && (event.key.keysym.mod & KMOD_ALL) == (KMOD_LCTRL | KMOD_LSHIFT)) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_FULL_SCREEN_TOGGLE_REVERSE;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-+: bigger
                if (event.key.keysym.sym == SDLK_EQUALS && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_BIGGER;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control--: smaller
                if (event.key.keysym.sym == SDLK_MINUS && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        WindowEvent we{};
                        we.sub_type  = DPPC_WINDOWEVENT_WINDOW_SMALLER;
                        we.window_id = event.window.windowID;
                        this->_window_signal.emit(we);
                    }
                    return;
                }
                // Control-L: log toggle
                if (event.key.keysym.sym == SDLK_l && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        loguru::g_stderr_verbosity = loguru::g_stderr_verbosity == loguru::Verbosity_MAX ?
                            loguru::Verbosity_INFO
                        :
                            loguru::Verbosity_MAX;
                    }
                    return;
                }
                // Control-D: debugger
                if (event.key.keysym.sym == SDLK_d && (event.key.keysym.mod & KMOD_ALL) == KMOD_LCTRL) {
                    if (event.type == SDL_KEYUP) {
                        power_on = false;
                        power_off_reason = po_enter_debugger;
                    }
                    return;
                }
                // Ralt+delete => ctrl+alt+del
                if (event.key.keysym.sym == SDLK_DELETE && ((event.key.keysym.mod & KMOD_ALL) == KMOD_RALT) != 0) {
                    KeyboardEvent ke{};
                    ke.key = AdbKey_Control;

                    if (event.type == SDL_KEYDOWN) {
                        ke.flags = KEYBOARD_EVENT_DOWN;
                        key_downs++;
                    } else {
                        ke.flags = KEYBOARD_EVENT_UP;
                        key_ups++;
                    }

                    this->_keyboard_signal.emit(ke);
                    ke.key = AdbKey_Delete;
                    this->_keyboard_signal.emit(ke);
                    return;
                }
                int key_code = get_sdl_event_key_code(event.key, this->kbd_locale);
                if (key_code != -1) {
                    KeyboardEvent ke{};
                    ke.key = key_code;
                    if (event.type == SDL_KEYDOWN) {
                        ke.flags = KEYBOARD_EVENT_DOWN;
                        key_downs++;
                    } else {
                        ke.flags = KEYBOARD_EVENT_UP;
                        key_ups++;
                    }
                    // Caps Lock is a special case, since it's a toggle key
                    if (ke.key == AdbKey_CapsLock) {
                        ke.flags = event.key.keysym.mod & KMOD_CAPS ?
                            KEYBOARD_EVENT_DOWN : KEYBOARD_EVENT_UP;
                    }
                    this->_keyboard_signal.emit(ke);
                } else {
                    LOG_F(WARNING, "Unknown key %x pressed", event.key.keysym.sym);
                }
            }
            break;

        case SDL_MOUSEMOTION: {
                MouseEvent me{};
                me.xrel  = event.motion.xrel;
                me.yrel  = event.motion.yrel;
                me.xabs  = event.motion.x;
                me.yabs  = event.motion.y;
                me.flags = MOUSE_EVENT_MOTION;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_MOUSEBUTTONDOWN: {
                MouseEvent me{};
                Uint8 adb_button;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT   : adb_button = 0; break;
                    case SDL_BUTTON_MIDDLE : adb_button = 2; break;
                    case SDL_BUTTON_RIGHT  : adb_button = 1; break;
                    default                : adb_button = event.button.button - 1;
                }
                me.buttons_state = (this->buttons_state |= (1 << adb_button));
                me.xabs  = event.button.x;
                me.yabs  = event.button.y;
                me.flags = MOUSE_EVENT_BUTTON;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_MOUSEBUTTONUP: {
                MouseEvent me{};
                Uint8 adb_button;
                switch (event.button.button) {
                    case SDL_BUTTON_LEFT   : adb_button = 0; break;
                    case SDL_BUTTON_MIDDLE : adb_button = 2; break;
                    case SDL_BUTTON_RIGHT  : adb_button = 1; break;
                    default                : adb_button = event.button.button - 1;
                }
                me.buttons_state = (this->buttons_state &= ~(1 << adb_button));
                me.xabs  = event.button.x;
                me.yabs  = event.button.y;
                me.flags = MOUSE_EVENT_BUTTON;
                this->_mouse_signal.emit(me);
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN: {
                GamepadEvent ge{};
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_BACK:          ge.button = GamepadButton::FrontLeft;    break;
                    case SDL_CONTROLLER_BUTTON_GUIDE:         ge.button = GamepadButton::FrontMiddle;  break;
                    case SDL_CONTROLLER_BUTTON_START:         ge.button = GamepadButton::FrontRight;   break;
                    case SDL_CONTROLLER_BUTTON_Y:             ge.button = GamepadButton::Blue;         break;
                    case SDL_CONTROLLER_BUTTON_X:             ge.button = GamepadButton::Yellow;       break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:       ge.button = GamepadButton::Up;           break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     ge.button = GamepadButton::Left;         break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    ge.button = GamepadButton::Right;        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     ge.button = GamepadButton::Down;         break;
                    case SDL_CONTROLLER_BUTTON_A:             ge.button = GamepadButton::Red;          break;
                    case SDL_CONTROLLER_BUTTON_B:             ge.button = GamepadButton::Green;        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: ge.button = GamepadButton::RightTrigger; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  ge.button = GamepadButton::LeftTrigger;  break;
                }
                ge.gamepad_id = event.cbutton.which;
                ge.flags = GAMEPAD_EVENT_DOWN;
                this->_gamepad_signal.emit(ge);
            }
            break;

        case SDL_CONTROLLERBUTTONUP: {
                GamepadEvent ge{};
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_BACK:          ge.button = GamepadButton::FrontLeft;    break;
                    case SDL_CONTROLLER_BUTTON_GUIDE:         ge.button = GamepadButton::FrontMiddle;  break;
                    case SDL_CONTROLLER_BUTTON_START:         ge.button = GamepadButton::FrontRight;   break;
                    case SDL_CONTROLLER_BUTTON_Y:             ge.button = GamepadButton::Blue;         break;
                    case SDL_CONTROLLER_BUTTON_X:             ge.button = GamepadButton::Yellow;       break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:       ge.button = GamepadButton::Up;           break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     ge.button = GamepadButton::Left;         break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    ge.button = GamepadButton::Right;        break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     ge.button = GamepadButton::Down;         break;
                    case SDL_CONTROLLER_BUTTON_A:             ge.button = GamepadButton::Red;          break;
                    case SDL_CONTROLLER_BUTTON_B:             ge.button = GamepadButton::Green;        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: ge.button = GamepadButton::RightTrigger; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  ge.button = GamepadButton::LeftTrigger;  break;
                }
                ge.gamepad_id = event.cbutton.which;
                ge.flags = GAMEPAD_EVENT_UP;
                this->_gamepad_signal.emit(ge);
            }
            break;

        default:
            unhandled_events++;
        }
    }

    // perform post-processing
    this->_post_signal.emit();
}

void EventManager::post_keyboard_state_events() {
    int numkeys;
    const Uint8 *states = SDL_GetKeyboardState(&numkeys);
    int modstate = SDL_GetModState();

    SDL_KeyboardEvent keyevent = { .type = SDL_KEYDOWN };
    SDL_Scancode scancode;
    KeyboardEvent ke{};

    typedef struct {
        SDL_Scancode scancode;
        SDL_Keymod   keymod;
        AdbKey       adbkey;
    } Modifier_t;

    static Modifier_t modifiers[] = {
        SDL_SCANCODE_LSHIFT       , KMOD_LSHIFT , AdbKey_Shift       ,
        SDL_SCANCODE_RSHIFT       , KMOD_RSHIFT , AdbKey_RightShift  ,
        SDL_SCANCODE_LCTRL        , KMOD_LCTRL  , AdbKey_Control     ,
        SDL_SCANCODE_RCTRL        , KMOD_RCTRL  , AdbKey_RightControl,
        SDL_SCANCODE_LALT         , KMOD_LALT   , AdbKey_Option      ,
        SDL_SCANCODE_RALT         , KMOD_RALT   , AdbKey_RightOption ,
        SDL_SCANCODE_LGUI         , KMOD_LGUI   , AdbKey_Command     ,
        SDL_SCANCODE_RGUI         , KMOD_RGUI   , AdbKey_Command     ,
//      SDL_SCANCODE_NUMLOCKCLEAR , KMOD_NUM    , AdbKey_KeypadClear ,
        SDL_SCANCODE_CAPSLOCK     , KMOD_CAPS   , AdbKey_CapsLock    ,
//      SDL_SCANCODE_MODE         , KMOD_MODE   , AdbKey_????        ,
//      SDL_SCANCODE_SCROLLLOCK   , KMOD_SCROLL , AdbKey_F14         ,
        SDL_SCANCODE_UNKNOWN
    };

    LOG_F(INFO, "Current keyboard state:");

    for (Modifier_t *mod = modifiers; mod->scancode != SDL_SCANCODE_UNKNOWN; mod++) {
        if (!(modstate & mod->keymod))
            continue;
        LOG_F(INFO, "    mod:%s", SDL_GetScancodeName(mod->scancode));
        ke.key = mod->adbkey;
        ke.flags = KEYBOARD_EVENT_DOWN;
        this->_keyboard_signal.emit(ke);
    }

    for (int i = 0; i < numkeys; i++) {
        if (!states[i])
            continue;

        scancode = (SDL_Scancode)i;

        Modifier_t *mod = modifiers;
        for (; mod->scancode != SDL_SCANCODE_UNKNOWN && mod->scancode != scancode; mod++);
        if (mod->scancode == scancode) {
            LOG_F(INFO, "    ignore:%s", SDL_GetScancodeName(scancode));
            continue;
        }

        LOG_F(INFO, "    key:%s", SDL_GetScancodeName(scancode));
        keyevent.keysym.scancode = scancode;
        keyevent.keysym.sym = SDL_GetKeyFromScancode(scancode);
        keyevent.keysym.mod = modstate;

        int key_code = get_sdl_event_key_code(keyevent, this->kbd_locale);
        if (key_code != -1) {
            ke.key = key_code;
            ke.flags = KEYBOARD_EVENT_DOWN;
            this->_keyboard_signal.emit(ke);
        } else {
            LOG_F(WARNING, "Unknown key %x pressed", keyevent.keysym.sym);
        }
    }
}

static int get_sdl_event_key_code(const SDL_KeyboardEvent &event, uint32_t kbd_locale)
{
    switch (event.keysym.sym) {
    case SDLK_a:            return AdbKey_A;
    case SDLK_b:            return AdbKey_B;
    case SDLK_c:            return AdbKey_C;
    case SDLK_d:            return AdbKey_D;
    case SDLK_e:            return AdbKey_E;
    case SDLK_f:            return AdbKey_F;
    case SDLK_g:            return AdbKey_G;
    case SDLK_h:            return AdbKey_H;
    case SDLK_i:            return AdbKey_I;
    case SDLK_j:            return AdbKey_J;
    case SDLK_k:            return AdbKey_K;
    case SDLK_l:            return AdbKey_L;
    case SDLK_m:            return AdbKey_M;
    case SDLK_n:            return AdbKey_N;
    case SDLK_o:            return AdbKey_O;
    case SDLK_p:            return AdbKey_P;
    case SDLK_q:            return AdbKey_Q;
    case SDLK_r:            return AdbKey_R;
    case SDLK_s:            return AdbKey_S;
    case SDLK_t:            return AdbKey_T;
    case SDLK_u:            return AdbKey_U;
    case SDLK_v:            return AdbKey_V;
    case SDLK_w:            return AdbKey_W;
    case SDLK_x:            return AdbKey_X;
    case SDLK_y:            return AdbKey_Y;
    case SDLK_z:            return AdbKey_Z;

    case SDLK_1:            return AdbKey_1;
    case SDLK_2:            return AdbKey_2;
    case SDLK_3:            return AdbKey_3;
    case SDLK_4:            return AdbKey_4;
    case SDLK_5:            return AdbKey_5;
    case SDLK_6:            return AdbKey_6;
    case SDLK_7:            return AdbKey_7;
    case SDLK_8:            return AdbKey_8;
    case SDLK_9:            return AdbKey_9;
    case SDLK_0:            return AdbKey_0;

    case SDLK_ESCAPE:       return AdbKey_Escape;
    case SDLK_BACKQUOTE:    return AdbKey_Grave;
    case SDLK_MINUS:        return AdbKey_Minus;
    case SDLK_EQUALS:       return AdbKey_Equal;
    case SDLK_LEFTBRACKET:  return AdbKey_LeftBracket;
    case SDLK_RIGHTBRACKET: return AdbKey_RightBracket;
    case SDLK_BACKSLASH:    return AdbKey_Backslash;
    case SDLK_SEMICOLON:    return AdbKey_Semicolon;
    case SDLK_QUOTE:        return AdbKey_Quote;
    case SDLK_COMMA:        return AdbKey_Comma;
    case SDLK_PERIOD:       return AdbKey_Period;
    case SDLK_SLASH:        return AdbKey_Slash;

    // Convert shifted variants to unshifted
    case SDLK_EXCLAIM:      return AdbKey_1;
    case SDLK_AT:           return AdbKey_2;
    case SDLK_HASH:         return AdbKey_3;
    case SDLK_DOLLAR:       return AdbKey_4;
    case SDLK_UNDERSCORE:   return AdbKey_Minus;
    case SDLK_PLUS:         return AdbKey_Equal;
    case SDLK_COLON:        return AdbKey_Semicolon;
    case SDLK_QUOTEDBL:     return AdbKey_Quote;
    case SDLK_LESS:         return AdbKey_Comma;
    case SDLK_GREATER:      return AdbKey_Period;
    case SDLK_QUESTION:     return AdbKey_Slash;

    case SDLK_TAB:          return AdbKey_Tab;
    case SDLK_RETURN:       return AdbKey_Return;
    case SDLK_SPACE:        return AdbKey_Space;
    case SDLK_BACKSPACE:    return AdbKey_Delete;

    case SDLK_DELETE:       return AdbKey_ForwardDelete;
    case SDLK_INSERT:       return AdbKey_Help;
    case SDLK_HOME:         return AdbKey_Home;
    case SDLK_HELP:         return AdbKey_Home;
    case SDLK_END:          return AdbKey_End;
    case SDLK_PAGEUP:       return AdbKey_PageUp;
    case SDLK_PAGEDOWN:     return AdbKey_PageDown;

    case SDLK_LCTRL:        return AdbKey_Control;
    case SDLK_RCTRL:        return AdbKey_RightControl;
    case SDLK_LSHIFT:       return AdbKey_Shift;
    case SDLK_RSHIFT:       return AdbKey_RightShift;
    case SDLK_LALT:         return AdbKey_Option;
    case SDLK_RALT:         return AdbKey_RightOption;
    case SDLK_LGUI:         return AdbKey_Command;
    case SDLK_RGUI:         return AdbKey_Command;
    case SDLK_MENU:         return AdbKey_Grave;
    case SDLK_CAPSLOCK:     return AdbKey_CapsLock;

    case SDLK_UP:           return AdbKey_ArrowUp;
    case SDLK_DOWN:         return AdbKey_ArrowDown;
    case SDLK_LEFT:         return AdbKey_ArrowLeft;
    case SDLK_RIGHT:        return AdbKey_ArrowRight;

    case SDLK_KP_0:         return AdbKey_Keypad0;
    case SDLK_KP_1:         return AdbKey_Keypad1;
    case SDLK_KP_2:         return AdbKey_Keypad2;
    case SDLK_KP_3:         return AdbKey_Keypad3;
    case SDLK_KP_4:         return AdbKey_Keypad4;
    case SDLK_KP_5:         return AdbKey_Keypad5;
    case SDLK_KP_6:         return AdbKey_Keypad6;
    case SDLK_KP_7:         return AdbKey_Keypad7;
    case SDLK_KP_9:         return AdbKey_Keypad9;
    case SDLK_KP_8:         return AdbKey_Keypad8;
    case SDLK_KP_PERIOD:    return AdbKey_KeypadDecimal;
    case SDLK_KP_PLUS:      return AdbKey_KeypadPlus;
    case SDLK_KP_MINUS:     return AdbKey_KeypadMinus;
    case SDLK_KP_MULTIPLY:  return AdbKey_KeypadMultiply;
    case SDLK_KP_DIVIDE:    return AdbKey_KeypadDivide;
    case SDLK_KP_ENTER:     return AdbKey_KeypadEnter;
    case SDLK_KP_EQUALS:    return AdbKey_KeypadEquals;
    case SDLK_NUMLOCKCLEAR: return AdbKey_KeypadClear;

    case SDLK_F1:           return AdbKey_F1;
    case SDLK_F2:           return AdbKey_F2;
    case SDLK_F3:           return AdbKey_F3;
    case SDLK_F4:           return AdbKey_F4;
    case SDLK_F5:           return AdbKey_F5;
    case SDLK_F6:           return AdbKey_F6;
    case SDLK_F7:           return AdbKey_F7;
    case SDLK_F8:           return AdbKey_F8;
    case SDLK_F9:           return AdbKey_F9;
    case SDLK_F10:          return AdbKey_F10;
    case SDLK_F11:          return AdbKey_F11;
    case SDLK_F12:          return AdbKey_F12;
    case SDLK_PRINTSCREEN:  return AdbKey_F13;
    case SDLK_SCROLLLOCK:   return AdbKey_F14;
    case SDLK_PAUSE:        return AdbKey_F15;

    //International keyboard support

    //Japanese keyboard
    case SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INTERNATIONAL3):
        if (kbd_locale == Jpn_JPN)
            return AdbKey_JIS_Yen;
        else
            return -1;
    case SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INTERNATIONAL1):
        return AdbKey_JIS_Underscore;
    case 0XBC:
        return AdbKey_JIS_KP_Comma;
    case 0X89:
        return AdbKey_JIS_Eisu;
    case SDL_SCANCODE_TO_KEYCODE(SDL_SCANCODE_INTERNATIONAL2):
        return AdbKey_JIS_Kana;

    //German keyboard
    case 0XB4:        return AdbKey_Slash;
    case 0X5E:        return AdbKey_ISO1;
    case 0XDF:        return AdbKey_Minus;       //Eszett
    case 0XE4:        return AdbKey_LeftBracket; //A-umlaut
    case 0XF6:        return AdbKey_Semicolon;   //O-umlaut
    case 0XFC:        return AdbKey_LeftBracket; //U-umlaut

    // French keyboard
    case 0X29:        return AdbKey_Minus;             // Right parenthesis
    case 0X43:        return AdbKey_KeypadMultiply;    // Star/Mu
    //0XB2 is superscript 2. Which Mac key should this one map to?
    case 0XF9:        return AdbKey_Quote;             // U-grave

    // Italian keyboard
    case 0XE0:        return AdbKey_9;              // A-grave
    case 0XE8:        return AdbKey_6;              // E-grave
    case 0XEC:        return AdbKey_LeftBracket;    // I-grave
    case 0XF2:        return AdbKey_KeypadMultiply; // O-grave

    // Spanish keyboard
    case 0XA1:        return AdbKey_Comma;        // Inverted question mark
    case 0XBA:        return AdbKey_6;            // Backslash
    case 0XE7:        return AdbKey_Slash;        // C-cedilla
    case 0XF1:        return AdbKey_Semicolon;    // N-tilde
    case 0X4000002f:
        return AdbKey_LeftBracket;    // Acute
    case 0X40000034:
        return AdbKey_Semicolon;    // Acute
    }
    return -1;
}
