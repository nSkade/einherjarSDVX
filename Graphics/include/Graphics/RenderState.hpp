#pragma once

namespace Graphics
{
	/*
		Contains all values that get passed as built-in values to material shaders
	*/
	class RenderState
	{
	public:
		Transform worldTransform;
		Transform projectionTransform;
		Transform cameraTransform;
		Transform cameraTransformNoMods;
		Vector2i viewportSize;
		float aspectRatio;
		float time;
	};
}