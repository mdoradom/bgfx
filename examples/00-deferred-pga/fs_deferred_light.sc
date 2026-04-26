$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_normal, 0);
SAMPLER2D(s_depth,  1);
SAMPLER2DSHADOW(s_shadowMap, 2);
SAMPLERCUBE(s_texCube, 3);

uniform vec4 u_lightDirIntensity;
uniform vec4 u_lightDirIntensity2;
uniform vec4 u_lightAmbient;
uniform mat4 u_lightMtx;
uniform vec4 u_shadowParams;
uniform mat4 u_invViewProjGeom;
uniform vec4 u_camPos;
uniform vec4 u_iblParams;

void main()
{
	vec3 normal = decodeNormalUint(texture2D(s_normal, v_texcoord0).xyz);
	float deviceDepth = texture2D(s_depth, v_texcoord0).x;
	float clipDepth = toClipSpaceDepth(deviceDepth);

	// Reconstruct world position (fixed logic from 21-deferred)
	vec3 clipPos = vec3(v_texcoord0 * 2.0 - 1.0, clipDepth);
#if !BGFX_SHADER_LANGUAGE_GLSL
	clipPos.y = -clipPos.y;
#endif
	vec3 wpos = clipToWorld(u_invViewProjGeom, clipPos);

	float depthMask = clipDepth < 1.0 ? 1.0 : 0.0;

	vec3 viewDir = normalize(u_camPos.xyz - wpos);

	// Primary Light (with shadows and specular)
	vec3 lightDir1 = normalize(-u_lightDirIntensity.xyz);
	float ndotl1 = max(dot(normal, lightDir1), 0.0);
	
	vec4 shadowProj = mul(u_lightMtx, vec4(wpos + normal * u_shadowParams.y, 1.0));
	float visibility = 1.0;
	if (u_shadowParams.z > 0.5)
	{
		visibility = shadow2D(s_shadowMap, shadowProj.xyz / shadowProj.w);
	}
	
	vec3 h1 = normalize(lightDir1 + viewDir);
	float hdotn1 = max(dot(h1, normal), 0.0);
	float specular1 = pow(hdotn1, 32.0) * visibility;

	vec3 color1 = vec3(1.0, 0.95, 0.8) * (ndotl1 * u_lightDirIntensity.w * visibility + specular1);

	// Secondary Light (Fill light, no shadows)
	vec3 lightDir2 = normalize(-u_lightDirIntensity2.xyz);
	float ndotl2 = max(dot(normal, lightDir2), 0.0);
	vec3 color2 = vec3(0.5, 0.7, 1.0) * ndotl2 * u_lightDirIntensity2.w;

	vec3 ambient = vec3_splat(u_lightAmbient.x);

	vec3 iblSpecular = vec3_splat(0.0);
	if (u_iblParams.z > 0.5)
	{
		float mip = 1.0 + 5.0 * clamp(u_iblParams.x, 0.0, 1.0);
		vec3 refl = reflect(-viewDir, normal);
		refl = fixCubeLookup(refl, mip, 256.0);
		iblSpecular = toLinear(textureCubeLod(s_texCube, refl, mip).xyz) * clamp(u_iblParams.y, 0.0, 1.0);
	}

	vec3 lightColor = (ambient + color1 + color2 + iblSpecular) * depthMask;
	gl_FragColor = vec4(toGamma(lightColor), 1.0);
}
