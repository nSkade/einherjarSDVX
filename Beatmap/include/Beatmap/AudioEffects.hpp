/*
	Contains a list of game specific audio effect types
*/
#pragma once
#include <Shared/Enum.hpp>
#include <Shared/Interpolation.hpp>
#include <kson/audio/audio_effect.hpp>

	/*
	Effect parameter that is used to define a certain time range/period/speed
*/
	class EffectDuration
{
public:
	EffectDuration() = default;
	// Duration in milliseconds
	EffectDuration(int32 duration);
	// Duration relative to whole note duration
	EffectDuration(float rate);

	static EffectDuration Lerp(const EffectDuration &lhs, const EffectDuration &rhs, float time);

	// Convert to ms duration
	// pass in the whole note duration
	uint32 Absolute(double noteDuration);

	// Either a float or integer value
	union
	{
		float rate;
		int32 duration;
	};

	// The type of timing that the value represents
	enum Type : uint8
	{
		Rate, // Relative (1/4, 1/2, 0.5, etc), all relative to whole notes
		Time, // Absolute, in milliseconds
	};
	Type type;
};

template <typename T>
T InterpolateEffectParamValue(T a, T b, float t)
{
	return T(a + (b - a) * t);
}
EffectDuration InterpolateEffectParamValue(EffectDuration a, EffectDuration b, float t);

/*
	Effect parameter that allows all the values which can be set for effects
*/
template <typename T>
class EffectParam
{
public:
	EffectParam() = default;
	EffectParam(T value)
	{
		values[0] = value;
		values[1] = value;
		isRange = false;
		timeFunction = Interpolation::Linear;
	}
	EffectParam(T valueA, T valueB, Interpolation::TimeFunction timeFunction = Interpolation::Linear)
	{
		values[0] = valueA;
		values[1] = valueB;
		this->timeFunction = timeFunction;
		isRange = true;
	}

	// Sample based on laser input, or without parameters for just the actual value
	T Sample(float t = 0.0f) const
	{
		t = Math::Clamp(timeFunction(t), 0.0f, 1.0f);
		return isRange ? InterpolateEffectParamValue(values[0], values[1], t) : values[0];
	}

	Interpolation::TimeFunction timeFunction;

	// Either 1 or 2 values based on if this value should be interpolated by laser input or not
	T values[2];

	// When set to true, this means the parameter is a range
	bool isRange;
};


struct MultiParam
{
	enum Type
	{
		Float,
		Samples,
		Milliseconds,
		Int,
	};
	Type type;
	union {
		float fval;
		int32 ival;
	};
};
struct MultiParamRange
{
	MultiParamRange() = default;
	MultiParamRange(const MultiParam& a)
	{
		params[0] = a;
	}
	MultiParamRange(const MultiParam& a, const MultiParam& b)
	{
		params[0] = a;
		params[1] = b;
		isRange = true;
	}
	EffectParam<float> ToFloatParam();
	EffectParam<EffectDuration> ToDurationParam();
	EffectParam<int32> ToSamplesParam();
	MultiParam params[2];
	bool isRange = false;
};


struct AudioEffect
{
	static MultiParam ParseParam(const String& in);
	// Use this to get default effect settings
	static const AudioEffect &GetDefault(kson::AudioEffectType type);
	static int GetDefaultEffectPriority(kson::AudioEffectType type);
	static Map<const std::string, kson::AudioEffectType> strToAudioEffectType() {
		return {
			{ "retrigger", kson::AudioEffectType::Retrigger },
			{ "gate", kson::AudioEffectType::Gate },
			{ "flanger", kson::AudioEffectType::Flanger },
			{ "pitch_shift", kson::AudioEffectType::PitchShift },
			{ "bitcrusher", kson::AudioEffectType::Bitcrusher },
			{ "phaser", kson::AudioEffectType::Phaser },
			{ "wobble", kson::AudioEffectType::Wobble },
			{ "tapestop", kson::AudioEffectType::Tapestop },
			{ "echo", kson::AudioEffectType::Echo },
			{ "sidechain", kson::AudioEffectType::Sidechain },
			{ "audio_swap", kson::AudioEffectType::SwitchAudio },
			{ "high_pass_filter", kson::AudioEffectType::HighPassFilter },
			{ "low_pass_filter", kson::AudioEffectType::LowPassFilter },
			{ "peaking_filter", kson::AudioEffectType::PeakingFilter },
		};
	}

