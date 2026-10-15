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

#include <algorithm>

#include <QDebug>
#include <QMutexLocker>

#include "audio/audiodevicemanager.h"
#include "dsp/downchannelizer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"

#include "drmdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(DRMDemodBaseband::MsgConfigureDRMDemodBaseband, Message)
MESSAGE_CLASS_DEFINITION(DRMDemodBaseband::MsgResetDRMDemodBaseband, Message)

DRMDemodBaseband::DRMDemodBaseband() :
    m_channelizer(new DownChannelizer(&m_sink)),
    m_running(false)
{
    m_channelSampleRate = m_channelizer->getChannelSampleRate();
    m_sampleFifo.setSize(fifoSize(DRMDemodSettings::ChannelSampleRate));
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue());
    m_sink.applyAudioSampleRate(audioDeviceManager->getOutputSampleRate());
}

DRMDemodBaseband::~DRMDemodBaseband()
{
    m_inputMessageQueue.clear();
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sink.getAudioFifo());
    delete m_channelizer;
}

unsigned int DRMDemodBaseband::fifoSize(int sampleRate)
{
    // Decoding runs on the thread that drains the FIFO, so a slow step (a cold
    // FAC search) fills it. Hold up to two seconds, within a memory bound; any
    // remaining overflow is bridged as a gap rather than resetting the receiver.
    const unsigned int policy = SampleSinkFifo::getSizePolicy(static_cast<unsigned int>(std::max(sampleRate, 0)));
    const unsigned int twoSeconds = static_cast<unsigned int>(std::min<qint64>(2LL * std::max(sampleRate, 0), 4LL * 1024 * 1024));
    return std::max(policy, twoSeconds);
}

void DRMDemodBaseband::reset()
{
    QMutexLocker locker(&m_mutex);
    // Preserve pending rate/settings messages queued during device startup.
    invalidateInput(false);
}

void DRMDemodBaseband::invalidateInput(bool retune, bool keepQueuedAudio)
{
    {
        QMutexLocker streamLocker(&m_streamMutex);
        m_sampleFifo.reset();
        m_writtenSamples = m_readSamples = 0;
        m_streamBoundaries.clear();
        m_inputGaps.clear();
    }

    resetDecoder(retune, keepQueuedAudio);
}

void DRMDemodBaseband::bridgeGap(quint64 basebandSamples)
{
    // handleData bridges only gaps the FIFO could have held (about 2 s); a
    // longer one restarts from the newest input instead.
    if (m_channelizer->getBasebandSampleRate() <= 0)
    {
        resetDecoder(false);
        return;
    }

    // Zeros keep every later sample at its true position, so tracking sees
    // an erasure (FAC failures, zero-reliability MSC cells) instead of
    // concatenated pre-gap and post-gap IQ.
    const SampleVector zeros(static_cast<std::size_t>(std::min<quint64>(basebandSamples, 16384U)));
    for (quint64 remaining = basebandSamples; remaining > 0 && !m_stopping;)
    {
        const std::size_t count = static_cast<std::size_t>(std::min<quint64>(remaining, zeros.size()));
        m_channelizer->feed(zeros.cbegin(), zeros.cbegin() + static_cast<std::ptrdiff_t>(count));
        remaining -= count;
    }
}

void DRMDemodBaseband::resetDecoder(bool retune, bool keepQueuedAudio)
{
    const int rate = m_channelizer->getBasebandSampleRate();
    delete m_channelizer;
    m_channelizer = new DownChannelizer(&m_sink);
    m_channelizer->setBasebandSampleRate(rate);
    m_channelizer->setChannelization(DRMDemodSettings::ChannelSampleRate, m_settings.m_inputFrequencyOffset);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    m_channelSampleRate = m_channelizer->getChannelSampleRate();
    m_sink.reset(retune, keepQueuedAudio);
}

void DRMDemodBaseband::startWork()
{
    m_stopping = false;
    QMutexLocker locker(&m_mutex);
    connect(&m_sampleFifo, &SampleSinkFifo::dataReady, this, &DRMDemodBaseband::handleData, Qt::QueuedConnection);
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &DRMDemodBaseband::handleInputMessages);
    m_sink.start();
    m_running = true;
}

void DRMDemodBaseband::stopWork()
{
    // The worker holds the mutex while it drains the FIFO; ask it to stop
    // first, or a decoder slower than its input would never let go.
    m_stopping = true;
    m_sink.requestAbort();
    QMutexLocker locker(&m_mutex);
    disconnect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &DRMDemodBaseband::handleInputMessages);
    disconnect(&m_sampleFifo, &SampleSinkFifo::dataReady, this, &DRMDemodBaseband::handleData);
    m_sink.stop();
    m_running = false;
}

void DRMDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    QMutexLocker streamLocker(&m_streamMutex);
    const quint64 count = static_cast<quint64>(end - begin);
    const quint64 written = m_sampleFifo.write(begin, end);
    if (written < count)
    {
        // The FIFO drops the tail of this write: record where it belonged.
        // Later drops before any new write or boundary extend the same gap.
        const quint64 position = m_writtenSamples + written;
        const quint64 dropped = count - written;
        if (!m_inputGaps.empty() && m_inputGaps.back().sample == position
            && m_inputGaps.back().sequence == m_streamSequence) {
            m_inputGaps.back().length += dropped;
        } else {
            m_inputGaps.push_back({position, dropped, ++m_streamSequence});
        }

        m_inputDroppedSamples.fetch_add(dropped);
    }

    m_writtenSamples += written;
}

void DRMDemodBaseband::markStreamBoundary(bool endOfStream)
{
    {
        QMutexLocker streamLocker(&m_streamMutex);
        m_streamBoundaries.push_back({m_writtenSamples, endOfStream, ++m_streamSequence});
    }

    QMetaObject::invokeMethod(this, [this]() { handleData(); }, Qt::QueuedConnection);
}

void DRMDemodBaseband::scheduleHandleData()
{
    QMetaObject::invokeMethod(this, [this]() { handleData(); }, Qt::QueuedConnection);
}

void DRMDemodBaseband::handleData()
{
    QMutexLocker locker(&m_mutex);
    // Decoding runs inside the channelizer feed, so bound each pass to about
    // 100 ms of input and continue from the event loop: stop and settings
    // messages are then handled between passes even when behind.
    const quint64 passBudget = std::max<quint64>(16384U,
        static_cast<quint64>(std::max(m_channelizer->getBasebandSampleRate(), 0)) / 10U);
    quint64 passSamples = 0;
    while (m_inputMessageQueue.size() == 0 && !m_stopping)
    {
        if (passSamples >= passBudget)
        {
            scheduleHandleData();
            break;
        }

        bool boundary = false, endOfStream = false, endOfStreamQueued = false;
        quint64 gap = 0;
        unsigned int available;
        {
            QMutexLocker streamLocker(&m_streamMutex);
            available = m_sampleFifo.fill();
            const bool gapHere = !m_inputGaps.empty() && m_inputGaps.front().sample == m_readSamples;
            const bool boundaryHere = !m_streamBoundaries.empty() && m_streamBoundaries.front().sample == m_readSamples;
            if (!m_inputGaps.empty()) {
                available = static_cast<unsigned int>(std::min<quint64>(available, m_inputGaps.front().sample - m_readSamples));
            }

            if (!m_streamBoundaries.empty()) {
                available = static_cast<unsigned int>(std::min<quint64>(available, m_streamBoundaries.front().sample - m_readSamples));
            }

            // At one position, whichever was recorded first: drops before a
            // boundary belong to the stream it ends, drops after it to the next.
            if (gapHere && (!boundaryHere || m_inputGaps.front().sequence < m_streamBoundaries.front().sequence))
            {
                gap = m_inputGaps.front().length; // removed once bridged
                endOfStreamQueued = std::any_of(m_streamBoundaries.begin(), m_streamBoundaries.end(),
                    [](const StreamBoundary& queued) { return queued.endOfStream; });
            }
            else if (boundaryHere)
            {
                boundary = true;
                endOfStream = m_streamBoundaries.front().endOfStream;
                m_streamBoundaries.pop_front();
            }
        }

        if (gap != 0)
        {
            // A gap the FIFO could not have held means decoding is too slow
            // for the input: replaying it as zeros would cost as much as real
            // input and the next gap would be longer. Start again from the
            // newest input instead, draining first when the discarded span
            // holds an EOF (keeping the audio that drain queues).
            if (gap > m_sampleFifo.size())
            {
                if (endOfStreamQueued) {
                    m_sink.flush();
                }

                invalidateInput(false, endOfStreamQueued);
                m_drainedAtEOF = false;
                continue;
            }

            // Bridge within this pass's budget; the rest waits for the next.
            const quint64 chunk = std::min<quint64>(gap, std::max<quint64>(1U, passBudget - passSamples));
            bridgeGap(chunk);
            {
                QMutexLocker streamLocker(&m_streamMutex);
                if (!m_inputGaps.empty())
                {
                    InputGap& front = m_inputGaps.front();
                    front.length -= std::min(front.length, chunk);
                    if (front.length == 0) {
                        m_inputGaps.pop_front();
                    }
                }
            }

            passSamples += chunk;
            m_drainedAtEOF = false;
            continue;
        }

        if (boundary)
        {
            // File Input's loop is an EOF then a seek at the same position:
            // keep the audio the drain queued rather than clear it at once.
            if (endOfStream) {
                m_sink.flush();
            } else {
                resetDecoder(false, m_drainedAtEOF);
            }

            m_drainedAtEOF = endOfStream;
            continue;
        }

        if (available == 0) {
            break;
        }

        available = static_cast<unsigned int>(std::min<quint64>(available, passBudget - passSamples));
        SampleVector::iterator part1Begin;
        SampleVector::iterator part1End;
        SampleVector::iterator part2Begin;
        SampleVector::iterator part2End;
        const std::size_t count = m_sampleFifo.readBegin(
            available, &part1Begin, &part1End, &part2Begin, &part2End
        );
        if (part1Begin != part1End) {
            m_channelizer->feed(part1Begin, part1End);
        }

        if (part2Begin != part2End) {
            m_channelizer->feed(part2Begin, part2End);
        }

        {
            QMutexLocker streamLocker(&m_streamMutex);
            m_sampleFifo.readCommit(static_cast<unsigned int>(count));
            m_readSamples += count;
        }

        passSamples += count;
        if (count > 0) {
            m_drainedAtEOF = false;
        }
    }
}

