#pragma once
#include "AudioStream.hpp"
#include "Sample.hpp"
#include <complex>

extern class Audio* g_audio;

/*
	Main audio manager
	keeps track of active samples and audio streams
	also handles mixing and DSP's on playing items
*/
class Audio : Unique
{ 
public:
	Audio();
	~Audio();
	// Initializes the audio device
	bool Init(bool exclusive);
	void SetGlobalVolume(float vol);

	// Opens a stream at path
	//	settings preload loads the whole file into memory before playing
	Ref<AudioStream> CreateStream(const String& path, bool preload = false);
	// Open a wav file at path
	Sample CreateSample(const String& path);

	// Target/Output sample rate
	uint32 GetSampleRate() const;

	// Private
	class Audio_Impl* GetImpl();

	// Calculated audio latency by the audio driver (currently unused)
	int64 audioLatency;

	//Skade wrapper for AudioImpl
	uint32 GetSampleBufferLength(); // uint32 m_sampleBufferLength = 384
	const float* GetSampleBuffer();

	void ProcessFFT(float* out_buckets, uint32 in_bucketAmount);
private:
	std::vector<std::complex<double>> Audio::FFT(std::vector<std::complex<double>>& samples);
	bool m_initialized = false;
};