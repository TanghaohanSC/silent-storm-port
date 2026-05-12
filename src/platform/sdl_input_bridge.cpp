#include "sdl_input_bridge.h"
#include "sdl_to_dik.h"

#include <SDL3/SDL.h>
#include <windows.h>  // DWORD, GetTickCount

// T11: forward-declare the hud-scale getter from the renderer module.
// We avoid including bgfx_init.h directly to prevent a circular library
// dependency (ss_renderer -> ss_platform -> ss_renderer).  The linker
// resolves this symbol from ss_renderer at link time.
namespace silent_storm::renderer {
    int get_hud_scale();
}

// ---------------------------------------------------------------------------
// Forward declarations for Nival's NInput types and the SDL bridge entry point
// we added to Input.cpp.  We deliberately avoid including Input.h because that
// header pulls in CDataStream (FileIO/Streams.h) and STLport string — both of
// which require Nival's PCH and are incompatible with this C++20 translation
// unit.  The linker resolves the symbol at link time from jan03_Input.
// ---------------------------------------------------------------------------
namespace NInput {

typedef unsigned long STime;

enum EPOVAxis { PA_UNKNOWN, PA_X, PA_Y };
enum EControlType {
    CT_KEY,
    CT_POV,
    CT_AXIS,
    CT_TIME,
    CT_LIMAXIS,
    CT_UNKNOWN
};

struct SMessage {
    int nAction;
    EPOVAxis ePOVAxis;
    EControlType cType;
    int nParam;
    bool bState;
    STime tTime;
};

// Defined in upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp (unconditionally
// compiled, outside the SS_USE_SDL_INPUT guard blocks).
void PushMessageSDL(const SMessage& msg);

}  // namespace NInput

