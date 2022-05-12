#include "stdafx.h"
#include "Audio.hpp"
#include "AudioStream.hpp"
#include "Audio_Impl.hpp"
#include "AudioOutput.hpp"
#include "DSP.hpp"

#include <complex>
# define M_PI           3.14159265358979323846  /* pi */

Audio* g_audio = nullptr;
static Audio_Impl g_impl;

Audio_Impl::Audio_Impl()
{
#if _DEBUG
	InitMemoryGuard();
#endif
}

void Audio_Impl::Mix(void* data, uint32& numSamples)
{
	double adv = GetSecondsPerSample();

	uint32 outputChannels = this->output->GetNumChannels();
	if (output->IsIntegerFormat())
	{
		memset(data, 0, numSamples * sizeof(int16) * outputChannels);
	}
	else
	{
		memset(data, 0, numSamples * sizeof(float) * outputChannels);
	}

	uint32 currentNumberOfSamples = 0;
	while (currentNumberOfSamples < numSamples)
	{
		// Generate new sample
		if (m_remainingSamples <= 0)
		{
			// Clear sample buffer storing a fixed amount of samples
			m_sampleBuffer.fill(0);

			// Render items
			lock.lock();
			for(auto& item : itemsToRender)
			{
				// Clear per-channel data
				m_itemBuffer.fill(0);
				item->Process(m_itemBuffer.data(), m_sampleBufferLength);
#if _DEBUG
				CheckMemoryGuard();
#endif
				item->ProcessDSPs(m_itemBuffer.data(), m_sampleBufferLength);
#if _DEBUG
				CheckMemoryGuard();
#endif

				// Mix into buffer and apply volume scaling
				for (uint32 i = 0; i < m_sampleBufferLength; i++)
				{
					m_sampleBuffer[i * 2 + 0] += m_itemBuffer[i * 2] * item->GetVolume();
					m_sampleBuffer[i * 2 + 1] += m_itemBuffer[i * 2 + 1] * item->GetVolume();
				}
			}

			// Process global DSPs
			for (auto dsp : globalDSPs)
			{
				dsp->Process(m_sampleBuffer.data(), m_sampleBufferLength);
			}
			lock.unlock();

			// Apply volume levels
			for (uint32 i = 0; i < m_sampleBufferLength; i++)
			{
				m_sampleBuffer[i * 2 + 0] *= globalVolume;
				m_sampleBuffer[i * 2 + 1] *= globalVolume;
				// Safety clamp to [-1, 1] that should help protect speakers a bit in case of corruption
				// this will clip, but so will values outside [-1, 1] anyway
				m_sampleBuffer[i * 2 + 0] = fmin(fmax(m_sampleBuffer[i * 2 + 0], -1.f), 1.f);
				m_sampleBuffer[i * 2 + 1] = fmin(fmax(m_sampleBuffer[i * 2 + 1], -1.f), 1.f);
			}

			// Set new remaining buffer data
			m_remainingSamples = m_sampleBufferLength;
		}

		// Copy samples from sample buffer
		uint32 sampleOffset = m_sampleBufferLength - m_remainingSamples;
		uint32 maxSamples = Math::Min(numSamples - currentNumberOfSamples, m_remainingSamples);
		for (uint32 c = 0; c < outputChannels; c++)
		{
			if (c < 2)
			{
				for (uint32 i = 0; i < maxSamples; i++)
				{
					if (output->IsIntegerFormat())
					{
						((int16*)data)[(currentNumberOfSamples + i) * outputChannels + c] = (int16)(0x7FFF * Math::Clamp(m_sampleBuffer[(sampleOffset + i) * 2 + c],-1.f,1.f));
					}
					else
					{
						((float*)data)[(currentNumberOfSamples + i) * outputChannels + c] = m_sampleBuffer[(sampleOffset + i) * 2 + c];
					}
				}
			}
			// TODO: Mix to surround channels as well?
		}
		m_remainingSamples -= maxSamples;
		currentNumberOfSamples += maxSamples;
	}
}
void Audio_Impl::Start()
{
	limiter = new LimiterDSP(GetSampleRate());
	limiter->releaseTime = 0.2f;

	globalDSPs.Add(limiter);
	output->Start(this);
}
void Audio_Impl::Stop()
{
	output->Stop();
	globalDSPs.Remove(limiter);

	delete limiter;
	limiter = nullptr;
}
void Audio_Impl::Register(AudioBase* audio)
{
	if (audio)
	{
		lock.lock();
		itemsToRender.AddUnique(audio);
		audio->audio = this;
		lock.unlock();
	}
}
void Audio_Impl::Deregister(AudioBase* audio)
{
	lock.lock();
	itemsToRender.Remove(audio);
	audio->audio = nullptr;
	lock.unlock();
}
uint32 Audio_Impl::GetSampleRate() const
{
	return output->GetSampleRate();
}
double Audio_Impl::GetSecondsPerSample() const
{
	return 1.0 / (double)GetSampleRate();
}

