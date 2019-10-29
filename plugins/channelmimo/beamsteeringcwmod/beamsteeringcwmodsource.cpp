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

#include <QMutexLocker>
#include <QDebug>

#include "dsp/upsamplechannelizer.h"
#include "dsp/dspcommands.h"

#include "beamsteeringcwmodsource.h"


MESSAGE_CLASS_DEFINITION(BeamSteeringCWModSource::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWModSource::MsgSignalNotification, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWModSource::MsgConfigureSteeringAngle, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWModSource::MsgConfigureChannelMute, Message)

BeamSteeringCWModSource::BeamSteeringCWModSource() :
    m_steeringDegrees(90),
    m_mutex(QMutex::Recursive)
{
    m_sampleMOFifo.init(2, 4096*64);
    m_vbegin.resize(2);

    for (int i = 0; i < 2; i++)
    {
        m_streamSources[i].setStreamIndex(i);
        m_channelizers[i] = new UpSampleChannelizer(&m_streamSources[i]);
        m_sizes[i] = 0;
    }

    QObject::connect(
        &m_sampleMOFifo,
        &SampleMOFifo::dataSyncRead,
        this,
        &BeamSteeringCWModSource::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_lastStream = 0;
}

BeamSteeringCWModSource::~BeamSteeringCWModSource()
{
    for (int i = 0; i < 2; i++) {
        delete m_channelizers[i];
    }
}

void BeamSteeringCWModSource::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleMOFifo.reset();

    for (int i = 0; i < 2; i++) {
        m_streamSources[i].reset();
    }
}

void BeamSteeringCWModSource::setSteeringDegrees(int steeringDegrees)
{
    m_steeringDegrees = steeringDegrees < -180 ? -180 : steeringDegrees > 180 ? 180 : steeringDegrees;
    MsgConfigureSteeringAngle *msg = MsgConfigureSteeringAngle::create((m_steeringDegrees/180.0f)*M_PI);
    m_inputMessageQueue.push(msg);
}

void BeamSteeringCWModSource::muteChannel(bool mute0, bool mute1)
{
    MsgConfigureChannelMute *msg = MsgConfigureChannelMute::create(mute0, mute1);
    m_inputMessageQueue.push(msg);
}

void BeamSteeringCWModSource::pull(const SampleVector::iterator& begin, unsigned int nbSamples, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    if (streamIndex == m_lastStream) {
        qWarning("BeamSteeringCWModSource::pull: twice same stream in a row: %u", streamIndex);
    }

    m_lastStream = streamIndex;
    m_vbegin[streamIndex] = begin;
    m_sizes[streamIndex] = nbSamples;

    if (streamIndex == 1)
    {
        unsigned int part1Begin, part1End, part2Begin, part2End, size;

        if (m_sizes[0] != m_sizes[1])
        {
            qWarning("BeamSteeringCWModSource::pull: unequal sizes: [0]: %d [1]: %d", m_sizes[0], m_sizes[1]);
            size = std::min(m_sizes[0], m_sizes[1]);
        }
        else
        {
            size = m_sizes[0];
        }

        std::vector<SampleVector>& data = m_sampleMOFifo.getData();
        m_sampleMOFifo.readSync(size, part1Begin, part1End, part2Begin, part2End);

        if (part1Begin != part1End)
        {
            std::copy(data[0].begin() + part1Begin, data[0].begin() + part1End, m_vbegin[0]);
            std::copy(data[1].begin() + part1Begin, data[1].begin() + part1End, m_vbegin[1]);
        }

        if (part2Begin != part2End)
        {
            std::copy(data[0].begin() + part2Begin, data[0].begin() + part2End, m_vbegin[0]);
            std::copy(data[1].begin() + part2Begin, data[1].begin() + part2End, m_vbegin[1]);
        }
    }
}

void BeamSteeringCWModSource::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    std::vector<SampleVector>& data = m_sampleMOFifo.getData();

    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    unsigned int remainder = m_sampleMOFifo.remainderSync();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleMOFifo.writeSync(ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleMOFifo.remainderSync();
    }
}

void BeamSteeringCWModSource::processFifo(std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend)
{
    for (unsigned int stream = 0; stream < 2; stream++) {
        m_channelizers[stream]->pull(data[stream].begin() + ibegin, iend - ibegin);
    }
}

void BeamSteeringCWModSource::handleInputMessages()
{
    qDebug("BeamSteeringCWModSource::handleInputMessage");
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool BeamSteeringCWModSource::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        int log2Interp = cfg.getLog2Interp();
        int filterChainHash = cfg.getFilterChainHash();

        qDebug() << "BeamSteeringCWModSource::handleMessage: MsgConfigureChannelizer:"
                << " log2Interp: " << log2Interp
                << " filterChainHash: " << filterChainHash;

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->setInterpolation(log2Interp, filterChainHash);
            m_streamSources[i].reset();
        }

        return true;
    }
    else if (MsgSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgSignalNotification& cfg = (MsgSignalNotification&) cmd;
        int outputSampleRate = cfg.getOutputSampleRate();

        qDebug() << "BeamSteeringCWModSource::handleMessage: MsgSignalNotification:"
                << " outputSampleRate: " << outputSampleRate
                << " centerFrequency: " << cfg.getCenterFrequency();

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->setOutputSampleRate(outputSampleRate);
            m_streamSources[i].reset();
        }

        return true;
    }
    else if (MsgConfigureSteeringAngle::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSteeringAngle& cfg = (MsgConfigureSteeringAngle&) cmd;
        float steeringAngle = cfg.getSteeringAngle();
        steeringAngle = steeringAngle < -M_PI ? -M_PI : steeringAngle > M_PI ? M_PI : steeringAngle;

        qDebug() << "BeamSteeringCWModSource::handleMessage: MsgConfigureSteeringAngle:"
                << " steeringAngle: " << steeringAngle;

        m_streamSources[1].setPhase(M_PI*cos(steeringAngle));

        return true;
    }
    else if (MsgConfigureChannelMute::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelMute& cfg = (MsgConfigureChannelMute&) cmd;
        m_streamSources[0].muteChannel(cfg.getMute0());
        m_streamSources[1].muteChannel(cfg.getMute1());

        qDebug() << "BeamSteeringCWModSource::handleMessage: MsgConfigureChannelMute:"
                << " mute0: " << cfg.getMute0()
                << " mute1: " << cfg.getMute1();

        return true;
    }
    else
    {
        qDebug("BeamSteeringCWModSource::handleMessage: unhandled: %s", cmd.getIdentifier());
        return false;
    }
}