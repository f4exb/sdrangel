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

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "vordemodmcbaseband.h"
#include "vordemodmcreport.h"

MESSAGE_CLASS_DEFINITION(VORDemodMCBaseband::MsgConfigureVORDemodBaseband, Message)

VORDemodMCBaseband::VORDemodMCBaseband() :
    m_running(false),
    m_mutex(QMutex::Recursive),
    m_messageQueueToGUI(nullptr),
    m_basebandSampleRate(0)
{
    qDebug("VORDemodMCBaseband::VORDemodMCBaseband");

    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));

    // FIXME: If we remove this audio stops working when this demod is closed
    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifoBug, getInputMessageQueue());
}

VORDemodMCBaseband::~VORDemodMCBaseband()
{
    m_inputMessageQueue.clear();

    for (int i = 0; i < m_sinks.size(); i++)
    {
        DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sinks[i]->getAudioFifo());
        delete m_sinks[i];
    }
    m_sinks.clear();

    // FIXME: If we remove this audio stops working when this demod is closed
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifoBug);

    for (int i = 0; i < m_channelizers.size(); i++)
        delete m_channelizers[i];
    m_channelizers.clear();
}

void VORDemodMCBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void VORDemodMCBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &VORDemodMCBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void VORDemodMCBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &VORDemodMCBaseband::handleData
    );
    m_running = false;
}

void VORDemodMCBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void VORDemodMCBaseband::handleData()
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
            for (int i = 0; i < m_channelizers.size(); i++)
                m_channelizers[i]->feed(part1begin, part1end);
        }

        // second part of FIFO data (used when block wraps around)
        if(part2begin != part2end) {
            for (int i = 0; i < m_channelizers.size(); i++)
                m_channelizers[i]->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void VORDemodMCBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool VORDemodMCBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureVORDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureVORDemodBaseband& cfg = (MsgConfigureVORDemodBaseband&) cmd;
        qDebug() << "VORDemodMCBaseband::handleMessage: MsgConfigureVORDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "VORDemodMCBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate() << " centerFrequency: " << notif.getCenterFrequency();
        m_centerFrequency = notif.getCenterFrequency();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(m_basebandSampleRate));

        return true;
    }
    else
    {
        return false;
    }
}

// Calculate offset of VOR center frequency from sample source center frequency
void VORDemodMCBaseband::calculateOffset(VORDemodMCSink *sink)
{
   int frequencyOffset = sink->m_vorFrequencyHz - m_centerFrequency;
   bool outOfBand = std::abs(frequencyOffset)+VORDEMOD_CHANNEL_BANDWIDTH > (m_basebandSampleRate/2);

    if (m_messageQueueToGUI != nullptr)
    {
        VORDemodMCReport::MsgReportFreqOffset *msg = VORDemodMCReport::MsgReportFreqOffset::create(sink->m_subChannelId, frequencyOffset, outOfBand);
        m_messageQueueToGUI->push(msg);
    }

   sink->m_frequencyOffset = frequencyOffset;
   sink->m_outOfBand = outOfBand;
}

void VORDemodMCBaseband::applySettings(const VORDemodMCSettings& settings, bool force)
{
    // Remove sub-channels no longer needed
    for (int i = 0; i < m_sinks.size(); i++)
    {
        if (!settings.m_subChannelSettings.contains(m_sinks[i]->m_subChannelId))
        {
            qDebug() << "VORDemodMCBaseband::applySettings: Removing sink " << m_sinks[i]->m_subChannelId;
            VORDemodMCSink *sink = m_sinks[i];
            DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sinks[i]->getAudioFifo());
            m_sinks.removeAt(i);
            delete sink;
            DownChannelizer *channelizer = m_channelizers[i];
            m_channelizers.removeAt(i);
            delete channelizer;
        }
    }

    // Add new sub channels
    QHash<int, VORDemodSubChannelSettings *>::const_iterator itr = settings.m_subChannelSettings.begin();
    while (itr != settings.m_subChannelSettings.end())
    {
        VORDemodSubChannelSettings *subChannelSettings = itr.value();
        int j;
        for (j = 0; j < m_sinks.size(); j++)
        {
            if (subChannelSettings->m_id == m_sinks[j]->m_subChannelId)
                break;
        }
        if (j == m_sinks.size())
        {
            // Add a sub-channel sink
            qDebug() << "VORDemodMCBaseband::applySettings: Adding sink " << subChannelSettings->m_id;
            VORDemodMCSink *sink = new VORDemodMCSink(settings, subChannelSettings->m_id, m_messageQueueToGUI);
            DownChannelizer *channelizer = new DownChannelizer(sink);
            channelizer->setBasebandSampleRate(m_basebandSampleRate);
            DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(sink->getAudioFifo(), getInputMessageQueue());
            sink->applyAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

            m_sinks.append(sink);
            m_channelizers.append(channelizer);

            calculateOffset(sink);

            channelizer->setChannelization(VORDEMOD_CHANNEL_SAMPLE_RATE, sink->m_frequencyOffset);
            sink->applyChannelSettings(channelizer->getChannelSampleRate(), channelizer->getChannelFrequencyOffset(), true);
            sink->applyAudioSampleRate(sink->getAudioSampleRate());
        }
        ++itr;
    }

    if (force)
    {
        for (int i = 0; i < m_sinks.size(); i++)
        {
            m_channelizers[i]->setChannelization(VORDEMOD_CHANNEL_SAMPLE_RATE, m_sinks[i]->m_frequencyOffset);
            m_sinks[i]->applyChannelSettings(m_channelizers[i]->getChannelSampleRate(), m_channelizers[i]->getChannelFrequencyOffset());
            m_sinks[i]->applyAudioSampleRate(m_sinks[i]->getAudioSampleRate()); // reapply in case of channel sample rate change
        }
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        for (int i = 0; i < m_sinks.size(); i++)
        {
            audioDeviceManager->removeAudioSink(m_sinks[i]->getAudioFifo());
            audioDeviceManager->addAudioSink(m_sinks[i]->getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
            int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

            if (m_sinks[i]->getAudioSampleRate() != audioSampleRate)
            {
                m_sinks[i]->applyAudioSampleRate(audioSampleRate);
            }
        }
    }

    for (int i = 0; i < m_sinks.size(); i++)
        m_sinks[i]->applySettings(settings, force);

    m_settings = settings;
}

void VORDemodMCBaseband::setBasebandSampleRate(int sampleRate)
{
    m_basebandSampleRate = sampleRate;
    for (int i = 0; i < m_sinks.size(); i++)
    {
        m_channelizers[i]->setBasebandSampleRate(sampleRate);
        calculateOffset(m_sinks[i]);
        m_sinks[i]->applyChannelSettings(m_channelizers[i]->getChannelSampleRate(), m_channelizers[i]->getChannelFrequencyOffset());
        m_sinks[i]->applyAudioSampleRate(m_sinks[i]->getAudioSampleRate()); // reapply in case of channel sample rate change
    }
}