namespace silent_storm::platform {

// ---------------------------------------------------------------------------
// NInput action-ID encoding mirrors Input.cpp's INPUT_KEYID macro:
//   ((deviceID & 0xFF) << 24) | offset
//
// Device IDs (matching InitInput registration order):
//   0 = mouse  (GUID_SysMouse,    registered first)
//   1 = keyboard (GUID_SysKeyboard, registered second)
//
// Keyboard offset  = DIK_* scan code (0x01–0xFF)
// Mouse axis offsets (from DIMOUSESTATE2 layout in dinput.h):
//   DIMOFS_X       = 0
//   DIMOFS_Y       = 4
//   DIMOFS_Z       = 8   (wheel)
// Mouse button offsets:
//   DIMOFS_BUTTON0 = 12  (left)
//   DIMOFS_BUTTON1 = 13  (right)
//   DIMOFS_BUTTON2 = 14  (middle)
//   DIMOFS_BUTTON3 = 15
//   DIMOFS_BUTTON4 = 16
// ---------------------------------------------------------------------------

static constexpr int DEVICE_MOUSE    = 0;
static constexpr int DEVICE_KEYBOARD = 1;

static constexpr int DIMOFS_X_VAL       = 0;
static constexpr int DIMOFS_Y_VAL       = 4;
static constexpr int DIMOFS_Z_VAL       = 8;
static constexpr int DIMOFS_BUTTON0_VAL = 12;
static constexpr int DIMOFS_BUTTON1_VAL = 13;
static constexpr int DIMOFS_BUTTON2_VAL = 14;
static constexpr int DIMOFS_BUTTON3_VAL = 15;
static constexpr int DIMOFS_BUTTON4_VAL = 16;

static constexpr int make_action(int device_id, int offset) noexcept {
    return ((device_id & 0xFF) << 24) | offset;
}

static inline NInput::STime now_ticks() noexcept {
    return static_cast<NInput::STime>(GetTickCount());
}

static void push_key(int dik, bool pressed) {
    if (dik < 0) return;
    NInput::SMessage msg{};
    msg.cType    = NInput::CT_KEY;
    msg.ePOVAxis = NInput::PA_UNKNOWN;
    msg.nAction  = make_action(DEVICE_KEYBOARD, dik);
    msg.nParam   = pressed ? 0x80 : 0;
    msg.bState   = pressed;
    msg.tTime    = now_ticks();
    NInput::PushMessageSDL(msg);
}

static void push_mouse_button(int dimofs_button, bool pressed) {
    NInput::SMessage msg{};
    msg.cType    = NInput::CT_KEY;
    msg.ePOVAxis = NInput::PA_UNKNOWN;
    msg.nAction  = make_action(DEVICE_MOUSE, dimofs_button);
    msg.nParam   = pressed ? 0x80 : 0;
    msg.bState   = pressed;
    msg.tTime    = now_ticks();
    NInput::PushMessageSDL(msg);
}

static void push_mouse_axis(int dimofs_axis, int delta) {
    if (delta == 0) return;
    NInput::SMessage msg{};
    msg.cType    = NInput::CT_AXIS;
    msg.ePOVAxis = NInput::PA_UNKNOWN;
    msg.nAction  = make_action(DEVICE_MOUSE, dimofs_axis);
    msg.nParam   = delta;
    msg.bState   = true;
    msg.tTime    = now_ticks();
    NInput::PushMessageSDL(msg);
}

void forward_to_ninput(const SDL_Event& ev) {
    switch (ev.type) {
        // ---- Keyboard ------------------------------------------------
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            // Ignore key-repeat events — NInput does not model auto-repeat.
            if (ev.key.repeat) break;
            const int dik = sdl_scancode_to_dik(ev.key.scancode);
            push_key(dik, ev.type == SDL_EVENT_KEY_DOWN);
            break;
        }

        // ---- Mouse motion --------------------------------------------
        case SDL_EVENT_MOUSE_MOTION: {
            // SDL3 provides relative deltas in xrel/yrel.
            // T11: when HUD integer-scale > 1, the HUD geometry is enlarged so
            // we must divide mouse deltas by the same scale factor so that
            // Nival's hit-test (which works in original 1:1 coords) still lands
            // on the correct UI elements.
            int hud_scale = silent_storm::renderer::get_hud_scale();
            int dx = (hud_scale > 1)
                     ? static_cast<int>(ev.motion.xrel / static_cast<float>(hud_scale))
                     : static_cast<int>(ev.motion.xrel);
            int dy = (hud_scale > 1)
                     ? static_cast<int>(ev.motion.yrel / static_cast<float>(hud_scale))
                     : static_cast<int>(ev.motion.yrel);
            push_mouse_axis(DIMOFS_X_VAL, dx);
            push_mouse_axis(DIMOFS_Y_VAL, dy);
            break;
        }

        // ---- Mouse buttons ------------------------------------------
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const bool pressed = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            int dimofs = -1;
            switch (ev.button.button) {
                case SDL_BUTTON_LEFT:   dimofs = DIMOFS_BUTTON0_VAL; break;
                case SDL_BUTTON_RIGHT:  dimofs = DIMOFS_BUTTON1_VAL; break;
                case SDL_BUTTON_MIDDLE: dimofs = DIMOFS_BUTTON2_VAL; break;
                case SDL_BUTTON_X1:     dimofs = DIMOFS_BUTTON3_VAL; break;
                case SDL_BUTTON_X2:     dimofs = DIMOFS_BUTTON4_VAL; break;
                default: break;
            }
            if (dimofs >= 0) push_mouse_button(dimofs, pressed);
            break;
        }

        // ---- Mouse wheel --------------------------------------------
        case SDL_EVENT_MOUSE_WHEEL: {
            // SDL3 wheel y: positive = scroll up / away from user.
            // DI8 mouse Z: positive = forward.  Scale by 120 (one notch = 120).
            const int delta = static_cast<int>(ev.wheel.y * 120.0f);
            push_mouse_axis(DIMOFS_Z_VAL, delta);
            break;
        }

        default:
            break;
    }
}

}  // namespace silent_storm::platform
