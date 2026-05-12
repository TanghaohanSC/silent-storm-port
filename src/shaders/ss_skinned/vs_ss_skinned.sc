$input  a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_normal, v_texcoord0

#include <bgfx_shader.sh>

// Bone matrices — up to 64 bones. Each bone is mat4 (16 floats).
uniform mat4 u_bones[64];

void main()
{
    mat4 skin =
        u_bones[a_indices.x] * a_weight.x +
        u_bones[a_indices.y] * a_weight.y +
        u_bones[a_indices.z] * a_weight.z +
        u_bones[a_indices.w] * a_weight.w;

    vec4 pos    = mul(skin, vec4(a_position, 1.0));
    vec4 normal = mul(skin, vec4(a_normal, 0.0));

    gl_Position = mul(u_modelViewProj, pos);
    v_normal    = normalize(mul(u_model[0], normal).xyz);
    v_texcoord0 = a_texcoord0;
}
