#include "stdafx.h"
#include "Application.hpp"
#include "GameConfig.hpp"
#include "Game.hpp"
#include "Track.hpp"
#include "LaserTrackBuilder.hpp"
#include "AsyncAssetLoader.hpp"
#include <unordered_set>

#include <functional>

const float Track::trackWidth = 1.0f;
const float Track::buttonWidth = 1.0f / 6;
const float Track::laserWidth = buttonWidth;
const float Track::fxbuttonWidth = buttonWidth * 2;
const float Track::buttonTrackWidth = buttonWidth * 4;
const float Track::opaqueTrackWidth = buttonTrackWidth * 1.357;

Track::Track()
{
	//TODO(skade) remove view range dependency on track by clipping track/hold/laser
	m_viewRange = 2.0f;
	if (g_aspectRatio < 1.0f)
		trackLength = 12.0f;
	else
		trackLength = 10.0f;

	for (uint32_t i=0;i<8;++i)
		m_meshOffsets[i] = std::vector<Transform>(m_mqLine);
}

Track::~Track()
{
	RemoveAllMods();
	g_input.OnButtonReleased.Remove(this, &Track::OnButtonReleasedDelta);

	delete loader;
	delete m_laserTrackBuilder;
	for (auto & m_hitEffect : m_hitEffects)
		delete m_hitEffect;
	delete timedHitEffect;
}

bool Track::AsyncLoad()
{
	g_input.OnButtonReleased.Add(this, &Track::OnButtonReleasedDelta);

	loader = new AsyncAssetLoader();
	String skin = g_application->GetCurrentSkin();

	float laserHues[2] = { 0.f };
	laserHues[0] = g_gameConfig.GetFloat(GameConfigKeys::Laser0Color);
	laserHues[1] = g_gameConfig.GetFloat(GameConfigKeys::Laser1Color);
	m_btOverFxScale = Math::Clamp(g_gameConfig.GetFloat(GameConfigKeys::BTOverFXScale), 0.01f, 1.0f);

	for (uint32 i = 0; i < 2; i++)
		laserColors[i] = Color::FromHSV(laserHues[i],1.0,1.0);

	// Load hit effect colors
	Image hitColorPalette;
	CheckedLoad(hitColorPalette = ImageRes::Create(Path::Absolute("skins/" + skin + "/textures/hitcolors.png")));
	assert(hitColorPalette->GetSize().x >= 5);
	for(uint32 i = 0; i < 5; i++)
		hitColors[i] = hitColorPalette->GetBits()[i];

	// mip-mapped and anisotropicaly filtered track textures
	loader->AddTexture(trackTexture, "track.png");
	loader->AddTexture(trackTickTexture, "tick.png");

	// Scoring texture
	loader->AddTexture(scoreHitTexture, "scorehit.png");


	for(uint32 i = 0; i < 3; i++)
	{
		loader->AddTexture(scoreHitTextures[i], Utility::Sprintf("score%d.png", i));
	}

	// Load Button object
	loader->AddTexture(buttonTexture, "button.png");
	loader->AddTexture(buttonHoldTexture, "buttonhold.png");

	// Load FX object
	loader->AddTexture(fxbuttonTexture, "fxbutton.png");
	loader->AddTexture(fxbuttonHoldTexture, "fxbuttonhold.png");

	// Load Laser object
	loader->AddTexture(laserTextures[0], "laser_l.png");
	loader->AddTexture(laserTextures[1], "laser_r.png");

	// Entry and exit textures for laser
	loader->AddTexture(laserTailTextures[0], "laser_entry_l.png");
	loader->AddTexture(laserTailTextures[1], "laser_entry_r.png");
	loader->AddTexture(laserTailTextures[2], "laser_exit_l.png");
	loader->AddTexture(laserTailTextures[3], "laser_exit_r.png");

	// Track materials
	loader->AddMaterial(trackMaterial, "track");
	loader->AddMaterial(spriteMaterial, "sprite"); // General purpose material
	loader->AddMaterial(buttonMaterial, "button");
	loader->AddMaterial(holdButtonMaterial, "holdbutton");
	loader->AddMaterial(laserMaterial, "laser");
	loader->AddMaterial(blackLaserMaterial, "blackLaser");
	loader->AddMaterial(trackOverlay, "overlay");

	loader->AddMaterial(m_lineMaterial, "lineT");

	return loader->Load();
}

