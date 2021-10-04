#pragma once

#include "HitStat.hpp"
#include "Beatmap/MapDatabase.hpp"

struct ReplayJudgement
{
	int8 rating = 0;
	int8 lane = 0;
	int16 delta = 0;
	MapTime time = 0;
	ReplayJudgement() = default;
	ReplayJudgement(const SimpleHitStat& s) :
		ReplayJudgement(s.rating, s.lane, s.delta, s.time) {};
	ReplayJudgement(int8 r, int8 l, int16 d, MapTime t) :
		rating(r), lane(l), delta(d), time(t) {};
	inline MapTime GetHitTime() const {
		return time + delta;
	}
};


template<int V>
struct ForwardCompatStruct
{
	static_assert(V > 0);
	uint8 version = 0;
	bool Version(BinaryStream& stream)
	{
		if (stream.IsWriting() && !IsInitialized())
			return false;

		version = V;
		stream << this->version;
		if (!stream.IsOk() || (stream.IsReading() && version > V))
			return false;
		return true;
	}
	void SetDone() {
		version = V;
	}
	bool IsInitialized() const
	{
		return version != 0;
	}
};

struct ReplayOffsets : ForwardCompatStruct<1>
{
	int32 global;
	int32 input;
	int32 laser;
	int32 song;
	ReplayOffsets() = default;
	ReplayOffsets(int32 g, int32 i, int32 l, int32 s) :
		global(g), input(i), laser(l), song(s) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayOffsets*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->global << t->input << t->laser << t->song;
		return stream.IsOk();
	}
};

struct ReplayChartInfo : ForwardCompatStruct<1>
{
	String hash;
	String title;
	String artist;
	String effector;
	int32 level;
	ReplayChartInfo() = default;
	ReplayChartInfo(const ChartIndex* c) :
		hash(c->hash), title(c->title), artist(c->artist),
		effector(c->effector), level(c->level) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayChartInfo*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->hash << t->title << t->artist << t->effector << t->level;
		return stream.IsOk();
	}
};

struct ReplayScoreInfo : ForwardCompatStruct<1>
{
	int32 score;
	int32 crit;
	int32 almost;
	int32 miss;
	float gauge;
	GaugeType gaugeType;
	uint32 gaugeOption;
	bool random; // TODO save random state?
	bool mirror;
	uint64 timestamp;
	String userName;
	String userId;
	ReplayScoreInfo() = default;
	ReplayScoreInfo(const ScoreIndex* s) :
		score(s->score), crit(s->crit), almost(s->almost), miss(s->miss),
		gauge(s->gauge), gaugeType(s->gaugeType), gaugeOption(s->gaugeOption),
		random(s->random), mirror(s->mirror),
		timestamp(s->timestamp), userName(s->userName), userId(s->userId) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayScoreInfo*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->score << t->crit << t->almost << t->miss << t->gauge <<
			t->gaugeType << t->gaugeOption << t->random << t->mirror <<
			t->timestamp << t->userName << t->userId;
		return stream.IsOk();
	}
};

struct ReplayInput
{
	uint16 a;
};

#define REPLAY_VERSION 1
#define REPLAY_MAGIC 0x52435355u
class Replay
{
public:
	enum class ReplayType {
		Legacy,
		NoInput, // No inputs loaded
		Normal
	};

	Replay() = default;
	Replay(ReplayType type) : m_type(type) {}

	bool Save(String path);
	static Replay* Load(String path, ReplayType type=ReplayType::Normal);
	static bool StaticSerialize(BinaryStream& stream, Replay*& obj);
	static bool SerializeLegacy(BinaryStream& stream, Replay*& obj);

	void AttachChartInfo(const ChartIndex* chart) {
		m_chartInfo = chart;
		m_chartInfo.SetDone();
	}
	const ReplayChartInfo& GetChartInfo() const { return m_chartInfo; }

	void AttachScoreInfo(ScoreIndex* score) {
		m_scoreIndex = score;
		m_scoreInfo = score;
		m_scoreInfo.SetDone();
	}
	const ReplayScoreInfo& GetScoreInfo() const { return m_scoreInfo; }

	void AttachJudgementEvents(const Vector<SimpleHitStat>& v)
	{
		if (m_initialized) return;
		m_judgementEvents.clear();
		for (auto& hs : v)
			m_judgementEvents.push_back(hs);
	}

	void SetHitWindow(const HitWindow& window) {
		m_hitWindow = window;
	}

	void SetOffsets(const ReplayOffsets& offs) {
		m_offsets = offs;
		m_offsets.SetDone();
	}

	void DoneInit()
	{
		m_initialized = true;
	}

	ScoreIndex* GetScoreStruct() const {
		return m_scoreIndex;
	}

