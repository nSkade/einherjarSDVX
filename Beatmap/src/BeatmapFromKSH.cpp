#include "stdafx.h"
#include "Beatmap.hpp"
#include "KShootMap.hpp"
#include "kson/kson.hpp"
#include "kson/util/timing_utils.hpp"


template <typename T>
void AssignAudioEffectParameter(EffectParam<T> &param, const String &paramName, Map<String, float> &floatParams, Map<String, int> &intParams)
{
	float *fval = floatParams.Find(paramName);
	if (fval)
	{
		param = *fval;
		return;
	}
	int32 *ival = intParams.Find(paramName);
	if (ival)
	{
		param = *ival;
		return;
	}
}

AudioEffect ParseCustomEffect(const kson::AudioEffectDef &def, Vector<String> &switchablePaths)
{
	AudioEffect effect;
	bool typeSet = false;

	// Get the default effect for this name
	

	effect = AudioEffect::GetDefault(def.type);
	typeSet = true;

	Map<String, MultiParamRange> params;
	for (const auto& s : def.v)
	{
		// Special case for SwitchAudio effect
		if (s.first == "fileName")
		{
			MultiParam switchableIndex;
			switchableIndex.type = MultiParam::Int;

			auto it = std::find(switchablePaths.begin(), switchablePaths.end(), s.second);
			if (it == switchablePaths.end())
			{
				switchableIndex.ival = static_cast<int32>(switchablePaths.size());
				switchablePaths.Add(s.second);
			}
			else
			{
				switchableIndex.ival = static_cast<int32>(std::distance(switchablePaths.begin(), it));
			}

			params.Add("index", switchableIndex);
			continue;
		}

		size_t splitArrow = s.second.find('>', 1);
		String param;
		if (splitArrow != -1)
		{
			param = s.second.substr(splitArrow + 1);
		}
		else
		{
			param = s.second;
		}
		size_t split = param.find('-', 1);
		if (split != -1)
		{
			String a, b;
			a = param.substr(0, split);
			b = param.substr(split + 1);

			MultiParamRange pr = { AudioEffect::ParseParam(a), AudioEffect::ParseParam(b)};
			if (pr.params[0].type != pr.params[1].type)
			{
				//Logf("Non matching parameters types \"[%s, %s]\" for key: %s", Logger::Severity::Warning, s.first, param, s.first);
				continue;
			}
			params.Add(s.first, pr);
		}
		else
		{
			params.Add(s.first, AudioEffect::ParseParam(param));
		}
	}
	

	if (!typeSet)
	{
		Logf("Type not set for custom effect type: %s", Logger::Severity::Warning, kson::AudioEffectTypeToStr(def.type));
		return effect;
	}

	auto AssignFloatIfSet = [&](EffectParam<float> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToFloatParam();
		}
	};
	auto AssignDurationIfSet = [&](EffectParam<EffectDuration> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToDurationParam();
		}
	};
	auto AssignSamplesIfSet = [&](EffectParam<int32> &target, const String &name) {
		auto *param = params.Find(name);
		if (param && param->params[0].type == MultiParam::Samples)
		{
			target = param->ToSamplesParam();
		}
	};
	auto AssignIntIfSet = [&](EffectParam<int32> &target, const String &name) {
		auto *param = params.Find(name);
		if (param)
		{
			target = param->ToSamplesParam();
		}
	};

	AssignFloatIfSet(effect.mix, "mix");

	// Set individual parameters per effect based on if they are specified or not
	// if they are not set the defaults will be kept (as aquired above)
	switch (effect.type)
	{
	case kson::AudioEffectType::PitchShift:
		AssignFloatIfSet(effect.pitchshift.amount, "pitch");
		break;
	case kson::AudioEffectType::Bitcrusher:
		AssignSamplesIfSet(effect.bitcrusher.reduction, "amount");
		break;
	case kson::AudioEffectType::Echo:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.echo.feedback, "feedbackLevel");
		break;
	case kson::AudioEffectType::Phaser:
		AssignDurationIfSet(effect.duration, "period");
		AssignIntIfSet(effect.phaser.stage, "stage");
		AssignFloatIfSet(effect.phaser.min, "loFreq");
		AssignFloatIfSet(effect.phaser.max, "hiFreq");
		AssignFloatIfSet(effect.phaser.q, "Q");
		AssignFloatIfSet(effect.phaser.feedback, "feedback");
		AssignFloatIfSet(effect.phaser.stereoWidth, "stereoWidth");
		AssignFloatIfSet(effect.phaser.hiCutGain, "hiCutGain");
		break;
	case kson::AudioEffectType::Flanger:
		AssignDurationIfSet(effect.duration, "period");
		AssignIntIfSet(effect.flanger.depth, "depth");
		AssignIntIfSet(effect.flanger.offset, "delay");
		AssignFloatIfSet(effect.flanger.feedback, "feedback");
		AssignFloatIfSet(effect.flanger.stereoWidth, "stereoWidth");
		AssignFloatIfSet(effect.flanger.volume, "volume");
		break;
	case kson::AudioEffectType::Gate:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.gate.gate, "rate");
		break;
	case kson::AudioEffectType::Retrigger:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.retrigger.gate, "rate");
		AssignDurationIfSet(effect.retrigger.reset, "updatePeriod");
		break;
	case kson::AudioEffectType::Wobble:
		AssignDurationIfSet(effect.duration, "waveLength");
		AssignFloatIfSet(effect.wobble.min, "loFreq");
		AssignFloatIfSet(effect.wobble.max, "hiFreq");
		AssignFloatIfSet(effect.wobble.q, "Q");
		break;
	case kson::AudioEffectType::Sidechain:
		AssignDurationIfSet(effect.duration, "period");
		AssignDurationIfSet(effect.sidechain.attackTime, "attackTime");
		AssignDurationIfSet(effect.sidechain.holdTime, "holdTime");
		AssignDurationIfSet(effect.sidechain.releaseTime, "releaseTime");
		AssignFloatIfSet(effect.sidechain.ratio, "ratio");
		break;
	case kson::AudioEffectType::Tapestop:
		AssignDurationIfSet(effect.duration, "speed");
		break;
	case kson::AudioEffectType::SwitchAudio:
		AssignIntIfSet(effect.switchaudio.index, "index");
		break;
	}
	effect.defParams = params;
	return effect;
};

