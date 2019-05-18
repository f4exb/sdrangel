///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#include <QDebug>

#include "dsp/dspcommands.h"
#include "threadedbasebandsamplesource.h"
#include "threadedbasebandsamplesink.h"
#include "devicesamplemimo.h"

#include "dspdevicemimoengine.h"

MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::SetSampleMIMO, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::GetErrorMessage, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::GetMIMODeviceDescription, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::ConfigureCorrection, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::SignalNotification, Message)

DSPDeviceMIMOEngine::DSPDeviceMIMOEngine(uint32_t uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
    m_state(StNotStarted),
    m_deviceSampleMIMO(nullptr)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	connect(&m_syncMessenger, SIGNAL(messageSent()), this, SLOT(handleSynchronousMessages()), Qt::QueuedConnection);

	moveToThread(this);
}

DSPDeviceMIMOEngine::~DSPDeviceMIMOEngine()
{
    stop();
	wait();
}

void DSPDeviceMIMOEngine::run()
{
	qDebug() << "DSPDeviceMIMOEngine::run";
	m_state = StIdle;
	exec();
}

void DSPDeviceMIMOEngine::start()
{
	qDebug() << "DSPDeviceMIMOEngine::start";
	QThread::start();
}

void DSPDeviceMIMOEngine::stop()
{
	qDebug() << "DSPDeviceMIMOEngine::stop";
    gotoIdle();
    m_state = StNotStarted;
    QThread::exit();
}

bool DSPDeviceMIMOEngine::initProcess()
{
	qDebug() << "DSPDeviceMIMOEngine::initGeneration";
	DSPGenerationInit cmd;

	return m_syncMessenger.sendWait(cmd) == StReady;
}

bool DSPDeviceMIMOEngine::startProcess()
{
	qDebug() << "DSPDeviceMIMOEngine::startGeneration";
	DSPGenerationStart cmd;

	return m_syncMessenger.sendWait(cmd) == StRunning;
}

void DSPDeviceMIMOEngine::stopProcess()
{
	qDebug() << "DSPDeviceMIMOEngine::stopGeneration";
	DSPGenerationStop cmd;
	m_syncMessenger.storeMessage(cmd);
	handleSynchronousMessages();
}

