#include "stdafx.h"
#include "AudioEffects.hpp"

EffectDuration::EffectDuration(int32 duration /*= 0*/)
{
	this->duration = duration;
	type = Time;
}
EffectDuration::EffectDuration(float rate)
{
	this->rate = rate;
	type = Rate;
}

EffectDuration EffectDuration::Lerp(const EffectDuration &lhs, const EffectDuration &rhs, float time)
{
	assert(rhs.type == lhs.type);
	if (lhs.type == Type::Rate)
		return lhs.rate + (lhs.rate - rhs.rate) * time;
	else
		return (int32)(lhs.duration + (float)(lhs.duration - rhs.duration) * time);
}
uint32 EffectDuration::Absolute(double noteDuration)
{
	if (type == Time)
		return duration;
	else
		return (uint32)(rate * noteDuration);
}

EffectDuration InterpolateEffectParamValue(EffectDuration a, EffectDuration b, float t)
{
	return EffectDuration::Lerp(a, b, t);
}

MultiParam AudioEffect::ParseParam(const String& in)
{
	MultiParam ret;
	if (in.find('/') != -1)
	{
		ret.type = MultiParam::Float;
		String a, b;
		in.Split("/", &a, &b);
		ret.fval = (float)(atof(*a) / atof(*b));
	}
	else if (in.find("samples") != -1)
	{
		ret.type = MultiParam::Samples;
		sscanf(*in, "%i", &ret.ival);
	}
	else if (in.find("ms") != -1)
	{
		ret.type = MultiParam::Milliseconds;
		float milliseconds = 0;
		sscanf(*in, "%f", &milliseconds);
		ret.ival = (int)(milliseconds);
	}
	else if (in.find("s") != -1)
	{
		ret.type = MultiParam::Milliseconds;
		float seconds = 0;
		sscanf(*in, "%f", &seconds);
		ret.ival = (int)(seconds * 1000.0);
	}
	else if (in.find("%") != -1)
	{
		ret.type = MultiParam::Float;
		int percentage = 0;
		sscanf(*in, "%i", &percentage);
		ret.fval = percentage / 100.0f;
	}
	else if (in.find('.') != -1)
	{
		ret.type = MultiParam::Float;
		sscanf(*in, "%f", &ret.fval);
	}
	else
	{
		ret.type = MultiParam::Int;
		sscanf(*in, "%i", &ret.ival);
	}
	return ret;
}

static AudioEffect CreateDefault(kson::AudioEffectType type)
{
	AudioEffect ret;
	ret.type = type;

	typedef EffectParam<float> FloatRange;
	typedef EffectParam<int32> IntRange;
	typedef EffectParam<EffectDuration> TimeRange;

	// Default timing is 1/4
	ret.duration = TimeRange(0.25f);
	ret.mix = FloatRange(1.0);
	Interpolation::CubicBezier laserEasingCurve = Interpolation::EaseInExpo;
	Interpolation::CubicBezier lpfEasingCurve = Interpolation::EaseOutCubic;

	// Set defaults based on effect type
	switch (type)
	{
		// These are assumed to mostly be laser effect types (at least when used with the defaults)
	case kson::AudioEffectType::PeakingFilter:
	{
		ret.peaking.freq = FloatRange(80.0f, 8000.0f, laserEasingCurve);
		ret.peaking.q = FloatRange(1.f, 0.8f);
		ret.peaking.gain = FloatRange(20.0f, 20.0f);
		break;
	}
	case kson::AudioEffectType::LowPassFilter:
	{
		ret.lpf.freq = FloatRange(10000.0f, 700.0f, lpfEasingCurve);
		ret.lpf.q = FloatRange(7.0f, 10.0f);
		break;
	}
	case kson::AudioEffectType::HighPassFilter:
	{
		ret.hpf.freq = FloatRange(80.0f, 2000.0f, laserEasingCurve);
		ret.hpf.q = FloatRange(10.0f, 5.0f);
		break;
	}
	case kson::AudioEffectType::Bitcrusher:
		ret.bitcrusher.reduction = IntRange(0, 45);
		break;
	case kson::AudioEffectType::Gate:
		ret.mix = FloatRange(0.9f);
		ret.gate.gate = 0.5f;
		break;
	case kson::AudioEffectType::Retrigger:
		ret.retrigger.gate = 0.7f;
		ret.retrigger.reset = TimeRange(0.5f);
		break;
	case kson::AudioEffectType::Echo:
		ret.echo.feedback = FloatRange(0.60f);
		break;
	case kson::AudioEffectType::Tapestop:
		break;
	case kson::AudioEffectType::Phaser:
		ret.duration = TimeRange(0.5f);
		ret.mix = FloatRange(0.5f);
		ret.phaser.stage = IntRange(6);
		ret.phaser.min = FloatRange(1500.0f);
		ret.phaser.max = FloatRange(20000.0f);
		ret.phaser.q = FloatRange(0.707f);
		ret.phaser.feedback = FloatRange(0.35f);
		ret.phaser.stereoWidth = FloatRange(0.75f);
		ret.phaser.hiCutGain = FloatRange(-8.0f);
		break;
	case kson::AudioEffectType::Wobble:
		// wobble is 1/12 by default
		ret.duration = TimeRange(1.0f / 12.0f);
		ret.mix = FloatRange(0.8f);
		ret.wobble.max = FloatRange(20000.0f);
		ret.wobble.min = FloatRange(500.0f);
		ret.wobble.q = FloatRange(1.414f);
		break;
	case kson::AudioEffectType::Sidechain:
		ret.duration = TimeRange(1.0f / 4.0f);
		ret.sidechain.attackTime = TimeRange(10);
		ret.sidechain.holdTime = TimeRange(50);
		ret.sidechain.releaseTime = TimeRange(1.0f / 16.0f);
		ret.sidechain.ratio = FloatRange(5.0f);
		break;
	case kson::AudioEffectType::Flanger:
		ret.duration = TimeRange(2.0f);
		ret.mix = FloatRange(0.8f);
		ret.flanger.offset = IntRange(30);
		ret.flanger.depth = IntRange(45);
		ret.flanger.feedback = FloatRange(0.6f);
		ret.flanger.stereoWidth = FloatRange(0.0f);
		ret.flanger.volume = FloatRange(0.75f);
		break;
	case kson::AudioEffectType::SwitchAudio:
		ret.switchaudio.index = -1;
		break;
	}

	return ret;
}
class DefaultEffectSettings
{
public:
	Map<kson::AudioEffectType, AudioEffect> effects;

