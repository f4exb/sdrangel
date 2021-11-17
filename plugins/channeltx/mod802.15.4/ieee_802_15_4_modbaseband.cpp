///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "dsp/upchannelizer.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "ieee_802_15_4_modbaseband.h"
#include "ieee_802_15_4_mod.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_ModBaseband::MsgConfigureIEEE_802_15_4_ModBaseband, Message)

IEEE_802_15_4_ModBaseband::IEEE_802_15_4_ModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);
    m_source.setScopeSink(&m_scopeSink);

    qDebug("IEEE_802_15_4_ModBaseband::IEEE_802_15_4_ModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &IEEE_802_15_4_ModBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

IEEE_802_15_4_ModBaseband::~IEEE_802_15_4_ModBaseband()
{
    delete m_channelizer;
}

void IEEE_802_15_4_ModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void IEEE_802_15_4_ModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
{
    unsigned int part1Begin, part1End, part2Begin, part2End;
    m_sampleFifo.read(nbSamples, part1Begin, part1End, part2Begin, part2End);
    SampleVector& data = m_sampleFifo.getData();

    if (part1Begin != part1End)
    {
        std::copy(
            data.begin() + part1Begin,
            data.begin() + part1End,
            begin
        );
    }

    unsigned int shift = part1End - part1Begin;

    if (part2Begin != part2End)
    {
        std::copy(
            data.begin() + part2Begin,
            data.begin() + part2End,
            begin + shift
        );
    }
}

void IEEE_802_15_4_ModBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    qreal rmsLevel, peakLevel;
    int numSamples;

    unsigned int remainder = m_sampleFifo.remainder();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleFifo.write(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleFifo.remainder();
    }

    m_source.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void IEEE_802_15_4_ModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void IEEE_802_15_4_ModBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool IEEE_802_15_4_ModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureIEEE_802_15_4_ModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureIEEE_802_15_4_ModBaseband& cfg = (MsgConfigureIEEE_802_15_4_ModBaseband&) cmd;
        qDebug() << "IEEE_802_15_4_ModBaseband::handleMessage: MsgConfigureIEEE_802_15_4_ModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (IEEE_802_15_4_Mod::MsgTxHexString::match(cmd))
    {
        IEEE_802_15_4_Mod::MsgTxHexString& tx = (IEEE_802_15_4_Mod::MsgTxHexString&) cmd;
        m_source.addTxFrame(tx.m_data);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "IEEE_802_15_4_ModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

        return true;
    }
    else
    {
        qDebug() << "IEEE_802_15_4_ModBaseband - Baseband got unknown message";
        return false;
    }
}

void IEEE_802_15_4_ModBaseband::applySettings(const IEEE_802_15_4_ModSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(m_channelizer->getChannelSampleRate(), settings.m_inputFrequencyOffset);
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    m_source.applySettings(settings, force);

    if ((settings.m_udpEnabled != m_settings.m_udpEnabled)
        || (settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort)
        || force)
    {
        qDebug() << "IEEE_802_15_4_ModBaseband::applySettings:"
            << " m_udpEnabled" << settings.m_udpEnabled
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort" << settings.m_udpPort;

        IEEE_802_15_4_ModSource::MsgCloseUDP *msg = IEEE_802_15_4_ModSource::MsgCloseUDP::create();
        m_source.getInputMessageQueue()->push(msg);

        if (settings.m_udpEnabled)
        {
            IEEE_802_15_4_ModSource::MsgOpenUDP *msg = IEEE_802_15_4_ModSource::MsgOpenUDP::create(settings.m_udpAddress, settings.m_udpPort);
            m_source.getInputMessageQueue()->push(msg);
        }
    }

    m_settings = settings;
}

int IEEE_802_15_4_ModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