bool Track::AsyncFinalize()
{
	// Finalizer loading textures/material/etc.
	bool success = loader->Finalize();
	delete loader;
	loader = nullptr;

	// Load track cover material & texture here for skin back-compat
	trackCoverMaterial = g_application->LoadMaterial("trackCover");
	trackCoverTexture = g_application->LoadTexture("trackCover.png");
	if (trackCoverMaterial)
	{
		trackCoverMaterial->opaque = false;
	}

	//laneLight
	laneLightMaterial = g_application->LoadMaterial("laneLight");
	laneLightMaterial->blendMode = MaterialBlendMode::Additive;
	laneLightMaterial->opaque = false;
	laneLightTexture = g_application->LoadTexture("laneLight.png");
	
	// Set Texture states
	trackTexture->SetMipmaps(false);
	trackTexture->SetFilter(true, true, 16.0f);
	trackTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	trackTickTexture->SetMipmaps(true);
	trackTickTexture->SetFilter(true, true, 16.0f);
	trackTickTexture->SetWrap(TextureWrap::Repeat, TextureWrap::Clamp);
	trackTickLength = trackTickTexture->CalculateHeight(buttonTrackWidth);
	scoreHitTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);

	buttonTexture->SetMipmaps(true);
	buttonTexture->SetFilter(true, true, 16.0f);
	buttonHoldTexture->SetMipmaps(true);
	buttonHoldTexture->SetFilter(true, true, 16.0f);
	buttonLength = buttonTexture->CalculateHeight(buttonWidth);
	buttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(buttonWidth, buttonLength));
	buttonMaterial->opaque = false;
	buttonMaterial->depthTest = false;

	fxbuttonTexture->SetMipmaps(true);
	fxbuttonTexture->SetFilter(true, true, 16.0f);
	fxbuttonHoldTexture->SetMipmaps(true);
	fxbuttonHoldTexture->SetFilter(true, true, 16.0f);
	fxbuttonLength = fxbuttonTexture->CalculateHeight(fxbuttonWidth);
	fxbuttonMesh = MeshGenerators::Quad(g_gl, Vector2(0.0f, 0.0f), Vector2(fxbuttonWidth, fxbuttonLength));

	holdButtonMaterial->opaque = false;
	holdButtonMaterial->depthTest = false;
	
	for (uint32_t i=0;i<8;++i) {
		m_lineMesh[i] = MeshRes::Create(g_gl);
		m_lineMesh[i]->SetPrimitiveType(PrimitiveType::LineStrip);
	}

	for (auto &laserTexture : laserTextures)
	{
		laserTexture->SetMipmaps(true);
		laserTexture->SetFilter(true, true, 16.0f);
		laserTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Repeat);
	}

	for (auto &laserTailTexture : laserTailTextures)
	{
		laserTailTexture->SetMipmaps(true);
		laserTailTexture->SetFilter(true, true, 16.0f);
		laserTailTexture->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
	}

	// Track and sprite material (all transparent)
	trackMaterial->opaque = false;
	trackMaterial->depthTest = true;
	spriteMaterial->opaque = false;

	// Laser object material, allows coloring and sampling laser edge texture
	laserMaterial->blendMode = MaterialBlendMode::Additive;
	laserMaterial->opaque = false;
	laserMaterial->depthTest = false;
	blackLaserMaterial->opaque = false;

	// Overlay shader
	trackOverlay->opaque = false;

	// Create a laser track builder
	// these will output and cache meshes for rendering lasers
	m_laserTrackBuilder = new LaserTrackBuilder(g_gl, this);
	m_laserTrackBuilder->laserBorderPixels = 12;
	m_laserTrackBuilder->laserLengthScale = trackLength / (GetViewRange() * laserSpeedOffset);
	m_laserTrackBuilder->Reset(); // Also initializes the track builder

	// Generate simple planes for the playfield track and elements
	trackMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -1), Vector2(trackWidth, trackLength + 1));
	
	UpdateTrackMeshData();

	for (size_t i = 0; i < 6; i++)
	{
		//TODO(skade)
		//track cover
		Vector2 pos = Vector2(-trackWidth*0.5f + (1.0f/6.0f)*i*trackWidth, -trackLength);
		Vector2 size = Vector2(trackWidth / 6.0f, trackLength * 2.0);
		Rect uv = Rect(float(i)/6.0f, 0.0f, float(i+1)/6.0f, 1.0f);
		Rect rect = Rect(pos, size);
		splitTrackCoverMesh[i] = MeshRes::Create(g_gl);
		splitTrackCoverMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		Vector<MeshGenerators::SimpleVertex> splitMeshData;
		MeshGenerators::GenerateSimpleXYQuad(rect, uv, splitMeshData);
		splitTrackCoverMesh[i]->SetData(splitMeshData);

		//tick meshes
		//pos = Vector2(-trackWidth * 0.5f + (1.0f/6.0f)*i*trackWidth, 0.0f);
		//size = Vector2(trackWidth / 6.0f, trackTickLength); // Skade-code buttonTrackWidth ->
		size = Vector2(trackWidth / 6.0f, trackTickLength); // Skade-code buttonTrackWidth ->
		if (i==0||i==5)
			size = Vector2(trackWidth/6.0f*0.67f,trackTickLength); // make tick width on laser lane smaller
		pos = Vector2(-size.x*.5f,-size.y*.5);
		rect = Rect(pos, size);
		splitTrackTickMesh[i] = MeshRes::Create(g_gl);
		splitTrackTickMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		splitMeshData.clear();
		MeshGenerators::GenerateSimpleXYQuad(rect, uv, splitMeshData);
		splitTrackTickMesh[i]->SetData(splitMeshData);
	}

	calibrationCritMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -0.02f), Vector2(trackWidth, 0.02f));
	calibrationDarkMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -1.0f), Vector2(trackWidth, 0.99f));
	trackCoverMesh = MeshGenerators::Quad(g_gl, Vector2(-trackWidth * 0.5f, -trackLength), Vector2(trackWidth, trackLength * 2));
	trackTickMesh = MeshGenerators::Quad(g_gl, Vector2(-opaqueTrackWidth * 0.37f, 0.0f), Vector2(opaqueTrackWidth, trackTickLength)); //Skade-code buttonTrackWidth -> buttonTrackWidth*1.357f
	centeredTrackMesh = MeshGenerators::Quad(g_gl, Vector2(-0.5f, -0.5f), Vector2(1.0f, 1.0f));
	uint8 whiteData[4] = { 255, 255, 255, 255 };
	whiteTexture = TextureRes::Create(g_gl);
	whiteTexture->SetData({ 1,1 }, (void*)whiteData);

	timedHitEffect = new TimedHitEffect(false);
	timedHitEffect->time = 0;
	timedHitEffect->track = this;

	bool delayedHitEffects = g_gameConfig.GetBool(GameConfigKeys::DelayedHitEffects);

	for (int i = 0; i < 6; ++i)
	{
		ButtonHitEffect& bfx = m_buttonHitEffects[i];
		if (delayedHitEffects)
		{
			if (i < 4)
			{
				bfx.delayFadeDuration = BT_DELAY_FADE_DURATION;
				bfx.hitEffectDuration = BT_HIT_EFFECT_DURATION;
				bfx.alphaScale = 0.6f; // Ranges from 0.6 to 0.85 depending on hispeed
			}
			else
			{
				bfx.delayFadeDuration = FX_DELAY_FADE_DURATION;
				bfx.hitEffectDuration = FX_HIT_EFFECT_DURATION;
				bfx.alphaScale = 0.45f;
			}
		}
		else
		{
			bfx.delayFadeDuration = 0;
			bfx.hitEffectDuration = 7 / 60.f;
			bfx.alphaScale = 1;
		}
		bfx.buttonCode = i;
		bfx.track = this;
	}

	// Dont loose scope of Materials on change.
	trackMaterialOG = trackMaterial;
	buttonMeshOG = buttonMesh;
	buttonMaterialOG = buttonMaterial;

	return success;
}
void Track::Tick(class BeatmapPlayback& playback, float deltaTime)
{
	const TimingPoint& currentTimingPoint = playback.GetCurrentTimingPoint();

	if (m_initBPM == 0.0f) m_initBPM = currentTimingPoint.GetBPM();

	if(&currentTimingPoint != m_lastTimingPoint)
	{
		m_lastTimingPoint = &currentTimingPoint;
	}

	// Button Hit FX
	for (auto it = m_hitEffects.begin(); it != m_hitEffects.end();)
	{
		(*it)->Tick(deltaTime);
		if((*it)->time <= 0.0f)
		{
			delete *it;
			it = m_hitEffects.erase(it);
			continue;
		}
		it++;
	}
	for (auto& bfx : m_buttonHitEffects)
		bfx.Tick(deltaTime);
		
	timedHitEffect->Tick(deltaTime);

	MapTime currentTime = playback.GetLastTime();
	m_cModSpeed = playback.cModSpeed * currentTimingPoint.GetBPM()/m_initBPM;

	// Set the view range of the track
	trackViewRange = Vector2((float)currentTime, 0.0f);
	trackViewRange.y = trackViewRange.x + GetViewRange();

	m_barTicks.clear();
	playback.GetBarPositionsInViewRange(m_viewRange, m_barTicks);

	// Update track hide status
	m_trackHide += m_trackHideSpeed * deltaTime;
	m_trackHide = Math::Clamp(m_trackHide, 0.0f, 1.0f);
	m_trackHideSud -= m_trackHideSudSpeed * deltaTime;
	m_trackHideSud = Math::Clamp(m_trackHideSud, 0.0f, 1.0f);

	// Set Object glow
	int32 startBeat = 0;
	//TODO(skade) useless remove. Maybe resurrect?
	//uint32 numBeats = playback.CountBeats(m_lastMapTime, currentTime - m_lastMapTime, startBeat, 4);
	objectGlowState = currentTime % 100 < 50 ? 0 : 1;
	m_lastMapTime = currentTime;
	m_laneLightTimer += deltaTime;

	objectGlow = fabs((currentTime % 100) / 50.0 - 1) * 0.5 + 0.5;

	/*
	if(numBeats > 0)
	{
		objectGlow = 1.0f;
	}
	else
	{
		objectGlow -= 7.0f * deltaTime;
		if(objectGlow < 0.0f)
			objectGlow = 0.0f;
	}
	*/

	// Perform laser track cache cleanup, etc.
	m_laserTrackBuilder->Update(m_lastMapTime);

	//laserAlertOpacity[i] = (-pow(m_alertTimer[i], 2.0f) + (1.5f * m_alertTimer[i])) * 5.0f;
	//laserAlertOpacity[i] = Math::Clamp<float>(laserAlertOpacity[i], 0.0f, 1.0f);
	//m_alertTimer[i] += deltaTime;

	UpdateMeshMods();
}

