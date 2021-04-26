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

#ifndef SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_
#define SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_

#include <QObject>
#include "dsp/dsptypes.h"
#include "export.h"
#include "util/messagequeue.h"

class Message;

class SDRBASE_API BasebandSampleSource : public QObject {
	Q_OBJECT
public:
	BasebandSampleSource();
	virtual ~BasebandSampleSource();

	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples) = 0;
	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication

protected:
	MessageQueue m_inputMessageQueue;     //!< Queue for asynchronous inbound communication

protected slots:
	void handleInputMessages();
};

#endif /* SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_ */
