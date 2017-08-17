///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_

#include <QObject>

#include "dsp/basebandsamplesource.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/interpolator.h"
#include "dsp/movingaverage.h"
#include "dsp/nco.h"
#include "util/message.h"

#include "udpsinkudphandler.h"

class UDPSinkGUI;

class UDPSink : public BasebandSampleSource {
    Q_OBJECT

public:
    enum SampleFormat {
        FormatS16LE,
        FormatNFM,
        FormatNFMMono,
        FormatLSB,
        FormatUSB,
        FormatLSBMono,
        FormatUSBMono,
        FormatAMMono,
        FormatNone
    };

    UDPSink(MessageQueue* uiMessageQueue, UDPSinkGUI* udpSinkGUI, BasebandSampleSink* spectrum);
    virtual ~UDPSink();

    virtual void start();
    virtual void stop();
    virtual void pull(Sample& sample);
    virtual bool handleMessage(const Message& cmd);

    double getMagSq() const { return m_magsq; }
    double getInMagSq() const { return m_inMagsq; }
    int32_t getBufferGauge() const { return m_udpHandler.getBufferGauge(); }
    bool getSquelchOpen() const { return m_squelchOpen; }

    void configure(MessageQueue* messageQueue,
            SampleFormat sampleFormat,
            Real inputSampleRate,
            Real rfBandwidth,
            int fmDeviation,
            QString& udpAddress,
            int udpPort,
            bool channelMute,
            Real gain,
            Real squelchDB,
            bool force = false);
    void setSpectrum(MessageQueue* messageQueue, bool enabled);

signals:
    /**
     * Level changed
     * \param rmsLevel RMS level in range 0.0 - 1.0
     * \param peakLevel Peak level in range 0.0 - 1.0
     * \param numSamples Number of audio samples analyzed
     */
    void levelChanged(qreal rmsLevel, qreal peakLevel, int numSamples);

private:
    class MsgUDPSinkConfigure : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        SampleFormat getSampleFormat() const { return m_sampleFormat; }
        Real getInputSampleRate() const { return m_inputSampleRate; }
        Real getRFBandwidth() const { return m_rfBandwidth; }
        int getFMDeviation() const { return m_fmDeviation; }
        const QString& getUDPAddress() const { return m_udpAddress; }
        int getUDPPort() const { return m_udpPort; }
        bool getChannelMute() const { return m_channelMute; }
        Real getGain() const { return m_gain; }
        Real getSquelchDB() const { return m_squelchDB; }
        bool getForce() const { return m_force; }

        static MsgUDPSinkConfigure* create(SampleFormat
                sampleFormat,
                Real inputSampleRate,
                Real rfBandwidth,
                int fmDeviation,
                QString& udpAddress,
                int udpPort,
                bool channelMute,
                Real gain,
                Real squelchDB,
                bool force)
        {
            return new MsgUDPSinkConfigure(sampleFormat,
                    inputSampleRate,
                    rfBandwidth,
                    fmDeviation,
                    udpAddress,
                    udpPort,
                    channelMute,
                    gain,
                    squelchDB,
                    force);
        }

    private:
        SampleFormat m_sampleFormat;
        Real m_inputSampleRate;
        Real m_rfBandwidth;
        int m_fmDeviation;
        QString m_udpAddress;
        int m_udpPort;
        bool m_channelMute;
        Real m_gain;
        Real m_squelchDB;
        bool m_force;

        MsgUDPSinkConfigure(SampleFormat sampleFormat,
                Real inputSampleRate,
                Real rfBandwidth,
                int fmDeviation,
                QString& udpAddress,
                int udpPort,
                bool channelMute,
                Real gain,
                Real squelchDB,
                bool force) :
            Message(),
            m_sampleFormat(sampleFormat),
            m_inputSampleRate(inputSampleRate),
            m_rfBandwidth(rfBandwidth),
            m_fmDeviation(fmDeviation),
            m_udpAddress(udpAddress),
            m_udpPort(udpPort),
            m_channelMute(channelMute),
            m_gain(gain),
            m_squelchDB(squelchDB),
            m_force(force)
        { }
    };

    class MsgUDPSinkSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getEnabled() const { return m_enabled; }

        static MsgUDPSinkSpectrum* create(bool enabled)
        {
            return new MsgUDPSinkSpectrum(enabled);
        }

    private:
        bool m_enabled;

        MsgUDPSinkSpectrum(bool enabled) :
            Message(),
            m_enabled(enabled)
        { }
    };

    struct Config {
        int m_basebandSampleRate;
        Real m_outputSampleRate;
        int m_sampleFormat;
        Real m_inputSampleRate;
        qint64 m_inputFrequencyOffset;
        Real m_rfBandwidth;
        int m_fmDeviation;
        bool m_channelMute;
        Real m_gain;
        Real m_squelch; //!< squared magnitude

        QString m_udpAddressStr;
        quint16 m_udpPort;

        Config() :
            m_basebandSampleRate(48000),
            m_outputSampleRate(48000),
            m_sampleFormat(0),
            m_inputSampleRate(48000),
            m_inputFrequencyOffset(0),
            m_rfBandwidth(12500),
            m_fmDeviation(1.0),
            m_channelMute(false),
            m_gain(1.0),
            m_squelch(-50.0),
            m_udpAddressStr("127.0.0.1"),
            m_udpPort(9999)
        {}
    };

    Config m_config;
    Config m_running;

    NCO m_carrierNco;
    Complex m_modSample;

    MessageQueue* m_uiMessageQueue;
    UDPSinkGUI* m_udpSinkGUI;
    BasebandSampleSink* m_spectrum;
    bool m_spectrumEnabled;
    SampleVector m_sampleBuffer;
    int m_spectrumChunkSize;
    int m_spectrumChunkCounter;

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    double m_inMagsq;
    MovingAverage<double> m_movingAverage;
    MovingAverage<double> m_inMovingAverage;

    UDPSinkUDPHandler m_udpHandler;
    Real m_actualInputSampleRate; //!< sample rate with UDP buffer skew compensation
    double m_sampleRateSum;
    int m_sampleRateAvgCounter;

    int m_levelCalcCount;
    Real m_peakLevel;
    double m_levelSum;
    int m_levelNbSamples;

    bool m_squelchOpen;
    int  m_squelchOpenCount;
    int  m_squelchCloseCount;
    int m_squelchThreshold;

    QMutex m_settingsMutex;

    static const int m_sampleRateAverageItems = 17;

    void apply(bool force);
    void modulateSample();
    void calculateLevel(Real sample);
    void calculateLevel(Complex sample);

    inline void calculateSquelch(double value)
    {
        if (value > m_running.m_squelch)
        {
            if (m_squelchOpenCount < m_squelchThreshold) {
                m_squelchOpenCount++;
            } else {
                m_squelchCloseCount = m_squelchThreshold;
                m_squelchOpen = true;
            }
        }
        else
        {
            if (m_squelchCloseCount > 0) {
                m_squelchCloseCount--;
            } else {
                m_squelchOpenCount = 0;
                m_squelchOpen = false;
            }
        }
    }

    inline void initSquelch(bool open)
    {
        if (open)
        {
            m_squelchOpen = true;
            m_squelchOpenCount = m_squelchThreshold;
            m_squelchCloseCount = m_squelchThreshold;
        }
        else
        {
            m_squelchOpen = false;
            m_squelchOpenCount = 0;
            m_squelchCloseCount = 0;
        }
    }
};




#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_ */
