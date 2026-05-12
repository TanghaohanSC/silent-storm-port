$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);

void main()
{
    vec4 tex = texture2D(s_diffuse, v_texcoord0);
    // Premultiplied by vertex color (incl alpha); facade sets blend mode (additive or alpha).
    gl_FragColor = vec4(tex.rgb * v_color0.rgb * v_color0.a, tex.a * v_color0.a);
}
