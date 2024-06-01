#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D mainTex;
uniform vec4 lCol;
uniform vec4 rCol;
uniform float hidden;
uniform float sudden;
uniform vec3 uColor;

#define M_E 2.718281828459 

void main()
{
	vec4 col = vec4(0.0);// = mainColor;
	
	// hidden effect
	if (fsTex.y < hidden || fsTex.y > sudden)
	{
		//TODO(skade) specialized hidden track option with
		col.xyz = vec3(0.);
		col.a = 0.3;
		//col.a = col.a > 0.0 ? 0.3 : 0.0; // slightly darkend lane
	} else {
		// logarithmic texture projection for skinned track
		//TODO(skade) manage with perspective?
		float logY = 1.0 - fsTex.y;
		//float logY = fsTex.y;
		logY = log(1.0 + logY*M_E - 1.0*logY);
		logY = 1.0 - logY;
		
		vec4 mainColor = texture(mainTex, vec2(fsTex.x, logY));
		//vec4 mainColor = texture(mainTex, fsTex.xy);
		col = mainColor;
		if(fsTex.y < .2) // fadeout at the end of the track
			col.a = col.a*(fsTex.y)*5.;
		if (fsTex.y > .95)
			col.a = col.a*(1.-fsTex.y)*20.;
	}
	
	col.xyz *= uColor;
	
	target = col;
}