void DSPDeviceMIMOEngine::setMIMO(DeviceSampleMIMO* mimo)
{
	qDebug() << "DSPDeviceMIMOEngine::setMIMO";
	SetSampleMIMO cmd(mimo);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::setMIMOSequence(int sequence)
{
	qDebug("DSPDeviceMIMOEngine::setSinkSequence: seq: %d", sequence);
	m_sampleMIMOSequence = sequence;
}

void DSPDeviceMIMOEngine::addChannelSource(ThreadedBasebandSampleSource* source, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::addThreadedSource: "
        << source->objectName().toStdString().c_str()
        << " at: "
        << index;
	AddThreadedBasebandSampleSource cmd(source, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::removeChannelSource(ThreadedBasebandSampleSource* source, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::removeThreadedSource: "
        << source->objectName().toStdString().c_str()
        << " at: "
        << index;
	RemoveThreadedBasebandSampleSource cmd(source, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::addChannelSink(ThreadedBasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::addThreadedSink: "
        << sink->objectName().toStdString().c_str()
        << " at: "
        << index;
	AddThreadedBasebandSampleSink cmd(sink, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::removeChannelSink(ThreadedBasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::removeThreadedSink: "
        << sink->objectName().toStdString().c_str()
        << " at: "
        << index;
	RemoveThreadedBasebandSampleSink cmd(sink, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::addAncillarySink(BasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::addSink: "
        << sink->objectName().toStdString().c_str()
        << " at: "
        << index;
	AddBasebandSampleSink cmd(sink, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::removeAncillarySink(BasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::removeSink: "
        << sink->objectName().toStdString().c_str()
        << " at: "
        << index;
	RemoveBasebandSampleSink cmd(sink, index);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::addSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceMIMOEngine::addSpectrumSink: " << spectrumSink->objectName().toStdString().c_str();
	AddSpectrumSink cmd(spectrumSink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::removeSpectrumSink: " << spectrumSink->objectName().toStdString().c_str();
	DSPRemoveSpectrumSink cmd(spectrumSink);
	m_syncMessenger.sendWait(cmd);
}

QString DSPDeviceMIMOEngine::errorMessage()
{
	qDebug() << "DSPDeviceMIMOEngine::errorMessage";
	GetErrorMessage cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getErrorMessage();
}

QString DSPDeviceMIMOEngine::deviceDescription()
{
	qDebug() << "DSPDeviceMIMOEngine::deviceDescription";
	GetMIMODeviceDescription cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getDeviceDescription();
}

/**
 * Routes samples from device source FIFO to sink channels that are registered for the FIFO
 * Routes samples from source channels registered for the FIFO to the device sink FIFO
 */
void DSPDeviceMIMOEngine::work(int nbWriteSamples)
{
    (void) nbWriteSamples;
    // Sources
    for (unsigned int isource = 0; isource < m_deviceSampleMIMO->getNbSourceFifos(); isource++)
    {
        if (isource >= m_sourceStreamSampleRates.size()) {
            continue;
        }

        SampleSinkFifo* sampleFifo = m_deviceSampleMIMO->getSampleSinkFifo(isource); // sink FIFO is for Rx
	    std::size_t samplesDone = 0;
	    bool positiveOnly = false;

        while ((sampleFifo->fill() > 0) && (m_inputMessageQueue.size() == 0) && (samplesDone < m_sourceStreamSampleRates[isource]))
        {
            SampleVector::iterator part1begin;
            SampleVector::iterator part1end;
            SampleVector::iterator part2begin;
            SampleVector::iterator part2end;

            std::size_t count = sampleFifo->readBegin(sampleFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

            // first part of FIFO data
            if (part1begin != part1end)
            {
                // TODO: DC and IQ corrections

                // feed data to direct sinks
                if (isource < m_basebandSampleSinks.size())
                {
                    for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[isource].begin(); it != m_basebandSampleSinks[isource].end(); ++it) {
                        (*it)->feed(part1begin, part1end, positiveOnly);
                    }
                }

                // feed data to threaded sinks
                if (isource < m_threadedBasebandSampleSinks.size())
                {
                    for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks[isource].begin(); it != m_threadedBasebandSampleSinks[isource].end(); ++it) {
                        (*it)->feed(part1begin, part1end, positiveOnly);
                    }
                }
            }

       		// second part of FIFO data (used when block wraps around)
            if(part2begin != part2end)
            {
                // TODO: DC and IQ corrections

                // feed data to direct sinks
                if (isource < m_basebandSampleSinks.size())
                {
                    for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[isource].begin(); it != m_basebandSampleSinks[isource].end(); ++it) {
                        (*it)->feed(part2begin, part2end, positiveOnly);
                    }
                }

                // feed data to threaded sinks
                if (isource < m_threadedBasebandSampleSinks.size())
                {
                    for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks[isource].begin(); it != m_threadedBasebandSampleSinks[isource].end(); ++it) {
                        (*it)->feed(part2begin, part2end, positiveOnly);
                    }
                }
            }


            // adjust FIFO pointers
            sampleFifo->readCommit((unsigned int) count);
            samplesDone += count;
        } // while stream FIFO
    } // for stream source

    // TODO: sinks
}

// notStarted -> idle -> init -> running -+
//                ^                       |
//                +-----------------------+

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoIdle()
{
	qDebug() << "DSPDeviceMIMOEngine::gotoIdle";

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

	if (!m_deviceSampleMIMO) {
		return StIdle;
	}

	// stop everything

    std::vector<BasebandSampleSinks>::const_iterator vbit = m_basebandSampleSinks.begin();

	for (; vbit != m_basebandSampleSinks.end(); ++vbit)
	{
        for (BasebandSampleSinks::const_iterator it = vbit->begin(); it != vbit->end(); ++it) {
		    (*it)->stop();
        }
	}

    std::vector<ThreadedBasebandSampleSinks>::const_iterator vtit = m_threadedBasebandSampleSinks.begin();

    for (; vtit != m_threadedBasebandSampleSinks.end(); vtit++)
    {
        for (ThreadedBasebandSampleSinks::const_iterator it = vtit->begin(); it != vtit->end(); ++it) {
            (*it)->stop();
        }
    }

	m_deviceSampleMIMO->stop();
	m_deviceDescription.clear();

    for (std::vector<uint32_t>::iterator it = m_sourceStreamSampleRates.begin(); it != m_sourceStreamSampleRates.end(); ++it) {
        *it = 0;
    }

	return StIdle;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoInit()
{
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

	if (!m_deviceSampleMIMO) {
		return gotoError("No sample MIMO configured");
	}

	// init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion


	m_deviceDescription = m_deviceSampleMIMO->getDeviceDescription();

	qDebug() << "DSPDeviceMIMOEngine::gotoInit: "
	        << " m_deviceDescription: " << m_deviceDescription.toStdString().c_str();

    for (unsigned int isource = 0; isource < m_deviceSampleMIMO->getNbSourceFifos(); isource++)
    {
        if (isource < m_sourcesCorrections.size())
        {
            m_sourcesCorrections[isource].m_iOffset = 0;
            m_sourcesCorrections[isource].m_qOffset = 0;
            m_sourcesCorrections[isource].m_iRange = 1 << 16;
            m_sourcesCorrections[isource].m_qRange = 1 << 16;
        }

        if ((isource < m_sourceCenterFrequencies.size()) && (isource < m_sourceStreamSampleRates.size()))
        {
            m_sourceCenterFrequencies[isource] = m_deviceSampleMIMO->getSourceCenterFrequency(isource);
            m_sourceStreamSampleRates[isource] = m_deviceSampleMIMO->getSourceSampleRate(isource);

            qDebug("DSPDeviceMIMOEngine::gotoInit: m_sourceCenterFrequencies[%d] = %llu", isource,  m_sourceCenterFrequencies[isource]);
            qDebug("DSPDeviceMIMOEngine::gotoInit: m_sourceStreamSampleRates[%d] = %d", isource,  m_sourceStreamSampleRates[isource]);

	        DSPSignalNotification notif(m_sourceStreamSampleRates[isource], m_sourceCenterFrequencies[isource]);

            if (isource < m_basebandSampleSinks.size())
            {
                for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[isource].begin(); it != m_basebandSampleSinks[isource].end(); ++it)
                {
                    qDebug() << "DSPDeviceMIMOEngine::gotoInit: initializing " << (*it)->objectName().toStdString().c_str();
                    (*it)->handleMessage(notif);
                }
            }

            if (isource < m_threadedBasebandSampleSinks.size())
            {
                for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks[isource].begin(); it != m_threadedBasebandSampleSinks[isource].end(); ++it)
                {
                    qDebug() << "DSPDeviceMIMOEngine::gotoInit: initializing ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
                    (*it)->handleSinkMessage(notif);
                }
            }

            // pass data to listeners
            // if (m_deviceSampleSource->getMessageQueueToGUI())
            // {
            //     DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy for the output queue
            //     m_deviceSampleSource->getMessageQueueToGUI()->push(rep);
            // }
        }
    }

	return StReady;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoRunning()
{
	qDebug() << "DSPDeviceMIMOEngine::gotoRunning";

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

	if (!m_deviceSampleMIMO) {
		return gotoError("DSPDeviceMIMOEngine::gotoRunning: No sample source configured");
	}

	qDebug() << "DSPDeviceMIMOEngine::gotoRunning: " << m_deviceDescription.toStdString().c_str() << " started";

	// Start everything

	if (!m_deviceSampleMIMO->start()) {
		return gotoError("Could not start sample source");
	}

    std::vector<BasebandSampleSinks>::const_iterator vbit = m_basebandSampleSinks.begin();

	for (; vbit != m_basebandSampleSinks.end(); ++vbit)
	{
        for (BasebandSampleSinks::const_iterator it = vbit->begin(); it != vbit->end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting " << (*it)->objectName().toStdString().c_str();
		    (*it)->start();
        }
	}

    std::vector<ThreadedBasebandSampleSinks>::const_iterator vtit = m_threadedBasebandSampleSinks.begin();

    for (; vtit != m_threadedBasebandSampleSinks.end(); vtit++)
    {
        for (ThreadedBasebandSampleSinks::const_iterator it = vtit->begin(); it != vtit->end(); ++it)
        {
    		qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
            (*it)->start();
        }
    }

	qDebug() << "DSPDeviceMIMOEngine::gotoRunning:input message queue pending: " << m_inputMessageQueue.size();

	return StRunning;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoError(const QString& errorMessage)
{
	qDebug() << "DSPDeviceMIMOEngine::gotoError: " << errorMessage;

	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	m_state = StError;
	return StError;
}

void DSPDeviceMIMOEngine::handleData()
{
	if(m_state == StRunning)
	{
		work(0); // TODO: implement Tx side
	}
}

void DSPDeviceMIMOEngine::handleSetMIMO(DeviceSampleMIMO* mimo)
{
    m_deviceSampleMIMO = mimo;

    if (mimo && (mimo->getNbSinkFifos() > 0))
    {
        // if there is at least one Rx then the first Rx drives the FIFOs
		qDebug("DSPDeviceMIMOEngine::handleSetMIMO: set %s", qPrintable(mimo->getDeviceDescription()));
		connect(m_deviceSampleMIMO->getSampleSinkFifo(0), SIGNAL(dataReady()), this, SLOT(handleData()), Qt::QueuedConnection);
    }

    // TODO: only Tx
}

void DSPDeviceMIMOEngine::handleSynchronousMessages()
{
    Message *message = m_syncMessenger.getMessage();
	qDebug() << "DSPDeviceMIMOEngine::handleSynchronousMessages: " << message->getIdentifier();

	if (DSPGenerationInit::match(*message))
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
	else if (GetMIMODeviceDescription::match(*message))
	{
		((GetMIMODeviceDescription*) message)->setDeviceDescription(m_deviceDescription);
	}
	else if (DSPGetErrorMessage::match(*message))
	{
		((DSPGetErrorMessage*) message)->setErrorMessage(m_errorMessage);
	}
	else if (SetSampleMIMO::match(*message)) {
		handleSetMIMO(((SetSampleMIMO*) message)->getSampleMIMO());
	}
	else if (AddBasebandSampleSink::match(*message))
	{
        const AddBasebandSampleSink *msg = (AddBasebandSampleSink *) message;
		BasebandSampleSink* sink = msg->getSampleSink();
        unsigned int isource = msg->getIndex();

        if ((isource < m_basebandSampleSinks.size()) && (isource < m_sourceStreamSampleRates.size()) && (isource < m_sourceCenterFrequencies.size()))
        {
            m_basebandSampleSinks[isource].push_back(sink);
            // initialize sample rate and center frequency in the sink:
            DSPSignalNotification msg(m_sourceStreamSampleRates[isource], m_sourceCenterFrequencies[isource]);
            sink->handleMessage(msg);
            // start the sink:
            if(m_state == StRunning) {
                sink->start();
            }
        }
	}
	else if (RemoveBasebandSampleSink::match(*message))
	{
        const RemoveBasebandSampleSink *msg = (RemoveBasebandSampleSink *) message;
		BasebandSampleSink* sink = ((DSPRemoveBasebandSampleSink*) message)->getSampleSink();
        unsigned int isource = msg->getIndex();

        if (isource < m_basebandSampleSinks.size())
        {
            if(m_state == StRunning) {
                sink->stop();
            }

		    m_basebandSampleSinks[isource].remove(sink);
        }
	}
	else if (AddThreadedBasebandSampleSink::match(*message))
	{
        const AddThreadedBasebandSampleSink *msg = (AddThreadedBasebandSampleSink *) message;
		ThreadedBasebandSampleSink *threadedSink = msg->getThreadedSampleSink();
        unsigned int isource = msg->getIndex();

        if ((isource < m_threadedBasebandSampleSinks.size()) && (isource < m_sourceStreamSampleRates.size()) && (isource < m_sourceCenterFrequencies.size()))
        {
		    m_threadedBasebandSampleSinks[isource].push_back(threadedSink);
            // initialize sample rate and center frequency in the sink:
            DSPSignalNotification msg(m_sourceStreamSampleRates[isource], m_sourceCenterFrequencies[isource]);
            threadedSink->handleSinkMessage(msg);
            // start the sink:
            if(m_state == StRunning) {
                threadedSink->start();
            }
        }
	}
	else if (RemoveThreadedBasebandSampleSink::match(*message))
	{
        const RemoveThreadedBasebandSampleSink *msg = (RemoveThreadedBasebandSampleSink *) message;
		ThreadedBasebandSampleSink* threadedSink = msg->getThreadedSampleSink();
        unsigned int isource = msg->getIndex();

        if (isource < m_threadedBasebandSampleSinks.size())
        {
            threadedSink->stop();
            m_threadedBasebandSampleSinks[isource].remove(threadedSink);
        }
	}
	else if (AddThreadedBasebandSampleSource::match(*message))
	{
        const AddThreadedBasebandSampleSource *msg = (AddThreadedBasebandSampleSource *) message;
		ThreadedBasebandSampleSource *threadedSource = msg->getThreadedSampleSource();
        unsigned int isink = msg->getIndex();

        if ((isink < m_threadedBasebandSampleSources.size()) && (isink < m_sinkStreamSampleRates.size()) && (isink < m_sinkCenterFrequencies.size()))
        {
		    m_threadedBasebandSampleSources[isink].push_back(threadedSource);
            // initialize sample rate and center frequency in the sink:
            DSPSignalNotification msg(m_sourceStreamSampleRates[isink], m_sourceCenterFrequencies[isink]);
            threadedSource->handleSourceMessage(msg);
            // start the sink:
            if(m_state == StRunning) {
                threadedSource->start();
            }
        }
	}
	else if (RemoveThreadedBasebandSampleSource::match(*message))
	{
        const RemoveThreadedBasebandSampleSource *msg = (RemoveThreadedBasebandSampleSource *) message;
		ThreadedBasebandSampleSource* threadedSource = msg->getThreadedSampleSource();
        unsigned int isink = msg->getIndex();

        if (isink < m_threadedBasebandSampleSources.size())
        {
            threadedSource->stop();
            m_threadedBasebandSampleSources[isink].remove(threadedSource);
        }
	}

	m_syncMessenger.done(m_state);
}

void DSPDeviceMIMOEngine::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("DSPDeviceMIMOEngine::handleInputMessages: message: %s", message->getIdentifier());

		if (ConfigureCorrection::match(*message))
		{
			ConfigureCorrection* conf = (ConfigureCorrection*) message;
            unsigned int isource = conf->getIndex();

            if (isource < m_sourcesCorrections.size())
            {
                m_sourcesCorrections[isource].m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();

                if (m_sourcesCorrections[isource].m_dcOffsetCorrection != conf->getDCOffsetCorrection())
                {
                    m_sourcesCorrections[isource].m_dcOffsetCorrection = conf->getDCOffsetCorrection();
                    m_sourcesCorrections[isource].m_iOffset = 0;
                    m_sourcesCorrections[isource].m_qOffset = 0;

                    if (m_sourcesCorrections[isource].m_iqImbalanceCorrection != conf->getIQImbalanceCorrection())
                    {
                        m_sourcesCorrections[isource].m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();
                        m_sourcesCorrections[isource].m_iRange = 1 << 16;
                        m_sourcesCorrections[isource].m_qRange = 1 << 16;
                        m_sourcesCorrections[isource].m_imbalance = 65536;
                    }
                }

                m_sourcesCorrections[isource].m_avgAmp.reset();
                m_sourcesCorrections[isource].m_avgII.reset();
                m_sourcesCorrections[isource].m_avgII2.reset();
                m_sourcesCorrections[isource].m_avgIQ.reset();
                m_sourcesCorrections[isource].m_avgPhi.reset();
                m_sourcesCorrections[isource].m_avgQQ2.reset();
                m_sourcesCorrections[isource].m_iBeta.reset();
                m_sourcesCorrections[isource].m_qBeta.reset();
            }

			delete message;
		}
		else if (SignalNotification::match(*message))
		{
			SignalNotification *notif = (SignalNotification *) message;

			// update DSP values

            bool sourceOrSink = notif->getSourceOrSink();
            unsigned int istream = notif->getIndex();
			int sampleRate = notif->getSampleRate();
			qint64 centerFrequency = notif->getCenterFrequency();

			qDebug() << "DeviceMIMOEngine::handleInputMessages: SignalNotification:"
                << " sourceOrSink: " << sourceOrSink
                << " istream: " << istream
				<< " sampleRate: " << sampleRate
				<< " centerFrequency: " << centerFrequency;


            if (sourceOrSink)
            {
                if ((istream < m_sourceStreamSampleRates.size()) && (istream < m_sourceCenterFrequencies.size()))
                {
                    m_sourceStreamSampleRates[istream] = sampleRate;
                    m_sourceCenterFrequencies[istream] = centerFrequency;
                    DSPSignalNotification *message = new DSPSignalNotification(sampleRate, centerFrequency);

                    // forward source changes to ancillary sinks with immediate execution (no queuing)
                    if (istream < m_basebandSampleSinks.size())
                    {
                        for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[istream].begin(); it != m_basebandSampleSinks[istream].end(); ++it)
                        {
                            qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting " << (*it)->objectName().toStdString().c_str();
                            (*it)->handleMessage(*message);
                        }
                    }

                    // forward source changes to channel sinks with immediate execution (no queuing)
                    if (istream < m_threadedBasebandSampleSinks.size())
                    {
                        for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks[istream].begin(); it != m_threadedBasebandSampleSinks[istream].end(); ++it)
                        {
                            qDebug() << "DSPDeviceMIMOEngine::handleSourceMessages: forward message to ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
                            (*it)->handleSinkMessage(*message);
                        }
                    }

                    // forward changes to source GUI input queue
                    // MessageQueue *guiMessageQueue = m_deviceSampleSource->getMessageQueueToGUI();
                    // qDebug("DSPDeviceSourceEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    // if (guiMessageQueue) {
                    //     DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the source GUI
                    //     guiMessageQueue->push(rep);
                    // }

                    delete message;
                }
            }
            else
            {
                if ((istream < m_sinkStreamSampleRates.size()) && (istream < m_sinkCenterFrequencies.size()))
                {
                    m_sinkStreamSampleRates[istream] = sampleRate;
                    m_sinkCenterFrequencies[istream] = centerFrequency;
                    DSPSignalNotification *message = new DSPSignalNotification(sampleRate, centerFrequency);

                    // forward source changes to channel sources with immediate execution (no queuing)
                    if (istream < m_threadedBasebandSampleSources.size())
                    {
                        for (ThreadedBasebandSampleSources::const_iterator it = m_threadedBasebandSampleSources[istream].begin(); it != m_threadedBasebandSampleSources[istream].end(); ++it)
                        {
                            qDebug() << "DSPDeviceMIMOEngine::handleSinkMessages: forward message to ThreadedSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
                            (*it)->handleSourceMessage(*message);
                        }
                    }

                    // forward changes to source GUI input queue
                    // MessageQueue *guiMessageQueue = m_deviceSampleSource->getMessageQueueToGUI();
                    // qDebug("DSPDeviceSourceEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    // if (guiMessageQueue) {
                    //     DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the source GUI
                    //     guiMessageQueue->push(rep);
                    // }
                }

			    delete message;
            }
		}
	}
}

void DSPDeviceMIMOEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection, int isource)
{
	qDebug() << "DSPDeviceMIMOEngine::configureCorrections";
	ConfigureCorrection* cmd = new ConfigureCorrection(dcOffsetCorrection, iqImbalanceCorrection, isource);
	m_inputMessageQueue.push(cmd);
}
