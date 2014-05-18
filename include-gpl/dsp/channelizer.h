#ifndef INCLUDE_CHANNELIZER_H
#define INCLUDE_CHANNELIZER_H

#include <list>
#include "dsp/samplesink.h"
#include "util/export.h"

class MessageQueue;
class IntHalfbandFilter;

class SDRANGELOVE_API Channelizer : public SampleSink {
public:
	Channelizer(SampleSink* sampleSink);
	~Channelizer();

	void configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* cmd);

protected:
	struct FilterStage {
		enum Mode {
			ModeCenter,
			ModeLowerHalf,
			ModeUpperHalf
		};

		typedef bool (IntHalfbandFilter::*WorkFunction)(Sample* s);
		IntHalfbandFilter* m_filter;
		WorkFunction m_workFunction;

		FilterStage(Mode mode);
		~FilterStage();

		bool work(Sample* sample)
		{
			return (m_filter->*m_workFunction)(sample);
		}
	};
	typedef std::list<FilterStage*> FilterStages;
	FilterStages m_filterStages;
	SampleSink* m_sampleSink;
	int m_inputSampleRate;
	int m_requestedOutputSampleRate;
	int m_requestedCenterFrequency;
	int m_currentOutputSampleRate;
	int m_currentCenterFrequency;
	SampleVector m_sampleBuffer;

	void applyConfiguration();
	bool signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const;
	Real createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd);
	void freeFilterChain();
};

#endif // INCLUDE_CHANNELIZER_H

#if 0

#ifndef INCLUDE_CHANNELIZER_H
#define INCLUDE_CHANNELIZER_H

#include "samplesink.h"
#include "spectrum.h"
#include "nco.h"
#include "interpolator.h"
#include "pidcontroller.h"
#include "hardware/audiofifo.h"

class AudioOutput;

class Channelizer : public SampleSink {
public:
	Channelizer();
	~Channelizer();

#if 0
	void setGLSpectrum(GLSpectrum* glSpectrum);
#endif

	size_t workUnitSize();
	size_t work(SampleVector::const_iterator begin, SampleVector::const_iterator end);

private:
#if 0
	NCO m_nco;
	Interpolator m_interpolator;
	Real m_distance;
	Interpolator m_interpolator2;
	Real m_distance2;

	SampleVector m_buffer;
	size_t m_bufferFill;
	Complex m_lastSample;

	AudioOutput* m_audioOutput;
	AudioFifo m_audioFifo;
	Real m_resampler;
	PIDController m_resamplerCtrl;

	Spectrum m_spectrum;
#endif
};

#endif // INCLUDE_CHANNELIZER_H
#endif
