$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_depth,  1);

uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightAmbient;

void main()
{
	vec3 normal = decodeNormalUint(texture2D(s_normal, v_texcoord0).xyz);
	float deviceDepth = texture2D(s_depth, v_texcoord0).x;
	float clipDepth = toClipSpaceDepth(deviceDepth);

	vec3 lightDir = normalize(-u_lightDirIntensity.xyz);
	float ndotl = max(dot(normalize(normal), lightDir), 0.0);
	float depthMask = clipDepth <= 1.0 ? 1.0 : 0.0;

	vec3 lightColor = vec3(u_lightAmbient.x + ndotl * u_lightDirIntensity.w) * depthMask;
	gl_FragColor = vec4(toGamma(lightColor), 1.0);
}

