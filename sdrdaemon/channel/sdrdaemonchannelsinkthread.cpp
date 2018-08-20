///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) UDP sender thread                                 //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////


#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"
#include "channel/sdrdaemonchannelsinkthread.h"

SDRDaemonChannelSinkThread::SDRDaemonChannelSinkThread(SDRDaemonDataQueue *dataQueue, QObject* parent) :
    QThread(parent),
    m_running(false),
    m_dataQueue(dataQueue)
{
    connect(m_dataQueue, SIGNAL(dataBlockEnqueued()), this, SLOT(handleData()));
}

SDRDaemonChannelSinkThread::~SDRDaemonChannelSinkThread()
{
    stopWork();
}

void SDRDaemonChannelSinkThread::startWork()
{
    qDebug("SDRDaemonChannelSinkThread::startWork");
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void SDRDaemonChannelSinkThread::stopWork()
{
	qDebug("SDRDaemonChannelSinkThread::stopWork");
	m_running = false;
	wait();
}

void SDRDaemonChannelSinkThread::run()
{
    qDebug("SDRDaemonChannelSinkThread::run: begin");
	m_running = true;
	m_startWaiter.wakeAll();

    while (m_running)
    {
        sleep(1); // Do nothing as everything is in the data handler (dequeuer)
    }

    m_running = false;    
    qDebug("SDRDaemonChannelSinkThread::run: end");
}

bool SDRDaemonChannelSinkThread::handleDataBlock(SDRDaemonDataBlock& dataBlock)
{
    return true;
}

void SDRDaemonChannelSinkThread::handleData()
{
    SDRDaemonDataBlock* dataBlock;

    while (m_running && ((dataBlock = m_dataQueue->pop()) != 0))
    {
        if (handleDataBlock(*dataBlock))
        {
            delete dataBlock;
        }
    }    
}