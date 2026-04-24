$input v_normal, v_texcoord0

#include "../common/common.sh"

SAMPLER2D(s_texColor, 0);

void main()
{
	gl_FragData[0] = texture2D(s_texColor, v_texcoord0);
	gl_FragData[1] = vec4(encodeNormalUint(normalize(v_normal) ), 1.0);
}

