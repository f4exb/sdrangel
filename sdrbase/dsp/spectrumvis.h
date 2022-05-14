///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#include <QObject>
#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "dsp/spectrumsettings.h"
#include "export.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/movingaverage2d.h"
#include "util/fixedaverage2d.h"
#include "util/max2d.h"
#include "websockets/wsspectrum.h"

class GLSpectrumInterface;

namespace SWGSDRangel {
    class SWGGLSpectrum;
    class SWGSpectrumServer;
    class SWGSuccessResponse;
};

class SDRBASE_API SpectrumVis : public QObject, public BasebandSampleSink {
    Q_OBJECT
public:
    class SDRBASE_API MsgConfigureSpectrumVis : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SpectrumSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSpectrumVis* create(const SpectrumSettings& settings, bool force) {
            return new MsgConfigureSpectrumVis(settings, force);
        }

    private:
        SpectrumSettings m_settings;
        bool m_force;

        MsgConfigureSpectrumVis(const SpectrumSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class SDRBASE_API MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class SDRBASE_API MsgConfigureWSpectrumOpenClose : public Message
    {
		MESSAGE_CLASS_DECLARATION

	public:
        Real getOpenClose() const { return m_openClose; }

        static MsgConfigureWSpectrumOpenClose* create(bool openClose) {
            return new MsgConfigureWSpectrumOpenClose(openClose);
        }

    private:
        bool m_openClose;

        MsgConfigureWSpectrumOpenClose(bool openClose) :
            Message(),
            m_openClose(openClose)
        {}
    };

    class SDRBASE_API MsgFrequencyZooming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getFrequencyZoomFactor() const { return m_frequencyZoomFactor; }
        float getFrequencyZoomPos() const { return m_frequencyZoomPos; }

        static MsgFrequencyZooming* create(float frequencyZoomFactor, float frequencyZoomPos) {
            return new MsgFrequencyZooming(frequencyZoomFactor, frequencyZoomPos);
        }

    private:
        float m_frequencyZoomFactor;
        float m_frequencyZoomPos;

        MsgFrequencyZooming(float frequencyZoomFactor, float frequencyZoomPos) :
            Message(),
            m_frequencyZoomFactor(frequencyZoomFactor),
            m_frequencyZoomPos(frequencyZoomPos)
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
    void setWorkspaceIndex(int index) { m_workspaceIndex = index; }
    int getWorkspaceIndex() const { return m_workspaceIndex; }

    void setScalef(Real scalef);
    void configureWSSpectrum(const QString& address, uint16_t port);
    const SpectrumSettings& getSettings() const { return m_settings; }
    Real getSpecMax() const { return m_specMax / m_powFFTDiv; }
    void getPowerSpectrumCopy(std::vector<Real>& copy) { copy.assign(m_powerSpectrum.begin(), m_powerSpectrum.end()); }
    void getPSDCopy(std::vector<Real>& copy) const { copy.assign(m_psd.begin(), m_psd.begin() + m_settings.m_fftSize); }
    void getZoomedPSDCopy(std::vector<Real>& copy) const;

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
    void feed(const ComplexVector::const_iterator& begin, const ComplexVector::const_iterator& end, bool positiveOnly);
    virtual void feed(const Complex *begin, unsigned int length); //!< direct FFT feed
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
    virtual void pushMessage(Message *msg);
    virtual QString getSinkName();
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

    int webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const;
    int webapiSpectrumSettingsPutPatch(
            bool force,
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response, // query + response
            QString& errorMessage);
    int webapiSpectrumServerGet(SWGSDRangel::SWGSpectrumServer& response, QString& errorMessage) const;
    int webapiSpectrumServerPost(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage);
    int webapiSpectrumServerDelete(SWGSDRangel::SWGSuccessResponse& response, QString& errorMessage);

private:
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

    bool m_running;
	FFTEngine* m_fft;
	FFTWindow m_window;
    unsigned int m_fftEngineSequence;
    int m_workspaceIndex;

	std::vector<Complex> m_fftBuffer;
	std::vector<Real> m_powerSpectrum; //!< displayable power spectrum
    std::vector<Real> m_psd; //!< real PSD

    SpectrumSettings m_settings;
	int m_overlapSize;
	int m_refillSize;
	int m_fftBufferFill;
	bool m_needMoreSamples;

    float m_frequencyZoomFactor;
    float m_frequencyZoomPos;

	Real m_scalef;
	GLSpectrumInterface* m_glSpectrum;
    WSSpectrum m_wsSpectrum;
	MovingAverage2D<double> m_movingAverage;
	FixedAverage2D<double> m_fixedAverage;
	Max2D<double> m_max;
    Real m_specMax;

    uint64_t m_centerFrequency;
    int m_sampleRate;

	Real m_ofs;
	Real m_powFFTDiv;
	static const Real m_mult;

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

	QMutex m_mutex;

    void processFFT(bool positiveOnly);
    void setRunning(bool running) { m_running = running; }
    void applySettings(const SpectrumSettings& settings, bool force = false);
  	bool handleMessage(const Message& message);
    void handleConfigureDSP(uint64_t centerFrequency, int sampleRate);
    void handleScalef(Real scalef);
    void handleWSOpenClose(bool openClose);
    void handleConfigureWSSpectrum(const QString& address, uint16_t port);

    static void webapiFormatSpectrumSettings(SWGSDRangel::SWGGLSpectrum& response, const SpectrumSettings& settings);
    static void webapiUpdateSpectrumSettings(
            SpectrumSettings& settings,
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response);

private slots:
	void handleInputMessages();
};

#endif // INCLUDE_SPECTRUMVIS_H
