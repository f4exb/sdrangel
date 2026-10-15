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

#include "wefaxdemodbaseband.h"

#include <algorithm>

#include <QDebug>
#include <QMutexLocker>

#include "dsp/downchannelizer.h"
#include "dsp/dspcommands.h"

MESSAGE_CLASS_DEFINITION(WefaxDemodBaseband::MsgConfigureWefaxDemodBaseband, Message)
MESSAGE_CLASS_DEFINITION(WefaxDemodBaseband::MsgDecoderAction, Message)
MESSAGE_CLASS_DEFINITION(WefaxDemodBaseband::MsgDecoderReport, Message)

WefaxDemodBaseband::WefaxDemodBaseband() :
    m_channelizer(new DownChannelizer(&m_sink)),
    m_messageQueueToChannel(nullptr),
    m_running(false)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
}

WefaxDemodBaseband::~WefaxDemodBaseband()
{
    m_inputMessageQueue.clear();
    delete m_channelizer;
}

void WefaxDemodBaseband::reset()
{
    QMutexLocker locker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
    m_sink.stop();
}

void WefaxDemodBaseband::startWork()
{
    QMutexLocker locker(&m_mutex);
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued,
        this, &WefaxDemodBaseband::handleInputMessages);
    connect(&m_sampleFifo, &SampleSinkFifo::dataReady,
        this, &WefaxDemodBaseband::handleData, Qt::QueuedConnection);
    m_running = true;
}

void WefaxDemodBaseband::stopWork()
{
    QMutexLocker locker(&m_mutex);
    disconnect(&m_inputMessageQueue, &MessageQueue::messageEnqueued,
        this, &WefaxDemodBaseband::handleInputMessages);
    disconnect(&m_sampleFifo, &SampleSinkFifo::dataReady,
        this, &WefaxDemodBaseband::handleData);
    m_running = false;
}

void WefaxDemodBaseband::feed(
    const SampleVector::const_iterator& begin,
    const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void WefaxDemodBaseband::handleData()
{
    QMutexLocker locker(&m_mutex);

    while ((m_sampleFifo.fill() > 0)
        && (m_inputMessageQueue.size() == 0)
        && (m_channelizer->getBasebandSampleRate() > 0))
    {
        SampleVector::iterator part1Begin;
        SampleVector::iterator part1End;
        SampleVector::iterator part2Begin;
        SampleVector::iterator part2End;
        const std::size_t count = m_sampleFifo.readBegin(
            m_sampleFifo.fill(),
            &part1Begin,
            &part1End,
            &part2Begin,
            &part2End);
        const WefaxDecoder::State stateBefore = m_sink.decoder().state();

        if (part1Begin != part1End) {
            m_channelizer->feed(part1Begin, part1End);
        }
        if (part2Begin != part2End) {
            m_channelizer->feed(part2Begin, part2End);
        }

        m_sampleFifo.readCommit(static_cast<unsigned int>(count));

        auto lines = m_sink.takeCompletedLines();
        const bool stateChanged = stateBefore != m_sink.decoder().state();

        if (!lines.empty() || stateChanged)
        {
            const bool rowLimitReached = m_settings.m_maxRows > 0
                && m_sink.decoder().completedLineCount()
                    >= static_cast<std::size_t>(m_settings.m_maxRows);

            if (rowLimitReached) {
                m_sink.stop(WefaxDemodSink::CompletionReason::RowLimit);
                auto trailingLines = m_sink.takeCompletedLines();
                for (auto& line : trailingLines) {
                    lines.emplace_back(std::move(line));
                }
            }

            sendDecoderReport(std::move(lines));
        }
    }
}

void WefaxDemodBaseband::finishCapture(WefaxDemodSink::CompletionReason reason)
{
    QMutexLocker locker(&m_mutex);

    if (m_sink.decoder().state() != WefaxDecoder::State::Idle)
    {
        m_sink.stop(reason);
        sendDecoderReport();
    }
}

void WefaxDemodBaseband::handleInputMessages()
{
    Message *message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug() << "WefaxDemodBaseband: unhandled message" << message->getIdentifier();
        }
        delete message;
    }
}

