$input v_normal, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);
uniform vec4 u_lightDir;

void main()
{
    vec3 n = normalize(v_normal);
    vec3 l = normalize(u_lightDir.xyz);
    float ndotl = max(dot(n, l), 0.0);
    vec4 diffuse = texture2D(s_diffuse, v_texcoord0);
    gl_FragColor = vec4(diffuse.rgb * (0.3 + 0.7 * ndotl), diffuse.a);
}
