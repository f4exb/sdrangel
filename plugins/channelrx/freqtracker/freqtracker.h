///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_FREQTRACKER_H
#define INCLUDE_FREQTRACKER_H

#include <vector>

#include <QNetworkRequest>
#include <QMutex>

#include "dsp/basebandsamplesink.h"
#include "channel/channelsinkapi.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/bandpass.h"
#include "dsp/lowpass.h"
#include "dsp/phaselockcomplex.h"
#include "dsp/freqlockcomplex.h"
#include "util/message.h"
#include "util/doublebufferfifo.h"

#include "freqtrackersettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceSourceAPI;
class DownChannelizer;
class ThreadedBasebandSampleSink;
class QTimer;
class fftfilt;

class FreqTracker : public BasebandSampleSink, public ChannelSinkAPI {
	Q_OBJECT
public:
    class MsgConfigureFreqTracker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreqTrackerSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreqTracker* create(const FreqTrackerSettings& settings, bool force)
        {
            return new MsgConfigureFreqTracker(settings, force);
        }

    private:
        FreqTrackerSettings m_settings;
        bool m_force;

        MsgConfigureFreqTracker(const FreqTrackerSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgConfigureChannelizer : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        int getCenterFrequency() const { return m_centerFrequency; }

        static MsgConfigureChannelizer* create(int sampleRate, int centerFrequency)
        {
            return new MsgConfigureChannelizer(sampleRate, centerFrequency);
        }

    private:
        int m_sampleRate;
        int  m_centerFrequency;

        MsgConfigureChannelizer(int sampleRate, int centerFrequency) :
            Message(),
            m_sampleRate(sampleRate),
            m_centerFrequency(centerFrequency)
        { }
    };

    class MsgSampleRateNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSampleRateNotification* create(int sampleRate, int frequencyOffset) {
            return new MsgSampleRateNotification(sampleRate, frequencyOffset);
        }

        int getSampleRate() const { return m_sampleRate; }
        int getFrequencyOffset() const { return m_frequencyOffset; }

    private:
        MsgSampleRateNotification(int sampleRate, int frequencyOffset) :
            Message(),
            m_sampleRate(sampleRate),
            m_frequencyOffset(frequencyOffset)
        { }

        int m_sampleRate;
        int m_frequencyOffset;
    };

    FreqTracker(DeviceSourceAPI *deviceAPI);
	~FreqTracker();
	virtual void destroy() { delete this; }

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    uint32_t getSampleRate() const { return m_channelSampleRate; }
	double getMagSq() const { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }
	bool getPllLocked() const { return (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL) && m_pll.locked(); }
	Real getFrequency() const;
    Real getAvgDeltaFreq() const { return m_avgDeltaFreq; }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples)
    {
        if (m_magsqCount > 0)
        {
            m_magsq = m_magsqSum / m_magsqCount;
            m_magSqLevelStore.m_magsq = m_magsq;
            m_magSqLevelStore.m_magsqPeak = m_magsqPeak;
        }

        avg = m_magSqLevelStore.m_magsq;
        peak = m_magSqLevelStore.m_magsqPeak;
        nbSamples = m_magsqCount == 0 ? 1 : m_magsqCount;

        m_magsqSum = 0.0f;
        m_magsqPeak = 0.0f;
        m_magsqCount = 0;
    }

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    struct MagSqLevelsStore
    {
        MagSqLevelsStore() :
            m_magsq(1e-12),
            m_magsqPeak(1e-12)
        {}
        double m_magsq;
        double m_magsqPeak;
    };

	enum RateState {
		RSInitialFill,
		RSRunning
	};

	DeviceSourceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    FreqTrackerSettings m_settings;

    uint32_t m_deviceSampleRate;
    int m_inputSampleRate;
    int m_inputFrequencyOffset;
    uint32_t m_channelSampleRate;
    bool m_running;

	NCOF m_nco;
    PhaseLockComplex m_pll;
    FreqLockComplex m_fll;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;

	fftfilt* m_rrcFilter;

	Real m_squelchLevel;
	uint32_t m_squelchCount;
	bool m_squelchOpen;
    uint32_t m_squelchGate; //!< Squelch gate in samples
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
	int  m_magsqCount;
	MagSqLevelsStore m_magSqLevelStore;

	MovingAverageUtil<Real, double, 16> m_movingAverage;

    static const int m_udpBlockSize;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    const QTimer *m_timer;
    bool m_timerConnected;
    uint32_t m_tickCount;
    int m_lastCorrAbs;
    Real m_avgDeltaFreq;
	QMutex m_settingsMutex;

    void applySettings(const FreqTrackerSettings& settings, bool force = false);
    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
    void setInterpolator();
    void configureChannelizer();
    void connectTimer();
    void disconnectTimer();
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreqTrackerSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreqTrackerSettings& settings, bool force);

    void processOneSample(Complex &ci);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
	void tick();
};

#endif // INCLUDE_FREQTRACKER_H
