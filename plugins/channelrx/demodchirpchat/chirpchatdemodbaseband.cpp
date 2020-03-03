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

#include "chirpchatdemodbaseband.h"

MESSAGE_CLASS_DEFINITION(ChirpChatDemodBaseband::MsgConfigureChirpChatDemodBaseband, Message)

ChirpChatDemodBaseband::ChirpChatDemodBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);

    qDebug("ChirpChatDemodBaseband::ChirpChatDemodBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &ChirpChatDemodBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

ChirpChatDemodBaseband::~ChirpChatDemodBaseband()
{
    delete m_channelizer;
}

void ChirpChatDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void ChirpChatDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void ChirpChatDemodBaseband::handleData()
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

void ChirpChatDemodBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool ChirpChatDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureChirpChatDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChirpChatDemodBaseband& cfg = (MsgConfigureChirpChatDemodBaseband&) cmd;
        qDebug() << "ChirpChatDemodBaseband::handleMessage: MsgConfigureChirpChatDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "ChirpChatDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_sink.applyChannelSettings(
            m_channelizer->getChannelSampleRate(),
            ChirpChatDemodSettings::bandwidths[m_settings.m_bandwidthIndex],
            m_channelizer->getChannelFrequencyOffset()
        );

		return true;
    }
    else
    {
        return false;
    }
}

void ChirpChatDemodBaseband::applySettings(const ChirpChatDemodSettings& settings, bool force)
{
    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex)
     || (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer->setChannelization(
            ChirpChatDemodSettings::bandwidths[settings.m_bandwidthIndex]*ChirpChatDemodSettings::oversampling,
            settings.m_inputFrequencyOffset
        );
        m_sink.applyChannelSettings(
            m_channelizer->getChannelSampleRate(),
            ChirpChatDemodSettings::bandwidths[settings.m_bandwidthIndex],
            m_channelizer->getChannelFrequencyOffset()
        );
    }

    m_sink.applySettings(settings, force);

    m_settings = settings;
}

int ChirpChatDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}


void ChirpChatDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(
        m_channelizer->getChannelSampleRate(),
        ChirpChatDemodSettings::bandwidths[m_settings.m_bandwidthIndex],
        m_channelizer->getChannelFrequencyOffset()
    );
}
