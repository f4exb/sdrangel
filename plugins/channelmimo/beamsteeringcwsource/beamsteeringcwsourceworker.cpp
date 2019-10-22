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

#include "beamsteeringcwsourceworker.h"


MESSAGE_CLASS_DEFINITION(BeamSteeringCWSourceWorker::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(BeamSteeringCWSourceWorker::MsgSignalNotification, Message)

BeamSteeringCWSourceWorker::BeamSteeringCWSourceWorker() :
    m_mutex(QMutex::Recursive)
{
    m_sampleMOFifo.init(2, 96000 * 8);
    m_vbegin.resize(2);

    for (int i = 0; i < 2; i++)
    {
        //TODO: m_channelizers[i] = new UpChannelizer(&m_sinks[i]);
        m_sizes[i] = 0;
    }

    QObject::connect(
        &m_sampleMOFifo,
        &SampleMOFifo::dataSyncRead,
        this,
        &BeamSteeringCWSourceWorker::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_lastStream = 0;
}

BeamSteeringCWSourceWorker::~BeamSteeringCWSourceWorker()
{
    for (int i = 0; i < 2; i++)
    {
        delete m_channelizers[i];
    }
}

void BeamSteeringCWSourceWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleMOFifo.reset();

    for (int i = 0; i < 2; i++) {
        //TODO: m_sinks[i].reset();
    }
}

void BeamSteeringCWSourceWorker::pull(const SampleVector::const_iterator& begin, unsigned int nbSamples, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    if (streamIndex == m_lastStream) {
        qWarning("BeamSteeringCWSourceWorker::pull: twice same stream in a row: %u", streamIndex);
    }

    m_lastStream = streamIndex;
    m_vbegin[streamIndex] = begin;
    m_sizes[streamIndex] = nbSamples;

    if (streamIndex == 1)
    {
        if (m_sizes[0] != m_sizes[1])
        {
            qWarning("BeamSteeringCWSourceWorker::pull: unequal sizes: [0]: %d [1]: %d", m_sizes[0], m_sizes[1]);
            m_sampleMOFifo.writeSync(m_vbegin, std::min(m_sizes[0], m_sizes[1]));
        }
        else
        {
            m_sampleMOFifo.writeSync(m_vbegin, m_sizes[0]);
        }
    }
}

void BeamSteeringCWSourceWorker::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    const std::vector<SampleVector>& data = m_sampleMOFifo.getData();

    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    unsigned int remainder = m_sampleMOFifo.remainderSync();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleMOFifo.readSync(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleMOFifo.remainderSync();
    }
}

void BeamSteeringCWSourceWorker::processFifo(const std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend)
{
    for (unsigned int stream = 0; stream < 2; stream++) {
        //TODO: m_channelizers[stream]->pull(data[stream].begin() + ibegin, data[stream].begin() + iend, false);
    }

    run();
}

void BeamSteeringCWSourceWorker::run()
{
    // TODO
}

void BeamSteeringCWSourceWorker::handleInputMessages()
{
    qDebug("BeamSteeringCWSourceWorker::handleInputMessage");
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool BeamSteeringCWSourceWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        int log2Interp = cfg.getLog2Interp();
        int filterChainHash = cfg.getFilterChainHash();

        qDebug() << "BeamSteeringCWSourceWorker::handleMessage: MsgConfigureChannelizer:"
                << " log2Interp: " << log2Interp
                << " filterChainHash: " << filterChainHash;

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->set(m_channelizers[i]->getInputMessageQueue(),
                log2Interp,
                filterChainHash);
            //TODO: m_sinks[i].reset();
        }

        return true;
    }
    else if (MsgSignalNotification::match(cmd))
    {
        MsgSignalNotification& cfg = (MsgSignalNotification&) cmd;
        int outputSampleRate = cfg.getOutputSampleRate();
        qint64 centerFrequency = cfg.getCenterFrequency();
        int streamIndex = cfg.getStreamIndex();

        qDebug() << "BeamSteeringCWSourceWorker::handleMessage: MsgSignalNotification:"
                << " outputSampleRate: " << outputSampleRate
                << " centerFrequency: " << centerFrequency
                << " streamIndex: " << streamIndex;

        if (streamIndex < 2)
        {
            DSPSignalNotification *notif = new DSPSignalNotification(outputSampleRate, centerFrequency);
            m_channelizers[streamIndex]->getInputMessageQueue()->push(notif);
        }

        return true;
    }
    else
    {
        qDebug("BeamSteeringCWSourceWorker::handleMessage: unhandled: %s", cmd.getIdentifier());
        return false;
    }
}