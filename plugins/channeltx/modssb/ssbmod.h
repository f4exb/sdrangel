///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_
#define PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_

#include <QMutex>
#include <vector>
#include <iostream>
#include <fstream>

#include "dsp/basebandsamplesource.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/fftfilt.h"
#include "dsp/cwkeyer.h"
#include "audio/audiofifo.h"
#include "util/message.h"

class SSBMod : public BasebandSampleSource {
    Q_OBJECT

public:
    typedef enum
    {
        SSBModInputNone,
        SSBModInputTone,
        SSBModInputFile,
        SSBModInputAudio,
        SSBModInputCWTone
    } SSBModInputAF;

    class MsgConfigureFileSourceName : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgConfigureFileSourceName* create(const QString& fileName)
        {
            return new MsgConfigureFileSourceName(fileName);
        }

    private:
        QString m_fileName;

        MsgConfigureFileSourceName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgConfigureFileSourceSeek : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentage() const { return m_seekPercentage; }

        static MsgConfigureFileSourceSeek* create(int seekPercentage)
        {
            return new MsgConfigureFileSourceSeek(seekPercentage);
        }

    protected:
        int m_seekPercentage; //!< percentage of seek position from the beginning 0..100

        MsgConfigureFileSourceSeek(int seekPercentage) :
            Message(),
            m_seekPercentage(seekPercentage)
        { }
    };

    class MsgConfigureFileSourceStreamTiming : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgConfigureFileSourceStreamTiming* create()
        {
            return new MsgConfigureFileSourceStreamTiming();
        }

    private:

        MsgConfigureFileSourceStreamTiming() :
            Message()
        { }
    };

    class MsgConfigureAFInput : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        SSBModInputAF getAFInput() const { return m_afInput; }

        static MsgConfigureAFInput* create(SSBModInputAF afInput)
        {
            return new MsgConfigureAFInput(afInput);
        }

    private:
        SSBModInputAF m_afInput;

        MsgConfigureAFInput(SSBModInputAF afInput) :
            Message(),
            m_afInput(afInput)
        { }
    };

    class MsgReportFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        std::size_t getSamplesCount() const { return m_samplesCount; }

        static MsgReportFileSourceStreamTiming* create(std::size_t samplesCount)
        {
            return new MsgReportFileSourceStreamTiming(samplesCount);
        }

    protected:
        std::size_t m_samplesCount;

        MsgReportFileSourceStreamTiming(std::size_t samplesCount) :
            Message(),
            m_samplesCount(samplesCount)
        { }
    };

    class MsgReportFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSampleRate() const { return m_sampleRate; }
        quint32 getRecordLength() const { return m_recordLength; }

        static MsgReportFileSourceStreamData* create(int sampleRate,
                quint32 recordLength)
        {
            return new MsgReportFileSourceStreamData(sampleRate, recordLength);
        }

    protected:
        int m_sampleRate;
        quint32 m_recordLength;

        MsgReportFileSourceStreamData(int sampleRate,
                quint32 recordLength) :
            Message(),
            m_sampleRate(sampleRate),
            m_recordLength(recordLength)
        { }
    };

    //=================================================================

    SSBMod(BasebandSampleSink* sampleSink);
    ~SSBMod();

    void configure(MessageQueue* messageQueue,
            Real bandwidth,
			Real lowCutoff,
            float toneFrequency,
			float volumeFactor,
			int spanLog2,
			bool audioBinaural,
			bool audioFlipChannels,
			bool dsb,
			bool audioMute,
            bool playLoop,
            bool agc,
            float agcOrder,
            int agcTime,
            int agcThreshold,
            int agcThresholdGate,
            int agcThresholdDelay);

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    double getMagSq() const { return m_magsq; }

    CWKeyer *getCWKeyer() { return &m_cwKeyer; }

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);


