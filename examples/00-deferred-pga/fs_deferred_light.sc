$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_depth,  1);
SAMPLER2DSHADOW(s_shadowMap, 2);

uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightAmbient;
uniform mat4 u_lightMtx;
uniform vec4 u_shadowParams;
uniform mat4 u_invViewProjGeom;

void main()
{
	vec3 normal = decodeNormalUint(texture2D(s_normal, v_texcoord0).xyz);
	float deviceDepth = texture2D(s_depth, v_texcoord0).x;
	float clipDepth = toClipSpaceDepth(deviceDepth);

	// Reconstruct world position
	vec3 clipPos = vec3(v_texcoord0 * 2.0 - 1.0, deviceDepth);
#if BGFX_SHADER_LANGUAGE_GLSL
	clipPos.y = -clipPos.y;
#endif
	vec3 wpos = clipToWorld(u_invViewProjGeom, clipPos);

	vec3 lightDir = normalize(-u_lightDirIntensity.xyz);
	float ndotl = max(dot(normalize(normal), lightDir), 0.0);
	float depthMask = clipDepth <= 1.0 ? 1.0 : 0.0;

	// Shadow calculation
	vec4 shadowProj = mul(u_lightMtx, vec4(wpos + normal * u_shadowParams.y, 1.0));
	float visibility = 1.0;
	if (u_shadowParams.z > 0.5)
	{
		visibility = shadow2D(s_shadowMap, shadowProj.xyz / shadowProj.w);
	}

	vec3 lightColor = vec3(u_lightAmbient.x + ndotl * u_lightDirIntensity.w * visibility) * depthMask;
	gl_FragColor = vec4(toGamma(lightColor), 1.0);
}
