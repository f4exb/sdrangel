///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <QDateTime>
#include <QMutex>
#include <QRecursiveMutex>

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
#include "util/min2d.h"
#include "websockets/wsspectrum.h"

class GLSpectrumInterface;

namespace SWGSDRangel {
    class SWGGLSpectrum;
    class SWGGLSpectrumReport;
    class SWGSpectrumActions;
    class SWGGLSpectrumData;
    class SWGSpectrumServer;
    class SWGSuccessResponse;
};


// The latest value of whichever spectrum measurement is switched on. Which members mean anything
// depends on m_measurement, and with no measurement selected nothing is computed at all, so the
// report says so rather than returning zeros.
//
// Held as plain values behind a lock rather than pushed as a message: the measuring code runs on
// every displayed frame regardless of whether anyone is looking, so storing a handful of scalars
// costs nothing next to the FFT, and a report is only built when one is asked for.
struct SDRBASE_API SpectrumMeasurementResults
{
    struct Peak
    {
        qint64 m_frequency; //!< Hz
        float m_power;      //!< dB

        Peak() : m_frequency(0), m_power(0.0f) {}
        Peak(qint64 frequency, float power) : m_frequency(frequency), m_power(power) {}
    };

    SpectrumSettings::Measurement m_measurement;
    QList<Peak> m_peaks;
    float m_channelPower;            //!< dB
    float m_adjChannelPowerLeft;     //!< dB
    float m_adjChannelPowerLeftACPR; //!< dB
    float m_adjChannelPowerCentre;   //!< dB
    float m_adjChannelPowerRight;    //!< dB
    float m_adjChannelPowerRightACPR;//!< dB
    float m_occupiedBandwidth;       //!< Hz
    float m_bandwidth3dB;            //!< Hz
    float m_snr;                     //!< dB
    float m_snfr;                    //!< dB
    float m_thd;                     //!< dB
    float m_thdPlusNoise;            //!< dB
    float m_sinad;                   //!< dB
    float m_sfdr;                    //!< dB
    QDateTime m_updated;             //!< When these were last measured, invalid until the first one

    SpectrumMeasurementResults() :
        m_measurement(SpectrumSettings::MeasurementNone),
        m_channelPower(0.0f),
        m_adjChannelPowerLeft(0.0f),
        m_adjChannelPowerLeftACPR(0.0f),
        m_adjChannelPowerCentre(0.0f),
        m_adjChannelPowerRight(0.0f),
        m_adjChannelPowerRightACPR(0.0f),
        m_occupiedBandwidth(0.0f),
        m_bandwidth3dB(0.0f),
        m_snr(0.0f),
        m_snfr(0.0f),
        m_thd(0.0f),
        m_thdPlusNoise(0.0f),
        m_sinad(0.0f),
        m_sfdr(0.0f)
    {}
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


	SpectrumVis(Real scalef);
	virtual ~SpectrumVis();

    void setGLSpectrum(GLSpectrumInterface* glSpectrum) { m_glSpectrum = glSpectrum; }
    void setWorkspaceIndex(int index) { m_workspaceIndex = index; }
    int getWorkspaceIndex() const { return m_workspaceIndex; }

    void setScalef(Real scalef);
    void configureWSSpectrum(const QString& address, uint16_t port);
    const SpectrumSettings& getSettings() const { return m_settings; }
    Real getSpecMax() const { return m_specMax; }

    void setMathMemory(const QList<Real> &values);
    void getMathMovingAverageCopy(QList<Real>& copy);

	void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) override;
    void feed(const ComplexVector::const_iterator& begin, const ComplexVector::const_iterator& end, bool positiveOnly);
    void feed(const Complex *begin, unsigned int length) override; //!< feed output of FFT
	void feedTriggered(const SampleVector::const_iterator& triggerPoint, const SampleVector::const_iterator& end, bool positiveOnly);
	void start() override;
	void stop() override;
    void pushMessage(Message *msg) override;
    QString getSinkName() override;
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

    int webapiSpectrumSettingsGet(SWGSDRangel::SWGGLSpectrum& response, QString& errorMessage) const;
    int webapiSpectrumReportGet(SWGSDRangel::SWGGLSpectrumReport& response, QString& errorMessage) const;
    int webapiSpectrumDataGet(int bins, qint64 startFrequency, qint64 stopFrequency, const QString& reduce,
        SWGSDRangel::SWGGLSpectrumData& response, QString& errorMessage) const;

    static const int m_maxDataBins = 4096; //!< A reduced spectrum any larger is not a summary
    int webapiActionsPost(const QStringList& spectrumActionsKeys, SWGSDRangel::SWGSpectrumActions& query, QString& errorMessage);

    //!< Called by whatever measures the spectrum, once per set of results
    void setMeasurementResults(const SpectrumMeasurementResults& results);
    void getMeasurementResults(SpectrumMeasurementResults& results) const;
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
    std::vector<Real> m_mathMemory;

    SpectrumSettings m_settings;
	int m_overlapSize;
	int m_refillSize;
	int m_fftBufferFill;
	bool m_needMoreSamples;

	Real m_scalef;
	GLSpectrumInterface* m_glSpectrum;
    SpectrumMeasurementResults m_measurementResults;
    QDateTime m_powerSpectrumUpdated; //!< When the power spectrum was last computed
    mutable QMutex m_measurementResultsMutex; //!< Kept away from the DSP mutex: read from HTTP threads
    WSSpectrum m_wsSpectrum;
	MovingAverage2D<double> m_movingAverage;
	FixedAverage2D<double> m_fixedAverage;
	Max2D<Real> m_max;
    Min2D<Real> m_min;
    Real m_specMax;
    MovingAverage2D<double> m_mathMovingAverage;

    uint64_t m_centerFrequency;
    int m_sampleRate;

    Real m_powFFTMul;                   //!< 1/fftSize^2
	static const Real m_mult;           //!< 10/log2(10)

    MessageQueue m_inputMessageQueue;
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

	mutable QRecursiveMutex m_mutex; //!< mutable so a const read, such as the web API ones, can take it

    void performFFT(bool positiveOnly);
    void processFFT(const Complex* fftOut, bool reorder, bool positiveOnly, int fftSize);
    void setRunning(bool running) { m_running = running; }
    void applySettings(const SpectrumSettings& settings, bool force = false);
  	bool handleMessage(const Message& message);
    void handleConfigureDSP(uint64_t centerFrequency, int sampleRate);
    void handleScalef(Real scalef);
    void handleWSOpenClose(bool openClose);
    void handleConfigureWSSpectrum(const QString& address, uint16_t port);
    float log2fapprox(float x) const;
    void mathLinear(std::vector<Real> &spectrum);
    void mathDB(std::vector<Real> &spectrum);

    static void webapiFormatSpectrumSettings(SWGSDRangel::SWGGLSpectrum& response, const SpectrumSettings& settings);
    static void webapiUpdateSpectrumSettings(
            SpectrumSettings& settings,
            const QStringList& spectrumSettingsKeys,
            SWGSDRangel::SWGGLSpectrum& response);

private slots:
	void handleInputMessages();
};

#endif // INCLUDE_SPECTRUMVIS_H
