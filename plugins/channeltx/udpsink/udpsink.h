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

    void configure(MessageQueue* messageQueue,
            SampleFormat sampleFormat,
            Real inputSampleRate,
            Real rfBandwidth,
            int fmDeviation,
            QString& udpAddress,
            int udpPort,
            bool channelMute);
    void setSpectrum(MessageQueue* messageQueue, bool enabled);

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

        static MsgUDPSinkConfigure* create(SampleFormat
                sampleFormat,
                Real inputSampleRate,
                Real rfBandwidth,
                int fmDeviation,
                QString& udpAddress,
                int udpPort,
                bool channelMute)
        {
            return new MsgUDPSinkConfigure(sampleFormat,
                    inputSampleRate,
                    rfBandwidth,
                    fmDeviation,
                    udpAddress,
                    udpPort,
                    channelMute);
        }

    private:
        SampleFormat m_sampleFormat;
        Real m_inputSampleRate;
        Real m_rfBandwidth;
        int m_fmDeviation;
        QString m_udpAddress;
        int m_udpPort;
        bool m_channelMute;

        MsgUDPSinkConfigure(SampleFormat sampleFormat,
                Real inputSampleRate,
                Real rfBandwidth,
                int fmDeviation,
                QString& udpAddress,
                int udpPort,
                bool channelMute) :
            Message(),
            m_sampleFormat(sampleFormat),
            m_inputSampleRate(inputSampleRate),
            m_rfBandwidth(rfBandwidth),
            m_fmDeviation(fmDeviation),
            m_udpAddress(udpAddress),
            m_udpPort(udpPort),
            m_channelMute(channelMute)
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

    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;
    bool m_interpolatorConsumed;

    double m_magsq;
    MovingAverage<double> m_movingAverage;

    UDPSinkUDPHandler m_udpHandler;

    QMutex m_settingsMutex;

    void apply(bool force);
    void modulateSample();
};




#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSINK_H_ */
