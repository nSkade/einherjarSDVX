#include "stdafx.h"
#include "KShootMap.hpp"

#include "Shared/Profiling.hpp"
#include "Shared/StringEncodingDetector.hpp"
#include "Shared/StringEncodingConverter.hpp"
#include "kson/kson.hpp"

String KShootTick::ToString() const
{
	return Sprintf("%s|%s|%s", *buttons, *fx, *laser);
}
void KShootTick::Clear()
{
	buttons = "0000";
	fx = "00";
	laser = "--";
}

KShootTime::KShootTime() : block(-1), tick(-1)
{
}
KShootTime::KShootTime(uint32_t block, uint32_t tick) : block(block), tick(tick)
{
}
KShootTime::operator bool() const
{ 
	return block != -1; 
}

KShootMap::TickIterator::TickIterator(KShootMap& map, KShootTime start /*= KShootTime(0, 0)*/) : m_time(start), m_map(map)
{
	if(!m_map.GetBlock(m_time, m_currentBlock))
		m_currentBlock = nullptr;
}
KShootMap::TickIterator& KShootMap::TickIterator::operator++()
{
	m_time.tick++;
	if(m_time.tick >= m_currentBlock->ticks.size())
	{
		m_time.tick = 0;
		m_time.block++;
		if(!m_map.GetBlock(m_time, m_currentBlock))
			m_currentBlock = nullptr;
	}
	return *this;
}
KShootMap::TickIterator::operator bool() const
{
	return m_currentBlock != nullptr;
}
KShootTick& KShootMap::TickIterator::operator*()
{
	return m_currentBlock->ticks[m_time.tick];
}
KShootTick* KShootMap::TickIterator::operator->()
{
	return &m_currentBlock->ticks[m_time.tick];
}
const KShootTime& KShootMap::TickIterator::GetTime() const
{
	return m_time;
}
const KShootBlock& KShootMap::TickIterator::GetCurrentBlock() const
{
	return *m_currentBlock;
}

bool ParseKShootCourse(BinaryStream& input, Map<String, String>& settings, Vector<String>& charts)
{
	StringEncoding chartEncoding = StringEncoding::Unknown;

	// Read Byte Order Mark
	uint32_t bom = 0;
	input.Serialize(&bom, 3);

	// If the BOM is not present, the chart might not be UTF-8.
	// This is forbidden by the spec, but there are old charts which did not use UTF-8. (#314)
	if (bom == 0x00bfbbef)
	{
		chartEncoding = StringEncoding::UTF8;
	}
	else
	{
		input.Seek(0);
	}

	uint32_t lineNumber = 0;
	String line;
	static const String lineEnding = "\r\n";

	// Parse header (encoding-agnostic)
	while(TextStream::ReadLine(input, line, lineEnding))
	{
		line.Trim();
		lineNumber++;
		if(line == "--")
		{
			break;
		}
		
		String k, v;
		if (line.empty())
			continue;
		if (line.substr(0, 2).compare("//") == 0)
			continue;
		if(!line.Split("=", &k, &v))
			return false;

		settings.FindOrAdd(k) = v;
	}

	if (chartEncoding == StringEncoding::Unknown)
	{
		chartEncoding = StringEncodingDetector::Detect(input, 0, input.Tell());

		if (chartEncoding != StringEncoding::Unknown)
			Logf("Course encoding is assumed to be %s", Logger::Severity::Info, GetDisplayString(chartEncoding));
		else
			Log("Course encoding couldn't be assumed. (Assuming UTF-8)", Logger::Severity::Warning);

	}
	if (chartEncoding != StringEncoding::Unknown)
	{
		for (auto& it : settings)
		{
			const String& value = it.second;
			if (value.empty()) continue;

			it.second = StringEncodingConverter::ToUTF8(chartEncoding, value);
		}
	}

	while (TextStream::ReadLine(input, line, lineEnding))
	{
		line.Trim();
		lineNumber++;
		if (line.empty() || line[0] != '[')
			continue;

		line.TrimFront('[');
		line.TrimBack(']');
		if (line.empty())
		{
			Logf("Empty course chart found on line %u", Logger::Severity::Warning, lineNumber);
			return false;
		}

		line = Path::Normalize(line);
		charts.push_back(line);
	}
	return true;
}

KShootMap::KShootMap()
{

}
KShootMap::~KShootMap()
{

}
bool KShootMap::Init(std::istream& input, bool metadataOnly)
{
	ProfilerScope $("Load KShootMap");
	
	return true;
}
bool KShootMap::GetBlock(const KShootTime& time, KShootBlock*& tickOut)
{
	if(!time)
		return false;
	if(time.block >= blocks.size())
		return false;
	tickOut = &blocks[time.block];
	return tickOut->ticks.size() > 0;
}
bool KShootMap::GetTick(const KShootTime& time, KShootTick*& tickOut)
{
	if(!time)
		return false;
	if(time.block >= blocks.size())
		return false;
	KShootBlock& b = blocks[time.block];
	if(time.tick >= b.ticks.size() || time.tick < 0)
		return false;
	tickOut = &b.ticks[time.tick];
	return true;
}
float KShootMap::TimeToFloat(const KShootTime& time) const
{
	KShootBlock* block;
	if(!const_cast<KShootMap*>(this)->GetBlock(time, block))
		return -1.0f;
	float seg = (float)time.tick / (float)block->ticks.size();
	return (float)time.block + seg;
}
float KShootMap::TranslateLaserChar(char c) const
{
	class LaserCharacters : public Map<char, uint32>
	{
	public:
		LaserCharacters()
		{
			uint32 numChars = 0;
			auto AddRange = [&](char start, char end)
			{
				for(char c = start; c <= end; c++)
				{
					Add(c, numChars++);
				}
			};
			AddRange('0', '9');
			AddRange('A', 'Z');
			AddRange('a', 'o');
		}
	};
	static LaserCharacters laserCharacters;

	uint32* index = laserCharacters.Find(c);
	if(!index)
	{
		Logf("Invalid laser control point '%c'", Logger::Severity::Warning, c);
		return 0.0f;
	}
	return (float)index[0] / (float)(laserCharacters.size()-1);
}
const char* KShootMap::c_sep = "--";