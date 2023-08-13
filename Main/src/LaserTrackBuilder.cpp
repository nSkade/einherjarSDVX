#include "stdafx.h"
#include "LaserTrackBuilder.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include "Track.hpp"
#include "GameConfig.hpp"
#include <algorithm>

using Shared::Rect;
using Shared::Rect3D;

LaserTrackBuilder::LaserTrackBuilder(class OpenGL* gl, class Track* track)
{
	m_gl = gl;
	m_track = track;
	m_trackWidth = track->trackWidth;
	m_laserWidth = track->laserWidth;
	laserTextureSize = track->laserTextures[0]->GetSize(); // NOTE: expects left/right textures to be the same size!
	laserEntryTextureSize = track->laserTailTextures[0]->GetSize();
	laserExitTextureSize = track->laserTailTextures[2]->GetSize();
}

Mesh LaserTrackBuilder::GenerateHold(class BeatmapPlayback& playback, HoldObjectState* hold, Vector3 t, float yPos, float scale, uint32_t quality, float buttonLength) {
	Mesh mesh;
	if(m_objectCacheHold.Contains(hold))
		mesh = m_objectCacheHold[hold];
	else {
		mesh = MeshRes::Create(m_gl);
		m_objectCacheHold.Add(hold, mesh);
	}

	float length = scale*buttonLength;

	Vector<MeshGenerators::SimpleVertex> verts;
	uint32_t rows = float(length/(m_track->trackLength)*float(quality))+1;

	//TODO(skade) clamp to maximum trackLength
	for (uint32_t i = 0; i <= rows; ++i) {
		MeshGenerators::SimpleVertex left, right;
		float rf = (float)i/rows;
		float rfs = rf*length;
		float emo =yPos+rfs/(m_track->trackLength); // effective mod offset
		
		//TODO(skade) is bad by resizing GPU buffer.
		if (emo > 1.f) {
			if (verts.size() > 0) {
				verts.push_back(verts.back());
				verts.push_back(verts.back());
			}
			else {
				MeshGenerators::SimpleVertex v;
				v.pos = Vector3(FLT_MAX,FLT_MAX,FLT_MAX);
				verts.push_back(v);
				verts.push_back(v);
			}
			continue;
		}

		Vector3 pos = t+Vector3(0.f,rfs,0.f);
		
		Transform m = m_track->EvaluateModTransform(pos,emo,hold->index,Track::MA_HOLD);

		float w = hold->index < 4 ? m_track->buttonWidth : m_track->fxbuttonWidth;

		left.pos = Vector3(-.5f,0.f,0.f)*w;
		right.pos = Vector3(.5f,0.f,0.f)*w;
		
		left.pos = m*left.pos;
		right.pos = m*right.pos;
		
		left.tex = Vector2(0.f,rf);
		right.tex = Vector2(1.f,rf);
		verts.push_back(left);
		verts.push_back(right);
	}
	
	Vector<MeshGenerators::SimpleVertex> vt = MeshGenerators::Triangulate(verts);
	mesh->SetData(vt);
	mesh->SetPrimitiveType(PrimitiveType::TriangleList);
	return mesh;
}

