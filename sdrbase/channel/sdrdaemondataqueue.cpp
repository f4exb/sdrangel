///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) data blocks queue                                 //
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

#include <QDebug>
#include <QMutexLocker>
#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"

SDRDaemonDataQueue::SDRDaemonDataQueue(QObject* parent) :
	QObject(parent),
	m_lock(QMutex::Recursive),
	m_queue()
{
}

SDRDaemonDataQueue::~SDRDaemonDataQueue()
{
	SDRDaemonDataBlock* data;

	while ((data = pop()) != 0)
	{
		qDebug() << "SDRDaemonDataQueue::~SDRDaemonDataQueue: data block was still in queue";
		delete data;
	}
}

void SDRDaemonDataQueue::push(SDRDaemonDataBlock* data, bool emitSignal)
{
	if (data)
	{
		m_lock.lock();
		m_queue.append(data);
		m_lock.unlock();
	}

	if (emitSignal)
	{
		emit dataBlockEnqueued();
	}
}

SDRDaemonDataBlock* SDRDaemonDataQueue::pop()
{
	QMutexLocker locker(&m_lock);

	if (m_queue.isEmpty())
	{
		return 0;
	}
	else
	{
		return m_queue.takeFirst();
	}
}

int SDRDaemonDataQueue::size()
{
	QMutexLocker locker(&m_lock);

	return m_queue.size();
}

void SDRDaemonDataQueue::clear()
{
	QMutexLocker locker(&m_lock);
	m_queue.clear();
}
