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

#include "dsp/downchannelizer.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "remotesinkbaseband.h"

MESSAGE_CLASS_DEFINITION(RemoteSinkBaseband::MsgConfigureRemoteSinkBaseband, Message)

RemoteSinkBaseband::RemoteSinkBaseband() :
    m_mutex(QMutex::Recursive)
{
    qDebug("RemoteSinkBaseband::RemoteSinkBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

RemoteSinkBaseband::~RemoteSinkBaseband()
{
    delete m_channelizer;
}

void RemoteSinkBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
    m_sink.init();
}

void RemoteSinkBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &RemoteSinkBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_sink.start();
    m_running = true;
}

void RemoteSinkBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sink.stop();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &RemoteSinkBaseband::handleData
    );
    m_running = false;
}

void RemoteSinkBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void RemoteSinkBaseband::handleData()
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

void RemoteSinkBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool RemoteSinkBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureRemoteSinkBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureRemoteSinkBaseband& cfg = (MsgConfigureRemoteSinkBaseband&) cmd;
        qDebug() << "RemoteSinkBaseband::handleMessage: MsgConfigureRemoteSinkBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        qDebug() << "RemoteSinkBaseband::handleMessage: DSPSignalNotification: basebandSampleRate:" << m_basebandSampleRate;
        m_channelizer->setBasebandSampleRate(m_basebandSampleRate);
        m_sink.applyBasebandSampleRate(m_basebandSampleRate);
        m_sink.setDeviceCenterFrequency(notif.getCenterFrequency());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(m_basebandSampleRate));

        return true;
    }
    else
    {
        return false;
    }
}

void RemoteSinkBaseband::applySettings(const RemoteSinkSettings& settings, bool force)
{
    qDebug() << "RemoteSinkBaseband::applySettings:"
        << "m_log2Decim:" << settings.m_log2Decim
        << "m_filterChainHash:" << settings.m_filterChainHash
        << " force: " << force;

    if ((settings.m_log2Decim != m_settings.m_log2Decim)
     || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        m_channelizer->setDecimation(settings.m_log2Decim, settings.m_filterChainHash);
    }

    m_sink.applySettings(settings, force);
    m_settings = settings;
}

int RemoteSinkBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void RemoteSinkBaseband::setBasebandSampleRate(int sampleRate)
{
    m_basebandSampleRate = sampleRate;
    m_channelizer->setBasebandSampleRate(m_basebandSampleRate);
    m_sink.applyBasebandSampleRate(m_basebandSampleRate);
}
