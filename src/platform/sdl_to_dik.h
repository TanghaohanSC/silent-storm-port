#pragma once
// SDL3 scancode -> DirectInput8 DIK_* mapping for NInput bridge.
// DIK_* values are raw scan codes matching <dinput.h>.
// SDL_Scancode values from <SDL3/SDL_scancode.h>.
//
// Scancodes not found in this table return -1 (unmapped).

#include <SDL3/SDL_scancode.h>

// Raw DIK_* constants (subset, matching dinput.h values).
// We define them here so this header doesn't pull in dinput.h.
namespace dik {
    // clang-format off
    static constexpr int ESCAPE       = 0x01;
    static constexpr int K1           = 0x02;
    static constexpr int K2           = 0x03;
    static constexpr int K3           = 0x04;
    static constexpr int K4           = 0x05;
    static constexpr int K5           = 0x06;
    static constexpr int K6           = 0x07;
    static constexpr int K7           = 0x08;
    static constexpr int K8           = 0x09;
    static constexpr int K9           = 0x0A;
    static constexpr int K0           = 0x0B;
    static constexpr int MINUS        = 0x0C;
    static constexpr int EQUALS       = 0x0D;
    static constexpr int BACK         = 0x0E;  // backspace
    static constexpr int TAB          = 0x0F;
    static constexpr int Q            = 0x10;
    static constexpr int W            = 0x11;
    static constexpr int E            = 0x12;
    static constexpr int R            = 0x13;
    static constexpr int T            = 0x14;
    static constexpr int Y            = 0x15;
    static constexpr int U            = 0x16;
    static constexpr int I            = 0x17;
    static constexpr int O            = 0x18;
    static constexpr int P            = 0x19;
    static constexpr int LBRACKET     = 0x1A;
    static constexpr int RBRACKET     = 0x1B;
    static constexpr int RETURN       = 0x1C;  // enter
    static constexpr int LCONTROL     = 0x1D;
    static constexpr int A            = 0x1E;
    static constexpr int S            = 0x1F;
    static constexpr int D            = 0x20;
    static constexpr int F            = 0x21;
    static constexpr int G            = 0x22;
    static constexpr int H            = 0x23;
    static constexpr int J            = 0x24;
    static constexpr int K            = 0x25;
    static constexpr int L            = 0x26;
    static constexpr int SEMICOLON    = 0x27;
    static constexpr int APOSTROPHE   = 0x28;
    static constexpr int GRAVE        = 0x29;  // tilde/backtick
    static constexpr int LSHIFT       = 0x2A;
    static constexpr int BACKSLASH    = 0x2B;
    static constexpr int Z            = 0x2C;
    static constexpr int X            = 0x2D;
    static constexpr int C            = 0x2E;
    static constexpr int V            = 0x2F;
    static constexpr int B            = 0x30;
    static constexpr int N            = 0x31;
    static constexpr int M            = 0x32;
    static constexpr int COMMA        = 0x33;
    static constexpr int PERIOD       = 0x34;
    static constexpr int SLASH        = 0x35;
    static constexpr int RSHIFT       = 0x36;
    static constexpr int MULTIPLY     = 0x37;  // numpad *
    static constexpr int LMENU        = 0x38;  // left alt
    static constexpr int SPACE        = 0x39;
    static constexpr int CAPITAL      = 0x3A;  // caps lock
    static constexpr int F1           = 0x3B;
    static constexpr int F2           = 0x3C;
    static constexpr int F3           = 0x3D;
    static constexpr int F4           = 0x3E;
    static constexpr int F5           = 0x3F;
    static constexpr int F6           = 0x40;
    static constexpr int F7           = 0x41;
    static constexpr int F8           = 0x42;
    static constexpr int F9           = 0x43;
    static constexpr int F10          = 0x44;
    static constexpr int NUMLOCK      = 0x45;
    static constexpr int SCROLL       = 0x46;
    static constexpr int NUMPAD7      = 0x47;
    static constexpr int NUMPAD8      = 0x48;
    static constexpr int NUMPAD9      = 0x49;
    static constexpr int SUBTRACT     = 0x4A;  // numpad -
    static constexpr int NUMPAD4      = 0x4B;
    static constexpr int NUMPAD5      = 0x4C;
    static constexpr int NUMPAD6      = 0x4D;
    static constexpr int ADD          = 0x4E;  // numpad +
    static constexpr int NUMPAD1      = 0x4F;
    static constexpr int NUMPAD2      = 0x50;
    static constexpr int NUMPAD3      = 0x51;
    static constexpr int NUMPAD0      = 0x52;
    static constexpr int DECIMAL      = 0x53;  // numpad .
    static constexpr int OEM_102      = 0x56;  // < > | on 102-key
    static constexpr int F11          = 0x57;
    static constexpr int F12          = 0x58;
    static constexpr int F13          = 0x64;
    static constexpr int F14          = 0x65;
    static constexpr int F15          = 0x66;
    static constexpr int KANA         = 0x70;
    static constexpr int ABNT_C1      = 0x73;
    static constexpr int CONVERT      = 0x79;
    static constexpr int NOCONVERT    = 0x7B;
    static constexpr int YEN          = 0x7D;
    static constexpr int ABNT_C2      = 0x7E;
    static constexpr int NUMPADEQUALS = 0x8D;
    static constexpr int PREVTRACK    = 0x90;
    static constexpr int AT           = 0x91;
    static constexpr int COLON        = 0x92;
    static constexpr int UNDERLINE    = 0x93;
    static constexpr int KANJI        = 0x94;
    static constexpr int STOP         = 0x95;
    static constexpr int AX           = 0x96;
    static constexpr int UNLABELED    = 0x97;
    static constexpr int NEXTTRACK    = 0x99;
    static constexpr int NUMPADENTER  = 0x9C;
    static constexpr int RCONTROL     = 0x9D;
    static constexpr int MUTE         = 0xA0;
    static constexpr int CALCULATOR   = 0xA1;
    static constexpr int PLAYPAUSE    = 0xA2;
    static constexpr int MEDIASTOP    = 0xA4;
    static constexpr int VOLUMEDOWN   = 0xAE;
    static constexpr int VOLUMEUP     = 0xB0;
    static constexpr int WEBHOME      = 0xB2;
    static constexpr int NUMPADCOMMA  = 0xB3;
    static constexpr int DIVIDE       = 0xB5;  // numpad /
    static constexpr int SYSRQ        = 0xB7;
    static constexpr int RMENU        = 0xB8;  // right alt
    static constexpr int PAUSE        = 0xC5;
    static constexpr int HOME         = 0xC7;
    static constexpr int UP           = 0xC8;
    static constexpr int PRIOR        = 0xC9;  // page up
    static constexpr int LEFT         = 0xCB;
    static constexpr int RIGHT        = 0xCD;
    static constexpr int END          = 0xCF;
    static constexpr int DOWN         = 0xD0;
    static constexpr int NEXT         = 0xD1;  // page down
    static constexpr int INSERT       = 0xD2;
    static constexpr int DELETE_KEY   = 0xD3;
    static constexpr int LWIN         = 0xDB;
    static constexpr int RWIN         = 0xDC;
    static constexpr int APPS         = 0xDD;
    static constexpr int POWER        = 0xDE;
    static constexpr int SLEEP        = 0xDF;
    static constexpr int WAKE         = 0xE3;
    static constexpr int WEBSEARCH    = 0xE5;
    static constexpr int WEBFAVORITES = 0xE6;
    static constexpr int WEBREFRESH   = 0xE7;
    static constexpr int WEBSTOP      = 0xE8;
    static constexpr int WEBFORWARD   = 0xE9;
    static constexpr int WEBBACK      = 0xEA;
    static constexpr int MYCOMPUTER   = 0xEB;
    static constexpr int MAIL         = 0xEC;
    static constexpr int MEDIASELECT  = 0xED;
    // clang-format on
}  // namespace dik

