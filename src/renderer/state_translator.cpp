#include "state_translator.h"
#include <bgfx/bgfx.h>

namespace silent_storm::renderer {

namespace {

uint64_t map_blend_func(D3DBLEND src, D3DBLEND dst) {
    auto map_factor = [](D3DBLEND f) -> uint64_t {
        switch (f) {
            case D3DBLEND_ONE:          return BGFX_STATE_BLEND_ONE;
            case D3DBLEND_ZERO:         return BGFX_STATE_BLEND_ZERO;
            case D3DBLEND_SRCCOLOR:     return BGFX_STATE_BLEND_SRC_COLOR;
            case D3DBLEND_INVSRCCOLOR:  return BGFX_STATE_BLEND_INV_SRC_COLOR;
            case D3DBLEND_SRCALPHA:     return BGFX_STATE_BLEND_SRC_ALPHA;
            case D3DBLEND_INVSRCALPHA:  return BGFX_STATE_BLEND_INV_SRC_ALPHA;
            case D3DBLEND_DESTCOLOR:    return BGFX_STATE_BLEND_DST_COLOR;
            case D3DBLEND_INVDESTCOLOR: return BGFX_STATE_BLEND_INV_DST_COLOR;
            case D3DBLEND_DESTALPHA:    return BGFX_STATE_BLEND_DST_ALPHA;
            case D3DBLEND_INVDESTALPHA: return BGFX_STATE_BLEND_INV_DST_ALPHA;
            default:                    return BGFX_STATE_BLEND_ONE;
        }
    };
    uint64_t src_bits = map_factor(src);
    uint64_t dst_bits = map_factor(dst);
    return BGFX_STATE_BLEND_FUNC(src_bits, dst_bits);
}

} // namespace

void DeviceState::reset_defaults() {
    // D3D9 default render states (from MSDN / d3d9types.h)
    render_state[D3DRS_ZENABLE]           = TRUE;
    render_state[D3DRS_ZWRITEENABLE]      = TRUE;
    render_state[D3DRS_ZFUNC]             = D3DCMP_LESSEQUAL;
    render_state[D3DRS_CULLMODE]          = D3DCULL_CCW;
    render_state[D3DRS_ALPHABLENDENABLE]  = FALSE;
    render_state[D3DRS_SRCBLEND]          = D3DBLEND_ONE;
    render_state[D3DRS_DESTBLEND]         = D3DBLEND_ZERO;
    render_state[D3DRS_FILLMODE]          = D3DFILL_SOLID;
    render_state[D3DRS_ALPHATESTENABLE]   = FALSE;
    render_state[D3DRS_ALPHAREF]          = 0;
    render_state[D3DRS_ALPHAFUNC]         = D3DCMP_ALWAYS;
    render_state[D3DRS_LIGHTING]          = TRUE;
    render_state[D3DRS_COLORWRITEENABLE]  = 0x0F;  // R|G|B|A
    render_state[D3DRS_SCISSORTESTENABLE] = FALSE;
    render_state[D3DRS_STENCILENABLE]     = FALSE;
    render_state[D3DRS_FOGENABLE]         = FALSE;
    render_state[D3DRS_SPECULARENABLE]    = FALSE;
    render_state[D3DRS_DITHERENABLE]      = FALSE;
    render_state[D3DRS_SHADEMODE]         = D3DSHADE_GOURAUD;
    render_state[D3DRS_LASTPIXEL]         = TRUE;
    render_state[D3DRS_SRCBLENDALPHA]     = D3DBLEND_ONE;
    render_state[D3DRS_DESTBLENDALPHA]    = D3DBLEND_ZERO;
    render_state[D3DRS_BLENDOP]           = D3DBLENDOP_ADD;
    render_state[D3DRS_BLENDOPALPHA]      = D3DBLENDOP_ADD;
}

uint64_t DeviceState::build_bgfx_state() const {
    uint64_t s = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;

    // Depth write
    if (render_state[D3DRS_ZWRITEENABLE]) {
        s |= BGFX_STATE_WRITE_Z;
    }

    // Depth test
    if (render_state[D3DRS_ZENABLE]) {
        switch (render_state[D3DRS_ZFUNC]) {
            case D3DCMP_LESS:        s |= BGFX_STATE_DEPTH_TEST_LESS;     break;
            case D3DCMP_LESSEQUAL:   s |= BGFX_STATE_DEPTH_TEST_LEQUAL;   break;
            case D3DCMP_GREATER:     s |= BGFX_STATE_DEPTH_TEST_GREATER;  break;
            case D3DCMP_GREATEREQUAL:s |= BGFX_STATE_DEPTH_TEST_GEQUAL;   break;
            case D3DCMP_EQUAL:       s |= BGFX_STATE_DEPTH_TEST_EQUAL;    break;
            case D3DCMP_NOTEQUAL:    s |= BGFX_STATE_DEPTH_TEST_NOTEQUAL; break;
            case D3DCMP_ALWAYS:      s |= BGFX_STATE_DEPTH_TEST_ALWAYS;   break;
            default:                 s |= BGFX_STATE_DEPTH_TEST_LEQUAL;   break;
        }
    }

    // Alpha blend
    if (render_state[D3DRS_ALPHABLENDENABLE]) {
        auto src = static_cast<D3DBLEND>(render_state[D3DRS_SRCBLEND]);
        auto dst = static_cast<D3DBLEND>(render_state[D3DRS_DESTBLEND]);
        s |= map_blend_func(src, dst);
    }

    // Cull mode
    switch (render_state[D3DRS_CULLMODE]) {
        case D3DCULL_NONE: break;
        case D3DCULL_CW:   s |= BGFX_STATE_CULL_CW;  break;
        case D3DCULL_CCW:  s |= BGFX_STATE_CULL_CCW; break;
        default:           break;
    }

    // Wireframe
    if (render_state[D3DRS_FILLMODE] == D3DFILL_WIREFRAME) {
        s |= BGFX_STATE_PT_LINES;
    }

    return s;
}

const char* DeviceState::select_shader_archetype() const {
    bool lit      = render_state[D3DRS_LIGHTING] != FALSE;
    bool textured = texture[0] != nullptr;
    bool blend    = render_state[D3DRS_ALPHABLENDENABLE] != FALSE;

    // UI: no depth test, ortho-like (viewport covers full screen)
    if (!render_state[D3DRS_ZENABLE] && !lit) {
        return "ss_ui";
    }

    // Particles: blended, unlit, textured
    if (blend && !lit && textured) {
        return "ss_particle";
    }

    // Shadow pass: depth-only (no color writes)
    if ((render_state[D3DRS_COLORWRITEENABLE] & 0x0F) == 0) {
        return "ss_shadow";
    }

    if (textured && lit) return "ss_diffuse_lit";
    if (textured && !lit) return "ss_diffuse_unlit";
    if (blend) return "ss_particle";

    return "ss_diffuse_unlit";
}

} // namespace silent_storm::renderer
