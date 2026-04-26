$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_normal, 1);
SAMPLER2D(s_depth,  2);
SAMPLER2DSHADOW(s_shadowMap, 3);
SAMPLERCUBE(s_texCube, 4);

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
	vec4 albedoData = toLinear(texture2D(s_albedo, v_texcoord0) );
	vec4 normalData = texture2D(s_normal, v_texcoord0);
	vec3 normal = decodeNormalUint(normalData.xyz);
	float roughness = clamp(albedoData.w, 0.02, 1.0);
	float metalness = clamp(normalData.w, 0.0, 1.0);
	vec3 albedo = albedoData.xyz;
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

	// Primary light (with shadows and material response).
	vec3 lightDir1 = normalize(-u_lightDirIntensity.xyz);
	float ndotl1 = max(dot(normal, lightDir1), 0.0);
	
	vec4 shadowProj = mul(u_lightMtx, vec4(wpos + normal * u_shadowParams.y, 1.0));
	float visibility = 1.0;
	if (u_shadowParams.z > 0.5)
	{
		visibility = shadow2D(s_shadowMap, shadowProj.xyz / shadowProj.w);
	}
	
	vec3 f0 = mix(vec3_splat(0.04), albedo, metalness);
	float gloss = 1.0 - roughness;
	float specPower = mix(8.0, 128.0, gloss * gloss);
	vec3 h1 = normalize(lightDir1 + viewDir);
	float hdotn1 = max(dot(h1, normal), 0.0);
	vec3 specular1 = f0 * pow(hdotn1, specPower) * visibility;
	vec3 diffuse1 = albedo * (1.0 - metalness) * ndotl1 * u_lightDirIntensity.w * visibility;
	vec3 color1 = vec3(1.0, 0.95, 0.8) * (diffuse1 + specular1);

	// Secondary light (fill, no shadows).
	vec3 lightDir2 = normalize(-u_lightDirIntensity2.xyz);
	float ndotl2 = max(dot(normal, lightDir2), 0.0);
	vec3 h2 = normalize(lightDir2 + viewDir);
	float hdotn2 = max(dot(h2, normal), 0.0);
	vec3 specular2 = f0 * pow(hdotn2, specPower * 0.5) * 0.5;
	vec3 diffuse2 = albedo * (1.0 - metalness) * ndotl2 * u_lightDirIntensity2.w;
	vec3 color2 = vec3(0.5, 0.7, 1.0) * (diffuse2 + specular2);

	vec3 ambient = albedo * (1.0 - metalness) * u_lightAmbient.x;

	vec3 iblSpecular = vec3_splat(0.0);
	if (u_iblParams.z > 0.5)
	{
		float iblRoughness = clamp(roughness * u_iblParams.x, 0.02, 1.0);
		float mip = 1.0 + 5.0 * iblRoughness;
		vec3 refl = reflect(-viewDir, normal);
		refl = fixCubeLookup(refl, mip, 256.0);
		iblSpecular = toLinear(textureCubeLod(s_texCube, refl, mip).xyz) * f0 * clamp(u_iblParams.y, 0.0, 1.0);
	}

	vec3 lightColor = (ambient + color1 + color2 + iblSpecular) * depthMask;
	gl_FragColor = vec4(toGamma(lightColor), 1.0);
}