	DefaultEffectSettings()
	{
		for (auto&& defaultEffect : AudioEffect::strToAudioEffectType()) {
			effects.Add( defaultEffect.second, CreateDefault(defaultEffect.second));
		}
	}
};

const AudioEffect &AudioEffect::GetDefault(kson::AudioEffectType type)
{
	static DefaultEffectSettings defaults;
	return defaults.effects.at(type);
}

int AudioEffect::GetDefaultEffectPriority(kson::AudioEffectType type)
{
	const Map<kson::AudioEffectType, int> priorityMap = {
		{kson::AudioEffectType::Retrigger, 0},
		{kson::AudioEffectType::Echo, 1},
		{kson::AudioEffectType::Gate, 2},
		{kson::AudioEffectType::Tapestop, 3},
		{kson::AudioEffectType::Sidechain, 4},
		{kson::AudioEffectType::Bitcrusher, 5},
	};

	if (priorityMap.Contains(type))
		return priorityMap.at(type);

	return 99;
}

void AudioEffect::SetDefaultEffectParams(int16 *params)
{
	// Set defaults based on effect type
	switch (type)
	{
	case kson::AudioEffectType::Bitcrusher:
		params[0] = bitcrusher.reduction.Sample(1);
		break;
	case kson::AudioEffectType::Wobble:
		break;
	case kson::AudioEffectType::Flanger:
		params[0] = flanger.depth.Sample(1);
		params[1] = flanger.offset.Sample(1);
		break;
	default:
		break;
	}
}

EffectParam<float> MultiParamRange::ToFloatParam()
{
	auto r = params[0].type == MultiParam::Float ? EffectParam<float>(params[0].fval, params[1].fval) : EffectParam<float>((float)params[0].ival, (float)params[1].ival);
	r.isRange = isRange;
	return r;
}

EffectParam<EffectDuration> MultiParamRange::ToDurationParam()
{
	EffectParam<EffectDuration> r;
	if (params[0].type == MultiParam::Milliseconds)
	{
		r = EffectParam<EffectDuration>(params[0].ival, params[1].ival);
	}
	else if (params[0].type == MultiParam::Float)
	{
		r = EffectParam<EffectDuration>(params[0].fval, params[1].fval);
	}
	else
	{
		r = EffectParam<EffectDuration>((float)params[0].ival, (float)params[1].ival);
	}
	r.isRange = isRange;
	return r;
}

EffectParam<int32> MultiParamRange::ToSamplesParam()
{
	EffectParam<int32> r;
	if (params[0].type == MultiParam::Int || params[0].type == MultiParam::Samples)
		r = EffectParam<int32>(params[0].ival, params[1].ival);
	r.isRange = isRange;
	return r;
}
