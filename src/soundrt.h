#pragma once

#include "headers.h"
#include "fmgen/types.h"
#include "fmgen/opna.h"
#include "soundbuf.h"
#include "rtaudio/RtAudio.h"

class RtAudio;

// ---------------------------------------------------------------------------

namespace SoundDriver
{

	class Driver
	{
	public:
		Driver() {}
		virtual ~Driver() {}

		virtual bool Init(uint rate, uint ch, uint buflen) = 0;
		virtual bool Cleanup() = 0;
		void MixAlways(bool yes) { mixalways = yes; }

	protected:
		// uint buffersize;
		// uint sampleshift;
		// volatile bool playing;
		bool mixalways;
	};

	class SoundBlockGenerator
	{
	public:
		virtual ~SoundBlockGenerator() {}
		virtual void StartGenerateSound() = 0;
		virtual void EndGenerateSound() = 0;
		virtual uint GenerateSoundBlock(float **data) = 0;
	};


class DriverRT : public Driver
{
public:
	DriverRT();
	~DriverRT();

	bool Init(uint rate, uint ch, uint buflen);
	bool Cleanup();
	void SetGenerator(SoundBlockGenerator *g) { gen = g; }
	void SetReady() { ready = true; }

private:
	RtAudio *audio;
	SoundBlockGenerator *gen;
	struct Resampler;
	Resampler *src;
	double src_ratio;
	bool ready;
	static int AudioCallback(void *outbuf, void *, uint nframes, double, RtAudioStreamStatus, void *user);
	static long SrcCallback(void *cb_data, float **data);
};

}
