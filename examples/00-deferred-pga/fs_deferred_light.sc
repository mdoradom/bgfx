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

// Calculate shadows with dynamic 3x3 PCF
float calculateShadow(vec3 wpos, vec3 normal)
{
	if (u_shadowParams.z < 0.5) return 1.0;

	// Apply normal bias
	vec4 shadowProj = mul(u_lightMtx, vec4(wpos + normal * u_shadowParams.y, 1.0));
	vec3 shadowCoord = shadowProj.xyz / shadowProj.w;

	// Apply depth bias
	shadowCoord.z -= u_shadowParams.x;

	// Dynamic texel size based on current resolution
	float currentRes = max(u_shadowParams.w, 512.0);
	vec2 texelSize = vec2(1.0 / currentRes, 1.0 / currentRes);

	float visibility = 0.0;
	// PCF filter with 9 samples
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			vec2 offset = vec2(float(x), float(y)) * texelSize;
			visibility += shadow2D(s_shadowMap, vec3(shadowCoord.xy + offset, shadowCoord.z));
		}
	}
	return visibility / 9.0;
}

void main()
{
	// 1. G-Buffer Unpacking
	vec4 albedoData = toLinear(texture2D(s_albedo, v_texcoord0));
	vec4 normalData = texture2D(s_normal, v_texcoord0);
	vec3 albedo = albedoData.xyz;
	float roughness = clamp(albedoData.w, 0.02, 1.0);
	float metalness = clamp(normalData.w, 0.0, 1.0);
	vec3 normal = decodeNormalUint(normalData.xyz);

	float deviceDepth = texture2D(s_depth, v_texcoord0).x;
	float clipDepth = toClipSpaceDepth(deviceDepth);
	float depthMask = clipDepth < 1.0 ? 1.0 : 0.0;

	// 2. World Position Reconstruction
	vec3 clipPos = vec3(v_texcoord0 * 2.0 - 1.0, clipDepth);
	clipPos.y = -clipPos.y;
	vec3 wpos = clipToWorld(u_invViewProjGeom, clipPos);
	vec3 viewDir = normalize(u_camPos.xyz - wpos);

	// 3. Material and Shadow Calculations
	vec3 f0 = mix(vec3_splat(0.04), albedo, metalness);
	float gloss = 1.0 - roughness;
	float specPower = mix(8.0, 128.0, gloss * gloss);

	float visibility = calculateShadow(wpos, normal);

	// 4. Directional Light
	vec3 lightDir1 = normalize(-u_lightDirIntensity.xyz);
	vec3 h1 = normalize(lightDir1 + viewDir);
	float ndotl1 = max(dot(normal, lightDir1), 0.0);
	vec3 specular1 = f0 * pow(max(dot(h1, normal), 0.0), specPower) * visibility;
	vec3 diffuse1 = albedo * (1.0 - metalness) * ndotl1 * u_lightDirIntensity.w * visibility;
	vec3 color1 = vec3(1.0, 0.95, 0.8) * (diffuse1 + specular1);

	// 5. Fill Light
	vec3 lightDir2 = normalize(-u_lightDirIntensity2.xyz);
	vec3 h2 = normalize(lightDir2 + viewDir);
	float ndotl2 = max(dot(normal, lightDir2), 0.0);
	vec3 specular2 = f0 * pow(max(dot(h2, normal), 0.0), specPower * 0.5) * 0.5;
	vec3 diffuse2 = albedo * (1.0 - metalness) * ndotl2 * u_lightDirIntensity2.w;
	vec3 color2 = vec3(0.5, 0.7, 1.0) * (diffuse2 + specular2);

	// 6. SSAO
	float ssao = 1.0;
	float ssaoRadius = u_lightAmbient.y;
	if (ssaoRadius > 0.0)
	{
		float occlusion = 0.0;
		float ssaoIntensity = u_lightAmbient.z;
		vec3 dpdx = dFdx(wpos);
		vec3 dpdy = dFdy(wpos);
		float dUVdx = length(dFdx(v_texcoord0));
		float dUVdy = length(dFdy(v_texcoord0));
		vec2 uvRadius = ssaoRadius * vec2(dUVdx / max(length(dpdx), 0.001), dUVdy / max(length(dpdy), 0.001));
		float random = fract(sin(dot(v_texcoord0.xy, vec2(12.9898, 78.233))) * 43758.5453);
		for (int i = 0; i < 8; ++i)
		{
			float angle = float(i) * (2.0 * 3.14159 / 8.0) + random;
			vec2 offset = vec2(cos(angle), sin(angle)) * uvRadius;
			float sampleDeviceDepth = texture2D(s_depth, v_texcoord0 + offset).x;
			float sampleClipDepth = toClipSpaceDepth(sampleDeviceDepth);
			vec3 sampleClipPos = vec3((v_texcoord0 + offset) * 2.0 - 1.0, sampleClipDepth);
			sampleClipPos.y = -sampleClipPos.y;
			vec3 sampleWpos = clipToWorld(u_invViewProjGeom, sampleClipPos);
			vec3 dir = sampleWpos - wpos;
			float dist = length(dir);
			occlusion += max(0.0, dot(normal, dir) / dist - 0.2) * (1.0 / (1.0 + dist));
		}
		ssao = clamp(1.0 - (occlusion / 8.0) * ssaoIntensity, 0.0, 1.0);
	}

	// 7. Ambient and IBL
	vec3 ambient = albedo * (1.0 - metalness) * u_lightAmbient.x * ssao;
	vec3 iblSpecular = vec3_splat(0.0);
	if (u_iblParams.z > 0.5)
	{
		float iblRoughness = clamp(roughness * u_iblParams.x, 0.02, 1.0);
		float mip = 1.0 + 5.0 * iblRoughness;
		vec3 refl = reflect(-viewDir, normal);
		refl = fixCubeLookup(refl, mip, 256.0);
		iblSpecular = toLinear(textureCubeLod(s_texCube, refl, mip).xyz) * f0 * clamp(u_iblParams.y, 0.0, 1.0) * ssao;
	}

	// 8. Final result
	vec3 lightColor = (ambient + color1 + color2 + iblSpecular) * depthMask;
	gl_FragColor = vec4(toGamma(lightColor), 1.0);
}