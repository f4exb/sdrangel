///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "heatmapbaseband.h"

MESSAGE_CLASS_DEFINITION(HeatMapBaseband::MsgConfigureHeatMapBaseband, Message)

HeatMapBaseband::HeatMapBaseband() :
    m_running(false)
{
    qDebug("HeatMapBaseband::HeatMapBaseband");

    m_sink.setScopeSink(&m_scopeSink);
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);
}

HeatMapBaseband::~HeatMapBaseband()
{
    m_inputMessageQueue.clear();

    delete m_channelizer;
}

void HeatMapBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void HeatMapBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &HeatMapBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void HeatMapBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &HeatMapBaseband::handleData
    );
    m_running = false;
}

void HeatMapBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void HeatMapBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void HeatMapBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
        SampleVector::iterator part1begin;
        SampleVector::iterator part1end;
        SampleVector::iterator part2begin;
        SampleVector::iterator part2end;

        std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

        // first part of FIFO data
        if (part1begin != part1end) {
            m_channelizer->feed(part1begin, part1end);
        }

        // second part of FIFO data (used when block wraps around)
        if(part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void HeatMapBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool HeatMapBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureHeatMapBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureHeatMapBaseband& cfg = (MsgConfigureHeatMapBaseband&) cmd;
        qDebug() << "HeatMapBaseband::handleMessage: MsgConfigureHeatMapBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "HeatMapBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));

        return true;
    }
    else
    {
        return false;
    }
}

void HeatMapBaseband::applySettings(const HeatMapSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)
        || (settings.m_sampleRate != m_settings.m_sampleRate)
        || force)
    {
        m_channelizer->setChannelization(settings.m_sampleRate, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    m_sink.applySettings(settings, force);

    m_settings = settings;
}

void HeatMapBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}

