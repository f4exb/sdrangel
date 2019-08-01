///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FREEDVDEMOD_H
#define INCLUDE_FREEDVDEMOD_H

#include <vector>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/agc.h"
#include "audio/audiofifo.h"
#include "audio/audioresampler.h"
#include "util/message.h"
#include "util/doublebufferfifo.h"

#include "freedvdemodsettings.h"

#define ssbFftLen 1024
#define agcTarget 3276.8 // -10 dB amplitude => -20 dB power: center of normal signal

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class ThreadedBasebandSampleSink;
class DownChannelizer;

struct freedv;

class FreeDVDemod : public BasebandSampleSink, public ChannelAPI {
	Q_OBJECT
public:
    class MsgConfigureFreeDVDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreeDVDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreeDVDemod* create(const FreeDVDemodSettings& settings, bool force)
        {
            return new MsgConfigureFreeDVDemod(settings, force);
        }

    private:
        FreeDVDemodSettings m_settings;
        bool m_force;

        MsgConfigureFreeDVDemod(const FreeDVDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

	class MsgResyncFreeDVDemod : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
        static MsgResyncFreeDVDemod* create() {
            return new MsgResyncFreeDVDemod();
        }

	private:
		MsgResyncFreeDVDemod()
		{}
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

    FreeDVDemod(DeviceAPI *deviceAPI);
	virtual ~FreeDVDemod();
	virtual void destroy() { delete this; }
	void setSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

	void configure(MessageQueue* messageQueue,
			Real Bandwidth,
			Real LowCutoff,
			Real volume,
			int spanLog2,
			bool audioBinaural,
			bool audioFlipChannels,
			bool dsb,
			bool audioMute,
			bool agc,
			bool agcClamping,
			int agcTimeLog2,
			int agcPowerThreshold,
			int agcThresholdGate);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
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

    uint32_t getAudioSampleRate() const { return m_audioSampleRate; }
    uint32_t getModemSampleRate() const { return m_modemSampleRate; }
    double getMagSq() const { return m_magsq; }
	bool getAudioActive() const { return m_audioActive; }

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

	void getSNRLevels(double& avg, double& peak, int& nbSamples);
	int getBER() const { return m_freeDVStats.m_ber; }
	float getFrequencyOffset() const { return m_freeDVStats.m_freqOffset; }
	bool isSync() const { return m_freeDVStats.m_sync; }

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
        const FreeDVDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            FreeDVDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelInChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

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

	struct FreeDVStats
	{
		FreeDVStats();
		void init();
		void collect(struct freedv *freedv);

		bool m_sync;
		float m_snrEst;
		float m_clockOffset;
		float m_freqOffset;
		float m_syncMetric;
		int m_totalBitErrors;
		int m_lastTotalBitErrors;
		int m_ber; //!< estimated BER (b/s)
		uint32_t m_frameCount;
		uint32_t m_berFrameCount; //!< count of frames for BER estimation
		uint32_t m_fps; //!< frames per second
	};

	struct FreeDVSNR
	{
		FreeDVSNR();
		void accumulate(float snrdB);

		double m_sum;
		float m_peak;
		int m_n;
		bool m_reset;
	};

	struct LevelRMS
	{
		LevelRMS();
		void accumulate(float fsample);

		double m_sum;
		float m_peak;
		int m_n;
		bool m_reset;
	};

	class MsgConfigureFreeDVDemodPrivate : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		Real getBandwidth() const { return m_Bandwidth; }
		Real getLoCutoff() const { return m_LowCutoff; }
		Real getVolume() const { return m_volume; }
		int  getSpanLog2() const { return m_spanLog2; }
		bool getAudioBinaural() const { return m_audioBinaural; }
		bool getAudioFlipChannels() const { return m_audioFlipChannels; }
		bool getDSB() const { return m_dsb; }
		bool getAudioMute() const { return m_audioMute; }
		bool getAGC() const { return m_agc; }
		bool getAGCClamping() const { return m_agcClamping; }
		int  getAGCTimeLog2() const { return m_agcTimeLog2; }
		int  getAGCPowerThershold() const { return m_agcPowerThreshold; }
        int  getAGCThersholdGate() const { return m_agcThresholdGate; }

		static MsgConfigureFreeDVDemodPrivate* create(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
                bool agc,
                bool agcClamping,
                int  agcTimeLog2,
                int  agcPowerThreshold,
                int  agcThresholdGate)
		{
			return new MsgConfigureFreeDVDemodPrivate(
			        Bandwidth,
			        LowCutoff,
			        volume,
			        spanLog2,
			        audioBinaural,
			        audioFlipChannels,
			        dsb,
			        audioMute,
			        agc,
			        agcClamping,
			        agcTimeLog2,
			        agcPowerThreshold,
			        agcThresholdGate);
		}

	private:
		Real m_Bandwidth;
		Real m_LowCutoff;
		Real m_volume;
		int  m_spanLog2;
		bool m_audioBinaural;
		bool m_audioFlipChannels;
		bool m_dsb;
		bool m_audioMute;
		bool m_agc;
		bool m_agcClamping;
		int  m_agcTimeLog2;
		int  m_agcPowerThreshold;
		int  m_agcThresholdGate;

		MsgConfigureFreeDVDemodPrivate(Real Bandwidth,
				Real LowCutoff,
				Real volume,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
				bool agc,
				bool agcClamping,
				int  agcTimeLog2,
				int  agcPowerThreshold,
				int  agcThresholdGate) :
			Message(),
			m_Bandwidth(Bandwidth),
			m_LowCutoff(LowCutoff),
			m_volume(volume),
			m_spanLog2(spanLog2),
			m_audioBinaural(audioBinaural),
			m_audioFlipChannels(audioFlipChannels),
			m_dsb(dsb),
			m_audioMute(audioMute),
			m_agc(agc),
			m_agcClamping(agcClamping),
			m_agcTimeLog2(agcTimeLog2),
			m_agcPowerThreshold(agcPowerThreshold),
			m_agcThresholdGate(agcThresholdGate)
		{ }
	};