	bool IsPlaying() const {
		return m_isPlaying;
	}

	void Restart() {
		m_currentMaxScore = 0;
		m_currentScore = 0;
		m_nextJudgementIndex = 0;
		m_lastEvalTime = 0;
		if (m_isPlaying)
		{
			for (int i = 0; i < 8; i++) {
				m_playbackQueue[i] = std::deque<const ReplayJudgement*>();
			}
		}
	}

	uint32 CurrentMaxScore() const {
		return m_currentMaxScore;
	}

	uint32 CurrentScore() const {
		return m_currentScore;
	}

	// Put this replay into playback mode
	void StartPlaying()
	{
		m_isPlaying = true;
		Restart();
	}

	void UpdateToTime(MapTime lastTime)
	{
		if (lastTime < m_lastEvalTime)
			return;

		// TODO(replay) adjust the time using the offsets?
		for (; m_nextJudgementIndex < m_judgementEvents.size(); m_nextJudgementIndex++)
		{
			const ReplayJudgement* j = &m_judgementEvents[m_nextJudgementIndex];

			// Legacy has the recorded time in the time field
			MapTime time = (m_type == ReplayType::Legacy ?
				j->time : j->GetHitTime());

			if (time > lastTime)
				break;

			if (m_isPlaying)
			{
				// We can only do this if we don't reallocate this vector
				// so we use m_initialized to keep it immutable after init
				m_playbackQueue[j->lane].push_back(j);
			}

			if (j->rating < 3)
			{
				m_currentMaxScore += 2;
				m_currentScore += j->rating;
			}
		}
		m_lastEvalTime = lastTime;
	}

	bool HasJudgement(int lane) const
	{
		assert(m_isPlaying);
		assert(lane < 8);
		return m_playbackQueue[lane].empty();
	}

	const ReplayJudgement* PeekNextJudgement(int lane) const
	{
		assert(m_isPlaying);
		assert(lane < 8);
		const auto& q = m_playbackQueue[lane];
		if (!q.empty())
			return q.front();
		return nullptr;
	}

	const ReplayJudgement* PopNextJudgement(int lane)
	{
		assert(m_isPlaying);
		assert(lane < 8);
		auto& q = m_playbackQueue[lane];
		if (q.empty())
			return nullptr;

		auto* ret = q.front();
		q.pop_front();
		return ret;
	}


	const ReplayJudgement* FindNextJudgement(int lane, MapTime future = 0) const
	{
		assert(m_isPlaying);
		assert(lane < 8);
		if (auto* ret = PeekNextJudgement(lane))
			return ret;

		for (size_t i = m_nextJudgementIndex; i < m_judgementEvents.size(); i++)
		{
			const ReplayJudgement* j = &m_judgementEvents[i];

			// Legacy has the recorded time in the time field
			MapTime time = (m_type == ReplayType::Legacy ?
				j->time : j->GetHitTime());

			if (time > m_lastEvalTime + future)
				break;

			if (j->lane != lane)
				continue;

			return j;
		}
		return nullptr;
	}

protected:
	ReplayType m_type = ReplayType::Normal;

	ReplayChartInfo m_chartInfo;
	ReplayScoreInfo m_scoreInfo;

	HitWindow m_hitWindow = HitWindow::NORMAL;
	ReplayOffsets m_offsets;

	Vector<ReplayJudgement> m_judgementEvents;
	Vector<ReplayInput> m_inputEvents;
	bool m_initialized = false;

	ScoreIndex* m_scoreIndex = nullptr;

	// Playback related vars
	bool m_isPlaying = false;
	int32 m_currentScore = 0;
	int32 m_currentMaxScore = 0; // May be wrong for some legacy replays
	std::deque<const ReplayJudgement*> m_playbackQueue[8];
	size_t m_nextJudgementIndex = 0;
	MapTime m_lastEvalTime = 0;
};





/*
struct ScoreReplay
{
	int32 currentScore = 0; //< Current score; updated during playback
	int32 currentMaxScore = 0; //< Current max possible score; updated during playback
	int32 maxScore = 0;
	size_t nextHitStat = 0;
	Vector<SimpleHitStat> replay;
	bool isPlayback = false;

	HitWindow hitWindow = HitWindow::NORMAL;

	std::deque<const SimpleHitStat*> playbackQueue[8];

struct SimpleHitStat
{
	// 0 = miss, 1 = near, 2 = crit, 3 = idle
	int8 rating;
	int8 lane;
	int32 time;
	int32 delta;
	// Hold state
	// This is the amount of gotten ticks in a hold sequence
	uint32 hold = 0;
	// This is the amount of total ticks in this hold sequence
	uint32 holdMax = 0;
};
*/
