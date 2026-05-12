$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);

void main()
{
    vec4 tex = texture2D(s_diffuse, v_texcoord0);
    gl_FragColor = tex * v_color0;
}
