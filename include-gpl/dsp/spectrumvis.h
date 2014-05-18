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
	~SpectrumVis();

	void configure(MessageQueue* msgQueue, int fftSize, int overlapPercent, FFTWindow::Function window);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst);
	void start();
	void stop();
	bool handleMessage(Message* message);

private:
	FFTEngine* m_fft;
	FFTWindow m_window;

	std::vector<Complex> m_fftBuffer;
	std::vector<Real> m_logPowerSpectrum;

	size_t m_fftSize;
	size_t m_overlapPercent;
	size_t m_overlapSize;
	size_t m_refillSize;
	size_t m_fftBufferFill;

	GLSpectrum* m_glSpectrum;

	void handleConfigure(int fftSize, int overlapPercent, FFTWindow::Function window);
};

#endif // INCLUDE_SPECTRUMVIS_H
