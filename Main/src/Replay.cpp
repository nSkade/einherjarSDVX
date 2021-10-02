#include "stdafx.h"
#include "Replay.hpp"
#include "Shared/FileStream.hpp"
#include "Beatmap/MapDatabase.hpp"

bool Replay::Save(String path)
{
	File replayFile;
	if (!replayFile.OpenWrite(path))
		return false;
	FileWriter fw(replayFile);
	bool res = fw.SerializeObject(*this);
	replayFile.Close();
	return res;
}

Replay* Replay::Load(String path, ReplayType type)
{
	Replay* replay = new Replay(type);
	File replayFile;
	if (!replayFile.OpenRead(path))
		return nullptr;

	FileReader fr(replayFile);
	if (!fr.SerializeObject(replay))
	{
		delete replay;
		replayFile.Close();
		return nullptr;
	}
	replayFile.Close();
	return replay;
}

bool Replay::SerializeLegacy(BinaryStream& stream, Replay*& obj)
{
	if (obj == nullptr)
		return false;

	// We don't support writing legacy replays
	if (!stream.IsReading() || obj->m_initialized)
		return false;

	Vector<SimpleHitStat> legacy;
	if (!stream.SerializeObject(legacy))
		return false;
	for (auto& l : legacy) {
		obj->m_judgementEvents.push_back(l);
	}

	// Its ok if this fails, the default is normal
	stream << obj->m_hitWindow;

	// Maybe use current offset instead?
	obj->m_offsets = { 0 };
	// TODO set player and other info?

	obj->m_initialized = true;
	obj->m_type = Replay::ReplayType::Legacy;
	return true;
}

bool Replay::StaticSerialize(BinaryStream& stream, Replay*& obj)
{
	bool isRead = stream.IsReading();

	if (obj == nullptr)
		return false;

	uint16 version = REPLAY_VERSION;
	if (isRead)
	{
		if (obj->m_initialized)
			return false;

		uint32 magic = 0;
		if (!stream.SerializeObject(magic))
			return false;

		if (magic != REPLAY_MAGIC)
		{
			stream.Seek(0);
			return Replay::SerializeLegacy(stream, obj);
		}

		// File is not legacy
		if (obj->m_type == Replay::ReplayType::Legacy)
			return false;

		if (!stream.SerializeObject(version) || version > REPLAY_VERSION)
		{
			// May not be backwards compatable so ignore it
			return false;
		}
	}
	else if (obj->m_initialized)
	{
		uint32 magic = REPLAY_MAGIC;
		stream << magic << version;
		if (!stream.IsOk())
			return false;
	}
	else
	{
		return false;
	}

	// c++ is mean :(

	auto* ci = &obj->m_chartInfo;
	if (!ReplayChartInfo::StaticSerialize(stream, ci))
		return false;
	auto* si = &obj->m_scoreInfo;
	if (!ReplayScoreInfo::StaticSerialize(stream, si))
		return false;

	stream << obj->m_hitWindow;

	auto* oi = &obj->m_offsets;
	if (!ReplayOffsets::StaticSerialize(stream, oi))
		return false;

	stream << obj->m_judgementEvents;

	// Must be last thing since its optional to read
	if (!isRead || obj->m_type != Replay::ReplayType::NoInput)
		stream << obj->m_inputEvents;

	if (isRead && stream.IsOk())
		obj->m_initialized = true;

	return stream.IsOk();
}