//TODO(skade) Not working currently (obsolete?)
void Track::DrawLaserBase(RenderQueue& rq, class BeatmapPlayback& playback, const Vector<ObjectState*>& objects)
{
	for (auto obj : objects)
	{
		if (obj->type != ObjectType::Laser)
			continue;

		LaserObjectState* laser = (LaserObjectState*)obj;
		if ((laser->flags & LaserObjectState::flag_Extended) != 0 || m_trackHide > 0.f)
		{
			// Calculate height based on time on current track
			float viewRange = GetViewRange();
			float position = playback.TimeToViewDistance(obj->time);
			float posmult = trackLength / (m_viewRange * laserSpeedOffset);

			Mesh laserMesh;// = m_laserTrackBuilder->GenerateTrackMesh(playback, laser);

			MaterialParameterSet laserParams;
			laserParams.SetParameter("mainTex", laserTextures[laser->index]);

			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			laserTransform *= Transform::Translation(Vector3{ 0.0f, posmult * position, 0.0f });

			if (laserMesh)
			{
				rq.Draw(laserTransform, laserMesh, blackLaserMaterial, laserParams);
			}
		}
	}
}

void Track::DrawBase(class RenderQueue& rq)
{
	// Base
	MaterialParameterSet params;
	Transform transform = trackOrigin;
	params.SetParameter("mainTex", trackTexture);
	params.SetParameter("lCol", laserColors[0]);
	params.SetParameter("rCol", laserColors[1]);
	params.SetParameter("hidden", m_trackHide);
	params.SetParameter("sudden", m_trackHideSud);
	params.insert(trackParamsCust.begin(),trackParamsCust.end());

	bool mode_seven = true; //TODO make configurable
	
	if (centerSplit != 0.0f || mode_seven) {

		for (uint32_t i = 0; i < 6; ++i) {
			uint32_t idx = i;
			if (idx==0) idx = 6; // laser left
			else if (idx==5) idx = 7;
			else idx--; // BTA is 1
			
			Vector3 ml = Vector3(-.5f*trackWidth/6.f,0,0);
			Vector3 mr = Vector3(.5f*trackWidth/6.f,0,0);
			
			Vector<MeshGenerators::SimpleVertex> msmd = m_splitMeshData[i];
			for (uint32_t j = 0; j < m_mqTrack; j++) {
			
				uint32_t im = (m_mqTrack-1-j)*2;
				Transform t = EvaluateModTransform(msmd[im].pos+Vector3(.5f*trackWidth / 6.0f,0,0),((float)j)/(m_mqTrack-1),idx,MA_TRACK);

				msmd[im].pos = t*ml;
				msmd[im+1].pos = t*mr;
			}
			uint32_t cls = (m_mqTrack)*2; // crit line start
			for (uint32_t j=0;j<m_mqTrackNeg;++j) { //TODO
				uint32_t im = j*2+cls;
				float tl = 1.f/trackLength;
				Transform t = EvaluateModTransform(msmd[im].pos+Vector3(.5f*trackWidth / 6.0f,0,0),-((float)j)/(m_mqTrackNeg-1)*tl,idx,MA_TRACK);

				msmd[im].pos = t*ml;
				msmd[im+1].pos = t*mr;
			}
			//TODO
			//uint32_t last = msmd.size()-1;
			//Transform t = EvaluateModTransform(msmd[last-1].pos+Vector3(.5f*trackWidth / 6.0f,0,0),0.f,idx,MA_TRACK);
			//msmd[last-0].pos = t*mr;
			//msmd[last-1].pos = t*ml;
			
			//TODO(skade) more efficient?
			msmd = MeshGenerators::Triangulate(msmd);
			splitTrackMesh[i]->SetData(msmd);
		}
		
		rq.Draw(transform * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[0], trackMaterial, params);
		params.SetParameter("uColor",Vector3(255.f,157.f,45.f)/Vector3(255.f)*4.f);
		rq.Draw(transform * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[1], trackMaterial, params);
		params.SetParameter("uColor",Vector3(255.f,123.f,206.f)/Vector3(255.f)*4.f);
		rq.Draw(transform * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[2], trackMaterial, params);
		params.SetParameter("uColor",Vector3(0.f,143.f,255.f)/Vector3(255.f)*4.f);
		rq.Draw(transform * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[3], trackMaterial, params);
		params.SetParameter("uColor",Vector3(37.f,227.f,89.f)/Vector3(255.f)*4.f);
		rq.Draw(transform * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[4], trackMaterial, params);
		params.SetParameter("uColor",Vector3(1.f));
		rq.Draw(transform * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[5], trackMaterial, params);
	} else {
		rq.Draw(transform, trackMesh, trackMaterial, params);
	}

	// Draw the main beat ticks on the track
	params.SetParameter("mainTex", trackTickTexture);
	params.SetParameter("hasSample", false);
	params.SetParameter("uColor",Vector3(1.f,1.f,1.f));
	//TODO(skade) cleanup
	for (float f : m_barTicks)
	{
		float fLocal = f / m_viewRange;
		Vector3 tickPosition = Vector3(0.0f, trackLength * fLocal - trackTickLength * 0.5f, 0.0f); // Skade-code 0.0f -> -buttonTrackWidth / 5.6
		Transform tT = trackOrigin;
		if (centerSplit != 0.0f || true) {
			
			for (uint32_t i=0;i<6;++i) {
				uint32_t idx = i-1;
				if (i==0) idx = 6;
				if (i==5) idx = 7;

				Vector3 centerSplitPos;
				if (i==0)
					centerSplitPos = Vector3({-centerSplit * 0.5f * buttonWidth + .25f*0.33f*buttonWidth, 0.0f, 0.0f});
				else if (i==5)
					centerSplitPos = Vector3({-centerSplit * 0.5f * buttonWidth - .25f*0.33f*buttonWidth, 0.0f, 0.0f});
				else
					centerSplitPos = Vector3({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f});
				float lw = trackWidth/6.f;
				Vector3 tickx = Vector3({-trackWidth * 0.5f + i*lw+lw*.5f,0.f,0.f});
				Transform mt = EvaluateModTransform(tickx+tickPosition+centerSplitPos,fLocal,idx,MA_TICK);
				rq.Draw(tT * mt, splitTrackTickMesh[i], buttonMaterial, params);
			}
		}// else {
		//	tT *= Transform::Translation(tickPosition);
		//	rq.Draw(tT * Transform::Translation({-buttonTrackWidth/5.6f, 0.0f, 0.0f}), trackTickMesh, buttonMaterial, params);
		//}
	}
}
void Track::DrawObjectState(RenderQueue& rq, class BeatmapPlayback& playback, ObjectState* obj, bool active, const std::unordered_set<MapTime> chipFXTimes[2])
{
	// Calculate height based on time on current track
	float viewRange = GetViewRange();
	float glow = 0.0f;

	const bool dontUseScrollSpeedForPos =
		obj->type == ObjectType::Hold ? ((MultiObjectState*)obj)->hold.GetRoot()->time <= playback.GetLastTime()
		: obj->type == ObjectType::Laser ? obj->time <= playback.GetLastTime()
		: false;

	float position = dontUseScrollSpeedForPos ? playback.TimeToViewDistanceIgnoringScrollSpeed(obj->time) : playback.TimeToViewDistance(obj->time);
	position /= viewRange;
	// position is now between 0.0 and 1.0

	if(obj->type == ObjectType::Single)
	{
		bool isHold = obj->type == ObjectType::Hold; //TODO(skade)
		MultiObjectState* mobj = (MultiObjectState*)obj;
		MaterialParameterSet params;
		Material mat = buttonMaterial;
		Mesh mesh;
		float xscale = 1.0f;
		float width;
		float xposition;
		float zposition = 0.f;
		float length;
		float currentObjectGlow = active ? objectGlow : 0.3f;
		int currentObjectGlowState = active ? 2 + objectGlowState : 0;

		if(mobj->button.index < 4) { // Normal button
			width = buttonWidth;
			xposition = buttonTrackWidth * -0.5f + width * mobj->button.index + width*.5f;
			int fxIdx = 0;
			if (mobj->button.index < 2) {
				xposition -= 0.5 * centerSplit * buttonWidth;
			} else {
				xposition += 0.5 * centerSplit * buttonWidth;
				fxIdx = 1;
			}
			if (!isHold && chipFXTimes[fxIdx].count(mobj->time)) {
				xscale = m_btOverFxScale;
				xposition += width * ((1.0 - xscale) / 2.0);
			}
			length = buttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex", isHold ? buttonHoldTexture : buttonTexture);
			mesh = buttonMesh;
		
		} else { // FX Button
			width = fxbuttonWidth;
			xposition = buttonTrackWidth * -0.5f + fxbuttonWidth *(mobj->button.index - 4)+width*.5f;
			if (mobj->button.index < 5) {
				xposition -= 0.5f * centerSplit * buttonWidth;
			} else {
				xposition += 0.5f * centerSplit * buttonWidth;
			}
			length = fxbuttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex", isHold ? fxbuttonHoldTexture : fxbuttonTexture);
			mesh = fxbuttonMesh;
		}

		params.SetParameter("trackPos", position);

		Vector3 buttonPos = Vector3(xposition, trackLength * position, zposition);

		Transform buttonTransform;
		
		Transform mt = EvaluateModTransform(buttonPos,position,mobj->button.index, MA_BUTTON);

		float scale = 1.0f; // Skade-code 1.0f -> 0.4f + position
		float bscale = 1.0f;
		//TODO(skade) binding to modify the 1.5f
		if (mobj->button.index < 4) { // bt button scale
			bscale = 0.2f;
			scale = 1.f+(1.5f) * mt[13]/trackLength/bscale;
		}
		else { //fx button scale
			bscale = 0.35f;
			scale = 1.f+(1.5f) * mt[13]/trackLength/bscale;
		}

		buttonTransform = Transform::Scale({xscale,bscale,1.f})*Transform::Translation({-width*.5f,-length*.5f,0.f});// * buttonTransform;
		buttonTransform = Transform::Scale({ 1.f, scale, 1.0f }) * mt * buttonTransform;
		// Only multiply scaling part.
		buttonTransform[13] /= scale;
		buttonTransform = trackOrigin * buttonTransform;

		params.SetParameter("trackScale", 1.0f / trackLength);

		//TODO(skade) make settable with spline.
		params.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
		params.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
		params.SetParameter("suddenCutoff", suddenCutoff); // Sudden cutoff (% of track)
		params.SetParameter("suddenFadeWindow", suddenFadewindow); // Sudden cutoff (% of track)

		//TODO(skade) mod binding
		bool skipFX = mobj->button.index < 4; //TODO make option

		if (skipFX) {
			
			int32 barTime = playback.GetTimingPointAt(obj->time)->time;
			double beatD = playback.GetTimingPointAt(obj->time)->GetBarDuration();
			double inBeat = std::fmod(obj->time-barTime,beatD);
			double beatP = ((double) inBeat)/beatD;
			double tolerance = 0.01;
			
			int c = 5; // case
			for (uint32_t i = 0; i < 6; ++i) {
				int o = 0; // offset after 12th for pow
				int th; // 4,8,12,16...
				if (i > 2)
					o = 1;
				th = std::pow(2,i+2-o);
				if (i==2)
					th = 12;
				double iv = 1./th;
				double m = std::fmod(beatP,iv);
				if (m <= tolerance || std::abs(m-iv) <= tolerance) {
					c = i;
					break;
				}
			}

			//TODO(skade) mod binding
			switch (c)
			{
			case 0: // 4th
				//TODOs hardly visiable with fx, create a new bt mesh with a smaller colored core and black white border
				//params.SetParameter("uColor",Vector3(238.f,107.f,33.f)/Vector3(255.f,255.f,255.f));
				params.SetParameter("uColor",Vector3(1.f,1.f,1.f));
				break;
			case 1: // 8th
				params.SetParameter("uColor",Vector3(0.f,129.f,255.f)/Vector3(255.f,255.f,255.f));
				break;
			case 2: // 12th
				params.SetParameter("uColor",Vector3(185.f,75.f,232.f)/Vector3(255.f,255.f,255.f));
				break;
			case 3: // 16th
				params.SetParameter("uColor",Vector3(115.f,205.f,52.f)/Vector3(255.f,255.f,255.f));
				break;
			case 4: // 32th
				params.SetParameter("uColor",Vector3(237.f,185.f,3.f)/Vector3(255.f,255.f,255.f));
				break;
			default: // 64th+
				params.SetParameter("uColor",Vector3(26.f,217.f,153.f)/Vector3(255.f,255.f,255.f));
				break;
			}
		} else
			params.SetParameter("uColor",Vector3(1.f,1.f,1.f));
		params.insert(buttonParamsCust.begin(),buttonParamsCust.end());
		rq.Draw(buttonTransform, mesh, mat, params);
	}
	else if (obj->type == ObjectType::Hold) {
		MultiObjectState* mobj = (MultiObjectState*)obj;
		MaterialParameterSet params;
		Material mat = buttonMaterial;
		Mesh mesh;
		float xscale = 1.0f;
		float width;
		float xposition;
		float zposition = 0.f;
		float length;
		float currentObjectGlow = active ? objectGlow : 0.3f;
		int currentObjectGlowState = active ? 2 + objectGlowState : 0;
		
		if(mobj->button.index < 4) { // Normal button
			width = buttonWidth;
			xposition = buttonTrackWidth * -0.5f + width * mobj->button.index + width*.5f;
			int fxIdx = 0;
			if (mobj->button.index < 2) {
				xposition -= 0.5 * centerSplit * buttonWidth;
			} else {
				xposition += 0.5 * centerSplit * buttonWidth;
				fxIdx = 1;
			}
			length = buttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex", buttonHoldTexture);
			//mesh = buttonMesh;
		
		} else { // FX Button
			width = fxbuttonWidth;
			xposition = buttonTrackWidth * -0.5f + fxbuttonWidth *(mobj->button.index - 4) + fxbuttonWidth*.5f;
			if (mobj->button.index < 5) {
				xposition -= 0.5f * centerSplit * buttonWidth;
			} else {
				xposition += 0.5f * centerSplit * buttonWidth;
			}
			length = fxbuttonLength;
			params.SetParameter("hasSample", mobj->button.hasSample);
			params.SetParameter("mainTex",fxbuttonHoldTexture);
			//mesh = fxbuttonMesh;
		}
		float trackScale = 0.0f;
		if(!active && mobj->hold.GetRoot()->time > playback.GetLastTime())
			params.SetParameter("hitState", 1);
		else
			params.SetParameter("hitState", currentObjectGlowState);

		params.SetParameter("objectGlow", currentObjectGlow);
		mat = holdButtonMaterial;
		
		if (dontUseScrollSpeedForPos) {
			if (mobj->time + mobj->hold.duration <= playback.GetLastTime()) {
				trackScale = playback.ToViewDistanceIgnoringScrollSpeed(mobj->time, mobj->hold.duration);
			} else {
				const float remainingDistance = playback.TimeToViewDistance(mobj->time + mobj->hold.duration);
				trackScale = Math::Max(0.0f, remainingDistance) - playback.TimeToViewDistanceIgnoringScrollSpeed(mobj->time);
			}
		} else {
			trackScale = playback.ToViewDistance(mobj->time, mobj->hold.duration);
		}

		trackScale /= viewRange * length;
		float scale = 1.0f; // Skade-code 1.0f -> 0.4f + position
		scale = trackScale * trackLength;

		Transform buttonTransform = trackOrigin;
		Vector3 buttonPos = Vector3(xposition, trackLength * position, zposition);

		HoldObjectState* hold = (HoldObjectState*)obj;
		float buttonLength = length;
		mesh = m_laserTrackBuilder->GenerateHold(playback,hold,buttonPos,position,scale,m_mqHold,buttonLength);

		//buttonTransform *= Transform::Translation(buttonPos);
		//buttonTransform *= Transform::Scale({ xscale, scale, 1.0f });
		params.SetParameter("trackScale", trackScale);
		rq.Draw(buttonTransform, mesh, mat, params);
	}
	else if (obj->type == ObjectType::Laser) // Draw laser
	{
		position = playback.TimeToViewDistance(obj->time);
		float posmult = trackLength / (m_viewRange * laserSpeedOffset);
		float posy = posmult * position / trackLength;
		LaserObjectState* laser = (LaserObjectState*)obj;
		Vector3 laserPos = Vector3(0.0f, posmult * position, 0.0f);

		bool isEntry = !laser->prev;
		bool isExit = !laser->next && (laser->flags & LaserObjectState::flag_Instant) != 0; // Only draw exit on slams

		// Draw segment function
		auto DrawSegment = [&](Mesh mesh, Texture texture, int part)
		{
			MaterialParameterSet laserParams;
			laserParams.SetParameter("trackPos", posmult * position / trackLength);
			laserParams.SetParameter("trackScale", 1.0f / trackLength);
			
			//TODO(skade) make settable with spline.
			laserParams.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
			laserParams.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
			laserParams.SetParameter("suddenCutoff", suddenCutoff); // Hidden cutoff (% of track)
			laserParams.SetParameter("suddenFadeWindow", suddenFadewindow); // Hidden cutoff (% of track)

			// Make not yet hittable lasers slightly glowing
			if (laser->GetRoot()->time > playback.GetLastTime())
			{
				laserParams.SetParameter("objectGlow", 0.6f);
				laserParams.SetParameter("hitState", 2);
			}
			else
			{
				laserParams.SetParameter("objectGlow", active ? objectGlow : 0.4f);
				laserParams.SetParameter("hitState", active ? 2 + objectGlowState : 0);
			}
			laserParams.SetParameter("mainTex", texture);
			laserParams.SetParameter("laserPart", part);

			// Get the length of this laser segment
			Transform laserTransform = trackOrigin;
			//laserTransform *= Transform::Translation(laserPos);
			
			//uint32_t idx = 1-laser->index+6;

			// Set laser color
			laserParams.SetParameter("color", laserColors[laser->index]);

			if(mesh)
				rq.Draw(laserTransform, mesh, laserMaterial, laserParams);
		};

		// Draw entry
		if(isEntry)
		{
			Mesh laserTail = m_laserTrackBuilder->GenerateTrackEntry(playback, laser,laserPos,posy);
			DrawSegment(laserTail, laserTailTextures[laser->index], 1);
		}

		// Body
		Mesh laserMesh = m_laserTrackBuilder->GenerateTrackMesh(playback, laser, laserPos, posy,1.f,m_mqLaser);
		DrawSegment(laserMesh, laserTextures[laser->index], 0);

		// Draw exit
		if(isExit) // Only draw exit on slams
		{
			Mesh laserTail = m_laserTrackBuilder->GenerateTrackExit(playback, laser,laserPos,posy);
			DrawSegment(laserTail, laserTailTextures[2 + laser->index], 2);
		}
	}
}

