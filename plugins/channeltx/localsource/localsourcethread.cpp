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

#include "dsp/samplesourcefifo.h"

#include "localsourcethread.h"

MESSAGE_CLASS_DEFINITION(LocalSourceThread::MsgStartStop, Message)

LocalSourceThread::LocalSourceThread(QObject* parent) :
    QThread(parent),
    m_running(false),
    m_sampleFifo(nullptr)
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

LocalSourceThread::~LocalSourceThread()
{
    qDebug("LocalSourceThread::~LocalSourceThread");
}

void LocalSourceThread::startStop(bool start)
{
    MsgStartStop *msg = MsgStartStop::create(start);
    m_inputMessageQueue.push(msg);
}

void LocalSourceThread::setSampleFifo(SampleSourceFifo *sampleFifo)
{
    m_sampleFifo = sampleFifo;
}

void LocalSourceThread::pullSamples(unsigned int count)
{
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    m_sampleFifo->read(count, iPart1Begin, iPart1End, iPart2Begin, iPart2End);
    emit samplesAvailable(iPart1Begin, iPart1End, iPart2Begin, iPart2End);
}

void LocalSourceThread::startWork()
{
    qDebug("LocalSourceThread::startWork");
	m_startWaitMutex.lock();
	start();

    while(!m_running) {
		m_startWaiter.wait(&m_startWaitMutex, 100);
    }

    m_startWaitMutex.unlock();
}

void LocalSourceThread::stopWork()
{
	qDebug("LocalSourceThread::stopWork");
	m_running = false;
	wait();
}

void LocalSourceThread::run()
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

void LocalSourceThread::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (MsgStartStop::match(*message))
        {
            MsgStartStop* notif = (MsgStartStop*) message;
            qDebug("LocalSourceThread::handleInputMessages: MsgStartStop: %s", notif->getStartStop() ? "start" : "stop");

            if (notif->getStartStop()) {
                startWork();
            } else {
                stopWork();
            }

            delete message;
        }
    }
}
