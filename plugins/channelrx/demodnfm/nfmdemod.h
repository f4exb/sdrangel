///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_NFMDEMOD_H
#define INCLUDE_NFMDEMOD_H

#include <vector>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "dsp/phasediscri.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/lowpass.h"
#include "dsp/bandpass.h"
#include "dsp/afsquelch.h"
#include "dsp/agc.h"
#include "dsp/ctcssdetector.h"
#include "audio/audiofifo.h"
#include "util/message.h"
#include "util/movingaverage.h"
#include "util/doublebufferfifo.h"

#include "nfmdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

class NFMDemod : public BasebandSampleSink, public ChannelAPI {
    Q_OBJECT
public:
    class MsgConfigureNFMDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const NFMDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureNFMDemod* create(const NFMDemodSettings& settings, bool force)
        {
            return new MsgConfigureNFMDemod(settings, force);
        }

    private:
        NFMDemodSettings m_settings;
        bool m_force;

        MsgConfigureNFMDemod(const NFMDemodSettings& settings, bool force) :
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

    class MsgReportCTCSSFreq : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getFrequency() const { return m_freq; }

        static MsgReportCTCSSFreq* create(Real freq)
        {
            return new MsgReportCTCSSFreq(freq);
        }

    private:
        Real m_freq;

        MsgReportCTCSSFreq(Real freq) :
            Message(),
            m_freq(freq)
        { }
    };

    NFMDemod(DeviceAPI *deviceAPI);
	~NFMDemod();
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

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

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

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const NFMDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            NFMDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

	const Real *getCtcssToneSet(int& nbTones) const {
		nbTones = m_ctcssDetector.getNTones();
		return m_ctcssDetector.getToneSet();
	}

	void setSelectedCtcssIndex(int selectedCtcssIndex) {
		m_ctcssIndexSelected = selectedCtcssIndex;
	}

	Real getMag() { return m_magsq; }
	bool getSquelchOpen() const { return m_squelchOpen; }

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

    uint32_t getNumberOfDeviceStreams() const;

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

    DeviceAPI* m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;

    int m_inputSampleRate;
    int m_inputFrequencyOffset;
	NFMDemodSettings m_settings;
	uint32_t m_audioSampleRate;
	float m_discriCompensation; //!< compensation factor that depends on audio rate (1 for 48 kS/s)
	bool m_running;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	Lowpass<Real> m_ctcssLowpass;
	Bandpass<Real> m_bandpass;
    Lowpass<Real> m_lowpass;
	CTCSSDetector m_ctcssDetector;
	int m_ctcssIndex; // 0 for nothing detected
	int m_ctcssIndexSelected;
	int m_sampleCount;
	int m_squelchCount;
	int m_squelchGate;

	Real m_squelchLevel;
	bool m_squelchOpen;
	bool m_afSquelchOpen;
	double m_magsq; //!< displayed averaged value
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;

	MovingAverageUtil<Real, double, 32> m_movingAverage;
	AFSquelch m_afSquelch;
	Real m_agcLevel; // AGC will aim to  this level
	DoubleBufferFIFO<Real> m_squelchDelayLine;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

	QMutex m_settingsMutex;

    PhaseDiscriminators m_phaseDiscri;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    static const int m_udpBlockSize;

//    void apply(bool force = false);
    void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
    void applySettings(const NFMDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NFMDemodSettings& settings, bool force);

    void processOneSample(Complex &ci);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_NFMDEMOD_H