void Track::DrawOverlays(class RenderQueue& rq)
{
	// Draw button hit effect sprites
	for (auto& hfx : m_hitEffects)
		hfx->Draw(rq);

	if (timedHitEffect->time > 0.0f)
		timedHitEffect->Draw(rq);
}

void Track::DrawHitEffects(RenderQueue& rq)
{
	for (auto& bfx : m_buttonHitEffects)
		bfx.Draw(rq);
}

void Track::DrawTrackOverlay(RenderQueue& rq, Texture texture, float heightOffset /*= 0.05f*/, float widthScale /*= 1.0f*/)
{
	MaterialParameterSet params;
	params.SetParameter("mainTex", texture);
	Transform transform = trackOrigin;
	transform *= Transform::Scale({ widthScale, 1.0f, 1.0f });
	transform *= Transform::Translation({ 0.0f, heightOffset, 0.0f });
	rq.Draw(transform, trackMesh, trackOverlay, params);
}

void Track::DrawSprite(RenderQueue& rq, Vector3 pos, Vector2 size, Texture tex, Color color /*= Color::White*/, float tilt /*= 0.0f*/)
{
	Transform spriteTransform = trackOrigin;
	spriteTransform *= Transform::Translation(pos);
	spriteTransform *= Transform::Scale({ size.x, size.y, 1.0f });
	if(tilt != 0.0f)
		spriteTransform *= Transform::Rotation({ tilt, 0.0f, 0.0f });

	MaterialParameterSet params;
	params.SetParameter("mainTex", tex);
	params.SetParameter("color", color);
	rq.Draw(spriteTransform, centeredTrackMesh, spriteMaterial, params);
}