	void SetDefaultEffectParams(int16 *params);

	// The effect type
	kson::AudioEffectType type = kson::AudioEffectType::Unspecified;

	// Timing division for time based effects
	// Wobble:		length of single cycle
	// Phaser:		length of single cycle
	// Flanger:		length of single cycle
	// Tapestop:	duration of stop
	// Gate:		length of a single
	// Sidechain:	duration before reset
	// Echo:		delay
	EffectParam<EffectDuration> duration = EffectDuration(0.25f); // 1/4
	Map<String, MultiParamRange> defParams;

	// How much of the effect is mixed in with the source audio
	EffectParam<float> mix = 0.0f;

	union
	{
		struct
		{
			// Amount of gating on this effect (0-1)
			EffectParam<float> gate;
			// Duration after which the retriggered sample area resets
			// 0 for no reset
			// TODO: This parameter allows this effect to be merged with gate
			EffectParam<EffectDuration> reset;
		} retrigger;
		struct
		{
			// Amount of gating on this effect (0-1)
			EffectParam<float> gate;
		} gate;
		struct
		{
			// Number of samples that is offset from the source audio to create the flanging effect (Samples)
			EffectParam<int32> offset;
			// Depth of the effect (samples)
			EffectParam<int32> depth;
			// Feedback (0-1)
			EffectParam<float> feedback;
			// Stereo width (0-1)
			EffectParam<float> stereoWidth;
			// Volume of added source audio + delayed source audio (0-1)
			EffectParam<float> volume;
		} flanger;
		struct
		{
			// Number of stages
			EffectParam<int32> stage;
			// Minimum frequency (Hz)
			EffectParam<float> min;
			// Maximum frequency (Hz)
			EffectParam<float> max;
			// Q factor (0-1)
			EffectParam<float> q;
			// Feedback (0-1)
			EffectParam<float> feedback;
			// Stereo width (0-1)
			EffectParam<float> stereoWidth;
			// High cut gain (dB)
			EffectParam<float> hiCutGain;
		} phaser;
		struct
		{
			// The duration in samples of how long a sample in the source audio gets reduced (creating square waves) (samples)
			EffectParam<int32> reduction;
		} bitcrusher;
		struct
		{
			// Top frequency of the wobble (Hz)
			EffectParam<float> max;
			// Bottom frequency of the wobble (Hz)
			EffectParam<float> min;
			// Q factor for filter (>0)
			EffectParam<float> q;
		} wobble;
		struct
		{
			// Time to fully reduce volume
			EffectParam<EffectDuration> attackTime;
			// Time to keep volume at maximum reduced value
			EffectParam<EffectDuration> holdTime;
			// Time to restore volume
			EffectParam<EffectDuration> releaseTime;
			// Ratio to reduce volume by
			EffectParam<float> ratio;
		} sidechain;
		struct
		{
			// Ammount of echo (0-1)
			EffectParam<float> feedback;
		} echo;
		struct
		{
			// Panning position, 0 is center (-1-1)
			EffectParam<float> panning;
		} panning;
		struct
		{
			// Pitch shift amount, in semitones
			EffectParam<float> amount;
		} pitchshift;
		struct
		{
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} lpf;
		struct
		{
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} hpf;
		struct
		{
			// Peak amplification (>=0)
			EffectParam<float> gain;
			// Q factor for filter (>0)
			EffectParam<float> q;
			// Cuttoff frequency (Hz)
			EffectParam<float> freq;
		} peaking;
		struct
		{
			// Audio Index
			EffectParam<int32> index;
		} switchaudio;
	};
};
