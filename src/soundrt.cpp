#include "soundrt.h"
#include <samplerate.h>
#include <math.h>

namespace SoundDriver
{

static const double streamLatency = 20e-3;

DriverRT::DriverRT()
{
	audio = NULL;
	gen = NULL;
	ready = false;
}

DriverRT::~DriverRT()
{
	Cleanup();
}

bool DriverRT::Init(uint rate, uint ch, uint buflen)
{
	Cleanup();

	RtAudio *audio = this->audio = new RtAudio;

	RtAudio::StreamParameters param;
	param.deviceId = audio->getDefaultOutputDevice();
	param.nChannels = ch;

	RtAudio::DeviceInfo info = audio->getDeviceInfo(param.deviceId);
	uint deviceRate = info.preferredSampleRate;

	RtAudio::StreamOptions opts;
	opts.flags = RTAUDIO_ALSA_USE_DEFAULT|RTAUDIO_JACK_DONT_CONNECT;
	opts.streamName = "mucom88";

	uint bufferframes = (uint)ceil(deviceRate * streamLatency);
	audio->openStream(
		&param, NULL, RTAUDIO_FLOAT32, deviceRate, &bufferframes, &AudioCallback, this, &opts);

	fprintf(stderr, "Audio Driver %s  %u Hz  %f ms\n",
			RtAudio::getApiDisplayName(audio->getCurrentApi()).c_str(),
			deviceRate, (double)bufferframes / deviceRate * 1e3);

	int srcerror;
	src_ratio = (double)deviceRate / rate;
	src = (Resampler *)src_callback_new(SrcCallback, SRC_SINC_MEDIUM_QUALITY, 2, &srcerror, this);

	if (!src)
	{
		delete audio;
		this->audio = NULL;
		fprintf(stderr, "resampler: %s\n", src_strerror(srcerror));
		return false;
	}

	audio->startStream();
	return true;
}

bool DriverRT::Cleanup()
{
	ready = false;

	delete audio;
	audio = NULL;

	src_delete((SRC_STATE *)src);
	src = NULL;

	return true;
}

int DriverRT::AudioCallback(void *outbuf, void *, uint nframes, double, RtAudioStreamStatus, void *user)
{
	DriverRT *self = (DriverRT *)user;
	if (!self->ready) {
		memset(outbuf, 0, 2 * nframes * sizeof(float));
		return 0;
	}

	SoundBlockGenerator *gen = self->gen;
	SRC_STATE *src = (SRC_STATE *)self->src;
	double src_ratio = self->src_ratio;
	float *out = (float *)outbuf;

	gen->StartGenerateSound();
	while (nframes > 0) {
		uint count = (uint)src_callback_read(src, src_ratio, nframes, out);
		nframes -= count;
		out += 2 * count;
	}
	gen->EndGenerateSound();

	return 0;
}

long DriverRT::SrcCallback(void *cb_data, float **data)
{
	DriverRT *self = (DriverRT *)cb_data;
	SoundBlockGenerator *gen = self->gen;
	return (long)gen->GenerateSoundBlock(data);
}

}
