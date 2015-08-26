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

#ifndef INCLUDE_SAMPLESOURCE_H
#define INCLUDE_SAMPLESOURCE_H

#include <QtGlobal>
#include "dsp/samplefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/export.h"

class SDRANGELOVE_API SampleSource : public QObject {
	Q_OBJECT
public:
	SampleSource();
	virtual ~SampleSource();

	virtual bool init(const Message& cmd) = 0;
	virtual bool start(int device) = 0;
	virtual void stop() = 0;

	virtual const QString& getDeviceDescription() const = 0;
	virtual int getSampleRate() const = 0; //!< Sample rate exposed by the source
	virtual quint64 getCenterFrequency() const = 0; //!< Center frequency exposed by the source

	virtual bool handleMessage(const Message& message) = 0;
	
	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	MessageQueue *getOutputMessageQueue() { return &m_outputMessageQueue; }
	MessageQueue *getOutputMessageQueueToGUI() { return &m_outputMessageQueueToGUI; }
    SampleFifo* getSampleFifo() { return &m_sampleFifo; }

protected slots:
	void handleInputMessages();

protected:
    SampleFifo m_sampleFifo;
	MessageQueue m_inputMessageQueue; //!< Input queue to the source
	MessageQueue m_outputMessageQueue; //!< Generic output queue
	MessageQueue m_outputMessageQueueToGUI; //!< Output queue specialized for the source GUI
};

#endif // INCLUDE_SAMPLESOURCE_H
