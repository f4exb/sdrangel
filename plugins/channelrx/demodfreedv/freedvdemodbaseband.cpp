///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "freedvdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(FreeDVDemodBaseband::MsgConfigureFreeDVDemodBaseband, Message)
MESSAGE_CLASS_DEFINITION(FreeDVDemodBaseband::MsgResyncFreeDVDemod, Message)

FreeDVDemodBaseband::FreeDVDemodBaseband() :
    m_mutex(QMutex::Recursive),
    m_messageQueueToGUI(nullptr)
{
    qDebug("FreeDVDemodBaseband::FreeDVDemodBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);

    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &FreeDVDemodBaseband::handleData,
        Qt::QueuedConnection
    );

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue());
    m_sink.applyAudioSampleRate(DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate());

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

FreeDVDemodBaseband::~FreeDVDemodBaseband()
{
    DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(m_sink.getAudioFifo());
    delete m_channelizer;
}

void FreeDVDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void FreeDVDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void FreeDVDemodBaseband::handleData()
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

    qreal rmsLevel, peakLevel;
    int numSamples;
    m_sink.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void FreeDVDemodBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool FreeDVDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureFreeDVDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        qDebug() << "FreeDVDemodBaseband::handleMessage: MsgConfigureFreeDVDemodBaseband";
        MsgConfigureFreeDVDemodBaseband& cfg = (MsgConfigureFreeDVDemodBaseband&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DSDDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

		return true;
    }
    else if (MsgResyncFreeDVDemod::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        qDebug() << "FreeDVDemodBaseband::handleMessage: MsgResyncFreeDVDemod";
        m_sink.resyncFreeDV();

        return true;
    }
    else
    {
        return false;
    }
}

void FreeDVDemodBaseband::applySettings(const FreeDVDemodSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(m_sink.getModemSampleRate(), settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        //qDebug("AMDemod::applySettings: audioDeviceName: %s audioDeviceIndex: %d", qPrintable(settings.m_audioDeviceName), audioDeviceIndex);
        audioDeviceManager->removeAudioSink(m_sink.getAudioFifo());
        audioDeviceManager->addAudioSink(m_sink.getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
        int audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (m_sink.getAudioSampleRate() != audioSampleRate) {
            m_sink.applyAudioSampleRate(audioSampleRate);
        }
    }

    if ((settings.m_freeDVMode != m_settings.m_freeDVMode) || force)
    {
        uint32_t modemSampleRate = FreeDVDemodSettings::getModSampleRate(settings.m_freeDVMode);

        if (m_sink.getModemSampleRate() != modemSampleRate)
        {
            m_channelizer->setChannelization(m_sink.getModemSampleRate(), settings.m_inputFrequencyOffset);
            m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

            if (getMessageQueueToGUI())
            {
                DSPConfigureAudio *cfg = new DSPConfigureAudio(modemSampleRate, DSPConfigureAudio::AudioOutput);
                getMessageQueueToGUI()->push(cfg);
            }
        }

        m_sink.applyFreeDVMode(settings.m_freeDVMode);
    }

    m_sink.applySettings(settings, force);
    m_settings = settings;
}

int FreeDVDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}


void FreeDVDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}