void Track::DrawSpriteLane(RenderQueue& rq, uint32_t buttonIndex, Vector3 pos, Vector2 size, Texture tex, Color color /*= Color::White*/, float tilt /*= 0.0f*/)
{
	Transform spriteTransform = trackOrigin;

	//TODO(skade) more freedom
	MaterialParameterSet params;
	params.SetParameter("mainTex", tex);
	params.SetParameter("color", color);
	rq.Draw(spriteTransform * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[buttonIndex+1], spriteMaterial, params);
}

void Track::DrawCombo(RenderQueue& rq, uint32 score, Color color, float scale)
{
	if(score == 0)
		return;
	Vector<Mesh> meshes;
	while(score > 0)
	{
		uint32 c = score % 10;
		meshes.Add(comboSpriteMeshes[c]);
		score -= c;
		score /= 10;
	}
	const float charWidth = trackWidth * 0.15f * scale;
	const float seperation = charWidth * 0.7f;
	float size = (float)(meshes.size()-1) * seperation;
	float halfSize = size * 0.5f;

	///TODO: cleanup
	MaterialParameterSet params;
	params.SetParameter("mainTex", 0);
	params.SetParameter("color", color);
	for(uint32 i = 0; i < meshes.size(); i++)
	{
		float xpos = -halfSize + seperation * (meshes.size()-1-i);
		Transform t = trackOrigin;
		t *= Transform::Translation({ xpos, 0.3f, -0.004f});
		t *= Transform::Scale({charWidth, charWidth, 1.0f});
		rq.Draw(t, meshes[i], spriteMaterial, params);
	}
}

