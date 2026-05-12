$input  a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_time;  // x = seconds since boot

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    // Scroll UV in both axes at different rates
    v_texcoord0 = a_texcoord0 + vec2(u_time.x * 0.03, u_time.x * 0.017);
}
