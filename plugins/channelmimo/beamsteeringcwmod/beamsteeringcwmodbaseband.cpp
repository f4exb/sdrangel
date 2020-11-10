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

#include "dsp/upchannelizer.h"
#include "dsp/dspcommands.h"

#include "beamsteeringcwmodbaseband.h"


MESSAGE_CLASS_DEFINITION(BeamSteeringCWModBaseband::MsgConfigureBeamSteeringCWModBaseband, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWModBaseband::MsgSignalNotification, Message)

BeamSteeringCWModBaseband::BeamSteeringCWModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleMOFifo.init(2, SampleMOFifo::getSizePolicy(48000));
    m_vbegin.resize(2);

    for (int i = 0; i < 2; i++)
    {
        m_streamSources[i].setStreamIndex(i);
        m_channelizers[i] = new UpChannelizer(&m_streamSources[i]);
        m_sizes[i] = 0;
    }

    QObject::connect(
        &m_sampleMOFifo,
        &SampleMOFifo::dataReadSync,
        this,
        &BeamSteeringCWModBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_lastStream = 0;
}

BeamSteeringCWModBaseband::~BeamSteeringCWModBaseband()
{
    for (int i = 0; i < 2; i++) {
        delete m_channelizers[i];
    }
}

void BeamSteeringCWModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleMOFifo.reset();

    for (int i = 0; i < 2; i++)
    {
        m_streamSources[i].reset();
        m_sizes[i] = 0;
    }
}

void BeamSteeringCWModBaseband::pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    if (streamIndex == m_lastStream) {
        qWarning("BeamSteeringCWModBaseband::pull: twice same stream in a row: %u", streamIndex);
    }

    m_lastStream = streamIndex;
    m_vbegin[streamIndex] = begin;
    m_sizes[streamIndex] = nbSamples;

    if (streamIndex == 1)
    {
        unsigned int part1Begin, part1End, part2Begin, part2End, size;

        if (m_sizes[0] != m_sizes[1])
        {
            qWarning("BeamSteeringCWModBaseband::pull: unequal sizes: [0]: %d [1]: %d", m_sizes[0], m_sizes[1]);
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

void BeamSteeringCWModBaseband::handleData()
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
        m_sampleMOFifo.writeSync(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleMOFifo.remainderSync();
    }
}

void BeamSteeringCWModBaseband::processFifo(std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend)
{
    for (unsigned int stream = 0; stream < 2; stream++) {
        m_channelizers[stream]->pull(data[stream].begin() + ibegin, iend - ibegin);
    }
}

void BeamSteeringCWModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool BeamSteeringCWModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureBeamSteeringCWModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureBeamSteeringCWModBaseband& cfg = (MsgConfigureBeamSteeringCWModBaseband&) cmd;
        qDebug() << "BeamSteeringCWModBaseband::handleMessage: MsgConfigureBeamSteeringCWModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgSignalNotification& cfg = (MsgSignalNotification&) cmd;
        int basebandSampleRate = cfg.getBasebandSampleRate();

        qDebug() << "BeamSteeringCWModBaseband::handleMessage: MsgSignalNotification:"
                << " basebandSampleRate: " << basebandSampleRate;

        m_sampleMOFifo.resize(SampleMOFifo::getSizePolicy(basebandSampleRate));

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->setBasebandSampleRate(basebandSampleRate, true);
            m_streamSources[i].reset();
        }

        return true;
    }
    else
    {
        qDebug("BeamSteeringCWModBaseband::handleMessage: unhandled: %s", cmd.getIdentifier());
        return false;
    }
}

void BeamSteeringCWModBaseband::applySettings(const BeamSteeringCWModSettings& settings, bool force)
{
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || (m_settings.m_log2Interp != settings.m_log2Interp) || force)
    {
        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->setInterpolation(settings.m_log2Interp, settings.m_filterChainHash);
            m_streamSources[i].reset();
        }
    }

    if ((m_settings.m_steerDegrees != settings.m_steerDegrees) || force)
    {
        float steeringAngle = settings.m_steerDegrees / 180.0f;
        steeringAngle = steeringAngle < -M_PI ? -M_PI : steeringAngle > M_PI ? M_PI : steeringAngle;
        m_streamSources[1].setPhase(M_PI*cos(steeringAngle));
    }

    if ((m_settings.m_channelOutput != settings.m_channelOutput) || force)
    {
        if (settings.m_channelOutput == 0)
        {
            m_streamSources[0].muteChannel(false);
            m_streamSources[1].muteChannel(false);
        }
        else if (settings.m_channelOutput == 1)
        {
            m_streamSources[0].muteChannel(false);
            m_streamSources[1].muteChannel(true);
        }
        else if (settings.m_channelOutput == 2)
        {
            m_streamSources[0].muteChannel(true);
            m_streamSources[1].muteChannel(false);
        }
        else
        {
            m_streamSources[0].muteChannel(false);
            m_streamSources[1].muteChannel(false);
        }
    }

    m_settings = settings;
}