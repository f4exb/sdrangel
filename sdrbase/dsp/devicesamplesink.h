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

#ifndef SDRBASE_DSP_DEVICESAMPLESINK_H_
#define SDRBASE_DSP_DEVICESAMPLESINK_H_

#include <QtGlobal>

#include "samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/export.h"

class SDRANGEL_API DeviceSampleSink : public QObject {
	Q_OBJECT
public:
	DeviceSampleSink();
	virtual ~DeviceSampleSink();
	virtual void destroy() = 0;

	virtual bool start() = 0;
	virtual void stop() = 0;

	virtual const QString& getDeviceDescription() const = 0;
	virtual int getSampleRate() const = 0; //!< Sample rate exposed by the sink
	virtual quint64 getCenterFrequency() const = 0; //!< Center frequency exposed by the sink

	virtual bool handleMessage(const Message& message) = 0;

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
	SampleSourceFifo* getSampleFifo() { return &m_sampleSourceFifo; }

protected slots:
	void handleInputMessages();

protected:
    SampleSourceFifo m_sampleSourceFifo;
	MessageQueue m_inputMessageQueue; //!< Input queue to the sink
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
};

#endif /* SDRBASE_DSP_DEVICESAMPLESINK_H_ */