//TODO(skade) make combined method to split and control track,trackcover and lanelight (tick mesh)
void Track::DrawTrackCover(RenderQueue& rq)
{
	#ifndef EMBEDDED
	if (trackCoverMaterial && trackCoverTexture)
	{
		Transform t = trackOrigin;
		MaterialParameterSet p;
		p.SetParameter("mainTex", trackCoverTexture);
		p.SetParameter("hiddenCutoff", hiddenCutoff); // Hidden cutoff (% of track)
		p.SetParameter("hiddenFadeWindow", hiddenFadewindow); // Hidden cutoff (% of track)
		p.SetParameter("suddenCutoff", suddenCutoff); // Hidden cutoff (% of track)
		p.SetParameter("suddenFadeWindow", suddenFadewindow); // Hidden cutoff (% of track)

		if (centerSplit != 0.0f)
		{
			//TODO(skade)
			//rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[0], trackCoverMaterial, p);
			//rq.Draw(t * Transform::Translation({ -centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[1], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[0], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[1], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[2], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[3], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[4], trackCoverMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackCoverMesh[5], trackCoverMaterial, p);
		}
		else
		{
			rq.Draw(t, trackCoverMesh, trackCoverMaterial, p);
		}
	}
	#endif
}
void Track::DrawLaneLight(RenderQueue& rq)
{
	if (laneLightMaterial && laneLightTexture)
	{
		Transform t = trackOrigin;
		MaterialParameterSet p;
		p.SetParameter("mainTex", laneLightTexture);
		p.SetParameter("timer", m_laneLightTimer);
		p.SetParameter("speed", m_cModSpeed);
		//TODO improve with beat/m_viewRange
		//p.SetParameter("scroll", m_viewRange);

		if (centerSplit != 0.0f || true)
		{
			//TODO(skade)
			//rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[0], laneLightMaterial, p);
			//rq.Draw(t * Transform::Translation({ -centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f }), splitTrackCoverMesh[1], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[0], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[1], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({-centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[2], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[3], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[4], laneLightMaterial, p);
			rq.Draw(t * Transform::Translation({ centerSplit * 0.5f * buttonWidth, 0.0f, 0.0f}), splitTrackMesh[5], laneLightMaterial, p);
		}
		else
		{
			rq.Draw(t, trackCoverMesh, laneLightMaterial, p);
		}
	}
}

void Track::DrawCalibrationCritLine(RenderQueue& rq)
{
	Transform t = trackOrigin;
	{
		MaterialParameterSet params;
		params.SetParameter("color", Color::Red);
		params.SetParameter("mainTex", whiteTexture);
		rq.Draw(t, calibrationCritMesh, spriteMaterial, params);
	}
	{
		MaterialParameterSet params;
		params.SetParameter("color", Color::Black.WithAlpha(0.6));
		params.SetParameter("mainTex", whiteTexture);
		rq.Draw(t, calibrationDarkMesh, spriteMaterial, params);
	}
}

void Track::DrawLineMesh(RenderQueue& rq)
{
	if (!drawModLines)
		return;

	for (uint32_t i = 0; i < 8; ++i) {
		Vector<MeshGenerators::SimpleVertex> data;
		std::vector<Vector3> offsets;
		MaterialParameterSet params;
		
		for (uint32_t j = 0; j < m_mqLine; ++j) {
			MeshGenerators::SimpleVertex v;
			float xposition = 0.f;
			
			if (i<4)
				xposition = buttonTrackWidth * -0.5f + buttonWidth * i + buttonWidth*.5f;
			else if (i<6)
				xposition = buttonTrackWidth * -0.5f + .5f*fxbuttonWidth *(i+1-4)+(i-4)*fxbuttonWidth*.5f;
			else if (i==6)
				xposition = buttonTrackWidth * -.5f - buttonWidth*.5f;
			else
				xposition = buttonTrackWidth *.5f + buttonWidth*.5f;
			
			v.pos = m_meshOffsets[i][j].GetPosition();

			v.pos.x += xposition;
			v.pos.y += trackLength*((float)j)/m_mqLine;
			data.push_back(v);
		}
		m_lineMesh[i]->SetData(data);
	
		if (i >= 4 && i <= 5)
			params.SetParameter("uColor",Vector3(212.f,53.f,49.f)/Vector3(255.f,255.f,255.f));
		else
			params.SetParameter("uColor",Vector3(1.f));
		
		rq.Draw(trackOrigin, m_lineMesh[i], m_lineMaterial, params);
	}
	//}
}

Vector3 Track::TransformPoint(const Vector3 & p)
{
	return trackOrigin.TransformPoint(p);
}

void Track::AddEffect(TimedEffect* effect)
{
	m_hitEffects.Add(effect);
	effect->track = this;
}

void Track::AddHitEffect(uint32 buttonCode, Color color, bool hold)
{
	m_buttonHitEffects[buttonCode].Reset(buttonCode, color, hold);
}

void Track::ClearEffects()
{
	m_trackHide = 0.0f;
	m_trackHideSpeed = 0.0f;

	for(auto it = m_hitEffects.begin(); it != m_hitEffects.end(); it++)
	{
		delete *it;
	}
	m_hitEffects.clear();
}

void Track::SetViewRange(float newRange)
{
	if(newRange != m_viewRange)
	{
		m_viewRange = newRange;

		// Update view range
		float newLaserLengthScale = trackLength / (m_viewRange * laserSpeedOffset);
		m_laserTrackBuilder->laserLengthScale = newLaserLengthScale;

		// Reset laser tracks cause these won't be correct anymore
		m_laserTrackBuilder->Reset();
	}
}

void Track::SendLaserAlert(uint8 laserIdx)
{
	if (m_alertTimer[laserIdx] > 3.0f)
		m_alertTimer[laserIdx] = 0.0f;
}

void Track::SetLaneHide(bool hide, double duration)
{
	m_trackHideSpeed = hide ? 1.0f / duration : -1.0f / duration;
}

void Track::SetLaneHideSud(bool hide, double duration)
{
	m_trackHideSudSpeed = hide ? 1.0f / duration : -1.0f / duration;
}

void Track::SetLaneHideD(float val) {
	m_trackHide = val;
}

void Track::SetLaneHideSudD(float val) {
	m_trackHideSud = val;
}

float Track::GetViewRange() const
{
	return m_viewRange;
}

