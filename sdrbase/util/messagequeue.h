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

#ifndef INCLUDE_MESSAGEQUEUE_H
#define INCLUDE_MESSAGEQUEUE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include "export.h"

class Message;

class SDRBASE_API MessageQueue : public QObject {
	Q_OBJECT

public:
	MessageQueue(QObject* parent = NULL);
	~MessageQueue();

	void push(Message* message, bool emitSignal = true);  //!< Push message onto queue
	Message* pop(); //!< Pop message from queue

	int size(); //!< Returns queue size
	void clear(); //!< Empty queue

signals:
	void messageEnqueued();

private:
	QMutex m_lock;
	QQueue<Message*> m_queue;
};

#endif // INCLUDE_MESSAGEQUEUE_H
