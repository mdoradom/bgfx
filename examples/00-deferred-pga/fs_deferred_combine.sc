$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_light, 0);
SAMPLER2D(s_depth, 1);
SAMPLERCUBE(s_texCube, 2);

uniform mat4 u_invViewProjGeom;
uniform vec4 u_camPos;
uniform vec4 u_iblParams;

void main()
{
	vec3 litColor = toLinear(texture2D(s_light, v_texcoord0) ).xyz;

	float deviceDepth = texture2D(s_depth, v_texcoord0).x;
	float hasGeometry = deviceDepth < 0.9999 ? 1.0 : 0.0;

	vec3 clipPos = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(1.0));
#if !BGFX_SHADER_LANGUAGE_GLSL
	clipPos.y = -clipPos.y;
#endif
	vec3 farWpos = clipToWorld(u_invViewProjGeom, clipPos);
	vec3 viewDir = normalize(farWpos - u_camPos.xyz);

	float mip = 1.0 + 5.0 * clamp(u_iblParams.x, 0.0, 1.0);
	vec3 cubeDir = fixCubeLookup(viewDir, mip, 256.0);
	vec3 skybox = toLinear(textureCubeLod(s_texCube, cubeDir, mip).xyz) * max(u_iblParams.w, 0.0);

	vec3 outColor = mix(skybox, litColor, hasGeometry);
	gl_FragColor = toGamma(vec4(outColor, 1.0));
}
