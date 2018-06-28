#ifndef INCLUDE_SPECTRUMVIS_H
#define INCLUDE_SPECTRUMVIS_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "export.h"
#include "util/message.h"
#include "util/movingaverage2d.h"

class GLSpectrum;
class MessageQueue;

class SDRGUI_API SpectrumVis : public BasebandSampleSink {

public:
	class MsgConfigureSpectrumVis : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		MsgConfigureSpectrumVis(int fftSize, int overlapPercent, FFTWindow::Function window) :
			Message(),
			m_fftSize(fftSize),
			m_overlapPercent(overlapPercent),
			m_window(window)
		{ }

		int getFFTSize() const { return m_fftSize; }
		int getOverlapPercent() const { return m_overlapPercent; }
		FFTWindow::Function getWindow() const { return m_window; }

	private:
		int m_fftSize;
		int m_overlapPercent;
		FFTWindow::Function m_window;
	};

	SpectrumVis(Real scalef, GLSpectrum* glSpectrum = 0);
	virtual ~SpectrumVis();

	void configure(MessageQueue* msgQueue, int fftSize, int overlapPercent, FFTWindow::Function window);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
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

	Real m_scalef;
	GLSpectrum* m_glSpectrum;
	MovingAverage2D<double> m_average;
	unsigned int m_averageNb;

	Real m_ofs;
	static const Real m_mult;

	QMutex m_mutex;

	void handleConfigure(int fftSize, int overlapPercent, FFTWindow::Function window);
};

#endif // INCLUDE_SPECTRUMVIS_H
