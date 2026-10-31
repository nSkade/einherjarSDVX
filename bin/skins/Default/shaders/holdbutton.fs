#version 330
#extension GL_ARB_separate_shader_objects : enable

layout(location=1) in vec2 fsTex;
layout(location=0) out vec4 target;

uniform sampler2D mainTex;
uniform float objectGlow;

in vec4 position;
in float len;

uniform float trackPos;
uniform float trackScale;
uniform float hiddenCutoff;
uniform float hiddenFadeWindow;
uniform float suddenCutoff;
uniform float suddenFadeWindow;

// 20Hz flickering. 0 = Miss, 1 = Inactive, 2 & 3 = Active alternating.
uniform int hitState;

void main()
{
	float s = float(hitState);
	vec4 color = texture(mainTex, vec2(fsTex.x, s / 4 + fsTex.y / 4));
	target = color;

	float off = trackPos + position.y * trackScale;

	float fadeOutDist = .9; // natural cutoff at in
	if (trackPos + len > fadeOutDist)
		target.a *= (1.-(trackPos+len))/(1.-fadeOutDist);
	else if (trackPos + len < -fadeOutDist + 8.5/11.) // and out //TODO find better value than 8.5
		target.a *= (1.-(8.5/11.)+(trackPos+len))/(1.-fadeOutDist);
	
	if(hiddenCutoff < suddenCutoff) {
		float hiddenCutoffFade = hiddenCutoff - hiddenFadeWindow;
		if (off < hiddenCutoff) {
			target = target * max(0.0f, (off - hiddenCutoffFade) / hiddenFadeWindow);
		}
	
		float suddenCutoffFade = suddenCutoff + suddenFadeWindow;
		if (off > suddenCutoff) {
			target = target * max(0.0f, (suddenCutoffFade - off) / suddenFadeWindow);
		}
	} else {
		float hiddenCutoffFade = hiddenCutoff + hiddenFadeWindow;
		if (off > hiddenCutoff) {
			target = target * clamp((off - hiddenCutoffFade) / hiddenFadeWindow, 0.0f, 1.0f);
		}
	
		float suddenCutoffFade = suddenCutoff - suddenFadeWindow;
		if (off < suddenCutoff) {
			target = target * clamp((suddenCutoffFade - off) / suddenFadeWindow, 0.0f, 1.0f);
		}
	
		if (off > suddenCutoff && off < hiddenCutoff) {
			target = target * 0;
		}
	}
}