Mesh LaserTrackBuilder::GenerateTrackMesh(class BeatmapPlayback& playback, LaserObjectState* laser, Vector3 t, float yPos, float scale, uint32_t quality)
{
	Mesh newMesh;
	if (m_objectCache.Contains(laser)) {
		newMesh = m_objectCache[laser];
		// Dont recompute slams.
		//if ((laser->flags & LaserObjectState::flag_Instant) != 0)
		//	return newMesh;
	}
	else {
		newMesh = MeshRes::Create(m_gl);
		// Cache this mesh
		m_objectCache.Add(laser, newMesh);
	}

	const float length = playback.ToViewDistance(laser->time, laser->duration);

	//TODO(skade)f still kinda buggy with rotations.
	if((laser->flags & LaserObjectState::flag_Instant) != 0) // Slam segment
	{
		float left, right, offsetT, offsetB, uvT, uvB;
		left = laser->points[0] * effectiveWidth - effectiveWidth * 0.5f;
		right = laser->points[1] * effectiveWidth - effectiveWidth * 0.5f;
		float halfWidth = actualLaserWidth * 0.5f;
		offsetT = -actualLaserWidth; // Offset Top
		offsetB = -offsetT;          // Offset Bottom
		uvB = 1.5f;
		uvT = -0.5f;

		if ((laser->flags & LaserObjectState::flag_Extended) != 0) {
			left = (laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;
			right = (laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;
		}

		uint32_t idx = 1-laser->index+6;

		// If corners should be placed, connecting the texture to the previous laser
		bool swapped = false;
		if(laser->points[0] > laser->points[1]) {
			// <------
			std::swap(left, right);
			std::swap(offsetT, offsetB);
			std::swap(uvB, uvT);
			swapped = true;
		}// else ------>
		
		//TODO(skade)
		yPos = yPos+offsetB > 1.f ? 1.f-offsetB : yPos;
		yPos = yPos+offsetT > 1.f ? 1.f-offsetT : yPos;
		
		//Vector3 bpos = Vector3(left,offsetB+yPos,0);
		//Vector3 tpos = Vector3(right,offsetT+yPos,0);
		Vector3 lpos = t+Vector3(left,0,0);
		Vector3 rpos = t+Vector3(right,0,0);
		Transform ml = m_track->EvaluateModTransform(lpos,yPos+offsetB,idx,Track::MA_LASER); // Bottom left
		Transform mr = m_track->EvaluateModTransform(rpos,yPos+offsetT,idx,Track::MA_LASER); // Top right

		// Generate positions for middle top and bottom
		float slamLength = playback.ToViewDistance(laser->time, slamDuration) * laserLengthScale;
		float halfLength = slamLength * 0.5;
		//Rect3D centerMiddle = Rect3D(left, slamLength + halfLength, right, -halfLength);
		Rect3D centerMiddle = Rect3D(0, slamLength + halfLength, 0, -halfLength);
		
		// Center Piece Connecting left and right laser. aka Horizontal Part of slam segment.
		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
			{ { centerMiddle.Right() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 1.0f } }, // BR
			{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR

			{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
			{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR
			{ { centerMiddle.Left() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 0.0f } }, // TL
		};

		// Generate left corner
		{
			Rect3D leftCenter = Rect3D(- actualLaserWidth, centerMiddle.Top() - halfLength,halfWidth, centerMiddle.Bottom() + halfLength);

			Vector<MeshGenerators::SimpleVertex> leftVerts;
			if(swapped) {
				leftVerts =
				{
					{ { leftCenter.Right(), leftCenter.Top(),  0.0f },{ 1.0f, 0.0f } }, // BR
					{ { leftCenter.Left(), leftCenter.Top(),  0.0f },{ -0.5f, 0.0f } }, // BL
					{ { leftCenter.Left(), leftCenter.Bottom() - halfLength,  0.0f },{ -0.5f, 1.0f } }, // TL
				};

			} else {
				leftVerts =
				{
					{ { leftCenter.Left(), leftCenter.Bottom(),  0.0f },{ -0.5f, 0.0f } }, // BL
					{ { leftCenter.Right(), leftCenter.Bottom(),  0.0f },{ 1.0f, 0.0f } }, // BR
					{ { leftCenter.Left(), leftCenter.Top() + halfLength,  0.0f },{ -0.5f, 1.0f } }, // TL
				};
			}
			for (auto& v : leftVerts) {
				v.pos = ml * v.pos;
				verts.Add(v);
			}
		}

		// Generate right corner
		{
			Rect3D rightCenter = Rect3D(- halfWidth, centerMiddle.Top() - halfLength, actualLaserWidth, centerMiddle.Bottom() + halfLength);
			Vector<MeshGenerators::SimpleVertex> rightVerts;
			if(swapped)
			{
				rightVerts =
				{
					{ { rightCenter.Left(), rightCenter.Bottom(),  0.0f },{ 0.0f, 0.0f } }, // BL
					{ { rightCenter.Right(), rightCenter.Bottom(),  0.0f },{ 1.5f, 1.0f } }, // BR
					{ { rightCenter.Right(), rightCenter.Top() + halfLength,  0.0f },{ 1.5f, 1.0f } }, // TR
				};
			}
			else
			{
				rightVerts =
				{
					{ { rightCenter.Right(), rightCenter.Bottom() - halfLength,  0.0f },{ 1.5f, 1.0f } }, // TR
					{ { rightCenter.Right(), rightCenter.Top(),  0.0f },{ 1.5f, 1.0f } }, // BR
					{ { rightCenter.Left(), rightCenter.Top(),  0.0f },{ 0.0f, 0.0f } }, // BL
				};
			}
			for (auto& v : rightVerts) {
				v.pos = mr * v.pos;
				verts.Add(v);
			}
		}

		verts[0].pos = ml * verts[0].pos;
		verts[1].pos = mr * verts[1].pos;
		verts[2].pos = mr * verts[2].pos;
		
		verts[3].pos = ml * verts[3].pos;
		verts[4].pos = mr * verts[4].pos;
		verts[5].pos = ml * verts[5].pos;

	//		{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
	//		{ { centerMiddle.Right() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 1.0f } }, // BR
	//		{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR

	//		{ { centerMiddle.Left() + offsetB, centerMiddle.Bottom(),  0.0f },{ uvB, 0.0f } }, // BL
	//		{ { centerMiddle.Right() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 1.0f } }, // TR
	//		{ { centerMiddle.Left() + offsetT, centerMiddle.Top(),  0.0f },{ uvT, 0.0f } }, // TL

		newMesh->SetData(verts);
		newMesh->SetPrimitiveType(PrimitiveType::TriangleList);
	}
	else // Normal Segment.
	{
		float prevLength = 0.0f;
		if(laser->prev && (laser->prev->flags & LaserObjectState::flag_Instant) != 0)
		{
			// Previous slam length
			prevLength = playback.ToViewDistance(laser->prev->time, slamDuration) * laserLengthScale;
		}

		//Vector2 points[2];

		//// Connecting center points
		//points[0] = Vector2(laser->points[0] * effectiveWidth - effectiveWidth * 0.5f, prevLength); // Bottom
		//points[1] = Vector2(laser->points[1] * effectiveWidth - effectiveWidth * 0.5f, length * laserLengthScale); // Top
		//if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		//{
		//	points[0] = Vector2((laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f, prevLength); // Bottom
		//	points[1] = Vector2((laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f, length * laserLengthScale); // Top
		//}

		float uMin = -0.5f;
		float uMax = 1.5f;

		float vMin = 0.0f;
		float vMax = (int)((length * laserLengthScale) / actualLaserHeight);

		//Vector<MeshGenerators::SimpleVertex> verts =
		//{
		//	{ { points[0].x - actualLaserWidth, points[0].y,  0.0f },{ uMin, vMax } }, // BL
		//	{ { points[0].x + actualLaserWidth, points[0].y,  0.0f },{ uMax, vMax } }, // BR
		//	{ { points[1].x + actualLaserWidth, points[1].y,  0.0f },{ uMax, vMin } }, // TR

		//	{ { points[0].x - actualLaserWidth, points[0].y,  0.0f },{ uMin, vMax } }, // BL
		//	{ { points[1].x + actualLaserWidth, points[1].y,  0.0f },{ uMax, vMin } }, // TR
		//	{ { points[1].x - actualLaserWidth, points[1].y,  0.0f },{ uMin, vMin } }, // TL
		//};

		scale = length;
		Vector<MeshGenerators::SimpleVertex> verts;
		uint32_t rows = (scale*laserLengthScale-prevLength)/(m_track->trackLength)*quality+1; //TODO(skade)
			
		float lp0 = laser->points[0];
		float lp1 = laser->points[1];

		if ((laser->flags & LaserObjectState::flag_Extended) != 0) {
			lp0 = lp0*2.f-.5f;
			lp1 = lp1*2.f-.5f;
		}

		for (uint32_t i = 0; i <= rows; ++i) {
			MeshGenerators::SimpleVertex left, right;
			float rf = (float)i/rows;
			float rfs = prevLength+rf*(scale*laserLengthScale-prevLength);
			float tl = m_track->trackLength;
			float emo = yPos+rfs/tl; // effective mod offset
			
			//TODO(skade) is bad by resizing GPU buffer.
			if (emo > 1.f) {
				if (verts.size() > 0) {
					verts.push_back(verts.back());
					verts.push_back(verts.back());
				}
				else {
					MeshGenerators::SimpleVertex v;
					v.pos = Vector3(FLT_MAX,FLT_MAX,FLT_MAX);
					verts.push_back(v);
					verts.push_back(v);
				}
				continue;
			}

			float xposition = (lp0*(1.f-rf)+rf*lp1) * effectiveWidth - effectiveWidth * 0.5f;

			Vector3 pos = t+Vector3(xposition,rfs,0.f);
			
			uint32_t idx = 1-laser->index+6;
			Transform m = m_track->EvaluateModTransform(pos,emo,idx,Track::MA_LASER);

			left.pos = Vector3(-actualLaserWidth,0.f,0.f);
			right.pos = Vector3(actualLaserWidth,0.f,0.f);
			
			left.pos = m*left.pos;
			right.pos = m*right.pos;
			
			left.tex = Vector2(uMin,rf);
			right.tex = Vector2(uMax,rf);
			verts.push_back(left);
			verts.push_back(right);
		}
		
		Vector<MeshGenerators::SimpleVertex> vt = MeshGenerators::Triangulate(verts);

		newMesh->SetData(vt);
		newMesh->SetPrimitiveType(PrimitiveType::TriangleList);
	}
	return newMesh;
}

Mesh LaserTrackBuilder::GenerateTrackEntry(class BeatmapPlayback& playback, LaserObjectState* laser, Vector3 t, float yPos)
{
	//TODO(skade) evaluate Mods
	assert(laser->prev == nullptr);
	Mesh newMesh;

	if (m_cachedEntries.Contains(laser))
		newMesh = m_cachedEntries[laser];
	else
		newMesh = MeshRes::Create(m_gl);

	// Starting point of laser
	float startingX = laser->points[0] * effectiveWidth - effectiveWidth * 0.5f;
	if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		startingX = (laser->points[0] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;

	// Length of the tail
	float length = (float)laserEntryTextureSize.y / (float)laserEntryTextureSize.x * actualLaserWidth;

	Vector<MeshGenerators::SimpleVertex> verts;

	uint32_t idx = 1-laser->index+6;
	Vector3 p = t + Vector3(startingX,0,0);
	yPos = std::min(1.f,yPos);
	Transform m = m_track->EvaluateModTransform(p,yPos,idx,Track::MA_LASER);
	
	Rect3D pos = Rect3D(Vector2(- actualLaserWidth, -length), Vector2(actualLaserWidth * 2, length));
	Rect uv = Rect(-0.5f, 0.0f, 1.5f, 1.0f);
	MeshGenerators::GenerateSimpleXYQuad(pos, uv, verts);

	for (auto& v : verts) {
		v.pos = m * v.pos;
	}

	newMesh->SetData(verts);
	newMesh->SetPrimitiveType(PrimitiveType::TriangleList);

	// Cache this mesh
	m_cachedEntries.Add(laser, newMesh);
	return newMesh;
}
Mesh LaserTrackBuilder::GenerateTrackExit(class BeatmapPlayback& playback, LaserObjectState* laser, Vector3 t, float yPos)
{
	assert(laser->next == nullptr);
	
	Mesh newMesh;
	
	if (m_cachedExits.Contains(laser))
		newMesh = m_cachedExits[laser];
	else
		newMesh = MeshRes::Create(m_gl);

	// Ending point of laser 
	float startingX = laser->points[1] * effectiveWidth - effectiveWidth * 0.5f;
	if ((laser->flags & LaserObjectState::flag_Extended) != 0)
		startingX = (laser->points[1] * 2.0f - 0.5f) * effectiveWidth - effectiveWidth * 0.5f;

	// Length of the tail
	float length = (float)laserExitTextureSize.y / (float)laserExitTextureSize.x * actualLaserWidth;

	// Length of this segment
	float prevLength = 0.0f;
	if((laser->flags & LaserObjectState::flag_Instant) != 0)
	{
		prevLength = playback.ToViewDistance(laser->time, slamDuration) * laserLengthScale;
	}
	else
	{
		prevLength = playback.ToViewDistance(laser->time, laser->duration) * laserLengthScale;
	}
	
	uint32_t idx = 1-laser->index+6;
	Vector3 p = t + Vector3(startingX,0,0);
	yPos = std::min(1.f,yPos);
	Transform m = m_track->EvaluateModTransform(p,yPos,idx,Track::MA_LASER);

	Vector<MeshGenerators::SimpleVertex> verts;
	Rect3D pos = Rect3D(Vector2(- actualLaserWidth, prevLength), Vector2(actualLaserWidth * 2, length));
	Rect uv = Rect(-0.5f, 0.0f, 1.5f, 1.0f);
	MeshGenerators::GenerateSimpleXYQuad(pos, uv, verts);

	for (auto& v : verts) {
		v.pos = m * v.pos;
	}

	newMesh->SetData(verts);
	newMesh->SetPrimitiveType(PrimitiveType::TriangleList);

	// Cache this mesh
	m_cachedExits.Add(laser, newMesh);
	return newMesh;
}

void LaserTrackBuilder::m_RecalculateConstants()
{
	// Calculate amount to scale laser size to fit the texture border in
	//assert(laserTextureSize.x == laserTextureSize.y); // Use Square texture
	const float laserCenterAmount = ((float)laserTextureSize.x - ((float)laserBorderPixels * 2)) / (float)(laserTextureSize.x);
	const float laserBorderAmount = (1.0f - laserCenterAmount);

	// The uv coordinates to sample the laser without the border, or only the border
	textureBorder = (float)laserBorderPixels / laserTextureSize.x;
	invTextureBorder = 1.0f - textureBorder;

	// The the size of the laser with compensation added for the border
	actualLaserWidth = m_laserWidth;
	actualLaserHeight = actualLaserWidth * laserTextureSize.y / laserTextureSize.x;

	// The width of the laser without the border
	laserWidthNoBorder = actualLaserWidth * laserCenterAmount;
	// World size of a single border around a laser
	realBorderSize = (actualLaserWidth - laserWidthNoBorder) * 0.5f;

	// The length of the horizontal slam segments
	slamDuration = 50 * g_gameConfig.GetFloat(GameConfigKeys::SlamThicknessMultiplier);

	// The effective area in which the center point of the laser can move
	effectiveWidth = m_trackWidth - m_laserWidth;
}
//TODO(skade) combine both.
void LaserTrackBuilder::m_Cleanup(MapTime newTime, Map<LaserObjectState*, Mesh>& arr)
{
	// Cleanup unused meshes
	for(auto it = arr.begin(); it != arr.end();)
	{
		LaserObjectState* obj = it->first;
		MapTime endTime = obj->time + obj->duration + 1000; //TODO(skade) make offset (1k) dependent on lanespeed
		if(newTime > endTime)
		{
			it = arr.erase(it);
			continue;
		}
		it++;
	}
}
void LaserTrackBuilder::m_Cleanup(MapTime newTime, Map<HoldObjectState*, Mesh>& arr)
{
	// Cleanup unused meshes
	for(auto it = arr.begin(); it != arr.end();)
	{
		HoldObjectState* obj = it->first;
		MapTime endTime = obj->time + obj->duration + 1000; //TODO(skade) make offset (1k) dependent on lanespeed
		if(newTime > endTime)
		{
			it = arr.erase(it);
			continue;
		}
		it++;
	}
}
void LaserTrackBuilder::Reset()
{
	m_objectCache.clear();
	m_cachedEntries.clear();
	m_cachedExits.clear();
	m_objectCacheHold.clear();
	m_RecalculateConstants();
}
void LaserTrackBuilder::Update(MapTime newTime)
{
	m_Cleanup(newTime, m_objectCache);
	m_Cleanup(newTime, m_cachedEntries);
	m_Cleanup(newTime, m_cachedExits);
	m_Cleanup(newTime, m_objectCacheHold); //TODO(skade) rework?
}