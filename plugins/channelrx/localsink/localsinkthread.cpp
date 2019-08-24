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

#include "localsinkthread.h"

MESSAGE_CLASS_DEFINITION(LocalSinkThread::MsgStartStop, Message)

LocalSinkThread::LocalSinkThread(QObject* parent) :
    QThread(parent),
    m_running(false),
    m_sampleFifo(0)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

LocalSinkThread::~LocalSinkThread()
{
    qDebug("LocalSinkThread::~LocalSinkThread");
}

void LocalSinkThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void LocalSinkThread::startWork()
{
    qDebug("LocalSinkThread::startWork");
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void LocalSinkThread::stopWork()
{
	qDebug("LocalSinkThread::stopWork");
	m_running = false;
	wait();
}

void LocalSinkThread::run()
{
    qDebug("LocalSinkThread::run: begin");
	m_running = true;
	m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;
    qDebug("LocalSinkThread::run: end");
}

void LocalSinkThread::processSamples(const quint8* data, uint count)
{
    if (m_sampleFifo && m_running) {
        m_sampleFifo->write(data, count);
    }
}

void LocalSinkThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("LocalSinkThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}
