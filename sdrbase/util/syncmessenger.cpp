///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "util/syncmessenger.h"
#include "util/message.h"

SyncMessenger::SyncMessenger() :
	m_complete(0),
	m_result(0)
{
	qRegisterMetaType<Message>("Message");
}

SyncMessenger::~SyncMessenger()
{}

int SyncMessenger::sendWait(Message& message, unsigned long msPollTime)
{
    m_message = &message;
	m_mutex.lock();
	m_complete.testAndSetAcquire(0, 1);

	emit messageSent();

	while (!m_complete.testAndSetAcquire(0, 1))
	{
		m_waitCondition.wait(&m_mutex, msPollTime);
	}

	m_complete = 0;
	int result = m_result;
	m_mutex.unlock();

	return result;
}

void SyncMessenger::done(int result)
{
	m_result = result;
	m_complete = 0;
	m_waitCondition.wakeAll();
}