float Track::GetButtonPlacement(uint32 buttonIdx)
{
	if (buttonIdx < 4)
	{
		float x = buttonIdx * buttonWidth - (buttonWidth * 1.5f);
		if (buttonIdx < 2)
		{
			x -= 0.5 * centerSplit * buttonWidth;
		}
		else
		{
			x += 0.5 * centerSplit * buttonWidth;
		}
		return x;
	}
	else
	{
		float x = (buttonIdx - 4) * fxbuttonWidth - (fxbuttonWidth * 0.5f);
		if (buttonIdx < 5)
		{
			x -= 0.5 * centerSplit * buttonWidth;
		}
		else
		{
			x += 0.5 * centerSplit * buttonWidth;
		}
		return x;
	}
}

void Track::OnHoldEnter(Input::Button buttonCode)
{
	const auto buttonIndex = (uint32)buttonCode;
	if (buttonIndex >= 6)
		return;
	m_buttonHitEffects[buttonIndex].Reset(buttonIndex, hitColors[(size_t)ScoreHitRating::Perfect], true);
}

void Track::OnButtonReleased(Input::Button buttonCode)
{
	const auto buttonIndex = (uint32)buttonCode;
	if (buttonIndex >= 6)
		return;
	m_buttonHitEffects[buttonIndex].held = false;
}

void Track::OnButtonReleasedDelta(Input::Button buttonCode, int32 delta)
{
	OnButtonReleased(buttonCode);
}

//TODO(skade) move into extra function.

float Track::EvaluateSpline(const std::vector<ModSpline>& spline, float height)
{
	if (spline.size() == 0)
		return 0.f;
	
	uint32_t idx = spline.size(); ///< Represents index of the next spline from height.
	for (uint32_t i = 0; i < spline.size(); ++i) {
		if (height <= spline[i].offset) {
			idx = i;
			break;
		}
	}
	
	float bOff = 0.f;
	float eOff = 1.f;
	float bVal = 0.f;
	float eVal = 0.f;
	
	if (idx == 0) { // interpolate with 0 offset on start
		eVal = spline[idx].value;
		eOff = spline[idx].offset;
	}
	else if (idx == spline.size()) { // interpolate with 0 offset on end
		bVal = spline[idx-1].value;
		bOff = spline[idx-1].offset;
		idx = idx-1; // Take for spline interpolation type the begin spline for now.
	}
	else { // interpolate between 2 spline
		bVal = spline[idx-1].value;
		bOff = spline[idx-1].offset;
		eVal = spline[idx].value;
		eOff = spline[idx].offset;
	}
	
	// get button pos relative to 2 spline
	float length = eOff-bOff;
	float rOff = height-bOff;
	
	//TODOs make more efficient
	// Make sure values are in expected bounds.
	rOff = std::clamp(rOff,FLT_EPSILON,1.f);
	//length = std::max(length,FLT_EPSILON);
	
	length = rOff > length ? rOff : length;
	
	//float ass = rOff/length;
	//assert(ass <= 1.f);
	//assert(ass >= 0.f);

	float s = 0.f;
	switch (spline[idx].type)
	{
	case SIT_CUBIC: //TODO(skade) more freedom needed?
		s = Interpolation::CubicBezier(Interpolation::Predefined::Linear).Sample(rOff/length);
		break;
	case SIT_COSINE:
		s = Interpolation::CosSpline(rOff/length);
		break;
	case SIT_NONE:
		break;
	case SIT_LINEAR:
	default:
		s = rOff/length;
		break;
	}
	
	// interpolate between 2 values
	float val = bVal+s*(eVal-bVal);
	return val;
}

Vector3 Track::EvaluateMods(const std::vector<Mod*>& mods, float yOffset, uint8_t btx, uint8_t af, uint32_t layer, bool isMult)
{
	Vector3 r;
	if (isMult)
		r = Vector3(1.f);

	uint8_t lane = ButtonIndexToAffectedLane(btx);

	// iterate through all active mods and evaluate.
	for (uint32_t i = 0; i < mods.size(); ++i) {
		Mod* m = mods[i];

		if (!(af & m->affection))
			continue;
		if (!(m->affectedLanes & lane) || !m->active)
			continue;

		// Check for the requested layer.
		if (m->layer < layer)
			continue;
		else if (m->layer > layer)
			break;
		
		for (uint32_t j = 0; j < MST_COUNT; ++j) {
			if (m->splines[j].size() > 0) {
				float val = EvaluateSpline(m->splines[j],yOffset);
				if (!isMult)
					r[j] += val;
				else
					r[j] *= val;
			}
		}
	}

	return r;
}

Transform Track::EvaluateModTransform(Vector3 tickPosition,float yOffset, uint8_t btx, uint8_t af)
{
	//TODOs much room for performance improvements here.
	Transform mt;
	for (uint32_t i=0;i<m_maxLayerSize;++i) {
		Vector3 scale, rot, trans; // scale starts in EvaluateMods with 1.
		scale = EvaluateMods(m_modv[MT_SCALE],yOffset,btx,af,i,true);
		rot = EvaluateMods(m_modv[MT_ROT],yOffset,btx,af,i);
		trans = EvaluateMods(m_modv[MT_TRANS],yOffset,btx,af,i);
		if (i==m_tickLayer)
			trans += tickPosition;

		mt = Transform::Translation(trans)*Transform::Rotation(rot)*Transform::Scale(scale) * mt;
	}
	return mt;
}

void Track::SetDepthTest(ModAffection type, bool isDT)
{
	//TODO lane line support, support for lanelight, support for hitFX
	switch (type)
	{
		case MA_BUTTON:
			buttonMaterial->depthTest = isDT;
			break;
		case MA_HOLD:
			holdButtonMaterial->depthTest = isDT;
			break;
		case MA_LASER:
			laserMaterial->depthTest = isDT;
			break;
		case MA_TRACK:
			trackMaterial->depthTest = isDT;
			break;
	default:
		break;
	}
}

//TODO(skade) combine set and reset, with toggle

void Track::SetTrackMaterial(Material mat, ModAffection af) {
	switch (af)
	{
	case MA_TRACK:
		{
			trackMaterial = mat;
		}
		break;
	default:
		break;
	}
}

void Track::SetTrackParameterSet(MaterialParameterSet params, ModAffection af) {
	switch (af)
	{
	case MA_TRACK:
			trackParamsCust = params;
		break;
	case MA_BUTTON:
			buttonParamsCust = params;
		break;
	default:
		break;
	}
}

void Track::SetTrackMesh(Mesh mesh, ModAffection af) {
	switch (af)
	{
	case MA_BUTTON:
		{
			buttonMesh = mesh;
		}
		break;
	default:
		break;
	}
}

void Track::ResetTrackMaterial(ModAffection af) {
	switch (af)
	{
	case MA_TRACK:
		{
			trackMaterial = trackMaterialOG;
		}
		break;
	case MA_BUTTON:
		{
			buttonMaterial = buttonMaterialOG;
		}
		break;
	default:
		break;
	}
}

void Track::ResetTrackParameterSet(ModAffection af) {
	switch (af)
	{
	case MA_TRACK:
		{
			trackParamsCust = MaterialParameterSet();
		}
		break;
	case MA_BUTTON:
		{
			buttonParamsCust = MaterialParameterSet();
		}
		break;
	default:
		break;
	}
}

void Track::ResetTrackMesh(ModAffection af) {
	switch (af)
	{
	case MA_BUTTON:
		{
			buttonMesh = buttonMeshOG;
		}
		break;
	default:
		break;
	}
}

