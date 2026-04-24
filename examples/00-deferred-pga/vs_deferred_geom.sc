$input a_position, a_normal, a_texcoord0
$output v_normal, v_texcoord0

#include "../common/common.sh"

void main()
{
	vec3 wpos = mul(u_model[0], vec4(a_position, 1.0) ).xyz;
	gl_Position = mul(u_viewProj, vec4(wpos, 1.0) );

	vec4 normal = a_normal * 2.0 - 1.0;
	v_normal = normalize(mul(u_model[0], vec4(normal.xyz, 0.0) ).xyz);
	v_texcoord0 = a_texcoord0;
}

