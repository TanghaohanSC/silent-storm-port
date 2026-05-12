$input  a_position, a_normal, a_texcoord0, a_texcoord1
$output v_normal, v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal    = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_texcoord0 = a_texcoord0;  // diffuse uv
    v_texcoord1 = a_texcoord1;  // lightmap uv
}
