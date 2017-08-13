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
#include "udpsink.h"

MESSAGE_CLASS_DEFINITION(UDPSink::MsgUDPSinkConfigure, Message)

UDPSink::UDPSink(MessageQueue* uiMessageQueue, UDPSinkGUI* udpSinkGUI, BasebandSampleSink* spectrum) :
    m_uiMessageQueue(uiMessageQueue),
    m_udpSinkGUI(udpSinkGUI),
    m_spectrum(spectrum),
    m_settingsMutex(QMutex::Recursive)
{
}

UDPSink::~UDPSink()
{
}

void UDPSink::start()
{
}

void UDPSink::stop()
{
}

void UDPSink::pull(Sample& sample)
{
    sample.m_real = 0.0f;
    sample.m_imag = 0.0f;
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

        m_settingsMutex.unlock();

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
        int udpPort)
{
    Message* cmd = MsgUDPSinkConfigure::create(sampleFormat,
            outputSampleRate,
            rfBandwidth,
            fmDeviation,
            udpAddress,
            udpPort);
    messageQueue->push(cmd);
}