void* Audio_Impl::GetSampleBuffer()
{
	return &m_sampleBuffer;
}

uint32 Audio_Impl::GetSampleBufferLength()
{
	return m_sampleBufferLength;
}

Audio::Audio()
{
	// Enforce single instance
	assert(g_audio == nullptr);
	g_audio = this;
}
Audio::~Audio()
{
	if (m_initialized)
	{
		g_impl.Stop();
		delete g_impl.output;
		g_impl.output = nullptr;
	}

	assert(g_audio == this);
	g_audio = nullptr;
}
bool Audio::Init(bool exclusive)
{
	audioLatency = 0;

	g_impl.output = new AudioOutput();
	if (!g_impl.output->Init(exclusive))
	{
		delete g_impl.output;
		g_impl.output = nullptr;
		return false;
	}

	g_impl.Start();

	return m_initialized = true;
}
void Audio::SetGlobalVolume(float vol)
{
	g_impl.globalVolume = vol;
}
uint32 Audio::GetSampleRate() const
{
	return g_impl.output->GetSampleRate();
}
class Audio_Impl* Audio::GetImpl()
{
	return &g_impl;
}

const float* Audio::GetSampleBuffer()
{
	return (float*) g_impl.GetSampleBuffer();
}

uint32 Audio::GetSampleBufferLength()
{
	return g_impl.GetSampleBufferLength();
}

void Audio::ProcessFFT(float* out_buckets, uint32 in_bucketAmount)
{
	const float* sampleBuffer = GetSampleBuffer();

	std::vector<std::complex<double>> samplesHann;

	for (int i = 0; i < 384; i++)
	{
		// assign left samples from sample buffer
		samplesHann.push_back(sampleBuffer[i*2]);

		//apply hann window
		samplesHann[i] = std::sin((M_PI*i)/384) * std::sin((M_PI*i)/384) * samplesHann[i];
	}

	std::vector<std::complex<double>> samplesFFT = FFT(samplesHann);

	int samplingFrequency = samplesFFT.size();
	int subset[] = { 0, 200, 400, 600, 800, 1000, 1250, 1500, 1750, 2000, 2500, 3000, 3500, 4000, 5000, 6000, 20000 };

	std::vector<double> bins(in_bucketAmount);
	double max_magnitude = 0.0;

	for (int j = 0; j < samplingFrequency / 2; j++) {

		// Calculates magnitude from freq bin
		double real = samplesFFT[j].real() * 2 / samplingFrequency;
		double imag = samplesFFT[j].imag() * 2 / samplingFrequency;
		double magnitude = sqrt(pow(real, 2) + pow(imag, 2));
		double freq = j * GetSampleRate() / samplingFrequency;

		for (int i = 0; i < in_bucketAmount; i++)
		{
			if (freq >= subset[i] && freq <= subset[i + 1] && magnitude > bins[i])
			{
				bins[i] = magnitude;
				if (magnitude > max_magnitude) max_magnitude = magnitude;
			}
		}
	}

	// scale output
	for (int i = 0; i < in_bucketAmount; i++)
	{
		double base = 0.5 * (bins[i] * 6.0); // bass
		double mag = 0.2 * (bins[i] * 0.5 / max_magnitude); // mid
		double lin = 0.7 * bins[i] * i; // high notes
		double exp = 0.6 * std::log(bins[i] * 1000.0f) * 0.125f;

		out_buckets[i] = (base + lin + mag + exp)*0.75;
	}
}

std::vector<std::complex<double>> Audio::FFT(std::vector<std::complex<double>>& samples)
{
	int size = samples.size();
	if (size <= 1) return samples;

	// split samples in even/odd
	int newSize = size / 2;
	std::vector<std::complex<double>> even(newSize, 0);
	std::vector<std::complex<double>> odd(newSize, 0);

	for (int i = 0; i < newSize; i++)
	{
		even[i] = samples[i * 2];
		odd[i] = samples[i * 2+1];
	}

	// apply FFT on 2 sub vectors
	std::vector<std::complex<double>> evenF(newSize, 0);
	evenF = FFT(even);
	std::vector<std::complex<double>> oddF(newSize, 0);
	oddF = FFT(odd);

	std::vector<std::complex<double>> result(size, 0);

	for (int i = 0; i < newSize; i++)
	{
		std::complex<double> w = (std::complex<double>) (std::polar(1.0, -2 * M_PI * i / size) * (std::complex<double>) oddF[i]);
		result[i] = evenF[i] + w;
		result[i + newSize] = evenF[i] - w;
	}

	return result;
}

Ref<AudioStream> Audio::CreateStream(const String& path, bool preload)
{
	return AudioStream::Create(this, path, preload);
}
Sample Audio::CreateSample(const String& path)
{
	return SampleRes::Create(this, path);
}

#if _DEBUG
void Audio_Impl::InitMemoryGuard()
{
	m_guard.fill(0);
}
void Audio_Impl::CheckMemoryGuard()
{
	for (auto x : m_guard)
		assert(x == 0);
}
#endif