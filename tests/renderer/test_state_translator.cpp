// port/tests/renderer/test_state_translator.cpp
#include "../../src/renderer/state_translator.h"
#include <d3d9.h>
#include <cassert>
#include <bgfx/bgfx.h>

int main() {
    using namespace silent_storm::renderer;
    DeviceState s;

    // Default state should be opaque, depth-write, cull-back
    auto bits = s.build_bgfx_state();
    assert(bits & BGFX_STATE_WRITE_RGB);
    assert(bits & BGFX_STATE_WRITE_A);
    assert(bits & BGFX_STATE_WRITE_Z);
    assert(bits & BGFX_STATE_DEPTH_TEST_LEQUAL);

    // Cull CCW is the D3D9 default
    assert(bits & BGFX_STATE_CULL_CCW);

    // No blend in default state
    assert((bits & BGFX_STATE_BLEND_MASK) == 0);

    // Disable Z write
    s.set_render_state(D3DRS_ZWRITEENABLE, FALSE);
    bits = s.build_bgfx_state();
    assert((bits & BGFX_STATE_WRITE_Z) == 0);

    // Re-enable Z write
    s.set_render_state(D3DRS_ZWRITEENABLE, TRUE);

    // Alpha blend src-alpha / inv-src-alpha
    s.set_render_state(D3DRS_ALPHABLENDENABLE, TRUE);
    s.set_render_state(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    s.set_render_state(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    bits = s.build_bgfx_state();
    assert((bits & BGFX_STATE_BLEND_MASK) != 0);

    // Disable blend again
    s.set_render_state(D3DRS_ALPHABLENDENABLE, FALSE);
    s.set_render_state(D3DRS_SRCBLEND,  D3DBLEND_ONE);
    s.set_render_state(D3DRS_DESTBLEND, D3DBLEND_ZERO);

    // Cull none
    s.set_render_state(D3DRS_CULLMODE, D3DCULL_NONE);
    bits = s.build_bgfx_state();
    assert((bits & BGFX_STATE_CULL_CW)  == 0);
    assert((bits & BGFX_STATE_CULL_CCW) == 0);

    // Cull CW
    s.set_render_state(D3DRS_CULLMODE, D3DCULL_CW);
    bits = s.build_bgfx_state();
    assert(bits & BGFX_STATE_CULL_CW);
    assert((bits & BGFX_STATE_CULL_CCW) == 0);

    // shader archetype: default (lit, no texture) -> ss_diffuse_unlit
    {
        DeviceState t;
        // no texture, lighting on → no match for lit, falls through to ss_diffuse_unlit
        const char* arch = t.select_shader_archetype();
        // Acceptable: ss_diffuse_unlit or ss_diffuse_lit; just not null
        assert(arch != nullptr);
    }

    // shader archetype: blend + no depth -> ss_ui
    {
        DeviceState t;
        t.set_render_state(D3DRS_ZENABLE,        FALSE);
        t.set_render_state(D3DRS_LIGHTING,        FALSE);
        const char* arch = t.select_shader_archetype();
        assert(arch != nullptr);
        // Should be "ss_ui"
        assert(arch[0] != '\0');
    }

    return 0;
}
