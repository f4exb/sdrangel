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

#include "datvdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(DATVDemodBaseband::MsgConfigureDATVDemodBaseband, Message)

DATVDemodBaseband::DATVDemodBaseband() :
    m_running(false),
    m_mutex(QMutex::Recursive)
{
    qDebug("DATVDemodBaseband::DATVDemodBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_sink = new DATVDemodSink();
    m_channelizer = new DownChannelizer(m_sink);
}

DATVDemodBaseband::~DATVDemodBaseband()
{
    delete m_channelizer;
    delete m_sink;
}

void DATVDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void DATVDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DATVDemodBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void DATVDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sink->stopVideo();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DATVDemodBaseband::handleData
    );
    m_running = false;
}

void DATVDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void DATVDemodBaseband::handleData()
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

void DATVDemodBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool DATVDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureDATVDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureDATVDemodBaseband& cfg = (MsgConfigureDATVDemodBaseband&) cmd;
        qDebug() << "DATVDemodBaseband::handleMessage: MsgConfigureDATVDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DATVDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_sink->applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

		return true;
    }
    else
    {
        return false;
    }
}

void DATVDemodBaseband::applySettings(const DATVDemodSettings& settings, bool force)
{
    qDebug("DATVDemodBaseband::applySettings");

    if ((settings.m_centerFrequency != m_settings.m_centerFrequency) ||
        (settings.m_symbolRate != m_settings.m_symbolRate) || force)
    {
        unsigned int desiredSampleRate = 2 * settings.m_symbolRate; // m_channelizer->getBasebandSampleRate();
        m_channelizer->setChannelization(desiredSampleRate, settings.m_centerFrequency);
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(m_channelizer->getBasebandSampleRate()));
        m_sink->applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSink(m_sink->getAudioFifo(), getInputMessageQueue(), audioDeviceIndex);
    }

    m_sink->applySettings(settings, force);
    m_settings = settings;
}

int DATVDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}


void DATVDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    qDebug("DATVDemodBaseband::setBasebandSampleRate: %d", sampleRate);
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink->applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}
