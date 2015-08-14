#ifndef INCLUDE_SPECTRUMVIS_H
#define INCLUDE_SPECTRUMVIS_H

#include "dsp/samplesink.h"
#include "dsp/fftengine.h"
#include "fftwindow.h"
#include "util/export.h"

class GLSpectrum;
class MessageQueue;

class SDRANGELOVE_API SpectrumVis : public SampleSink {
public:
	SpectrumVis(GLSpectrum* glSpectrum = NULL);
	virtual ~SpectrumVis();

	void configure(MessageQueue* msgQueue, int fftSize, int overlapPercent, FFTWindow::Function window);

	virtual bool init(const Message& cmd);
	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void feedTriggered(SampleVector::const_iterator triggerPoint, SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	FFTEngine* m_fft;
	FFTWindow m_window;

	std::vector<Complex> m_fftBuffer;
	std::vector<Real> m_logPowerSpectrum;

	std::size_t m_fftSize;
	std::size_t m_overlapPercent;
	std::size_t m_overlapSize;
	std::size_t m_refillSize;
	std::size_t m_fftBufferFill;
	bool m_needMoreSamples;

	GLSpectrum* m_glSpectrum;

	void handleConfigure(int fftSize, int overlapPercent, FFTWindow::Function window);
};

#endif // INCLUDE_SPECTRUMVIS_H
