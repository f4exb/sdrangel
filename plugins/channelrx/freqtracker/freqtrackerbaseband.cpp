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

#include "freqtrackerreport.h"
#include "freqtrackerbaseband.h"

MESSAGE_CLASS_DEFINITION(FreqTrackerBaseband::MsgConfigureFreqTrackerBaseband, Message)

FreqTrackerBaseband::FreqTrackerBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);

    qDebug("FreqTrackerBaseband::FreqTrackerBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &FreqTrackerBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

FreqTrackerBaseband::~FreqTrackerBaseband()
{
    delete m_channelizer;
}

void FreqTrackerBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void FreqTrackerBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void FreqTrackerBaseband::handleData()
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

void FreqTrackerBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool FreqTrackerBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureFreqTrackerBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureFreqTrackerBaseband& cfg = (MsgConfigureFreqTrackerBaseband&) cmd;
        qDebug() << "FreqTrackerBaseband::handleMessage: MsgConfigureFreqTrackerBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        qDebug() << "FreqTrackerBaseband::handleMessage: DSPSignalNotification:"
            << "basebandSampleRate:" << m_basebandSampleRate;
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(m_basebandSampleRate));
        m_channelizer->setBasebandSampleRate(m_basebandSampleRate);
        m_sink.applyChannelSettings(
            m_basebandSampleRate / (1<<m_settings.m_log2Decim),
            m_channelizer->getChannelSampleRate(),
            m_channelizer->getChannelFrequencyOffset()
        );

		return true;
    }
    else
    {
        return false;
    }
}

void FreqTrackerBaseband::applySettings(const FreqTrackerSettings& settings, bool force)
{
    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset)
     || (m_settings.m_log2Decim != settings.m_log2Decim)|| force)
    {
        m_channelizer->setChannelization(m_basebandSampleRate/(1<<settings.m_log2Decim), settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(
            m_basebandSampleRate/(1<<settings.m_log2Decim),
            m_channelizer->getChannelSampleRate(),
            m_channelizer->getChannelFrequencyOffset()
        );
    }

    m_sink.applySettings(settings, force);

    m_settings = settings;
}

int FreqTrackerBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}


void FreqTrackerBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
}
