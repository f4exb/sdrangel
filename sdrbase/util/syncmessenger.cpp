///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QtGlobal>

#include "util/syncmessenger.h"
#include "util/message.h"

SyncMessenger::SyncMessenger() :
	m_complete(0),
    m_message(0),
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	m_complete.storeRelaxed(0);
#else
	m_complete.store(0);
#endif

	emit messageSent();

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	while (!m_complete.loadRelaxed()) {
		m_waitCondition.wait(&m_mutex, msPollTime);
	}
#else
	while (!m_complete.load()) {
		m_waitCondition.wait(&m_mutex, msPollTime);
	}
#endif

	int result = m_result;
	m_mutex.unlock();

	return result;
}

void SyncMessenger::done(int result)
{
	m_result = result;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	m_complete.storeRelaxed(1);
#else
	m_complete.store(1);
#endif
	m_waitCondition.wakeAll();
}



