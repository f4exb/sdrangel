#ifndef INCLUDE_SPECTRUMVIS_H
#define INCLUDE_SPECTRUMVIS_H

#include <dsp/basebandsamplesink.h>
#include <QMutex>
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "export.h"
#include "util/message.h"
#include "util/movingaverage2d.h"
#include "util/fixedaverage2d.h"

class GLSpectrum;
class MessageQueue;

class SDRGUI_API SpectrumVis : public BasebandSampleSink {

public:
    enum AveragingMode
    {
        AvgModeNone,
        AvgModeMoving,
        AvgModeFixed
    };

	class MsgConfigureSpectrumVis : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		MsgConfigureSpectrumVis(
		        int fftSize,
		        int overlapPercent,
		        unsigned int averageNb,
		        int averagingMode,
		        FFTWindow::Function window,
		        bool linear) :
			Message(),
			m_fftSize(fftSize),
			m_overlapPercent(overlapPercent),
			m_averageNb(averageNb),
			m_window(window),
			m_linear(linear)
		{
		    m_averagingMode = averagingMode < 0 ? AvgModeNone : averagingMode > 2 ? AvgModeFixed : (SpectrumVis::AveragingMode) averagingMode;
		}

		int getFFTSize() const { return m_fftSize; }
		int getOverlapPercent() const { return m_overlapPercent; }
		unsigned int getAverageNb() const { return m_averageNb; }
		SpectrumVis::AveragingMode getAveragingMode() const { return m_averagingMode; }
		FFTWindow::Function getWindow() const { return m_window; }
		bool getLinear() const { return m_linear; }

	private:
		int m_fftSize;
		int m_overlapPercent;
		unsigned int m_averageNb;
		SpectrumVis::AveragingMode m_averagingMode;
		FFTWindow::Function m_window;
		bool m_linear;
	};

	SpectrumVis(Real scalef, GLSpectrum* glSpectrum = 0);
	virtual ~SpectrumVis();

	void configure(MessageQueue* msgQueue,
	        int fftSize,
	        int overlapPercent,
	        unsigned int averagingNb,
	        int averagingMode,
	        FFTWindow::Function window,
	        bool m_linear);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	FFTEngine* m_fft;
	FFTWindow m_window;

	std::vector<Complex> m_fftBuffer;
	std::vector<Real> m_powerSpectrum;

	std::size_t m_fftSize;
	std::size_t m_overlapPercent;
	std::size_t m_overlapSize;
	std::size_t m_refillSize;
	std::size_t m_fftBufferFill;
	bool m_needMoreSamples;

	Real m_scalef;
	GLSpectrum* m_glSpectrum;
	MovingAverage2D<double> m_movingAverage;
	FixedAverage2D<double> m_fixedAverage;
	unsigned int m_averageNb;
	AveragingMode m_averagingMode;
	bool m_linear;

	Real m_ofs;
	Real m_powFFTDiv;
	static const Real m_mult;

	QMutex m_mutex;

	void handleConfigure(int fftSize,
	        int overlapPercent,
	        unsigned int averageNb,
	        AveragingMode averagingMode,
	        FFTWindow::Function window,
	        bool linear);
};

#endif // INCLUDE_SPECTRUMVIS_H
