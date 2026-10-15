///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "meshcoredemodbaseband.h"
#include "meshcoredemodmsg.h"

MESSAGE_CLASS_DEFINITION(MeshcoreDemodBaseband::MsgConfigureMeshcoreDemodBaseband, Message)

MeshcoreDemodBaseband::MeshcoreDemodBaseband() :
    m_channelizer(&m_sink)
{
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));

    qDebug("MeshcoreDemodBaseband::MeshcoreDemodBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &MeshcoreDemodBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

MeshcoreDemodBaseband::~MeshcoreDemodBaseband()
{
}

void MeshcoreDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void MeshcoreDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void MeshcoreDemodBaseband::handleData()
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

void MeshcoreDemodBaseband::handleInputMessages()
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

bool MeshcoreDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureMeshcoreDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureMeshcoreDemodBaseband& cfg = (MsgConfigureMeshcoreDemodBaseband&) cmd;
        qDebug() << "MeshcoreDemodBaseband::handleMessage: MsgConfigureMeshcoreDemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "MeshcoreDemodBaseband::handleMessage: DSPSignalNotification:"
                 << " basebandSampleRate:" << notif.getSampleRate()
                 << " centerFrequency:" << notif.getCenterFrequency();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer.setBasebandSampleRate(notif.getSampleRate());
        m_sink.setDeviceCenterFrequency(notif.getCenterFrequency());
        m_sink.applyChannelSettings(
            m_channelizer.getChannelSampleRate(),
            MeshcoreDemodSettings::bandwidths[m_settings.m_bandwidthIndex],
            m_channelizer.getChannelFrequencyOffset()
        );

		return true;
    }
    else if (MeshcoreDemodMsg::MsgLoRaHeaderFeedback::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MeshcoreDemodMsg::MsgLoRaHeaderFeedback& feedback = (MeshcoreDemodMsg::MsgLoRaHeaderFeedback&) cmd;
        qDebug("MeshcoreDemodBaseband::handleMessage: header feedback frameId=%u valid=%d expected=%u",
            feedback.getFrameId(), feedback.isValid() ? 1 : 0, feedback.getExpectedSymbols());
        m_sink.applyLoRaHeaderFeedback(
            feedback.getFrameId(),
            feedback.isValid(),
            feedback.getHasCRC(),
            feedback.getNbParityBits(),
            feedback.getPacketLength(),
            feedback.getLdro(),
            feedback.getExpectedSymbols(),
            feedback.getHeaderParityStatus(),
            feedback.getHeaderCRCStatus()
        );
        return true;
    }
    else
    {
        return false;
    }
}

void MeshcoreDemodBaseband::applySettings(const MeshcoreDemodSettings& settings, bool force)
{
    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex)
     || (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_channelizer.setChannelization(
            MeshcoreDemodSettings::bandwidths[settings.m_bandwidthIndex]*MeshcoreDemodSettings::oversampling,
            settings.m_inputFrequencyOffset
        );
        m_sink.applyChannelSettings(
            m_channelizer.getChannelSampleRate(),
            MeshcoreDemodSettings::bandwidths[settings.m_bandwidthIndex],
            m_channelizer.getChannelFrequencyOffset()
        );
    }

    m_sink.applySettings(settings, force);

    m_settings = settings;
}

int MeshcoreDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer.getChannelSampleRate();
}


void MeshcoreDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer.setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(
        m_channelizer.getChannelSampleRate(),
        MeshcoreDemodSettings::bandwidths[m_settings.m_bandwidthIndex],
        m_channelizer.getChannelFrequencyOffset()
    );
}