private:
    class MsgConfigureSSBMod : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        Real getBandwidth() const { return m_bandwidth; }
        Real getLowCutoff() const { return m_lowCutoff; }
        float getToneFrequency() const { return m_toneFrequency; }
        float getVolumeFactor() const { return m_volumeFactor; }
        int getSpanLog2() const { return m_spanLog2; }
        bool getAudioBinaural() const { return m_audioBinaural; }
        bool getAudioFlipChannels() const { return m_audioFlipChannels; }
        bool getDSB() const { return m_dsb; }
        bool getAudioMute() const { return m_audioMute; }
        bool getPlayLoop() const { return m_playLoop; }
        bool getAGC() const { return m_agc; }
        float getAGCOrder() const { return m_agcOrder; }
        int getAGCTime() const { return m_agcTime; }
        int getAGCThreshold() const { return m_agcThreshold; }
        int getAGCThresholdGate() const { return m_agcThresholdGate; }
        int getAGCThresholdDelay() const { return m_agcThresholdDelay; }

        static MsgConfigureSSBMod* create(Real bandwidth,
        		Real lowCutoff,
        		float toneFrequency,
				float volumeFactor,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
				bool playLoop,
				bool agc,
				float agcOrder,
				int agcTime,
                int agcThreshold,
                int agcThresholdGate,
                int agcThresholdDelay)
        {
            return new MsgConfigureSSBMod(bandwidth,
            		lowCutoff,
					toneFrequency,
					volumeFactor,
					spanLog2,
					audioBinaural,
					audioFlipChannels,
					dsb,
					audioMute,
					playLoop,
					agc,
					agcOrder,
					agcTime,
					agcThreshold,
					agcThresholdGate,
					agcThresholdDelay);
        }

    private:
        Real m_bandwidth;
        Real m_lowCutoff;
        float m_toneFrequency;
        float m_volumeFactor;
		int  m_spanLog2;
		bool m_audioBinaural;
		bool m_audioFlipChannels;
		bool m_dsb;
        bool m_audioMute;
        bool m_playLoop;
        bool m_agc;
        float m_agcOrder;
        int m_agcTime;
        int m_agcThreshold;
        int m_agcThresholdGate;
        int m_agcThresholdDelay;

        MsgConfigureSSBMod(Real bandwidth,
        		Real lowCutoff,
        		float toneFrequency,
				float volumeFactor,
				int spanLog2,
				bool audioBinaural,
				bool audioFlipChannels,
				bool dsb,
				bool audioMute,
				bool playLoop,
				bool agc,
				float agcOrder,
				int agcTime,
				int agcThreshold,
				int agcThresholdGate,
				int agcThresholdDelay) :
            Message(),
            m_bandwidth(bandwidth),
			m_lowCutoff(lowCutoff),
            m_toneFrequency(toneFrequency),
            m_volumeFactor(volumeFactor),
			m_spanLog2(spanLog2),
			m_audioBinaural(audioBinaural),
			m_audioFlipChannels(audioFlipChannels),
			m_dsb(dsb),
            m_audioMute(audioMute),
			m_playLoop(playLoop),
			m_agc(agc),
			m_agcOrder(agcOrder),
			m_agcTime(agcTime),
			m_agcThreshold(agcThreshold),
			m_agcThresholdGate(agcThresholdGate),
			m_agcThresholdDelay(agcThresholdDelay)
        { }
    };

    //=================================================================

    struct AudioSample {
        qint16 l;
        qint16 r;
    };
    typedef std::vector<AudioSample> AudioVector;

    enum RateState {
        RSInitialFill,
        RSRunning
    };

    struct Config {
        int m_outputSampleRate;
        qint64 m_inputFrequencyOffset;
        Real m_bandwidth;
        Real m_lowCutoff;
        bool m_usb;
        float m_toneFrequency;
        float m_volumeFactor;
        quint32 m_audioSampleRate;
		int  m_spanLog2;
		bool m_audioBinaural;
		bool m_audioFlipChannels;
		bool m_dsb;
		bool m_audioMute;
        bool m_playLoop;
        bool m_agc;
        float m_agcOrder;
        int m_agcTime;
        bool m_agcThresholdEnable;
        double m_agcThreshold;
        int m_agcThresholdGate;
        int m_agcThresholdDelay;

        Config() :
            m_outputSampleRate(0),
            m_inputFrequencyOffset(0),
            m_bandwidth(3000.0f),
			m_lowCutoff(300.0f),
			m_usb(true),
            m_toneFrequency(1000.0f),
            m_volumeFactor(1.0f),
            m_audioSampleRate(0),
			m_spanLog2(3),
			m_audioBinaural(false),
			m_audioFlipChannels(false),
			m_dsb(false),
            m_audioMute(false),
			m_playLoop(false),
			m_agc(false),
			m_agcOrder(0.2),
			m_agcTime(9600),
			m_agcThresholdEnable(true),
			m_agcThreshold(1e-4),
			m_agcThresholdGate(192),
			m_agcThresholdDelay(2400)
        { }
    };

    //=================================================================

    Config m_config;
    Config m_running;

    NCOF m_carrierNco;
    NCOF m_toneNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;
	fftfilt* m_SSBFilter;
	fftfilt* m_DSBFilter;
	Complex* m_SSBFilterBuffer;
	Complex* m_DSBFilterBuffer;
	int m_SSBFilterBufferIndex;
	int m_DSBFilterBufferIndex;
	static const int m_ssbFftLen;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;

//    Real m_magsqSpectrum;
//    Real m_magsqSum;
//    Real m_magsqPeak;
//    int  m_magsqCount;
    fftfilt::cmplx m_sum;
    int m_undersampleCount;
    int m_sumCount;

    double m_magsq;
    MovingAverage<double> m_movingAverage;

    AudioVector m_audioBuffer;
    uint m_audioBufferFill;

    AudioFifo m_audioFifo;
    QMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize;     //!< raw file size (bytes)
    quint32 m_recordLength; //!< record length in seconds computed from file size
    int m_sampleRate;

    SSBModInputAF m_afInput;
    quint32 m_levelCalcCount;
    Real m_peakLevel;
    Real m_levelSum;
    CWKeyer m_cwKeyer;
    CWSmoother m_cwSmoother;

    MagAGC m_inAGC;

    static const int m_levelNbSamples;

    void apply();
    void pullAF(Complex& sample);
    void calculateLevel(Complex& sample);
    void modulateSample();
    void openFileStream();
    void seekFileStream(int seekPercentage);
};


#endif /* PLUGINS_CHANNELTX_MODSSB_SSBMOD_H_ */
