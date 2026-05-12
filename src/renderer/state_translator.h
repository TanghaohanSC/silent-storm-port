#pragma once
#include <cstdint>
#include <d3d9.h>

namespace silent_storm::renderer {

struct DeviceState {
    DWORD render_state[210] = {};       // D3DRS_* (D3D9 has ~200)
    DWORD tex_stage_state[8][33] = {};  // D3DTSS_* per stage
    void* texture[8] = {};              // IDirect3DTexture9*
    D3DMATRIX world{}, view{}, projection{};
    UINT viewport_x = 0, viewport_y = 0, viewport_w = 0, viewport_h = 0;
    int hud_scale_active = 1;           // T11: >1 when HUD integer-scale is applied

    DeviceState() { reset_defaults(); }
    void reset_defaults();
    void set_render_state(D3DRENDERSTATETYPE s, DWORD v) { render_state[s] = v; }
    void set_texture_stage_state(DWORD stage, D3DTEXTURESTAGESTATETYPE t, DWORD v) {
        if (stage < 8) tex_stage_state[stage][t] = v;
    }

    // Compute bgfx state bits from current D3D9 state.
    uint64_t build_bgfx_state() const;

    // Pick a shader archetype (string ID resolved via shader_registry).
    const char* select_shader_archetype() const;
};

} // namespace silent_storm::renderer