bool Beatmap::m_ProcessKShootMap(std::istream &input, bool metadataOnly)
{
	if (metadataOnly) {
		auto chartMeta = kson::LoadKSHMetaChartData(input);
		if (chartMeta.error == kson::Error::None) {
			m_SetMetadata(&chartMeta.meta);
			m_settings.previewDuration = chartMeta.audio.bgm.preview.duration;
			m_settings.previewOffset = chartMeta.audio.bgm.preview.offset;
			m_settings.audioNoFX = chartMeta.audio.bgm.filename;
			return true;
		}
		else {
			Logf("Ksh parse error: %s", Logger::Severity::Warning, kson::GetErrorString(chartMeta.error));
			return false;
		}
	}


	auto kshootMap = kson::LoadKSHChartData(input);

	m_SetMetadata(&kshootMap.meta);

	//Copied from libkson, ask masaka to make public instead?
	for (auto&& defaultEffect : AudioEffect::strToAudioEffectType()) {
		
		m_customAudioEffects.Add(defaultEffect.first.data(), AudioEffect::GetDefault(defaultEffect.second));
		m_customAudioFilters.Add(defaultEffect.first.data(), AudioEffect::GetDefault(defaultEffect.second));
	}

	for (auto&& effectDef : kshootMap.audio.audioEffect.fx.def) {
		auto effect = ParseCustomEffect(effectDef.second, m_switchablePaths);
		m_customAudioEffects.Add(effectDef.first, effect);
	}

	for (auto&& effectDef : kshootMap.audio.audioEffect.laser.def) {
		auto effect = ParseCustomEffect(effectDef.second, m_switchablePaths);
		m_customAudioFilters.Add(effectDef.first, effect);
	}

	// Process map settings
	m_settings.previewOffset = kshootMap.audio.bgm.preview.offset;
	m_settings.previewDuration = kshootMap.audio.bgm.preview.duration;

	//TODO: Bounds check
	m_settings.backgroundPath = kshootMap.bg.legacy.bg.at(0).filename;
	m_settings.foregroundPath = kshootMap.bg.legacy.layer.filename;
	m_settings.audioNoFX = kshootMap.audio.bgm.filename;
	m_settings.audioFX = kshootMap.audio.bgm.legacy.empty() ? "" : kshootMap.audio.bgm.legacy.filenameF;
	m_settings.offset = kshootMap.audio.bgm.offset;
	m_settings.total = kshootMap.gauge.total;
	m_settings.musicVolume = kshootMap.audio.bgm.vol;
	m_settings.speedBpm = kshootMap.meta.stdBPM;
	auto timingCache = kson::CreateTimingCache(kshootMap.beat);

	const TimingPoint* currTimingPoint = nullptr;

	/// For more accurate tracking of ticks for each timing point
	size_t refTimingPointInd = 0;
	double refTimingPointTime = 0.0;

	Vector<uint32> timingPointTicks = {0};

	auto TickToMapTime = [&](uint32 tick) {
		return Math::RoundToInt(kson::PulseToMs(tick, kshootMap.beat, timingCache)) + m_settings.offset;
	};

	// Add First Lane Toggle Point
	{
		LaneHideTogglePoint startLaneTogglePoint;
		startLaneTogglePoint.time = 0;
		startLaneTogglePoint.duration = 1;
		m_laneTogglePoints.Add(std::move(startLaneTogglePoint));
	}

	// Stop here if we're only going for metadata
	if (metadataOnly)
	{
		return true;
	}

	int lastMapTime = 0;

	//BT
	for (size_t i = 0; i < kson::kNumBTLanes; i++)
	{
		for (auto& note : kshootMap.note.bt[i]) {
			if (note.second.length > 0) {
				HoldObjectState* hos = new HoldObjectState();
				hos->index = i;
				hos->time = TickToMapTime(note.first);
				hos->duration = TickToMapTime(note.first + note.second.length) - hos->time;
				lastMapTime = Math::Max(lastMapTime, hos->time + hos->duration);
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*hos));
			}
			else {
				ButtonObjectState* bos = new ButtonObjectState();
				bos->index = i;
				bos->time = TickToMapTime(note.first);
				lastMapTime = Math::Max(lastMapTime, bos->time);
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*bos));
			}
		}
	}

	//FX
	//TODO: Split notes that have multiple effects
	for (size_t i = 0; i < kson::kNumFXLanes; i++)
	{
		for (auto& fxnote : kshootMap.note.fx[i]) {
			if (fxnote.second.length > 0) {
				HoldObjectState* hos = new HoldObjectState();
				hos->index = i + 4;
				hos->time = TickToMapTime(fxnote.first);
				hos->duration = TickToMapTime(fxnote.first + fxnote.second.length) - hos->time;
				Map<String, MultiParamRange> noteParams;
				
				for (auto&& effect : kshootMap.audio.audioEffect.fx.longEvent) {
					auto effectEvent = effect.second[i].find(fxnote.first);
					if (effectEvent != effect.second[i].end()) {
						hos->effectType = effect.first;
						hos->effectParams = m_customAudioEffects.at(effect.first).defParams; //start with def params
						for (auto&& effectParam : effectEvent->second)
						{
							noteParams[effectParam.first] = AudioEffect::ParseParam(effectParam.second); //save note params to apply after paramChange events
						}
						break;
					}
				}

				if (kshootMap.audio.audioEffect.fx.paramChange.find(hos->effectType) != kshootMap.audio.audioEffect.fx.paramChange.end()) {
					for (auto&& effectParams : kshootMap.audio.audioEffect.fx.paramChange.at(hos->effectType)) {
						for (auto&& paramChanges : effectParams.second) {
							if (paramChanges.first > fxnote.first)
								break;

							hos->effectParams[effectParams.first] = AudioEffect::ParseParam(paramChanges.second); // Apply paramChange events
						}
					}
				}

				for (auto&& p : noteParams) {
					hos->effectParams[p.first] = p.second; // Apply stored note params
				}

				lastMapTime = Math::Max(lastMapTime, hos->time + hos->duration);
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*hos));
			}
			else {
				ButtonObjectState* bos = new ButtonObjectState();
				bos->index = i + 4;
				bos->time = TickToMapTime(fxnote.first);
				lastMapTime = Math::Max(lastMapTime, bos->time);
				m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*bos));
			}
		}
	}

	//Laser
	for (size_t i = 0; i < kson::kNumLaserLanes; i++)
	{
		for (auto& laserSegment : kshootMap.note.laser[i]) {
			auto&& a = laserSegment.second.v.begin();
			LaserObjectState* prev = nullptr;
			do {
				auto b = a;
				++b;
				if (a->second.v != a->second.vf) {
					LaserObjectState* slam = new LaserObjectState();
					slam->index = i;
					slam->flags = 0;
					slam->flags |= LaserObjectState::flag_Instant;
					slam->time = TickToMapTime(a->first + laserSegment.first);
					slam->duration = 0;
					slam->points[0] = a->second.v;
					slam->points[1] = a->second.vf;

					if (laserSegment.second.w == 2) {
						slam->flags |= LaserObjectState::flag_Extended;
					}
					if (prev) {
						assert(prev->time < slam->time);
						prev->next = slam;
						slam->prev = prev;
					}
					prev = slam;
					m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*slam));

				}
				if (b != laserSegment.second.v.end())
				{
					LaserObjectState* los = new LaserObjectState();
					los->flags = 0;
					if (laserSegment.second.w == 2) {
						los->flags |= LaserObjectState::flag_Extended;
					}

					los->points[0] = a->second.vf;
					los->points[1] = b->second.v;
					los->time = TickToMapTime(a->first + laserSegment.first);
					los->duration = TickToMapTime(b->first + laserSegment.first) - los->time;
					assert(los->duration > 0);
					los->index = i;

					if (prev) {
						assert((prev->flags & LaserObjectState::flag_Instant != 0 && prev->time <= los->time) || (prev->time < los->time));
						prev->next = los;
						los->prev = prev;
					}

					prev = los;
					m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*los));
				}
				a = b;
			} while (a != laserSegment.second.v.end());
			if (prev)
			{
				lastMapTime = Math::Max(lastMapTime, prev->time + prev->duration);
			}
		}
	}

	//Laser effects
	for (auto&& laserEffect : kshootMap.audio.audioEffect.laser.pulseEvent) {
		for (auto&& pulse : laserEffect.second) {
			EventObjectState* evt = new EventObjectState();
			evt->key = EventKey::LaserEffectType;
			strcpy_s(evt->data.effectVal, sizeof(evt->data.effectVal), laserEffect.first.c_str());
			m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));
		}
	}

	//BPM
	for (auto&& bpm : kshootMap.beat.bpm) {
		auto sig = kson::TimeSigAt(bpm.first, kshootMap.beat, timingCache);
		TimingPoint tp;
		tp.numerator = sig.n;
		tp.denominator = sig.d;
		tp.time = TickToMapTime(bpm.first);
		tp.beatDuration = 60000.0 / bpm.second;
		m_timingPoints.Add(std::move(tp));
	}

	//TimeSig
	for (auto&& ts : kshootMap.beat.timeSig) {
		TimingPoint tp;
		tp.time = kson::MeasureIdxToMs(ts.first, kshootMap.beat, timingCache) + m_settings.offset;
		tp.beatDuration = 60000.0 / kson::TempoAt(kson::MeasureIdxToPulse(ts.first, kshootMap.beat, timingCache), kshootMap.beat);
		tp.denominator = ts.second.d;
		tp.numerator = ts.second.n;
		m_timingPoints.Add(std::move(tp));
	}

	auto&& laneToggles = kshootMap.compat.kshUnknown.option.find("lane_toggle");

	if (laneToggles != kshootMap.compat.kshUnknown.option.end()) {
		for (auto&& laneToggle : laneToggles->second) {
			LaneHideTogglePoint p;
			p.duration = atol(laneToggle.second.c_str());
			p.time = TickToMapTime(laneToggle.first);
			m_laneTogglePoints.Add(p);
		}
	}


	m_timingPoints.Sort([](TimingPoint& a, TimingPoint& b) {
		return a.time < b.time;
	});

	for (auto&& graphPoint : kshootMap.camera.cam.body.centerSplit) {
		m_centerSplit.Insert(TickToMapTime(graphPoint.first), graphPoint.second.v);
		if (graphPoint.second.vf != graphPoint.second.v)
			m_centerSplit.Insert(TickToMapTime(graphPoint.first), graphPoint.second.vf);
	}

	for (auto&& graphPoint : kshootMap.camera.cam.body.zoom) {
		m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_BOTTOM, TickToMapTime(graphPoint.first), graphPoint.second.v);
		if (graphPoint.second.vf != graphPoint.second.v)
			m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_BOTTOM, TickToMapTime(graphPoint.first), graphPoint.second.vf);
	}

	for (auto&& graphPoint : kshootMap.camera.cam.body.rotationX) {
		m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_TOP, TickToMapTime(graphPoint.first), graphPoint.second.v);
		if (graphPoint.second.vf != graphPoint.second.v)
			m_effects.InsertGraphValue(EffectTimeline::GraphType::ZOOM_TOP, TickToMapTime(graphPoint.first), graphPoint.second.vf);
	}

	for (auto&& graphPoint : kshootMap.camera.cam.body.shiftX) {
		m_effects.InsertGraphValue(EffectTimeline::GraphType::SHIFT_X, TickToMapTime(graphPoint.first), graphPoint.second.v);
		if (graphPoint.second.vf != graphPoint.second.v)
			m_effects.InsertGraphValue(EffectTimeline::GraphType::SHIFT_X, TickToMapTime(graphPoint.first), graphPoint.second.vf);
	}

	for (auto&& graphPoint : kshootMap.camera.cam.body.rotationZ) {
		m_effects.InsertGraphValue(EffectTimeline::GraphType::ROTATION_Z, TickToMapTime(graphPoint.first), graphPoint.second.v);
		if (graphPoint.second.vf != graphPoint.second.v)
			m_effects.InsertGraphValue(EffectTimeline::GraphType::ROTATION_Z, TickToMapTime(graphPoint.first), graphPoint.second.vf);
	}


	//TODO: Camera keep/manual/magnitude

	// Add chart end event
	EventObjectState *evt = new EventObjectState();
	evt->time = lastMapTime + 2000;
	evt->key = EventKey::ChartEnd;
	m_objectStates.emplace_back(std::unique_ptr<ObjectState>(*evt));

	// Re-sort collection to fix some inconsistencies caused by corrections after laser slams
	ObjectState::SortArray(m_objectStates);

	return true;
}
using kson::MetaInfo;
void Beatmap::m_SetMetadata(MetaInfo* data)
{
	/*
	* std::string title;
		std::string titleImgFilename; // UTF-8 guaranteed
		std::string artist;
		std::string artistImgFilename; // UTF-8 guaranteed
		std::string chartAuthor;
		DifficultyInfo difficulty;
		std::int32_t level = 1;
		std::string dispBPM;
		double stdBPM = 0.0;
		std::string jacketFilename; // UTF-8 guaranteed
		std::string jacketAuthor;
		std::string iconFilename; // UTF-8 guaranteed
		std::string information;
	*/
	m_settings.artist = data->artist;
	m_settings.title = data->title;
	m_settings.effector = data->chartAuthor;
	m_settings.difficulty = data->difficulty.idx;
	m_settings.level = data->level;
	m_settings.bpm = data->dispBPM;
	m_settings.speedBpm = data->stdBPM;
	m_settings.illustrator = data->jacketAuthor;
	m_settings.jacketPath = data->jacketFilename;
}
