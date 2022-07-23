#include "stdafx.h"
#include "AudioPlayback.hpp"
#include <Beatmap/BeatmapPlayback.hpp>
#include <Audio/DSP.hpp>
#include <Audio/Audio.hpp>

DSP *GameAudioEffect::CreateDSP(const TimingPoint &tp, float filterInput, uint32 sampleRate, float playbackSpeed)
{
	DSP *ret = nullptr;

	double noteDuration = tp.GetWholeNoteLength();
	if (playbackSpeed > 0) noteDuration /= playbackSpeed;

	const uint32 actualLength = duration.Sample(filterInput).Absolute(noteDuration);
	const uint32 maxLength = Math::Max(duration.Sample(0.f).Absolute(noteDuration), duration.Sample(1.f).Absolute(noteDuration));

	switch (type)
	{
	case kson::AudioEffectType::Bitcrusher:
	{
		BitCrusherDSP *bcDSP = new BitCrusherDSP(sampleRate);
		bcDSP->SetPeriod((float)bitcrusher.reduction.Sample(filterInput));
		ret = bcDSP;
		break;
	}
	case kson::AudioEffectType::Echo:
	{
		EchoDSP *echoDSP = new EchoDSP(sampleRate);
		echoDSP->feedback = echo.feedback.Sample(filterInput) / 100.0f;
		echoDSP->SetLength(actualLength);
		ret = echoDSP;
		break;
	}
	case kson::AudioEffectType::PeakingFilter:
	case kson::AudioEffectType::LowPassFilter:
	case kson::AudioEffectType::HighPassFilter:
	{
		// Don't set anthing for biquad Filters
		BQFDSP *bqfDSP = new BQFDSP(sampleRate);
		ret = bqfDSP;
		break;
	}
	case kson::AudioEffectType::Gate:
	{
		GateDSP *gateDSP = new GateDSP(sampleRate);
		gateDSP->SetLength(actualLength);
		gateDSP->SetGating(gate.gate.Sample(filterInput));
		ret = gateDSP;
		break;
	}
	case kson::AudioEffectType::Tapestop:
	{
		TapeStopDSP *tapestopDSP = new TapeStopDSP(sampleRate);
		tapestopDSP->SetLength(actualLength);
		ret = tapestopDSP;
		break;
	}
	case kson::AudioEffectType::Retrigger:
	{
		RetriggerDSP *retriggerDSP = new RetriggerDSP(sampleRate);
		retriggerDSP->SetMaxLength(maxLength);
		retriggerDSP->SetLength(actualLength);
		retriggerDSP->SetGating(retrigger.gate.Sample(filterInput));
		retriggerDSP->SetResetDuration(retrigger.reset.Sample(filterInput).Absolute(noteDuration));
		ret = retriggerDSP;
		break;
	}
	case kson::AudioEffectType::Wobble:
	{
		WobbleDSP *wb = new WobbleDSP(sampleRate);
		wb->SetLength(actualLength);
		wb->q = wobble.q.Sample(filterInput);
		wb->fmax = wobble.max.Sample(filterInput);
		wb->fmin = wobble.min.Sample(filterInput);
		ret = wb;
		break;
	}
	case kson::AudioEffectType::Phaser:
	{
		PhaserDSP *phs = new PhaserDSP(sampleRate);
		phs->SetLength(actualLength);
		phs->SetStage(phaser.stage.Sample(filterInput));
		phs->fmin = phaser.min.Sample(filterInput);
		phs->fmax = phaser.max.Sample(filterInput);
		phs->q = phaser.q.Sample(filterInput);
		phs->feedback = phaser.feedback.Sample(filterInput);
		phs->stereoWidth = phaser.stereoWidth.Sample(filterInput);
		phs->hiCutGain = phaser.hiCutGain.Sample(filterInput);
		ret = phs;
		break;
	}
	case kson::AudioEffectType::Flanger:
	{
		FlangerDSP *fl = new FlangerDSP(sampleRate);
		fl->SetLength(actualLength);
		fl->SetDelayRange(abs(flanger.offset.Sample(filterInput)),
						  abs(flanger.depth.Sample(filterInput)));
		fl->SetFeedback(flanger.feedback.Sample(filterInput));
		fl->SetStereoWidth(flanger.stereoWidth.Sample(filterInput));
		fl->SetVolume(flanger.volume.Sample(filterInput));
		ret = fl;
		break;
	}
	case kson::AudioEffectType::Sidechain:
	{
		SidechainDSP *sc = new SidechainDSP(sampleRate);
		sc->SetLength(actualLength);
		sc->SetAttackTime(sidechain.attackTime.Sample(filterInput).Absolute(noteDuration));
		sc->SetHoldTime(sidechain.holdTime.Sample(filterInput).Absolute(noteDuration));
		sc->SetReleaseTime(sidechain.releaseTime.Sample(filterInput).Absolute(noteDuration));
		sc->ratio = sidechain.ratio.Sample(filterInput);
		ret = sc;
		break;
	}
	case kson::AudioEffectType::PitchShift:
	{
		PitchShiftDSP *ps = new PitchShiftDSP(sampleRate);
		ps->amount = pitchshift.amount.Sample(filterInput);
		ret = ps;
		break;
	}
	default:
		break;
	}

	if (!ret)
	{
		Logf("Failed to create game audio effect for type \"%s\"", Logger::Severity::Warning, kson::AudioEffectTypeToStr(type));
		return nullptr;
	}

	ret->mix = mix.Sample(filterInput);
	return ret;
}
void GameAudioEffect::SetParams(DSP *dsp, AudioPlayback &playback, HoldObjectState *object)
{
	const TimingPoint &tp = *playback.GetBeatmapPlayback().GetTimingPointAt(object->time);
	double noteDuration = tp.GetWholeNoteLength();

	auto paramOrDefault = [&](String param, float defaultValue) {
		return object->effectParams.Contains(param) ? object->effectParams[param].ToFloatParam().Sample(1) : defaultValue;
	};


	//TODO: Params
	switch (type)
	{
	case kson::AudioEffectType::Bitcrusher:
	{
		BitCrusherDSP *bcDSP = (BitCrusherDSP *)dsp;
		bcDSP->SetPeriod(paramOrDefault("reduction", 10));
		break;
	}
	case kson::AudioEffectType::Gate:
	{
		GateDSP *gateDSP = (GateDSP *)dsp;
		gateDSP->SetGating(paramOrDefault("rate", 0.6));
		gateDSP->SetLength(noteDuration / paramOrDefault("wave_length", 2));
		break;
	}
	case kson::AudioEffectType::Tapestop:
	{
		TapeStopDSP *tapestopDSP = (TapeStopDSP *)dsp;
		tapestopDSP->SetLength((1000 * ((double)16 / Math::Max(paramOrDefault("speed", 1), 1.0f))));
		break;
	}
	case kson::AudioEffectType::Retrigger:
	{
		RetriggerDSP *retriggerDSP = (RetriggerDSP *)dsp;
		retriggerDSP->SetLength(noteDuration / paramOrDefault("wave_length", 2));
		break;
	}
	case kson::AudioEffectType::Echo:
	{
		EchoDSP *echoDSP = (EchoDSP *)dsp;
		echoDSP->SetLength(noteDuration / paramOrDefault("wave_length", 2));
		echoDSP->feedback = paramOrDefault("feedback", 0.6);
		break;
	}
	case kson::AudioEffectType::Wobble:
	{
		WobbleDSP *wb = (WobbleDSP *)dsp;
		wb->SetLength(noteDuration / paramOrDefault("wave_length", 12));
		break;
	}
	case kson::AudioEffectType::Phaser:
	{
		PhaserDSP *phs = (PhaserDSP *)dsp;
		break;
	}
	case kson::AudioEffectType::Flanger:
	{
		FlangerDSP *fl = (FlangerDSP *)dsp;
		break;
	}
	case kson::AudioEffectType::PitchShift:
	{
		PitchShiftDSP *ps = (PitchShiftDSP *)dsp;
		ps->amount = paramOrDefault("pitch", 0);
		break;
	}
	default:
		break;
	}
}
