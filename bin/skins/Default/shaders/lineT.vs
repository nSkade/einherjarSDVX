#version 330
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 inPos;

out gl_PerVertex
{
	vec4 gl_Position;
};

uniform mat4 proj;
uniform mat4 camera;
uniform mat4 world;

void main()
{
	gl_Position = proj * camera * world *vec4(inPos.xyz, 1);
}
