///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"

#include "wdsprxbaseband.h"

MESSAGE_CLASS_DEFINITION(WDSPRxBaseband::MsgConfigureWDSPRxBaseband, Message)

WDSPRxBaseband::WDSPRxBaseband() :
    m_channelizer(&m_sink),
    m_messageQueueToGUI(nullptr),
    m_spectrumVis(nullptr)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));

    qDebug("WDSPRxBaseband::WDSPRxBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &WDSPRxBaseband::handleData,
        Qt::QueuedConnection
    );

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue());
    m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();
    m_sink.applyAudioSampleRate(m_audioSampleRate);
    m_channelSampleRate = 0;

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

WDSPRxBaseband::~WDSPRxBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sink.getAudioFifo());
}

void WDSPRxBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sink.applyAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());
    mutexLocker.unlock();
    m_sampleFifo.reset();
    m_channelSampleRate = 0;
}

void WDSPRxBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void WDSPRxBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void WDSPRxBaseband::handleData()
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
            m_channelizer.feed(part1begin, part1end);
        }

		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
            m_channelizer.feed(part2begin, part2end);
        }

		m_sampleFifo.readCommit((unsigned int) count);
    }
}

void WDSPRxBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool WDSPRxBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureWDSPRxBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureWDSPRxBaseband& cfg = (MsgConfigureWDSPRxBaseband&) cmd;
        qDebug() << "WDSPRxBaseband::handleMessage: MsgConfigureWDSPRxBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "WDSPRxBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer.setBasebandSampleRate(notif.getSampleRate());
        m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());

        if (m_channelSampleRate != m_channelizer.getChannelSampleRate())
        {
            m_sink.applyAudioSampleRate(m_audioSampleRate); // reapply when channel sample rate changes
            m_channelSampleRate = m_channelizer.getChannelSampleRate();
        }

		return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        unsigned int audioSampleRate = cfg.getSampleRate();

        if (m_audioSampleRate != audioSampleRate)
        {
            qDebug("WDSPRxBaseband::handleMessage: DSPConfigureAudio: new sample rate %d",audioSampleRate);
            m_sink.applyAudioSampleRate(audioSampleRate);
            m_channelizer.setChannelization(audioSampleRate, m_settings.m_inputFrequencyOffset);
            m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());
            m_audioSampleRate = audioSampleRate;

            if (getMessageQueueToGUI())
            {
                qDebug("WDSPRxBaseband::handleMessage: DSPConfigureAudio: forward to GUI");
                DSPConfigureAudio *msg = new DSPConfigureAudio((int) audioSampleRate, DSPConfigureAudio::AudioOutput);
                getMessageQueueToGUI()->push(msg);
            }

            if (m_spectrumVis)
            {
                DSPSignalNotification *msg = new DSPSignalNotification(m_audioSampleRate/(1<<m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2), 0);
                m_spectrumVis->getInputMessageQueue()->push(msg);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

void WDSPRxBaseband::applySettings(const WDSPRxSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer.setChannelization(m_audioSampleRate, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());

        if (m_channelSampleRate != m_channelizer.getChannelSampleRate())
        {
            m_sink.applyAudioSampleRate(m_audioSampleRate); // reapply when channel sample rate changes
            m_channelSampleRate = m_channelizer.getChannelSampleRate();
        }
    }

    if ((settings.m_profiles[settings.m_profileIndex].m_spanLog2 != m_settings.m_profiles[settings.m_profileIndex].m_spanLog2) || force)
    {
        if (m_spectrumVis)
        {
            DSPSignalNotification *msg = new DSPSignalNotification(m_audioSampleRate/(1<<settings.m_profiles[settings.m_profileIndex].m_spanLog2), 0);
            m_spectrumVis->getInputMessageQueue()->push(msg);
        }
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        unsigned int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (m_audioSampleRate != audioSampleRate)
        {
            m_sink.applyAudioSampleRate(audioSampleRate);
            m_channelizer.setChannelization(audioSampleRate, settings.m_inputFrequencyOffset);
            m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());
            m_audioSampleRate = audioSampleRate;

            if (getMessageQueueToGUI())
            {
                DSPConfigureAudio *msg = new DSPConfigureAudio((int) audioSampleRate, DSPConfigureAudio::AudioOutput);
                getMessageQueueToGUI()->push(msg);
            }

            if (m_spectrumVis)
            {
                DSPSignalNotification *msg = new DSPSignalNotification(m_audioSampleRate/(1<<m_settings.m_profiles[settings.m_profileIndex].m_spanLog2), 0);
                m_spectrumVis->getInputMessageQueue()->push(msg);
            }
        }
    }

    m_sink.applySettings(settings, force);

    m_settings = settings;
}

int WDSPRxBaseband::getChannelSampleRate() const
{
    return m_channelizer.getChannelSampleRate();
}


void WDSPRxBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer.setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());
}