void DRMDemodBaseband::handleInputMessages()
{
    Message *message = nullptr;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug() << "DRMDemodBaseband: unhandled message" << message->getIdentifier();
        }

        delete message;
    }

    handleData(); // A boundary can be waiting behind a settings message.
}

bool DRMDemodBaseband::handleMessage(const Message& message)
{
    if (MsgResetDRMDemodBaseband::match(message))
    {
        invalidateInput(true);
        return true;
    }

    if (MsgConfigureDRMDemodBaseband::match(message))
    {
        const auto& configure = static_cast<const MsgConfigureDRMDemodBaseband&>(message);
        applySettings(configure.getSettings(), configure.getSettingsKeys(), configure.getForce());
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        const auto& notification = static_cast<const DSPSignalNotification&>(message);

        // Devices also notify when settings are resent unchanged: only a new
        // rate or centre frequency restarts reception
        if ((notification.getSampleRate() == m_notifiedSampleRate)
            && (notification.getCenterFrequency() == m_notifiedCenterFrequency)) {
            return true;
        }

        m_notifiedSampleRate = notification.getSampleRate();
        m_notifiedCenterFrequency = notification.getCenterFrequency();
        // Resizing discards queued samples, so discard markers for that span too.
        {
            QMutexLocker streamLocker(&m_streamMutex);
            m_sampleFifo.setSize(fifoSize(notification.getSampleRate()));
            m_writtenSamples = m_readSamples = 0;
            m_streamBoundaries.clear();
            m_inputGaps.clear();
        }

        setBasebandSampleRate(notification.getSampleRate());
        return true;
    }

    if (DSPConfigureAudio::match(message))
    {
        const auto& configure = static_cast<const DSPConfigureAudio&>(message);
        if (configure.getAudioType() == DSPConfigureAudio::AudioOutput) {
            m_sink.applyAudioSampleRate(configure.getSampleRate());
        }

        return true;
    }

    return false;
}

void DRMDemodBaseband::applySettings(const DRMDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    DRMDemodSettings sanitized = settings;
    sanitized.sanitize();
    if (force || settingsKeys.contains("inputFrequencyOffset"))
    {
        m_channelizer->setChannelization(DRMDemodSettings::ChannelSampleRate, sanitized.m_inputFrequencyOffset);
        // A new offset is a retune even when the channelizer's rate and
        // residual come out the same (offsets a filter-chain step apart).
        m_sink.applyChannelSettings(
            m_channelizer->getChannelSampleRate(),
            m_channelizer->getChannelFrequencyOffset(),
            force || sanitized.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset
        );
        m_channelSampleRate = m_channelizer->getChannelSampleRate();
    }

    if (force || (settingsKeys.contains("audioDeviceName")
        && sanitized.m_audioDeviceName != m_settings.m_audioDeviceName))
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        const int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(
            sanitized.m_audioDeviceName
        );
        audioDeviceManager->removeAudioSink(m_sink.getAudioFifo());
        audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        m_sink.applyAudioSampleRate(
            audioDeviceManager->getOutputSampleRate(audioDeviceIndex)
        );
    }

    m_sink.applySettings(sanitized, settingsKeys, force);
    if (force) {
        m_settings = sanitized;
    } else {
        m_settings.applySettings(settingsKeys, sanitized);
    }
}

void DRMDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_channelizer->setChannelization(DRMDemodSettings::ChannelSampleRate, m_settings.m_inputFrequencyOffset);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset(), true);
    m_channelSampleRate = m_channelizer->getChannelSampleRate();
}

int DRMDemodBaseband::getChannelSampleRate() const
{
    // Read from other threads (the HTTP report) while the worker may be
    // replacing the channelizer: use the copy, never the channelizer.
    return m_channelSampleRate.load();
}
