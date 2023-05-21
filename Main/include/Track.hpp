#pragma once
#include "Scoring.hpp"
#include "AsyncLoadable.hpp"
#include <unordered_set>
#include <Shared/Interpolation.hpp>

#define BT_DELAY_FADE_DURATION (4 / 60.f)
#define BT_HIT_EFFECT_DURATION (4 / 60.f)
#define FX_DELAY_FADE_DURATION (3 / 60.f)
#define FX_HIT_EFFECT_DURATION (3 / 60.f)

// Base class for sprite effects on the track
struct TimedEffect
{
	explicit TimedEffect(float duration);
	virtual ~TimedEffect() = default;
	void Reset(float duration);
	float GetRate() const { return time / duration; }
	virtual void Draw(class RenderQueue& rq) = 0;
	virtual void Tick(float deltaTime);

	Track* track;
	float duration;
	float time = 0;
};

// Button hit effect
struct ButtonHitEffect : TimedEffect
{
	ButtonHitEffect();
	void Draw(class RenderQueue& rq) override;
	void Tick(float deltaTime) override;
	void Reset(int buttonCode, Color color, bool hold);
	float GetRate() const { return Math::Min(time, hitEffectDuration) / duration; }

	uint32 buttonCode; // Only used for Draw
	Color color;
	float delayFadeDuration = 0;
	bool held = false;
	float hitEffectDuration;
	float alphaScale = 1;
};

// Button hit rating effect
struct ButtonHitRatingEffect : TimedEffect
{
	ButtonHitRatingEffect(uint32 buttonCode, ScoreHitRating rating);
	void Draw(class RenderQueue& rq) override;

	uint32 buttonCode;
	ScoreHitRating rating;
};

struct TimedHitEffect : TimedEffect
{
	TimedHitEffect(bool late);
	void Draw(class RenderQueue& rq) override;

	bool late;
};

enum SplineInterpType
{
	SIT_LINEAR,
	SIT_COSINE,
	SIT_CUBIC,
};

struct ModSpline
{
	SplineInterpType type = SIT_LINEAR;
	uint32_t splineIndex = 0; //TODO(skade) use for updating spline for indipendency of offset.
	float value = 0.f;
	float offset = 0.f; ///< Offset from critline.
};

enum ModSplineType
{
	MST_X,
	MST_Y,
	MST_Z,
	MST_COUNT,
};

//TODO useless?
enum ModLanes
{
	ML_BTA = 1,
	ML_BTB = 2,
	ML_BTC = 4,
	ML_BTD = 8,
	ML_FXL = 16,
	ML_FXR = 32,
	ML_LSL = 64,
	ML_LSR = 128,
};

enum ModType {
	MT_BASEROT,
	MT_TRANSLATE,
	MT_GLOBROT,
	MT_COUNT,
};

struct Mod
{
	uint32_t id = 0; //TODO(skade) hash name for faster access
	std::vector<ModSpline> splines[MST_COUNT];
	Transform gt; // global transform apllied last
	uint8_t affectedLanes = 0; ///< If Bit is set lane is affected.
	bool active = false;
};

/*
	The object responsible for drawing the track.
*/
class Track : Unique, public IAsyncLoadable
{
public:
	// Size constants of various elements
	static const float trackWidth;
	static const float buttonWidth;
	static const float laserWidth;
	static const float fxbuttonWidth;
	static const float buttonTrackWidth;
	static const float opaqueTrackWidth;

	float trackLength;
	float trackTickLength;
	float buttonLength;
	float fxbuttonLength;
	float distantButtonScale = 6.0f; // Skade-code 2.0f -> 6.0f

	// Laser color setting
	Color laserColors[2] = {};

	// Hit effect color settings
	// 0 = Miss
	// 1 = Good
	// 2 = Perfect
	// 3 = Idle
	// 4 = SPerfect
	Color hitColors[5] = {};

	class AsyncAssetLoader* loader = nullptr;

	Track();
	~Track();
	virtual bool AsyncLoad() override;
	virtual bool AsyncFinalize() override;
	void Tick(class BeatmapPlayback& playback, float deltaTime);

	// Draw black laser underlays for wide lasers or all lasers if lane is hidden
	void DrawLaserBase(RenderQueue& rq, class BeatmapPlayback& playback, const Vector<ObjectState*>& objects);
	// Just the board with tick lines
	void DrawBase(RenderQueue& rq);
	// Draws an object
	void DrawObjectState(RenderQueue& rq, class BeatmapPlayback& playback, ObjectState* obj, bool active, const std::unordered_set<MapTime> chipFXTimes[2]);
	// Things like the laser pointers, hit bar and effect
	void DrawOverlays(RenderQueue& rq);
	void DrawHitEffects(RenderQueue& rq);
	// Draws a plane over the track
	void DrawTrackOverlay(RenderQueue& rq, Texture texture, float heightOffset = 0.05f, float widthScale = 1.0f);
	// Draw a centered sprite at pos, relative from the track
	void DrawSprite(RenderQueue& rq, Vector3 pos, Vector2 size, Texture tex, Color color = Color::White, float tilt = 0.0f);
	void DrawCombo(RenderQueue& rq, uint32 score, Color color, float scale = 1.0f);
	void DrawTrackCover(RenderQueue& rq);
	void DrawLaneLight(RenderQueue& rq);
	void DrawCalibrationCritLine(RenderQueue& rq);

	void DrawLineMesh(RenderQueue& rq);

	Vector3 TransformPoint(const Vector3& p);

