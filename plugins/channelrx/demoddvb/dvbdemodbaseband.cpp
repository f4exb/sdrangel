///////////////////////////////////////////////////////////////////////////////////
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

#include <cmath>

#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"
#include "audio/audiodevicemanager.h"
#include "util/profiler.h"

#include "dvbdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(DVBDemodBaseband::MsgConfigureDVBDemodBaseband, Message)
MESSAGE_CLASS_DEFINITION(DVBDemodBaseband::MsgResetReceivers, Message)
MESSAGE_CLASS_DEFINITION(DVBDemodBaseband::MsgRateSearchBand, Message)

DVBDemodBaseband::DVBDemodBaseband() :
    m_running(false),
    m_audioSampleRate(48000),
    m_audioLatency(0.0),
    m_audioDeviceIndex(-1)
{
    qDebug("DVBDemodBaseband::DVBDemodBaseband");

    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(9142857));
    m_sampleFifo.setLabel("DVB Demod"); // So its overflows are told from the device's in the log
    m_channelizer = new DownChannelizer(&m_sink);

    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue());
    m_audioSampleRate = audioDeviceManager->getOutputSampleRate();
    m_audioLatency = audioDeviceManager->getOutputLatency();
}

void DVBDemodBaseband::getAudioOutput(int& sampleRate, double& latency) const
{
    sampleRate = m_audioSampleRate;
    latency = m_audioLatency;
}

DVBDemodBaseband::~DVBDemodBaseband()
{
    m_inputMessageQueue.clear();
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sink.getAudioFifo());
    delete m_channelizer;
}

void DVBDemodBaseband::setRateSearchBand(double frequency, double width, bool spectrumOnly, uint64_t generation)
{
    {
        QMutexLocker locker(&m_bandMutex);
        m_bandFrequency = frequency;
        m_bandWidth = width;
        m_bandSpectrumOnly = spectrumOnly;
        m_bandGeneration = generation;
    }

    m_inputMessageQueue.push(MsgRateSearchBand::create(frequency, width, spectrumOnly, generation));
}

void DVBDemodBaseband::reapplyRateSearchBand()
{
    QMutexLocker locker(&m_bandMutex);

    if (m_bandGeneration != 0) {
        m_inputMessageQueue.push(MsgRateSearchBand::create(m_bandFrequency, m_bandWidth, m_bandSpectrumOnly, m_bandGeneration));
    }
}

void DVBDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void DVBDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DVBDemodBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void DVBDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DVBDemodBaseband::handleData
    );
    m_running = false;
}

void DVBDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void DVBDemodBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
        SampleVector::iterator part1begin;
        SampleVector::iterator part1end;
        SampleVector::iterator part2begin;
        SampleVector::iterator part2end;

        // Behind on a satellite signal while searching: the oldest samples dropped in one go, the receivers told.
        // Overflowing, the FIFO dropped samples here and there, which left no frames whole for the DVB-S2 search, so it
        // never locked, and never got to the cheaper tracking
        const unsigned int drop = DVBDemodSink::samplesToDrop(m_sink.dropsWhenBehind(), m_sink.satelliteLocked(),
            m_sampleFifo.fill(), m_sampleFifo.size());

        if (drop > 0)
        {
            m_sampleFifo.readBegin(drop, &part1begin, &part1end, &part2begin, &part2end);
            m_sampleFifo.readCommit(drop);
            m_sink.discontinuity();
            m_droppedSearching += drop;

            if (!m_dropLogTimer.isValid() || (m_dropLogTimer.elapsed() > 5000))
            {
                qDebug("DVBDemodBaseband::handleData: behind: %llu samples dropped", (unsigned long long) m_droppedSearching);
                m_droppedSearching = 0;
                m_dropLogTimer.start();
            }

            continue;
        }

        std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);
        PROFILER_START()

        // first part of FIFO data
        if (part1begin != part1end) {
            m_channelizer->feed(part1begin, part1end);
        }

        // second part of FIFO data (used when block wraps around)
        if (part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
        PROFILER_STOP("DVBDemod baseband (channelizer and sink)")
    }
}

void DVBDemodBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

bool DVBDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureDVBDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureDVBDemodBaseband& cfg = (MsgConfigureDVBDemodBaseband&) cmd;
        qDebug() << "DVBDemodBaseband::handleMessage: MsgConfigureDVBDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgResetReceivers::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        // Those queued are from before (as of the transponder before a retune: behind, up to half a second of them)
        m_sampleFifo.readCommit(m_sampleFifo.fill());
        m_sink.resetReceivers();

        return true;
    }
    else if (MsgRateSearchBand::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        const MsgRateSearchBand& band = (const MsgRateSearchBand&) cmd;
        m_sink.setRateSearchBand(band.getFrequency(), band.getWidth(), band.getSpectrumOnly(), band.getGeneration());

        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;

        if ((cfg.getAudioType() == DSPConfigureAudio::AudioOutput) && (cfg.getSampleRate() > 0))
        {
            qDebug() << "DVBDemodBaseband::handleMessage: DSPConfigureAudio: audio sample rate:" << cfg.getSampleRate();
            m_audioSampleRate = cfg.getSampleRate();
            // The device has started, its buffer known
            m_audioLatency = DSPEngine::instance()->getAudioDeviceManager()->getOutputLatency(m_audioDeviceIndex);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DVBDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        setChannelization();

        return true;
    }
    else
    {
        return false;
    }
}

// Decimate by a power of 2 to the rate the channel bandwidth needs (or for DVB-S and S2, twice the symbol rate), if the
// baseband allows
void DVBDemodBaseband::setChannelization()
{
    // The symbol rate automatic: all the baseband, as it's not known how wide the signal is
    int sampleRate = DVBDemodSettings::isSatellite(m_settings.m_standard)
        ? (m_settings.m_symbolRate > 0 ? 2 * m_settings.m_symbolRate : m_channelizer->getBasebandSampleRate())
        : (int) std::round(DVBDemodSettings::terrestrialSampleRate(m_settings.m_bandwidth));

    if (sampleRate <= 0) {
        return; // The baseband's rate isn't known yet
    }

    m_channelizer->setChannelization(sampleRate, m_settings.m_inputFrequencyOffset);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}

void DVBDemodBaseband::applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    bool channelization = (settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset))
        || (settingsKeys.contains("bandwidth") && (settings.m_bandwidth != m_settings.m_bandwidth))
        || (settingsKeys.contains("symbolRate") && (settings.m_symbolRate != m_settings.m_symbolRate))
        || (settingsKeys.contains("standard") && (DVBDemodSettings::isSatellite(settings.m_standard) != DVBDemodSettings::isSatellite(m_settings.m_standard)))
        || force;

    if ((settingsKeys.contains("audioDeviceName") && (settings.m_audioDeviceName != m_settings.m_audioDeviceName)) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSink(m_sink.getAudioFifo());
        audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        m_audioDeviceIndex = audioDeviceIndex;
        m_audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);
        m_audioLatency = audioDeviceManager->getOutputLatency(audioDeviceIndex); // 0 till it's started, if it's new
    }

    m_sink.applySettings(settings, settingsKeys, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (channelization) {
        setChannelization();
    }
}

int DVBDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