/// Translate an SDL_Scancode to a DIK_* value.
/// Returns -1 if no mapping exists.
constexpr int sdl_scancode_to_dik(SDL_Scancode s) noexcept {
    // clang-format off
    switch (s) {
        case SDL_SCANCODE_A:            return dik::A;
        case SDL_SCANCODE_B:            return dik::B;
        case SDL_SCANCODE_C:            return dik::C;
        case SDL_SCANCODE_D:            return dik::D;
        case SDL_SCANCODE_E:            return dik::E;
        case SDL_SCANCODE_F:            return dik::F;
        case SDL_SCANCODE_G:            return dik::G;
        case SDL_SCANCODE_H:            return dik::H;
        case SDL_SCANCODE_I:            return dik::I;
        case SDL_SCANCODE_J:            return dik::J;
        case SDL_SCANCODE_K:            return dik::K;
        case SDL_SCANCODE_L:            return dik::L;
        case SDL_SCANCODE_M:            return dik::M;
        case SDL_SCANCODE_N:            return dik::N;
        case SDL_SCANCODE_O:            return dik::O;
        case SDL_SCANCODE_P:            return dik::P;
        case SDL_SCANCODE_Q:            return dik::Q;
        case SDL_SCANCODE_R:            return dik::R;
        case SDL_SCANCODE_S:            return dik::S;
        case SDL_SCANCODE_T:            return dik::T;
        case SDL_SCANCODE_U:            return dik::U;
        case SDL_SCANCODE_V:            return dik::V;
        case SDL_SCANCODE_W:            return dik::W;
        case SDL_SCANCODE_X:            return dik::X;
        case SDL_SCANCODE_Y:            return dik::Y;
        case SDL_SCANCODE_Z:            return dik::Z;

        case SDL_SCANCODE_1:            return dik::K1;
        case SDL_SCANCODE_2:            return dik::K2;
        case SDL_SCANCODE_3:            return dik::K3;
        case SDL_SCANCODE_4:            return dik::K4;
        case SDL_SCANCODE_5:            return dik::K5;
        case SDL_SCANCODE_6:            return dik::K6;
        case SDL_SCANCODE_7:            return dik::K7;
        case SDL_SCANCODE_8:            return dik::K8;
        case SDL_SCANCODE_9:            return dik::K9;
        case SDL_SCANCODE_0:            return dik::K0;

        case SDL_SCANCODE_RETURN:       return dik::RETURN;
        case SDL_SCANCODE_ESCAPE:       return dik::ESCAPE;
        case SDL_SCANCODE_BACKSPACE:    return dik::BACK;
        case SDL_SCANCODE_TAB:          return dik::TAB;
        case SDL_SCANCODE_SPACE:        return dik::SPACE;

        case SDL_SCANCODE_MINUS:        return dik::MINUS;
        case SDL_SCANCODE_EQUALS:       return dik::EQUALS;
        case SDL_SCANCODE_LEFTBRACKET:  return dik::LBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET: return dik::RBRACKET;
        case SDL_SCANCODE_BACKSLASH:    return dik::BACKSLASH;
        case SDL_SCANCODE_SEMICOLON:    return dik::SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE:   return dik::APOSTROPHE;
        case SDL_SCANCODE_GRAVE:        return dik::GRAVE;
        case SDL_SCANCODE_COMMA:        return dik::COMMA;
        case SDL_SCANCODE_PERIOD:       return dik::PERIOD;
        case SDL_SCANCODE_SLASH:        return dik::SLASH;

        case SDL_SCANCODE_CAPSLOCK:     return dik::CAPITAL;

        case SDL_SCANCODE_F1:           return dik::F1;
        case SDL_SCANCODE_F2:           return dik::F2;
        case SDL_SCANCODE_F3:           return dik::F3;
        case SDL_SCANCODE_F4:           return dik::F4;
        case SDL_SCANCODE_F5:           return dik::F5;
        case SDL_SCANCODE_F6:           return dik::F6;
        case SDL_SCANCODE_F7:           return dik::F7;
        case SDL_SCANCODE_F8:           return dik::F8;
        case SDL_SCANCODE_F9:           return dik::F9;
        case SDL_SCANCODE_F10:          return dik::F10;
        case SDL_SCANCODE_F11:          return dik::F11;
        case SDL_SCANCODE_F12:          return dik::F12;
        case SDL_SCANCODE_F13:          return dik::F13;
        case SDL_SCANCODE_F14:          return dik::F14;
        case SDL_SCANCODE_F15:          return dik::F15;

        case SDL_SCANCODE_PRINTSCREEN:  return dik::SYSRQ;
        case SDL_SCANCODE_SCROLLLOCK:   return dik::SCROLL;
        case SDL_SCANCODE_PAUSE:        return dik::PAUSE;
        case SDL_SCANCODE_INSERT:       return dik::INSERT;
        case SDL_SCANCODE_HOME:         return dik::HOME;
        case SDL_SCANCODE_PAGEUP:       return dik::PRIOR;
        case SDL_SCANCODE_DELETE:       return dik::DELETE_KEY;
        case SDL_SCANCODE_END:          return dik::END;
        case SDL_SCANCODE_PAGEDOWN:     return dik::NEXT;
        case SDL_SCANCODE_RIGHT:        return dik::RIGHT;
        case SDL_SCANCODE_LEFT:         return dik::LEFT;
        case SDL_SCANCODE_DOWN:         return dik::DOWN;
        case SDL_SCANCODE_UP:           return dik::UP;

        case SDL_SCANCODE_NUMLOCKCLEAR: return dik::NUMLOCK;
        case SDL_SCANCODE_KP_DIVIDE:    return dik::DIVIDE;
        case SDL_SCANCODE_KP_MULTIPLY:  return dik::MULTIPLY;
        case SDL_SCANCODE_KP_MINUS:     return dik::SUBTRACT;
        case SDL_SCANCODE_KP_PLUS:      return dik::ADD;
        case SDL_SCANCODE_KP_ENTER:     return dik::NUMPADENTER;
        case SDL_SCANCODE_KP_1:         return dik::NUMPAD1;
        case SDL_SCANCODE_KP_2:         return dik::NUMPAD2;
        case SDL_SCANCODE_KP_3:         return dik::NUMPAD3;
        case SDL_SCANCODE_KP_4:         return dik::NUMPAD4;
        case SDL_SCANCODE_KP_5:         return dik::NUMPAD5;
        case SDL_SCANCODE_KP_6:         return dik::NUMPAD6;
        case SDL_SCANCODE_KP_7:         return dik::NUMPAD7;
        case SDL_SCANCODE_KP_8:         return dik::NUMPAD8;
        case SDL_SCANCODE_KP_9:         return dik::NUMPAD9;
        case SDL_SCANCODE_KP_0:         return dik::NUMPAD0;
        case SDL_SCANCODE_KP_PERIOD:    return dik::DECIMAL;
        case SDL_SCANCODE_KP_EQUALS:    return dik::NUMPADEQUALS;
        case SDL_SCANCODE_KP_COMMA:     return dik::NUMPADCOMMA;

        case SDL_SCANCODE_NONUSBACKSLASH: return dik::OEM_102;

        case SDL_SCANCODE_LCTRL:        return dik::LCONTROL;
        case SDL_SCANCODE_LSHIFT:       return dik::LSHIFT;
        case SDL_SCANCODE_LALT:         return dik::LMENU;
        case SDL_SCANCODE_LGUI:         return dik::LWIN;
        case SDL_SCANCODE_RCTRL:        return dik::RCONTROL;
        case SDL_SCANCODE_RSHIFT:       return dik::RSHIFT;
        case SDL_SCANCODE_RALT:         return dik::RMENU;
        case SDL_SCANCODE_RGUI:         return dik::RWIN;
        case SDL_SCANCODE_APPLICATION:  return dik::APPS;

        case SDL_SCANCODE_MUTE:         return dik::MUTE;
        case SDL_SCANCODE_VOLUMEUP:     return dik::VOLUMEUP;
        case SDL_SCANCODE_VOLUMEDOWN:   return dik::VOLUMEDOWN;
        case SDL_SCANCODE_MEDIA_NEXT_TRACK: return dik::NEXTTRACK;
        case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK: return dik::PREVTRACK;
        case SDL_SCANCODE_MEDIA_STOP:   return dik::MEDIASTOP;
        case SDL_SCANCODE_MEDIA_PLAY:   return dik::PLAYPAUSE;
        case SDL_SCANCODE_AC_SEARCH:    return dik::WEBSEARCH;
        case SDL_SCANCODE_AC_BOOKMARKS: return dik::WEBFAVORITES;
        case SDL_SCANCODE_AC_REFRESH:   return dik::WEBREFRESH;
        case SDL_SCANCODE_AC_STOP:      return dik::WEBSTOP;
        case SDL_SCANCODE_AC_FORWARD:   return dik::WEBFORWARD;
        case SDL_SCANCODE_AC_BACK:      return dik::WEBBACK;
        // SDL_SCANCODE_COMPUTER / SDL_SCANCODE_MAIL removed in SDL3
        case SDL_SCANCODE_MEDIA_SELECT: return dik::MEDIASELECT;

        case SDL_SCANCODE_SLEEP:        return dik::SLEEP;
        case SDL_SCANCODE_POWER:        return dik::POWER;

        default: return -1;
    }
    // clang-format on
}
