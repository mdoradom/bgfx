$input v_normal, v_texcoord0

#include "../common/common.sh"

uniform vec4 u_baseColorRoughness;
uniform vec4 u_materialParams;

void main()
{
	float roughness = clamp(u_baseColorRoughness.w, 0.02, 1.0);
	float metalness = clamp(u_materialParams.x, 0.0, 1.0);
	gl_FragData[0] = vec4(u_baseColorRoughness.xyz, roughness);
	gl_FragData[1] = vec4(encodeNormalUint(normalize(v_normal) ), metalness);
}