bool WefaxDemodBaseband::handleMessage(const Message& message)
{
    if (MsgConfigureWefaxDemodBaseband::match(message))
    {
        QMutexLocker locker(&m_mutex);
        const auto& configure = static_cast<const MsgConfigureWefaxDemodBaseband&>(message);
        applySettings(configure.getSettingsKeys(), configure.getSettings(), configure.getForce());
        return true;
    }

    if (DSPSignalNotification::match(message))
    {
        QMutexLocker locker(&m_mutex);
        const auto& notification = static_cast<const DSPSignalNotification&>(message);
        setBasebandSampleRate(notification.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(std::max(notification.getSampleRate(), 48000)));
        return true;
    }

    if (MsgDecoderAction::match(message))
    {
        QMutexLocker locker(&m_mutex);
        const auto action = static_cast<const MsgDecoderAction&>(message).getAction();
        bool succeeded = true;

        switch (action)
        {
        case MsgDecoderAction::StartPhasing:
            m_sink.startPhasing();
            break;
        case MsgDecoderAction::FinishPhasing:
            succeeded = m_sink.finishPhasing();
            break;
        case MsgDecoderAction::StartReceiving:
            succeeded = m_sink.startReceiving();
            break;
        case MsgDecoderAction::Stop:
            m_sink.stop();
            break;
        case MsgDecoderAction::SourceChanged:
            m_sink.stop(WefaxDemodSink::CompletionReason::SourceChanged);
            break;
        case MsgDecoderAction::Shutdown:
            m_sink.stop(WefaxDemodSink::CompletionReason::Shutdown);
            break;
        }

        sendDecoderReport(succeeded);
        return true;
    }

    return false;
}

void WefaxDemodBaseband::applySettings(
    const QStringList& settingsKeys,
    const WefaxDemodSettings& settings,
    bool force)
{
    const bool captureChanged = force || WefaxDemodSettings::endsCapture(settingsKeys);
    const bool channelizationChanged = force
        || settingsKeys.contains("inputFrequencyOffset")
        || settingsKeys.contains("autoMode")
        || settingsKeys.contains("ioc")
        || settingsKeys.contains("linesPerMinute");

    // Finalise with the old settings first. This releases guarded rows and
    // gives the channel an ordered boundary at which it can save and clear the
    // old image before rows produced with the new settings arrive.
    if (captureChanged && (m_sink.decoder().state() != WefaxDecoder::State::Idle))
    {
        const bool sourceChanged = settingsKeys.contains("streamIndex");
        m_sink.stop(sourceChanged
            ? WefaxDemodSink::CompletionReason::SourceChanged
            : WefaxDemodSink::CompletionReason::ModeChanged);
        sendDecoderReport();
    }

    m_sink.applySettings(settingsKeys, settings, force);

    if (channelizationChanged)
    {
        // Unforced, an offset-only change just retunes the sink's NCO.
        m_channelizer->setChannelization(
            m_sink.internalSampleRate(),
            settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(
            m_channelizer->getChannelSampleRate(),
            m_channelizer->getChannelFrequencyOffset(),
            force);
    }

    if (force) {
        m_settings = settings;
        m_settings.validate();
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void WefaxDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(
        m_channelizer->getChannelSampleRate(),
        m_channelizer->getChannelFrequencyOffset(),
        true);
}

void WefaxDemodBaseband::sendDecoderReport(bool actionSucceeded)
{
    sendDecoderReport(m_sink.takeCompletedLines(), actionSucceeded);
}

void WefaxDemodBaseband::sendDecoderReport(MsgDecoderReport::Lines&& lines, bool actionSucceeded)
{
    if (!m_messageQueueToChannel) {
        return;
    }

    const WefaxDecoder& decoder = m_sink.decoder();
    m_messageQueueToChannel->push(MsgDecoderReport::create(
        std::move(lines),
        decoder.state(),
        decoder.phasingLineCount(),
        decoder.appliedSamplesPerLine(),
        decoder.lineClockCorrectionPpm(),
        decoder.appliedLineClockCorrectionPpm(),
        decoder.phasingConfidence(),
        m_sink.selectedIOC(),
        m_sink.selectedLinesPerMinute(),
        m_sink.completionReason(),
        m_sink.tuningErrorHz(),
        m_sink.effectiveLinesPerMinute(),
        m_sink.timingSource(),
        WefaxDecoder::timingStatusText(decoder.timingStatus()),
        actionSucceeded));
}
