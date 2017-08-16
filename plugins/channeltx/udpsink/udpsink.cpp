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

#include <QDebug>

#include "dsp/upchannelizer.h"
#include "udpsinkmsg.h"
#include "udpsink.h"

MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkConfigure, Message)
MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkSpectrum, Message)

UDPSink::UDPSink(MessageQueue* uiMessageQueue, UDPSinkGUI* udpSinkGUI, BasebandSampleSink* spectrum) :
    m_uiMessageQueue(uiMessageQueue),
    m_udpSinkGUI(udpSinkGUI),
    m_spectrum(spectrum),
    m_magsq(1e-10),
    m_movingAverage(16, 0),
    m_settingsMutex(QMutex::Recursive)
{
    setObjectName("UDPSink");
    m_udpHandler.setFeedbackMessageQueue(&m_inputMessageQueue);
    apply(true);
}

UDPSink::~UDPSink()
{
}

void UDPSink::start()
{
    m_udpHandler.start();
}

void UDPSink::stop()
{
    m_udpHandler.stop();
}

void UDPSink::pull(Sample& sample)
{
    if (m_running.m_channelMute)
    {
        sample.m_real = 0.0f;
        sample.m_imag = 0.0f;
        return;
    }

    Complex ci;

    m_settingsMutex.lock();

    if (m_interpolatorDistance > 1.0f) // decimate
    {
        modulateSample();

        while (!m_interpolator.decimate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }
    else
    {
        if (m_interpolator.interpolate(&m_interpolatorDistanceRemain, m_modSample, &ci))
        {
            modulateSample();
        }
    }

    m_interpolatorDistanceRemain += m_interpolatorDistance;

    ci *= m_carrierNco.nextIQ(); // shift to carrier frequency

    m_settingsMutex.unlock();

    Real magsq = ci.real() * ci.real() + ci.imag() * ci.imag();
    magsq /= (1<<30);
    m_movingAverage.feed(magsq);
    m_magsq = m_movingAverage.average();

    sample.m_real = (FixReal) ci.real();
    sample.m_imag = (FixReal) ci.imag();
}

void UDPSink::modulateSample()
{
    //Real t;
    Sample s;

    if (m_running.m_sampleFormat == FormatS16LE)
    {
        m_udpHandler.readSample(s);
        m_modSample.real(s.m_real);
        m_modSample.imag(s.m_imag);
    }
    else
    {
        m_modSample.real(0.0f);
        m_modSample.imag(0.0f);
    }
}

bool UDPSink::handleMessage(const Message& cmd)
{
    qDebug() << "UDPSink::handleMessage";

    if (UpChannelizer::MsgChannelizerNotification::match(cmd))
    {
        UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;

        m_settingsMutex.lock();

        m_config.m_basebandSampleRate = notif.getBasebandSampleRate();
        m_config.m_outputSampleRate = notif.getSampleRate();
        m_config.m_inputFrequencyOffset = notif.getFrequencyOffset();

        m_settingsMutex.unlock();

        qDebug() << "UDPSink::handleMessage: MsgChannelizerNotification:"
                << " m_basebandSampleRate: " << m_config.m_basebandSampleRate
                << " m_outputSampleRate: " << m_config.m_outputSampleRate
                << " m_inputFrequencyOffset: " << m_config.m_inputFrequencyOffset;

        return true;
    }
    else if (MsgUDPSinkConfigure::match(cmd))
    {
        MsgUDPSinkConfigure& cfg = (MsgUDPSinkConfigure&) cmd;

        m_settingsMutex.lock();

        m_config.m_sampleFormat = cfg.getSampleFormat();
        m_config.m_inputSampleRate = cfg.getInputSampleRate();
        m_config.m_rfBandwidth = cfg.getRFBandwidth();
        m_config.m_fmDeviation = cfg.getFMDeviation();
        m_config.m_udpAddressStr = cfg.getUDPAddress();
        m_config.m_udpPort = cfg.getUDPPort();
        m_config.m_channelMute = cfg.getChannelMute();

        apply(cfg.getForce());

        m_settingsMutex.unlock();

        qDebug() << "UDPSink::handleMessage: MsgUDPSinkConfigure:"
                << " m_sampleFormat: " << m_config.m_sampleFormat
                << " m_inputSampleRate: " << m_config.m_inputSampleRate
                << " m_rfBandwidth: " << m_config.m_rfBandwidth
                << " m_fmDeviation: " << m_config.m_fmDeviation
                << " m_udpAddressStr: " << m_config.m_udpAddressStr
                << " m_udpPort: " << m_config.m_udpPort
                << " m_channelMute: " << m_config.m_channelMute;

        return true;
    }
    else if (UDPSinkMessages::MsgSampleRateCorrection::match(cmd))
    {
        // TODO
        return true;
    }
    else
    {
        return false;
    }
}

void UDPSink::configure(MessageQueue* messageQueue,
        SampleFormat sampleFormat,
        Real outputSampleRate,
        Real rfBandwidth,
        int fmDeviation,
        QString& udpAddress,
        int udpPort,
        bool channelMute,
        bool force)
{
    Message* cmd = MsgUDPSinkConfigure::create(sampleFormat,
            outputSampleRate,
            rfBandwidth,
            fmDeviation,
            udpAddress,
            udpPort,
            channelMute,
            force);
    messageQueue->push(cmd);
}

void UDPSink::setSpectrum(MessageQueue* messageQueue, bool enabled)
{
    Message* cmd = MsgUDPSinkSpectrum::create(enabled);
    messageQueue->push(cmd);
}


void UDPSink::apply(bool force)
{
    if ((m_config.m_inputFrequencyOffset != m_running.m_inputFrequencyOffset) ||
        (m_config.m_outputSampleRate != m_running.m_outputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_carrierNco.setFreq(m_config.m_inputFrequencyOffset, m_config.m_outputSampleRate);
        m_settingsMutex.unlock();
    }

    if((m_config.m_outputSampleRate != m_running.m_outputSampleRate) ||
       (m_config.m_rfBandwidth != m_running.m_rfBandwidth) ||
       (m_config.m_inputSampleRate != m_running.m_inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolatorDistanceRemain = 0;
        m_interpolatorConsumed = false;
        m_interpolatorDistance = (Real) m_config.m_inputSampleRate / (Real) m_config.m_outputSampleRate;
        m_interpolator.create(48, m_config.m_inputSampleRate, m_config.m_rfBandwidth / 2.2, 3.0);
        m_settingsMutex.unlock();
    }

    if ((m_config.m_udpAddressStr != m_running.m_udpAddressStr) ||
        (m_config.m_udpPort != m_running.m_udpPort) || force)
    {
        m_udpHandler.configureUDPLink(m_config.m_udpAddressStr, m_config.m_udpPort);
    }

    if ((m_config.m_channelMute != m_running.m_channelMute) || force)
    {
        if (!m_config.m_channelMute) {
            m_udpHandler.resetReadIndex();
        }
    }

    m_running = m_config;
}
