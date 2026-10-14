///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "selcaldemodbaseband.h"
#include "dsp/dspengine.h"
#include "audio/audiodevicemanager.h"

MESSAGE_CLASS_DEFINITION(SELCALDemodBaseband::MsgConfigureSELCALDemodBaseband, Message)

SELCALDemodBaseband::SELCALDemodBaseband(SELCALDemod *selcalDemod) :
    m_sink(selcalDemod),
    m_running(false)
{
    qDebug("SELCALDemodBaseband::SELCALDemodBaseband");

    auto *audio = DSPEngine::instance()->getAudioDeviceManager();
    audio->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue());
    m_sink.applyAudioSampleRate(audio->getOutputSampleRate());
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);
}

SELCALDemodBaseband::~SELCALDemodBaseband()
{
    m_inputMessageQueue.clear();

    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sink.getAudioFifo());
    delete m_channelizer;
}

void SELCALDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
    m_sink.reset();
}

void SELCALDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(&m_sampleFifo, &SampleSinkFifo::dataReady, this, &SELCALDemodBaseband::handleData, Qt::QueuedConnection);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void SELCALDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(&m_sampleFifo, &SampleSinkFifo::dataReady, this, &SELCALDemodBaseband::handleData);
    m_running = false;
}

void SELCALDemodBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void SELCALDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void SELCALDemodBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0) && m_channelizer->getBasebandSampleRate())
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
        if (part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int)count);
    }
}

void SELCALDemodBaseband::handleInputMessages()
{
    Message *message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

bool SELCALDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureSELCALDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSELCALDemodBaseband& cfg = (MsgConfigureSELCALDemodBaseband&)cmd;
        qDebug() << "SELCALDemodBaseband::handleMessage: MsgConfigureSELCALDemodBaseband";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&)cmd;
        qDebug() << "SELCALDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        // Queued samples are discarded when the stream changes. The notification sent when the
        // channel starts repeats the current stream, and can be handled after the first samples
        // have been queued, so they are kept then.
        bool changed = (notif.getSampleRate() != m_channelizer->getBasebandSampleRate()) ||
                       (notif.getCenterFrequency() != m_centerFrequency);
        m_centerFrequency = notif.getCenterFrequency();
        m_sink.setCenterFrequency(notif.getCenterFrequency());
        setBasebandSampleRate(notif.getSampleRate());
        if (changed)
        {
            // A call in progress, the carrier measurement and learnt background
            // belong to the old stream.
            m_sink.reset();
            // We can run with very slow sample rate (E.g. 4k), but we don't want FIFO getting too small
            m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(std::max(notif.getSampleRate(), 48000)));
        }

        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        QMutexLocker lock(&m_mutex);
        const auto& audio = static_cast<const DSPConfigureAudio&>(cmd);
        m_sink.applyAudioSampleRate(audio.getSampleRate());
        return true;
    }
    else
    {
        return false;
    }
}

void SELCALDemodBaseband::applySettings(const QStringList& settingsKeys, const SELCALDemodSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) || force)
    {
        m_channelizer->setChannelization(SELCALDemodSettings::SELCALDEMOD_CHANNEL_SAMPLE_RATE, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    if ((settingsKeys.contains("audioDeviceName") && (settings.m_audioDeviceName != m_settings.m_audioDeviceName)) || force)
    {
        auto *audio = DSPEngine::instance()->getAudioDeviceManager();
        int index = audio->getOutputDeviceIndex(settings.m_audioDeviceName);
        audio->removeAudioSink(m_sink.getAudioFifo());
        audio->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue(), index);
        m_sink.applyAudioSampleRate(audio->getOutputSampleRate(index));
    }
    m_sink.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    }
    else
    {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int SELCALDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void SELCALDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}