//TODO only lane, put into lane
void Track::UpdateMeshMods() {
	//calculate offsets
	for (uint32_t i = 0; i < 8; ++i) {
		for (uint32_t j = 0; j < m_mqLine; ++j) {
			Transform mt = EvaluateModTransform(Vector3(),((float)j)/m_mqLine,i,MA_LINE);
			m_meshOffsets[i][j] = mt;
		}
	}
}

void Track::SetModLayer(uint32_t layer) {
	if (!m_pEMod)
		return;

	if (layer+1 > m_maxLayerSize)
		m_maxLayerSize = layer+1;
	//TODO(skade) Evaluate if maxLayerSize shrinks in favour of performance.
	
	m_pEMod->layer = layer;

	// Resort in mods list for faster access.
	std::vector<Mod*>* mv = &m_modv[m_pEMod->type];

	// Get distance
	uint32_t idx = 0;
	for (uint32_t i=0;i<mv->size();++i) {
		idx = i;
		if (mv->at(i)->layer > layer) {
			break;
		}
	}

	mv->erase(std::find(mv->begin(),mv->end(),m_pEMod));
	mv->insert(mv->begin()+idx,m_pEMod);

	//TODO(skade) more efficient implementation
	//std::rotate();
}

void Track::SetEditMod(std::string modName) {
	if (modName.size()==0) {
		m_pEMod = nullptr;
		return;
	}
	uint32_t key = std::hash<std::string>{}(modName);
	if (m_mods.find(key)==m_mods.end()) {
		m_pEMod = nullptr;
		//TODOs print message that requested Mod to set is not available.
		return;
	}
	m_pEMod = m_mods[key];
}

void Track::SetEditModSplineType(ModSplineType d) {
	m_cMST = d;
}

void Track::CreateSpline(ModSplineType d, uint32_t amount) {
	if (!m_pEMod)
		return;
	if (d==MST_NONE)
		d = m_cMST;
	std::vector<ModSpline>* spl = &m_pEMod->splines[d];

	while (spl->size() <= amount) {
		spl->push_back(ModSpline());
	}
	if (spl->size() > amount) {
		spl->erase(spl->begin()+amount,spl->end());
	}

	for (uint32_t i=0;i<amount;++i) {
		spl->at(i).offset = (float)i/(amount-1);
	}
}

void Track::SetSplineProperty(ModSplineType d, uint32_t idx, float yOffset, SplineInterpType type) {
	if (!m_pEMod)
		return;
	if (d==MST_NONE)
		d = m_cMST;
	if (idx >= m_pEMod->splines[d].size())
		return;
	m_pEMod->splines[d][idx].offset = yOffset;
	m_pEMod->splines[d][idx].type = type;
}

void Track::SetModSpline(ModSplineType d, uint32_t idx, float val) {
	if (!m_pEMod)
		return;
	if (d==MST_NONE)
		d = m_cMST;
	if (m_pEMod->splines[d].size() <= idx)
		return;
	m_pEMod->splines[d][idx].value = val;
}

float Track::GetModSplineValue(ModSplineType d, uint32_t idx) {
	if (!m_pEMod)
		return 0.f;
	if (d==MST_NONE)
		d = m_cMST;
	if (m_pEMod->splines[d].size() <= idx)
		return 0.f;
	return m_pEMod->splines[d][idx].value;
}

float Track::GetModSplineOffset(ModSplineType d, uint32_t idx) {
	if (!m_pEMod)
		return 0.f;
	if (d==MST_NONE)
		d = m_cMST;
	if (m_pEMod->splines[d].size() <= idx)
		return 0.f;
	return m_pEMod->splines[d][idx].offset;
}

void Track::SetModEnable(bool enable) {
	if (!m_pEMod)
		return;
	m_pEMod->active = enable;
}

void Track::SetModProperties(uint8_t affectedLanes, uint8_t affection) {
	if (!m_pEMod)
		return;
	m_pEMod->affectedLanes = affectedLanes;
	m_pEMod->affection = affection;
}

void Track::AddMod(std::string modName, ModType type) {
	
	uint32_t key = std::hash<std::string>{}(modName);
	if (m_mods.find(key)!=m_mods.end())
		return;
	
	Mod* nm = new Mod;
	nm->id = key;
	nm->type = type;

	std::vector<Mod*>* cModVec = &m_modv[type];
	cModVec->push_back(nm);

	// Set hash value to find mod easier.
	m_mods[nm->id] = nm;
}

void Track::RemoveMod(std::string modName) {
	uint32_t key = std::hash<std::string>{}(modName);
	if (m_mods.find(key)==m_mods.end())
		return;

	Mod* m = m_mods[key];
	
	// Delete reference from Access Vector.
	std::vector<Mod*>* cModVec = &m_modv[m->type];
	if (cModVec)
		cModVec->erase(std::find(cModVec->begin(),cModVec->end(),m));
	m_mods.erase(key);

	if (m==m_pEMod)
		m_pEMod = nullptr;

	delete m;
}

void Track::RemoveAllMods() {
	for (uint32_t i=0;i<MT_COUNT;++i) {
		for (uint32_t j=0;j<m_modv[i].size();++j)
			delete m_modv[i][j];
		m_modv[i].clear();
	}

	m_mods.clear();
	m_pEMod = nullptr;

	// Remove all Track Pipes
	//TODO(skade) improve
	this->buttonMaterial = this->buttonMaterialOG;
	this->buttonMesh = this->buttonMeshOG;
	this->trackMaterial = this->trackMaterialOG;
	this->buttonParamsCust = MaterialParameterSet();
	this->trackParamsCust = MaterialParameterSet();
}

// Meshing qualities

void Track::SetMQLine(uint32_t q) {
	m_mqLine = q;
	for (uint32_t i=0;i<8;++i)
		m_meshOffsets[i].resize(q);
}

void Track::SetMQTrack(uint32_t q) {
	m_mqTrack = q;
	// Update Track Mesh so m_splitMeshData gets resized.
	UpdateTrackMeshData();
}
void Track::SetMQTrackNeg(uint32_t q) {
	m_mqTrackNeg = q;
	// Update Track Mesh so m_splitMeshData gets resized.
	UpdateTrackMeshData();
}
void Track::SetMQLaser(uint32_t q) {
	m_mqLaser = q;
}
void Track::SetMQHold(uint32_t q) {
	m_mqHold = q;
}

void Track::UpdateTrackMeshData() {
	for (size_t i = 0; i < 6; i++) {
		//track base
		Vector2 pos = Vector2(-trackWidth*0.5f + (1.0f/6.0f)*i*trackWidth, -1);
		Vector2 size = Vector2(trackWidth / 6.0f, trackLength + 1);
		Rect rect = Rect(pos, size);
		Rect uv = Rect(float(i)/6.0f, 0.0f, float(i+1)/6.0f, 1.0f);
		splitTrackMesh[i] = MeshRes::Create(g_gl);
		splitTrackMesh[i]->SetPrimitiveType(PrimitiveType::TriangleList);
		Vector<MeshGenerators::SimpleVertex> splitMeshData;
		m_splitMeshData[i].clear();
		MeshGenerators::GenerateSubdividedTrack(rect, uv, m_mqTrack-1, m_mqTrackNeg-1, m_splitMeshData[i]);
		splitMeshData = MeshGenerators::Triangulate(m_splitMeshData[i]);
		splitTrackMesh[i]->SetData(m_splitMeshData[i]);
	}
}
