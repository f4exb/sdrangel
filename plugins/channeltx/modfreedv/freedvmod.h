///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODFREEDV_FREEDVMOD_H_
#define PLUGINS_CHANNELTX_MODFREEDV_FREEDVMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelsourceapi.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "util/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/fftfilt.h"
#include "dsp/cwkeyer.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "freedvmodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceSinkAPI;
class ThreadedBasebandSampleSource;
class UpChannelizer;
struct freedv;

class FreeDVMod : public BasebandSampleSource, public ChannelSourceAPI {
    Q_OBJECT

public:
    class MsgConfigureFreeDVMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreeDVModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreeDVMod* create(const FreeDVModSettings& settings, bool force)
        {
            return new MsgConfigureFreeDVMod(settings, force);
        }

    private:
        FreeDVModSettings m_settings;
        bool m_force;

        MsgConfigureFreeDVMod(const FreeDVModSettings& settings, bool force) :
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

    FreeDVMod(DeviceSinkAPI *deviceAPI);
    ~FreeDVMod();
    virtual void destroy() { delete this; }

    void setSpectrumSampleSink(BasebandSampleSink* sampleSink) { m_sampleSink = sampleSink; }

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
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

    uint32_t getAudioSampleRate() const { return m_audioSampleRate; }
    double getMagSq() const { return m_magsq; }
    Real getLowCutoff() const { return m_lowCutoff; }
    Real getHiCutoff() const { return m_hiCutoff; }

    CWKeyer *getCWKeyer() { return &m_cwKeyer; }

    static const QString m_channelIdURI;
    static const QString m_channelId;

signals:
	/**
	 * Level changed
	 * \param rmsLevel RMS level in range 0.0 - 1.0
	 * \param peakLevel Peak level in range 0.0 - 1.0
	 * \param numSamples Number of audio samples analyzed
	 */
	void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);


private:
    enum RateState {
        RSInitialFill,
        RSRunning
    };

    DeviceSinkAPI* m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;

    int m_basebandSampleRate;
    int m_outputSampleRate;
    int m_inputFrequencyOffset;
    Real m_lowCutoff;
    Real m_hiCutoff;
    FreeDVModSettings m_settings;
    quint32 m_audioSampleRate;

    NCOF m_carrierNco;
    NCOF m_toneNco;
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;
	fftfilt* m_SSBFilter;
	Complex* m_SSBFilterBuffer;
	int m_SSBFilterBufferIndex;
	static const int m_ssbFftLen;

	BasebandSampleSink* m_sampleSink;
	SampleVector m_sampleBuffer;

    fftfilt::cmplx m_sum;
    int m_undersampleCount;
    int m_sumCount;

    double m_magsq;
    MovingAverageUtil<double, double, 16> m_movingAverage;

    AudioVector m_audioBuffer;
    uint m_audioBufferFill;

    AudioFifo m_audioFifo;
    QMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize;     //!< raw file size (bytes)
    quint32 m_recordLength; //!< record length in seconds computed from file size
    int m_sampleRate;

    quint32 m_levelCalcCount;
    Real m_peakLevel;
    Real m_levelSum;
    CWKeyer m_cwKeyer;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    struct freedv *m_freeDV;
    int m_nSpeechSamples;
    int m_nNomModemSamples;
    int m_iSpeech;
    int m_iModem;
    int16_t *m_speechIn;
    int16_t *m_modOut;

    static const int m_levelNbSamples;

    void applyAudioSampleRate(int sampleRate);
    void applyChannelSettings(int basebandSampleRate, int outputSampleRate, int inputFrequencyOffset, bool force = false);
    void applySettings(const FreeDVModSettings& settings, bool force = false);
    void applyFreeDVMode(FreeDVModSettings::FreeDVMode mode);
    void pullAF(Complex& sample);
    void calculateLevel(Complex& sample);
    void modulateSample();
    void openFileStream();
    void seekFileStream(int seekPercentage);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreeDVModSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreeDVModSettings& settings, bool force);
    void webapiReverseSendCWSettings(const CWKeyerSettings& settings);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};


#endif /* PLUGINS_CHANNELTX_MODFREEDV_FREEDVMOD_H_ */
