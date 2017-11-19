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

#ifndef PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_
#define PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_

#include <QMutex>
#include <vector>
#include <iostream>
#include <fstream>

#include "dsp/basebandsamplesource.h"
#include "channel/channelsourceapi.h"
#include "dsp/nco.h"
#include "dsp/ncof.h"
#include "dsp/interpolator.h"
#include "dsp/fftfilt.h"
#include "dsp/movingaverage.h"
#include "dsp/agc.h"
#include "dsp/cwkeyer.h"
#include "audio/audiofifo.h"
#include "util/message.h"

#include "wfmmodsettings.h"

class DeviceSinkAPI;
class ThreadedBasebandSampleSource;
class UpChannelizer;

class WFMMod : public BasebandSampleSource, public ChannelSourceAPI {
    Q_OBJECT

public:
    typedef enum
    {
        WFMModInputNone,
        WFMModInputTone,
        WFMModInputFile,
        WFMModInputAudio,
        WFMModInputCWTone
    } WFMModInputAF;

    class MsgConfigureWFMMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const WFMModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureWFMMod* create(const WFMModSettings& settings, bool force)
        {
            return new MsgConfigureWFMMod(settings, force);
        }

    private:
        WFMModSettings m_settings;
        bool m_force;

        MsgConfigureWFMMod(const WFMModSettings& settings, bool force) :
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

    class MsgConfigureAFInput : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        WFMModInputAF getAFInput() const { return m_afInput; }

        static MsgConfigureAFInput* create(WFMModInputAF afInput)
        {
            return new MsgConfigureAFInput(afInput);
        }

    private:
        WFMModInputAF m_afInput;

        MsgConfigureAFInput(WFMModInputAF afInput) :
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

    WFMMod(DeviceSinkAPI *deviceAPI);
    ~WFMMod();

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual int getDeltaFrequency() const { return m_absoluteFrequencyOffset; }
    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }

    double getMagSq() const { return m_magsq; }

    CWKeyer *getCWKeyer() { return &m_cwKeyer; }

    static const QString m_channelID;

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

    WFMModSettings m_settings;
    int m_absoluteFrequencyOffset;

    NCO m_carrierNco;
    NCOF m_toneNco;
    NCOF m_toneNcoRF;
    float m_modPhasor; //!< baseband modulator phasor
    Complex m_modSample;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    fftfilt* m_rfFilter;
    static const int m_rfFilterFFTLength;
    fftfilt::cmplx *m_rfFilterBuffer;
    int m_rfFilterBufferIndex;

    double m_magsq;
    MovingAverage<double> m_movingAverage;
    SimpleAGC m_volumeAGC;

    AudioVector m_audioBuffer;
    uint m_audioBufferFill;

    AudioFifo m_audioFifo;
    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    std::ifstream m_ifstream;
    QString m_fileName;
    quint64 m_fileSize;     //!< raw file size (bytes)
    quint32 m_recordLength; //!< record length in seconds computed from file size
    int m_sampleRate;

    WFMModInputAF m_afInput;
    quint32 m_levelCalcCount;
    Real m_peakLevel;
    Real m_levelSum;
    CWKeyer m_cwKeyer;
    static const int m_levelNbSamples;

    void applySettings(const WFMModSettings& settings, bool force = false);
    void pullAF(Complex& sample);
    void calculateLevel(const Real& sample);
    void openFileStream();
    void seekFileStream(int seekPercentage);
};


#endif /* PLUGINS_CHANNELTX_MODWFM_WFMMOD_H_ */
