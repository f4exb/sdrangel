///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "acarsdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(AcarsDemodBaseband::MsgConfigureAcarsDemodBaseband, Message)

// Up to 16x the standard policy (10 s of nominal samples): a burst's decode chain
// (multiple Viterbi attempts, turbo re-estimation, header repair) runs synchronously at
// the end of the burst and can take tens of milliseconds - and clusters of bursts
// compound it - which at accelerated file replay (10x and beyond) would overflow the
// 0.64 s standard FIFO. But capped in samples, since the FIFO is allocated in full: at
// 16x an 8 MS/s Iridium channel took 655 MB for its FIFO alone, and several channels on
// a wideband device took gigabytes. The cap is 8M samples (64 MB with 24 bit samples), still over a
// second at 8 MS/s.
static unsigned int fifoSize(unsigned int sampleRate)
{
    const unsigned int standard = SampleSinkFifo::getSizePolicy(sampleRate);
    const unsigned int maxSamples = 8 * 1024 * 1024;
    return std::max(standard, std::min(standard * 16, maxSamples));
}

AcarsDemodBaseband::AcarsDemodBaseband(AcarsDemod *packetDemod) :
    m_sink(packetDemod),
    m_running(false)
{
    qDebug("AcarsDemodBaseband::AcarsDemodBaseband");

    m_sink.setScopeSink(&m_scopeSink);
    m_sampleFifo.setSize(fifoSize(48000));
    m_channelizer = new DownChannelizer(&m_sink);
}

AcarsDemodBaseband::~AcarsDemodBaseband()
{
    m_inputMessageQueue.clear();

    delete m_channelizer;
}

void AcarsDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void AcarsDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &AcarsDemodBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void AcarsDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &AcarsDemodBaseband::handleData
    );
    m_running = false;
}

void AcarsDemodBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void AcarsDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void AcarsDemodBaseband::handleData()
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

void AcarsDemodBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool AcarsDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureAcarsDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAcarsDemodBaseband& cfg = (MsgConfigureAcarsDemodBaseband&) cmd;
        qDebug() << "AcarsDemodBaseband::handleMessage: MsgConfigureAcarsDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "AcarsDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(fifoSize(notif.getSampleRate()));

        return true;
    }
    else
    {
        return false;
    }
}

void AcarsDemodBaseband::applySettings(const AcarsDemodSettings& settings, bool force)
{
    bool modeChanged = AcarsDemodSettings::rateChanged(settings, m_settings);

    // The sink has to know the mode before the channelization moves, as the mode sets
    // the rate it resamples the channelizer output to
    m_sink.applySettings(settings, force);

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || modeChanged || force)
    {
        m_channelizer->setChannelization(AcarsDemodSink::channelSampleRate(settings), settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset(), modeChanged || force);
    }

    m_settings = settings;
}

void AcarsDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}
