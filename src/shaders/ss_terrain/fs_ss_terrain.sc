$input v_normal, v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse,  0);
SAMPLER2D(s_lightmap, 1);
uniform vec4 u_lightDir;

void main()
{
    vec4 diffuse = texture2D(s_diffuse,  v_texcoord0);
    vec4 lm      = texture2D(s_lightmap, v_texcoord1);

    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_lightDir.xyz);
    float ndotl = max(dot(n, l), 0.0);

    vec3 rgb = diffuse.rgb * lm.rgb * (0.4 + 0.6 * ndotl);
    gl_FragColor = vec4(rgb, diffuse.a);
}
