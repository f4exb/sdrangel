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

#include <stdio.h>
#include <QDebug>
#include <QThread>

#include "dspdevicesinkengine.h"

#include "dsp/basebandsamplesource.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesink.h"
#include "dsp/dspcommands.h"
#include "samplesourcefifo.h"
#include "threadedbasebandsamplesource.h"

DSPDeviceSinkEngine::DSPDeviceSinkEngine(uint32_t uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
	m_state(StNotStarted),
	m_deviceSampleSink(0),
	m_sampleSinkSequence(0),
	m_basebandSampleSources(),
	m_spectrumSink(0),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_multipleSourcesDivisionFactor(1)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	connect(&m_syncMessenger, SIGNAL(messageSent()), this, SLOT(handleSynchronousMessages()), Qt::QueuedConnection);

	moveToThread(this);
}

DSPDeviceSinkEngine::~DSPDeviceSinkEngine()
{
	wait();
}

void DSPDeviceSinkEngine::run()
{
	qDebug() << "DSPDeviceSinkEngine::run";

	m_state = StIdle;

    m_syncMessenger.done(); // Release start() that is waiting in main thread
	exec();
}

void DSPDeviceSinkEngine::start()
{
	qDebug() << "DSPDeviceSinkEngine::start";
	QThread::start();
}

void DSPDeviceSinkEngine::stop()
{
	qDebug() << "DSPDeviceSinkEngine::stop";
	DSPExit cmd;
	m_syncMessenger.sendWait(cmd);
}

bool DSPDeviceSinkEngine::initGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::initGeneration";
	DSPGenerationInit cmd;

	return m_syncMessenger.sendWait(cmd) == StReady;
}

bool DSPDeviceSinkEngine::startGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::startGeneration";
	DSPGenerationStart cmd;

	return m_syncMessenger.sendWait(cmd) == StRunning;
}

void DSPDeviceSinkEngine::stopGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::stopGeneration";
	DSPGenerationStop cmd;
	m_syncMessenger.storeMessage(cmd);
	handleSynchronousMessages();
}

