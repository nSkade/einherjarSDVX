#version 420
//#version 330

#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 inPos;
layout(location=1) in vec2 inTex;

out vec4 position;
out float len;

out gl_PerVertex
{
	vec4 gl_Position;
};
layout(location=1) out vec2 fsTex;

uniform mat4 proj;
uniform mat4 camera;
uniform mat4 world;

void main()
{
	vec2 packLenUVx = unpackHalf2x16(floatBitsToUint(inTex.x));
	fsTex = vec2(packLenUVx.x,inTex.y);
	len = packLenUVx.y;

	position = vec4(inPos, 1);

	gl_Position = proj * camera * world * vec4(inPos, 1);
}