	DeviceAPI *m_deviceAPI;
    ThreadedBasebandSampleSink* m_threadedChannelizer;
    DownChannelizer* m_channelizer;
    FreeDVDemodSettings m_settings;

	Real m_hiCutoff;
	Real m_lowCutoff;
	Real m_volume;
	int m_spanLog2;
	fftfilt::cmplx m_sum;
	int m_undersampleCount;
	int m_inputSampleRate;
	uint32_t m_modemSampleRate;
	uint32_t m_speechSampleRate;
	uint32_t m_audioSampleRate;
	int m_inputFrequencyOffset;
	bool m_audioMute;
	double m_magsq;
	double m_magsqSum;
	double m_magsqPeak;
    int  m_magsqCount;
    MagSqLevelsStore m_magSqLevelStore;
	SimpleAGC<4800> m_simpleAGC;
    bool m_agcActive;
    DoubleBufferFIFO<fftfilt::cmplx> m_squelchDelayLine;
    bool m_audioActive;         //!< True if an audio signal is produced (no AGC or AGC and above threshold)

	NCOF m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
	fftfilt* SSBFilter;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;

	AudioVector m_audioBuffer;
	uint m_audioBufferFill;
	AudioFifo m_audioFifo;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    struct freedv *m_freeDV;
    int m_nSpeechSamples;
    int m_nMaxModemSamples;
    int m_nin;
    int m_iSpeech;
    int m_iModem;
    int16_t *m_speechOut;
    int16_t *m_modIn;
    AudioResampler m_audioResampler;
	FreeDVStats m_freeDVStats;
	FreeDVSNR m_freeDVSNR;
	LevelRMS m_levelIn;
	int m_levelInNbSamples;

	QMutex m_settingsMutex;

	void pushSampleToDV(int16_t sample);
	void pushSampleToAudio(int16_t sample);
	void applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force = false);
	void applySettings(const FreeDVDemodSettings& settings, bool force = false);
    void applyAudioSampleRate(int sampleRate);
	void applyFreeDVMode(FreeDVDemodSettings::FreeDVMode mode);
    void processOneSample(Complex &ci);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVDemodSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // INCLUDE_FREEDVDEMOD_H
