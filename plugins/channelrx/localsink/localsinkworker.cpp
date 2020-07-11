///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "dsp/samplesinkfifo.h"

#include "localsinkworker.h"

MESSAGE_CLASS_DEFINITION(LocalSinkWorker::MsgStartStop, Message)

LocalSinkWorker::LocalSinkWorker(QObject* parent) :
    QObject(parent),
    m_running(false),
    m_sampleFifo(0)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

LocalSinkWorker::~LocalSinkWorker()
{
    qDebug("LocalSinkWorker::~LocalSinkWorker");
}

void LocalSinkWorker::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void LocalSinkWorker::startWork()
{
    qDebug("LocalSinkWorker::startWork");
    m_running = true;
}

void LocalSinkWorker::stopWork()
{
	m_running = false;
}

void LocalSinkWorker::handleData()
{
    while ((m_sampleFifo->fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

        std::size_t count = m_sampleFifo->readBegin(m_sampleFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

        if (m_deviceSampleFifo && m_running)
        {
            // first part of FIFO data
            if (part1begin != part1end) {
                m_deviceSampleFifo->write(part1begin,part1end);
            }

            // second part of FIFO data (used when block wraps around)
            if(part2begin != part2end) {
                m_deviceSampleFifo->write(part2begin, part2end);
            }
        }

		m_sampleFifo->readCommit((unsigned int) count);
    }
}

void LocalSinkWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("LocalSinkWorker::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}
