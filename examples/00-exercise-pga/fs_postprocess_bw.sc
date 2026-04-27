$input v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_sceneColor, 0);
uniform vec4 u_filter;

void main()
{
	vec3 color = texture2D(s_sceneColor, v_texcoord0).xyz;
	float grayscale = dot(color, vec3(0.299, 0.587, 0.114));
	float bwMask = 1.0 - smoothstep(u_filter.x - u_filter.y, u_filter.x + u_filter.y, v_texcoord0.x);
	gl_FragColor = vec4(mix(color, vec3_splat(grayscale), bwMask), 1.0);
}

