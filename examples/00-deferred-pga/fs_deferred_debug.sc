$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_texColor, 0);
SAMPLERCUBE(s_texCube, 1);

uniform vec4 u_debugParams;
uniform mat4 u_invViewProjGeom;
uniform vec4 u_camPos;
uniform vec4 u_iblParams;

void main()
{
	vec3 color = texture2D(s_texColor, v_texcoord0).xyz;

	if (u_debugParams.x > 0.5 && u_debugParams.x < 1.5)
	{
		color = decodeNormalUint(color) * 0.5 + 0.5;
	}
	else if (u_debugParams.x > 1.5 && u_debugParams.x < 2.5)
	{
		float depth = texture2D(s_texColor, v_texcoord0).x;
		color = vec3_splat(depth);
	}
	else if (u_debugParams.x > 2.5 && u_debugParams.x < 3.5)
	{
		color = toGamma(toLinear(color));
	}
	else if (u_debugParams.x > 4.5)
	{
		vec3 clipPos = vec3(v_texcoord0 * 2.0 - 1.0, toClipSpaceDepth(1.0));
#if !BGFX_SHADER_LANGUAGE_GLSL
		clipPos.y = -clipPos.y;
#endif
		vec3 farWpos = clipToWorld(u_invViewProjGeom, clipPos);
		vec3 viewDir = normalize(farWpos - u_camPos.xyz);
		float mip = 1.0 + 5.0 * clamp(u_iblParams.x, 0.0, 1.0);
		vec3 cubeDir = fixCubeLookup(viewDir, mip, 256.0);
		color = toGamma(toLinear(textureCubeLod(s_texCube, cubeDir, mip).xyz) * max(u_iblParams.w, 0.0));
	}

	gl_FragColor = vec4(color, 1.0);
}
