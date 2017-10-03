///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef INCLUDE_SAMPLESINK_H
#define INCLUDE_SAMPLESINK_H

#include <QObject>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include "util/messagequeue.h"

class Message;

class SDRANGEL_API BasebandSampleSink : public QObject {
	Q_OBJECT
public:
	BasebandSampleSink();
	virtual ~BasebandSampleSink();

	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

protected:
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

protected slots:
	void handleInputMessages();
};

#endif // INCLUDE_SAMPLESINK_H
