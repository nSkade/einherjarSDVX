#pragma once

#include "Input.hpp"
#include "HitStat.hpp"

/* 
	Game background base class
*/
class Background
{
public:
	virtual ~Background() = default;
	virtual bool Init(bool foreground) = 0;
	virtual void Render(float deltaTime, int fgl) = 0;

	virtual bool hasFG() = 0;
	virtual bool hasFFG() = 0;

	virtual void OnButtonHit(Input::Button button, ScoreHitRating rating, /*MapTime*/int32 delta) = 0;

	class Game* game;
};

// Creates the default game background
Background* CreateBackground(class Game* game, bool foreground = false);