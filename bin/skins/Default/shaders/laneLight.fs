#version 330
#extension GL_ARB_separate_shader_objects : enable

#ifdef GL_ES
precision mediump float;
#endif

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D mainTex;
uniform float timer;
uniform float speed;

void main()
{	
	target = texture(mainTex, vec2(fsTex.x, (fsTex.y*8.0 - timer*speed/125)))*0.8;
}