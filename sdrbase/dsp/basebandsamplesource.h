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

#ifndef SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_
#define SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_

#include <QObject>
#include "dsp/dsptypes.h"
#include "dsp/samplesourcefifo.h"
#include "util/export.h"
#include "util/messagequeue.h"

class Message;

class SDRANGEL_API BasebandSampleSource : public QObject {
	Q_OBJECT
public:
	BasebandSampleSource();
	virtual ~BasebandSampleSource();

	virtual void start() = 0;
	virtual void stop() = 0;
	virtual void pull(Sample& sample) = 0;
	virtual void pullAudio(int nbSamples __attribute__((unused))) {}

    /** direct feeding of sample source FIFO */
	void feed(SampleSourceFifo* sampleFifo, int nbSamples)
	{
	    SampleVector::iterator writeAt;
	    sampleFifo->getWriteIterator(writeAt);
	    pullAudio(nbSamples); // Pre-fetch input audio samples this is mandatory to keep things running smoothly

	    for (int i = 0; i < nbSamples; i++)
	    {
	        pull((*writeAt));
	        sampleFifo->bumpIndex(writeAt);
	    }
	}

	SampleSourceFifo& getSampleSourceFifo() { return m_sampleFifo; }

	virtual bool handleMessage(const Message& cmd) = 0; //!< Processing of a message. Returns true if message has actually been processed

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; } //!< Get the queue for asynchronous inbound communication
    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
    void setDeviceSampleSourceFifo(SampleSourceFifo *deviceSampleFifo);

protected:
	MessageQueue m_inputMessageQueue;     //!< Queue for asynchronous inbound communication
    MessageQueue *m_guiMessageQueue;      //!< Input message queue to the GUI
	SampleSourceFifo m_sampleFifo;        //!< Internal FIFO for multi-channel processing
	SampleSourceFifo *m_deviceSampleFifo; //!< Reference to the device FIFO for single channel processing

	void handleWriteToFifo(SampleSourceFifo *sampleFifo, int nbSamples);

protected slots:
	void handleInputMessages();
	void handleWriteToFifo(int nbSamples);
    void handleWriteToDeviceFifo(int nbSamples);
};

#endif /* SDRBASE_DSP_BASEBANDSAMPLESOURCE_H_ */
