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
#include "dsp/glspectrumsettings.h"
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
    class MsgConfigureSpectrumVis : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const GLSpectrumSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSpectrumVis* create(const GLSpectrumSettings& settings, bool force)
        {
            return new MsgConfigureSpectrumVis(settings, force);
        }

    private:
        GLSpectrumSettings m_settings;
        bool m_force;

        MsgConfigureSpectrumVis(const GLSpectrumSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    enum AvgMode
    {
        AvgModeNone,
        AvgModeMovingAvg,
        AvgModeFixedAvg,
        AvgModeMax
    };

	SpectrumVis(Real scalef);
	virtual ~SpectrumVis();

    void setGLSpectrum(GLSpectrumInterface* glSpectrum) { m_glSpectrum = glSpectrum; }
    void openWSSpectrum();
    void closeWSSpectrum();

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
    void configureWSSpectrum(const QString& address, uint16_t port);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
    virtual void feed(const Complex *begin, unsigned int length); //!< direct FFT feed
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
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

    class MsgConfigureWSpectrumOpenClose : public Message
    {
		MESSAGE_CLASS_DECLARATION

	public:
        MsgConfigureWSpectrumOpenClose(bool openClose) :
            Message(),
            m_openClose(openClose)
        {}

        Real getOpenClose() const { return m_openClose; }

    private:
        bool m_openClose;
    };

    class MsgConfigureWSpectrum : public Message
    {
		MESSAGE_CLASS_DECLARATION

	public:
        MsgConfigureWSpectrum(const QString& address, uint16_t port) :
            Message(),
            m_address(address),
            m_port(port)
        {}

        const QString& getAddress() const { return m_address; }
        uint16_t getPort() const { return m_port; }

    private:
        QString m_address;
        uint16_t m_port;
    };

	FFTEngine* m_fft;
	FFTWindow m_window;
    unsigned int m_fftEngineSequence;

	std::vector<Complex> m_fftBuffer;
	std::vector<Real> m_powerSpectrum;

    GLSpectrumSettings m_settings;
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

    uint64_t m_centerFrequency;
    int m_sampleRate;

	Real m_ofs;
	Real m_powFFTDiv;
	static const Real m_mult;

	QMutex m_mutex;

    void applySettings(const GLSpectrumSettings& settings, bool force = false);
    void handleWSOpenClose(bool openClose);
    void handleConfigureDSP(uint64_t centerFrequency,
            int sampleRate);
    void handleScalef(Real scalef);
    void handleConfigureWSSpectrum(const QString& address, uint16_t port);
};

#endif // INCLUDE_SPECTRUMVIS_H
