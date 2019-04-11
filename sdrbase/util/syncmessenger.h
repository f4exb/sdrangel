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

#ifndef INCLUDE_UTIL_SYNCMESSENGER_H_
#define INCLUDE_UTIL_SYNCMESSENGER_H_

#include <QObject>
#include <QWaitCondition>
#include <QMutex>
#include <QAtomicInt>
#include "export.h"

class Message;

/**
 * This class is responsible of managing the synchronous processing of a message across threads
 */
class SDRBASE_API SyncMessenger : public QObject {
	Q_OBJECT

public:
	SyncMessenger();
	~SyncMessenger();

	int sendWait(Message& message, unsigned long msPollTime = 100); //!< Send message and waits for its process completion
    Message* getMessage() const { return m_message; }
    void storeMessage(Message& message) { m_message = &message; }
	void done(int result = 0); //!< Processing of the message is complete

signals:
	void messageSent();

protected:
	QWaitCondition m_waitCondition;
	QMutex m_mutex;
	QAtomicInt m_complete;
    Message *m_message;
	int m_result;
};



#endif /* INCLUDE_UTIL_SYNCMESSENGER_H_ */
