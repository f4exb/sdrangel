///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
//                                                                               //
// Symbol synchronizer or symbol clock recovery mostly encapsulating             //
// liquid-dsp's symsync "object"                                                 //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_SPECTRUMVIS_H
#define INCLUDE_SPECTRUMVIS_H

#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "export.h"
#include "util/message.h"
#include "util/movingaverage2d.h"
#include "util/fixedaverage2d.h"
#include "util/max2d.h"
#include "websockets/wsspectrum.h"

class GLSpectrumInterface;
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
                float refLevel,
                float powerRange,
		        int overlapPercent,
		        unsigned int averageNb,
		        AvgMode avgMode,
		        FFTWindow::Function window,
		        bool linear) :
			Message(),
			m_fftSize(fftSize),
			m_overlapPercent(overlapPercent),
            m_refLevel(refLevel),
            m_powerRange(powerRange),
			m_averageNb(averageNb),
            m_avgMode(avgMode),
			m_window(window),
			m_linear(linear)
		{}

		int getFFTSize() const { return m_fftSize; }
        float getRefLevel() const { return m_refLevel; }
        float getPowerRange() const { return m_powerRange; }
		int getOverlapPercent() const { return m_overlapPercent; }
		unsigned int getAverageNb() const { return m_averageNb; }
		SpectrumVis::AvgMode getAvgMode() const { return m_avgMode; }
		FFTWindow::Function getWindow() const { return m_window; }
		bool getLinear() const { return m_linear; }

	private:
		int m_fftSize;
		int m_overlapPercent;
        float m_refLevel;
        float m_powerRange;
		unsigned int m_averageNb;
		SpectrumVis::AvgMode m_avgMode;
		FFTWindow::Function m_window;
		bool m_linear;
	};

    class MsgConfigureDSP : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        MsgConfigureDSP(uint64_t centerFrequency, int sampleRate) :
            Message(),
            m_centerFrequency(centerFrequency),
            m_sampleRate(sampleRate)
        {}

        uint64_t getCenterFrequency() const { return m_centerFrequency; }
        int getSampleRate() const { return m_sampleRate; }

    private:
        uint64_t m_centerFrequency;
        int m_sampleRate;
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

	SpectrumVis(Real scalef, GLSpectrumInterface* glSpectrum = nullptr);
	virtual ~SpectrumVis();

	void configure(MessageQueue* msgQueue,
	        int fftSize,
            float refLevel,
            float powerRange,
	        int overlapPercent,
	        unsigned int averagingNb,
	        AvgMode averagingMode,
	        FFTWindow::Function window,
	        bool m_linear);
    void configureDSP(uint64_t centerFrequency, int sampleRate);
    void setScalef(Real scalef);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
    virtual void feed(const Complex *begin, unsigned int length); //!< direct FFT feed
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
    float m_refLevel;
    float m_powerRange;
	std::size_t m_overlapPercent;
	std::size_t m_overlapSize;
	std::size_t m_refillSize;
	std::size_t m_fftBufferFill;
	bool m_needMoreSamples;

	Real m_scalef;
	GLSpectrumInterface* m_glSpectrum;
    WSSpectrum m_wsSpectrum;
	MovingAverage2D<double> m_movingAverage;
	FixedAverage2D<double> m_fixedAverage;
	Max2D<double> m_max;
	unsigned int m_averageNb;
	AvgMode m_avgMode;
	bool m_linear;

    uint64_t m_centerFrequency;
    int m_sampleRate;

	Real m_ofs;
	Real m_powFFTDiv;
	static const Real m_mult;

	QMutex m_mutex;

	void handleConfigure(int fftSize,
            float refLevel,
            float powerRange,
	        int overlapPercent,
	        unsigned int averageNb,
	        AvgMode averagingMode,
	        FFTWindow::Function window,
	        bool linear);
    void handleConfigureDSP(uint64_t centerFrequency,
            int sampleRate);
    void handleScalef(Real scalef);
};

#endif // INCLUDE_SPECTRUMVIS_H
