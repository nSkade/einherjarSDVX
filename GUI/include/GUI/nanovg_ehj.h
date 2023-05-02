#pragma once
#include "Shared/Transform.hpp"

//TODO(skade) these are quite oboslete now with nanovg3D

//TODO make a class
//TODO interface for modificatable variables
float g_scale = 1.0f;
Vector2 g_center = Vector2(0.5f,0.5f);
Transform g_ot;

inline void ehj_applyScale(float* tr) {
	// ehj scale
	g_ot = g_guiState.t;
	nvgResetTransform(g_guiState.vg);
	nvgTransform(g_guiState.vg, tr[0]*g_scale, tr[1]*g_scale, tr[2]*g_scale, tr[3]*g_scale,tr[4]*g_scale,tr[5]*g_scale);
	g_guiState.t *= Transform::Scale(Vector2(g_scale));
}

inline void ehj_applyCenter() {
	//TODO why easy?
	float tr2[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	nvgCurrentTransform(g_guiState.vg, tr2);
	nvgResetTransform(g_guiState.vg);
	Vector2 center = Vector2(g_guiState.resolution)*g_center*(1.0-g_scale);
	nvgTransform(g_guiState.vg, tr2[0], tr2[1], tr2[2], tr2[3], tr2[4]+center.x,tr2[5]+center.y);
	g_guiState.t = Transform::Translation(center) * g_guiState.t;
}

inline void ehj_reset(float* tr) {
	nvgResetTransform(g_guiState.vg);
	nvgTransform(g_guiState.vg, tr[0], tr[1], tr[2], tr[3], tr[4], tr[5]);
	g_guiState.t = g_ot;
}