void DSPDeviceSinkEngine::setSink(DeviceSampleSink* sink)
{
	qDebug() << "DSPDeviceSinkEngine::setSink";
	DSPSetSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::setSinkSequence(int sequence)
{
	qDebug("DSPDeviceSinkEngine::setSinkSequence: seq: %d", sequence);
	m_sampleSinkSequence = sequence;
}

void DSPDeviceSinkEngine::addSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::addSource: " << source->objectName().toStdString().c_str();
	DSPAddSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::removeSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::removeSource: " << source->objectName().toStdString().c_str();
	DSPRemoveSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::addThreadedSource(ThreadedBasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::addThreadedSource: " << source->objectName().toStdString().c_str();
	DSPAddThreadedSampleSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::removeThreadedSource(ThreadedBasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::removeThreadedSource: " << source->objectName().toStdString().c_str();
	DSPRemoveThreadedSampleSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::addSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::addSpectrumSink: " << spectrumSink->objectName().toStdString().c_str();
	DSPAddSpectrumSink cmd(spectrumSink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::removeSpectrumSink: " << spectrumSink->objectName().toStdString().c_str();
	DSPRemoveSpectrumSink cmd(spectrumSink);
	m_syncMessenger.sendWait(cmd);
}

QString DSPDeviceSinkEngine::errorMessage()
{
	qDebug() << "DSPDeviceSinkEngine::errorMessage";
	DSPGetErrorMessage cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getErrorMessage();
}

QString DSPDeviceSinkEngine::sinkDeviceDescription()
{
	qDebug() << "DSPDeviceSinkEngine::sinkDeviceDescription";
	DSPGetSinkDeviceDescription cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getDeviceDescription();
}

void DSPDeviceSinkEngine::work(int nbWriteSamples)
{
	SampleSourceFifo* sampleFifo = m_deviceSampleSink->getSampleFifo();
	//unsigned int nbWriteSamples = sampleFifo->getChunkSize();

	// single channel source handling
	if ((m_threadedBasebandSampleSources.size() + m_basebandSampleSources.size()) == 1)
	{
//        qDebug("DSPDeviceSinkEngine::work: single channel source handling");

        for (ThreadedBasebandSampleSources::iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
		{
			(*it)->feed(sampleFifo, nbWriteSamples);
		}
		for (BasebandSampleSources::iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
		{
			(*it)->feed(sampleFifo, nbWriteSamples);
		}
	}
	// multiple channel sources handling
	else if ((m_threadedBasebandSampleSources.size() + m_basebandSampleSources.size()) > 1)
	{
//	    qDebug("DSPDeviceSinkEngine::work: multiple channel sources handling: %u", m_multipleSourcesDivisionFactor);

//        SampleVector::iterator writeBegin;
//        sampleFifo->getWriteIterator(writeBegin);
//        SampleVector::iterator writeAt = writeBegin;
//	    Sample s;
//	    int sourceOccurence = 0;
//
//	    // Read a chunk of data from the sources
//
//	    for (ThreadedBasebandSampleSources::iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
//	    {
//	        ((*it)->getSampleSourceFifo()).readAdvance(m_threadedBasebandSampleSourcesIteratorMap[*it], nbWriteSamples);
//	        m_threadedBasebandSampleSourcesIteratorMap[*it] -= nbWriteSamples;
//	    }
//
//        for (BasebandSampleSources::iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
//        {
//            ((*it)->getSampleSourceFifo()).readAdvance(m_basebandSampleSourcesIteratorMap[*it], nbWriteSamples);
//            m_basebandSampleSourcesIteratorMap[*it] -= nbWriteSamples;
//        }
//
//        // Process sample by sample
//
//        for (int is = 0; is < nbWriteSamples; is++)
//        {
//            // pull data from threaded sources and merge them in the device sample FIFO
//            for (ThreadedBasebandSampleSources::iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
//            {
//                s = *m_threadedBasebandSampleSourcesIteratorMap[*it];
//                s /= (m_threadedBasebandSampleSources.size() + m_basebandSampleSources.size());
//
//                if (sourceOccurence == 0) {
//                    (*writeAt) = s;
//                } else {
//                    (*writeAt) += s;
//                }
//
//                sourceOccurence++;
//                m_threadedBasebandSampleSourcesIteratorMap[*it]++;
//            }
//
//            // pull data from direct sources and merge them in the device sample FIFO
//            for (BasebandSampleSources::iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
//            {
//                s = *m_basebandSampleSourcesIteratorMap[*it];
//                s /= (m_threadedBasebandSampleSources.size() + m_basebandSampleSources.size());
//
//                if (sourceOccurence == 0) {
//                    (*writeAt) = s;
//                } else {
//                    (*writeAt) += s;
//                }
//
//                sourceOccurence++;
//                m_basebandSampleSourcesIteratorMap[*it]++;
//            }
//
//            sampleFifo->bumpIndex(writeAt);
//            sourceOccurence = 0;
//        }

	    SampleVector::iterator writeBegin;
	    sampleFifo->getWriteIterator(writeBegin);
	    SampleVector::iterator writeAt = writeBegin;
	    Sample s;
	    int sourceOccurence = 0;

	    for (ThreadedBasebandSampleSources::iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
	    {
	        (*it)->pullAudio(nbWriteSamples);
	    }

	    for (BasebandSampleSources::iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
	    {
	        (*it)->pullAudio(nbWriteSamples);
	    }

	    for (int is = 0; is < nbWriteSamples; is++)
		{
			// pull data from threaded sources and merge them in the device sample FIFO
			for (ThreadedBasebandSampleSources::iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
			{
				(*it)->pull(s);
				s /= m_multipleSourcesDivisionFactor;

				if (sourceOccurence == 0) {
				    (*writeAt) = s;
				} else {
	                (*writeAt) += s;
				}

				sourceOccurence++;
			}

			// pull data from direct sources and merge them in the device sample FIFO
			for (BasebandSampleSources::iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
			{
				(*it)->pull(s);
				s /= m_multipleSourcesDivisionFactor;

                if (sourceOccurence == 0) {
                    (*writeAt) = s;
                } else {
                    (*writeAt) += s;
                }

                sourceOccurence++;
			}

			sampleFifo->bumpIndex(writeAt);
            sourceOccurence = 0;
		}

		// feed the mix to the main spectrum sink
//		if (m_spectrumSink)
//		{
//			m_spectrumSink->feed(writeBegin, writeBegin + nbWriteSamples, false);
//		}
	}
}

// notStarted -> idle -> init -> running -+
//                ^                       |
//                +-----------------------+

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoIdle()
{
	qDebug() << "DSPDeviceSinkEngine::gotoIdle";

	switch(m_state) {
		case StNotStarted:
			return StNotStarted;

		case StIdle:
		case StError:
			return StIdle;

		case StReady:
		case StRunning:
			break;
	}

	if(m_deviceSampleSink == 0)
	{
		return StIdle;
	}

	// stop everything

	for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
	{
        qDebug() << "DSPDeviceSinkEngine::gotoIdle: stopping " << (*it)->objectName().toStdString().c_str();
		(*it)->stop();
	}

	for(ThreadedBasebandSampleSources::const_iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); it++)
	{
	    qDebug() << "DSPDeviceSinkEngine::gotoIdle: stopping ThreadedSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
		(*it)->stop();
	}

	disconnect(m_deviceSampleSink->getSampleFifo(), SIGNAL(dataRead(int)), this, SLOT(handleForwardToSpectrumSink(int)));

	m_spectrumSink->stop();
	m_deviceSampleSink->stop();
	m_deviceDescription.clear();
	m_sampleRate = 0;

	return StIdle;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoInit()
{
	qDebug() << "DSPDeviceSinkEngine::gotoInit";

	switch(m_state) {
		case StNotStarted:
			return StNotStarted;

		case StRunning: // FIXME: assumes it goes first through idle state. Could we get back to init from running directly?
			return StRunning;

		case StReady:
			return StReady;

		case StIdle:
		case StError:
			break;
	}

	if (m_deviceSampleSink == 0)
	{
		return gotoError("DSPDeviceSinkEngine::gotoInit: No sample source configured");
	}

	// init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion

	m_deviceDescription = m_deviceSampleSink->getDeviceDescription();
	m_centerFrequency = m_deviceSampleSink->getCenterFrequency();
	m_sampleRate = m_deviceSampleSink->getSampleRate();

	qDebug() << "DSPDeviceSinkEngine::gotoInit: " << m_deviceDescription.toStdString().c_str() << ": "
			<< " sampleRate: " << m_sampleRate
			<< " centerFrequency: " << m_centerFrequency;

	DSPSignalNotification notif(m_sampleRate, m_centerFrequency);

	for (BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
	{
		qDebug() << "DSPDeviceSinkEngine::gotoInit: initializing " << (*it)->objectName().toStdString().c_str();
		(*it)->handleMessage(notif);
	}

	for (ThreadedBasebandSampleSources::const_iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
	{
		qDebug() << "DSPDeviceSinkEngine::gotoInit: initializing ThreadedSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
		(*it)->handleSourceMessage(notif);
	}

	m_spectrumSink->handleMessage(notif);

	// pass data to listeners
	if (m_deviceSampleSink->getMessageQueueToGUI())
	{
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy for the output queue
        m_deviceSampleSink->getMessageQueueToGUI()->push(rep);
	}

	return StReady;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoRunning()
{
	qDebug() << "DSPDeviceSinkEngine::gotoRunning";

	switch(m_state)
    {
		case StNotStarted:
			return StNotStarted;

		case StIdle:
			return StIdle;

		case StRunning:
			return StRunning;

		case StReady:
		case StError:
			break;
	}

	if(m_deviceSampleSink == 0) {
		return gotoError("DSPDeviceSinkEngine::gotoRunning: No sample source configured");
	}

	qDebug() << "DSPDeviceSinkEngine::gotoRunning: " << m_deviceDescription.toStdString().c_str() << " started";

	// Start everything

	if(!m_deviceSampleSink->start())
	{
		return gotoError("DSPDeviceSinkEngine::gotoRunning: Could not start sample source");
	}

	uint32_t nbSources = m_basebandSampleSources.size() + m_threadedBasebandSampleSources.size();

	if (nbSources == 0) {
	    m_multipleSourcesDivisionFactor = 1;
	} else if (nbSources < 3) {
	    m_multipleSourcesDivisionFactor = nbSources;
	} else {
	    m_multipleSourcesDivisionFactor = 1<<nbSources;
	}

	for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
	{
        qDebug() << "DSPDeviceSinkEngine::gotoRunning: starting " << (*it)->objectName().toStdString().c_str();
        // not used with sample by sample processing
//        (*it)->getSampleSourceFifo().resize(2*m_deviceSampleSink->getSampleFifo()->size()); // Equalize channel source FIFO size with device sink FIFO size
		(*it)->start();
	}

	for (ThreadedBasebandSampleSources::const_iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
	{
		qDebug() << "DSPDeviceSinkEngine::gotoRunning: starting ThreadedSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
        // not used with sample by sample processing
//		(*it)->getSampleSourceFifo().resize(2*m_deviceSampleSink->getSampleFifo()->size()); // Equalize channel source FIFO size with device sink FIFO size
		(*it)->start();
	}

	connect(m_deviceSampleSink->getSampleFifo(), SIGNAL(dataRead(int)), this, SLOT(handleForwardToSpectrumSink(int)));
	m_spectrumSink->start();

	qDebug() << "DSPDeviceSinkEngine::gotoRunning: input message queue pending: " << m_inputMessageQueue.size();

	return StRunning;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoError(const QString& errorMessage)
{
	qDebug() << "DSPDeviceSinkEngine::gotoError";

	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	m_state = StError;
	return StError;
}

void DSPDeviceSinkEngine::handleSetSink(DeviceSampleSink* sink)
{
	gotoIdle();

//	if(m_sampleSource != 0)
//	{
//		disconnect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()));
//	}

	m_deviceSampleSink = sink;

	if(m_deviceSampleSink != 0)
	{
		qDebug("DSPDeviceSinkEngine::handleSetSink: set %s", qPrintable(sink->getDeviceDescription()));
		connect(m_deviceSampleSink->getSampleFifo(), SIGNAL(dataWrite(int)), this, SLOT(handleData(int)), Qt::QueuedConnection);
	}
	else
	{
		qDebug("DSPDeviceSinkEngine::handleSetSource: set none");
	}
}

void DSPDeviceSinkEngine::handleData(int nbSamples)
{
	if(m_state == StRunning)
	{
		work(nbSamples);
	}
}

void DSPDeviceSinkEngine::handleSynchronousMessages()
{
    Message *message = m_syncMessenger.getMessage();
	qDebug() << "DSPDeviceSinkEngine::handleSynchronousMessages: " << message->getIdentifier();

	if (DSPExit::match(*message))
	{
		gotoIdle();
		m_state = StNotStarted;
		exit();
	}
	else if (DSPGenerationInit::match(*message))
	{
		m_state = gotoIdle();

		if(m_state == StIdle) {
			m_state = gotoInit(); // State goes ready if init is performed
		}
	}
	else if (DSPGenerationStart::match(*message))
	{
		if(m_state == StReady) {
			m_state = gotoRunning();
		}
	}
	else if (DSPGenerationStop::match(*message))
	{
		m_state = gotoIdle();
	}
	else if (DSPGetSinkDeviceDescription::match(*message))
	{
		((DSPGetSinkDeviceDescription*) message)->setDeviceDescription(m_deviceDescription);
	}
	else if (DSPGetErrorMessage::match(*message))
	{
		((DSPGetErrorMessage*) message)->setErrorMessage(m_errorMessage);
	}
	else if (DSPSetSink::match(*message)) {
		handleSetSink(((DSPSetSink*) message)->getSampleSink());
	}
	else if (DSPAddSpectrumSink::match(*message))
	{
		m_spectrumSink = ((DSPAddSpectrumSink*) message)->getSampleSink();
	}
	else if (DSPRemoveSpectrumSink::match(*message))
	{
		BasebandSampleSink* spectrumSink = ((DSPRemoveSpectrumSink*) message)->getSampleSink();

		if(m_state == StRunning) {
			spectrumSink->stop();
		}

		m_spectrumSink = 0;
	}
	else if (DSPAddSource::match(*message))
	{
		BasebandSampleSource* source = ((DSPAddSource*) message)->getSampleSource();
		m_basebandSampleSources.push_back(source);
        // not used with sample by sample processing
//		BasebandSampleSourcesIteratorMapKV kv;
//		kv.first = source;
//		(source->getSampleSourceFifo()).getReadIterator(kv.second);
//		m_basebandSampleSourcesIteratorMap.insert(kv);
	}
	else if (DSPRemoveSource::match(*message))
	{
		BasebandSampleSource* source = ((DSPRemoveSource*) message)->getSampleSource();

		if(m_state == StRunning) {
			source->stop();
		}

        // not used with sample by sample processing
//		m_basebandSampleSourcesIteratorMap.erase(source);
		m_basebandSampleSources.remove(source);
	}
	else if (DSPAddThreadedSampleSource::match(*message))
	{
		ThreadedBasebandSampleSource *threadedSource = ((DSPAddThreadedSampleSource*) message)->getThreadedSampleSource();
		m_threadedBasebandSampleSources.push_back(threadedSource);
        // not used with sample by sample processing
//        ThreadedBasebandSampleSourcesIteratorMapKV kv;
//        kv.first = threadedSource;
//        (threadedSource->getSampleSourceFifo()).getReadIterator(kv.second);
//        m_threadedBasebandSampleSourcesIteratorMap.insert(kv);
//		threadedSource->start();
	}
	else if (DSPRemoveThreadedSampleSource::match(*message))
	{
		ThreadedBasebandSampleSource* threadedSource = ((DSPRemoveThreadedSampleSource*) message)->getThreadedSampleSource();
		if (m_state == StRunning) {
		    threadedSource->stop();
		}
        // not used with sample by sample processing
//		m_threadedBasebandSampleSourcesIteratorMap.erase(threadedSource);
		m_threadedBasebandSampleSources.remove(threadedSource);
	}

	m_syncMessenger.done(m_state);
}

void DSPDeviceSinkEngine::handleInputMessages()
{
	qDebug() << "DSPDeviceSinkEngine::handleInputMessages";

	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("DSPDeviceSinkEngine::handleInputMessages: message: %s", message->getIdentifier());

		if (DSPSignalNotification::match(*message))
		{
			DSPSignalNotification *notif = (DSPSignalNotification *) message;

			// update DSP values

			m_sampleRate = notif->getSampleRate();
			m_centerFrequency = notif->getCenterFrequency();

			qDebug() << "DSPDeviceSinkEngine::handleInputMessages: DSPSignalNotification(" << m_sampleRate << "," << m_centerFrequency << ")";

            // forward source changes to sources with immediate execution

			for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
			{
				qDebug() << "DSPDeviceSinkEngine::handleInputMessages: forward message to " << (*it)->objectName().toStdString().c_str();
				(*it)->handleMessage(*message);
			}

			for (ThreadedBasebandSampleSources::const_iterator it = m_threadedBasebandSampleSources.begin(); it != m_threadedBasebandSampleSources.end(); ++it)
			{
				qDebug() << "DSPDeviceSinkEngine::handleSourceMessages: forward message to ThreadedSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
				(*it)->handleSourceMessage(*message);
			}

			// forward changes to listeners on DSP output queue
			if (m_deviceSampleSink->getMessageQueueToGUI())
			{
                DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the output queue
                m_deviceSampleSink->getMessageQueueToGUI()->push(rep);
			}

			delete message;
		}
	}
}

void DSPDeviceSinkEngine::handleForwardToSpectrumSink(int nbSamples)
{
	if (m_spectrumSink)
	{
		SampleSourceFifo* sampleFifo = m_deviceSampleSink->getSampleFifo();
		SampleVector::iterator readUntil;
		sampleFifo->getReadIterator(readUntil);
		m_spectrumSink->feed(readUntil - nbSamples, readUntil, false);
	}
}
