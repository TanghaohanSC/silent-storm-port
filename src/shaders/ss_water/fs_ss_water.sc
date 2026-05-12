$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);

void main()
{
    vec4 tex = texture2D(s_diffuse, v_texcoord0);
    // Tint slightly blue-cyan; mild transparency.
    gl_FragColor = vec4(tex.rgb * vec3(0.7, 0.85, 1.0), tex.a * 0.8);
}
