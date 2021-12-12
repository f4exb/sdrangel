///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remtoe sink channel (Rx) data blocks queue                                    //
//                                                                               //
// SDRangel can serve as a remote SDR front end that handles the interface       //
// with a physical device and sends or receives the I/Q samples stream via UDP   //
// to or from another SDRangel instance or any program implementing the same     //
// protocol. The remote SDRangel is controlled via its Web REST API.             //
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

#include <channel/remotedatablock.h>
#include <channel/remotedataqueue.h>
#include <QDebug>
#include <QMutexLocker>

RemoteDataQueue::RemoteDataQueue(QObject* parent) :
	QObject(parent),
	m_lock(QMutex::Recursive),
	m_queue()
{
}

RemoteDataQueue::~RemoteDataQueue()
{
	RemoteDataFrame* data;

	while ((data = pop()) != nullptr)
	{
		qDebug() << "RemoteDataQueue::~RemoteDataQueue: data block was still in queue";
		delete data;
	}
}

void RemoteDataQueue::push(RemoteDataFrame* data, bool emitSignal)
{
	if (data)
	{
		m_lock.lock();
		m_queue.enqueue(data);
		// qDebug("RemoteDataQueue::push: %d", m_queue.size());
		m_lock.unlock();
	}

	if (emitSignal) {
		emit dataBlockEnqueued();
	}
}

RemoteDataFrame* RemoteDataQueue::pop()
{
	QMutexLocker locker(&m_lock);

	if (m_queue.isEmpty()) {
		return nullptr;
	} else {
		return m_queue.dequeue();
	}
}

int RemoteDataQueue::size()
{
	QMutexLocker locker(&m_lock);

	return m_queue.size();
}

void RemoteDataQueue::clear()
{
	QMutexLocker locker(&m_lock);
	m_queue.clear();
}
