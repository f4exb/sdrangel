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
#include "util/max2d.h"

class GLSpectrum;
class MessageQueue;

class SDRGUI_API SpectrumVis : public BasebandSampleSink {

public:
    enum AvgMode
    {
        AvgModeNone,
        AvgModeMovingAvg,
        AvgModeFixedAvg,
        AvgModeMax
    };

	class MsgConfigureSpectrumVis : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		MsgConfigureSpectrumVis(
		        int fftSize,
		        int overlapPercent,
		        unsigned int averageNb,
		        AvgMode avgMode,
		        FFTWindow::Function window,
		        bool linear) :
			Message(),
			m_fftSize(fftSize),
			m_overlapPercent(overlapPercent),
			m_averageNb(averageNb),
            m_avgMode(avgMode),
			m_window(window),
			m_linear(linear)
		{}

		int getFFTSize() const { return m_fftSize; }
		int getOverlapPercent() const { return m_overlapPercent; }
		unsigned int getAverageNb() const { return m_averageNb; }
		SpectrumVis::AvgMode getAvgMode() const { return m_avgMode; }
		FFTWindow::Function getWindow() const { return m_window; }
		bool getLinear() const { return m_linear; }

	private:
		int m_fftSize;
		int m_overlapPercent;
		unsigned int m_averageNb;
		SpectrumVis::AvgMode m_avgMode;
		FFTWindow::Function m_window;
		bool m_linear;
	};

    class MsgConfigureScalingFactor : public Message
    {
		MESSAGE_CLASS_DECLARATION

	public:
        MsgConfigureScalingFactor(Real scalef) :
            Message(),
            m_scalef(scalef)
        {}

        Real getScalef() const { return m_scalef; }

    private:
        Real m_scalef;
    };

	SpectrumVis(Real scalef, GLSpectrum* glSpectrum = 0);
	virtual ~SpectrumVis();

	void configure(MessageQueue* msgQueue,
	        int fftSize,
	        int overlapPercent,
	        unsigned int averagingNb,
	        AvgMode averagingMode,
	        FFTWindow::Function window,
	        bool m_linear);
    void setScalef(MessageQueue* msgQueue, Real scalef);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	FFTEngine* m_fft;
	FFTWindow m_window;
    unsigned int m_fftEngineSequence;

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
	Max2D<double> m_max;
	unsigned int m_averageNb;
	AvgMode m_avgMode;
	bool m_linear;

	Real m_ofs;
	Real m_powFFTDiv;
	static const Real m_mult;

	QMutex m_mutex;

	void handleConfigure(int fftSize,
	        int overlapPercent,
	        unsigned int averageNb,
	        AvgMode averagingMode,
	        FFTWindow::Function window,
	        bool linear);
    void handleScalef(Real scalef);
};

#endif // INCLUDE_SPECTRUMVIS_H