	// Adds a sprite effect to the track
	void AddEffect(struct TimedEffect* effect);
	void AddHitEffect(uint32 buttonCode, Color color, bool hold = false);
	void ClearEffects();

	void SetViewRange(float newRange);
	void SendLaserAlert(uint8 laserIdx);
	void SetLaneHide(bool hidden, double duration);
	float GetViewRange() const;

	// Normal/FX button X-axis placement
	float GetButtonPlacement(uint32 buttonIdx);

	void OnHoldEnter(Input::Button buttonCode);
	void OnButtonReleased(Input::Button buttonCode);
	void OnButtonReleasedDelta(Input::Button buttonCode, int32 delta);

	// Laser positions, as shown on the overlay
	float laserPositions[2];
	// Current lasers are extended
	bool lasersAreExtend[2] = { false, false };
	float laserPointerOpacity[2] = { 1.0f };
	float laserAlertOpacity[2] = { 1.0f };
	float hiddenCutoff = 0.0f;
	float hiddenFadewindow = 0.2f;

	float suddenCutoff = 0.5f;
	float suddenFadewindow = 0.2f;

	float laserSpeedOffset = 0.90f;
	float centerSplit = 0.0f;

	// Visible time elements on the playfield track
	// a single unit is 1 beat in distance
	Vector2 trackViewRange;

	/* Track base graphics */
	Mesh trackMesh;
	Mesh splitTrackMesh[6];
	Mesh trackTickMesh;
	Mesh splitTrackTickMesh[6];
	Mesh trackCoverMesh;
	Mesh splitTrackCoverMesh[6];
	Mesh calibrationCritMesh;
	Mesh calibrationDarkMesh;
	Material trackMaterial; // Also used for buttons
	Texture trackTexture;
	Texture trackCoverTexture;
	Texture trackTickTexture;

	Mesh m_lineMesh[4];
	Material m_lineMaterial;

	/* Object graphics */
	Mesh buttonMesh;
	Texture buttonTexture;
	Texture buttonHoldTexture;
	Mesh fxbuttonMesh;
	Texture fxbuttonTexture;
	Texture fxbuttonHoldTexture;
	Material holdButtonMaterial;
	Material buttonMaterial;
	Material trackCoverMaterial;
	Texture laserTextures[2];
	Texture laserTailTextures[4]; // Entry and exit textures, both sides
	Material laserMaterial;
	Material blackLaserMaterial;
	Texture laserAlertTextures[2];
	Texture whiteTexture;

	/* Overlay graphics */
	Material trackOverlay;

	/* LaneLight */
	Material laneLightMaterial;
	Texture laneLightTexture;

	/* Scoring and feedback elements */
	Texture scoreHitTexture;
	Texture scoreHitTextures[3]; // Ok, Miss, Perfect
	// Combo counter sprite sheet
	Mesh comboSpriteMeshes[10];
	/* Reusable sprite mesh and material */
	Mesh centeredTrackMesh;
	Material spriteMaterial;

	// For flickering objects, like hold objects that are active
	float objectGlow;
	// 20Hz flickering. 0 = Miss, 1 = Inactive, 2 & 3 = Active alternating.
	int objectGlowState;

	// Early/Late indicator
	struct TimedHitEffect* timedHitEffect = nullptr;

	// Track Origin position
	Transform trackOrigin;

	bool hitEffectAutoplay = false;
	float scrollSpeed = 0;

	float EvaluateSpline(const std::vector<ModSpline>& spline, float height);

	void AddMod(std::string modName, ModType type);

private:
	// Laser track generators
	class LaserTrackBuilder* m_laserTrackBuilder[2] = { 0 };

	const TimingPoint* m_lastTimingPoint = nullptr;

	// Bar tick locations
	Vector<float> m_barTicks;

	// Active effects
	Vector<struct TimedEffect*> m_hitEffects;

	struct ButtonHitEffect m_buttonHitEffects[6];

	// Distance of seen objects on the track
	float m_viewRange;

	MapTime m_lastMapTime = 0;
	float m_cModSpeed = 0.0f;
	float m_initBPM = 0.0f;

	float m_laneLightTimer = 0.0f;

	float m_alertTimer[2] = { 10.0f, 10.0f };

	// Camera variables Landscape, Portrait
	float m_basePitch[2] = { -35.f, -47.f };
	float m_baseRadius[2] = { 0.3f, 0.275f };
	float m_pitchOffset[2] = { 0.05f, 0.265f }; // how far from the bottom of the screen should the crit line be
	float m_fov[2] = { 70.f, 90.f };

	// How much the track is hidden. 1.0 = fully hidden, 0.0 = fully visible
	float m_trackHide = 0.0f;
	float m_trackHideSpeed = 0.0f;
	float m_btOverFxScale = 0.8f;

	/**
	 * @brief Converts single index number to multibit index for affectedLanes.
	 * @param index Button index 0-3 for bt, 4-5 for fx, 6-7 for laser.
	*/
	uint8_t ButtonIndexToAffectedLane(uint8_t index) { return 1 << index; }

	/**
	 * @brief Iterates through list of mods and returns the Offset Vector.
	 * @param mods List of mods to iterate through.
	 * @param p Current position of the button before any applied mods. //TODO
	 * @param btx Button Index. 0-3 bt 4-5 fx 6-7 laser
	*/
	Vector3 EvaluateMods(const std::vector<Mod>& mods, float yOffset, uint8_t btx);

	std::vector<Mod> m_modsRot;
	std::vector<Mod> m_modsTrans;
	std::vector<Mod> m_modsGRot;
};
