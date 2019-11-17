///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#ifndef INCLUDE_SAMPLESINK_H
#define INCLUDE_SAMPLESINK_H

#include <QObject>
#include "dsp/dsptypes.h"
#include "export.h"
#include "util/messagequeue.h"
#include "util/message.h"

class Message;

class SDRBASE_API BasebandSampleSink : public QObject {
	Q_OBJECT
public:
	/** Used to notify on which thread the sample sink is now running (with ThreadedSampleSink) */
    class SDRBASE_API MsgThreadedSink : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QThread* getThread() const { return m_thread; }

        static MsgThreadedSink* create(const QThread* thread)
        {
            return new MsgThreadedSink(thread);
        }

    private:
        const QThread *m_thread;

        MsgThreadedSink(const QThread *thread) :
            Message(),
            m_thread(thread)
        { }
    };

	BasebandSampleSink();
	virtual ~BasebandSampleSink();

	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

protected:
	MessageQueue m_inputMessageQueue; //!< Queue for asynchronous inbound communication
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI

protected slots:
	void handleInputMessages();
};

#endif // INCLUDE_SAMPLESINK_H
