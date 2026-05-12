#include <bgfx_shader.sh>

void main()
{
    // Depth-only pass. bgfx state should disable color write.
    // We still need a fragment shader for the pipeline to be valid; the
    // facade configures BGFX_STATE_WRITE_Z without WRITE_RGB/WRITE_A.
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
