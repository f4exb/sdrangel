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

#include "dspcommands.h"
#include "threadedbasebandsamplesource.h"
#include "threadedbasebandsamplesink.h"
#include "devicesamplemimo.h"
#include "mimochannel.h"

#include "dspdevicemimoengine.h"

MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::SetSampleMIMO, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveThreadedBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveThreadedBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddMIMOChannel, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveMIMOChannel, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveBasebandSampleSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveSpectrumSink, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::GetErrorMessage, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::GetMIMODeviceDescription, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::ConfigureCorrection, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::SetSpectrumSinkInput, Message)

DSPDeviceMIMOEngine::DSPDeviceMIMOEngine(uint32_t uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
    m_state(StNotStarted),
    m_deviceSampleMIMO(nullptr),
    m_spectrumInputSourceElseSink(true),
    m_spectrumInputIndex(0)
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
	qDebug() << "DSPDeviceMIMOEngine::initProcess";
	DSPGenerationInit cmd;

	return m_syncMessenger.sendWait(cmd) == StReady;
}

bool DSPDeviceMIMOEngine::startProcess()
{
	qDebug() << "DSPDeviceMIMOEngine::startProcess";
	DSPGenerationStart cmd;

	return m_syncMessenger.sendWait(cmd) == StRunning;
}

void DSPDeviceMIMOEngine::stopProcess()
{
	qDebug() << "DSPDeviceMIMOEngine::stopProcess";
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

void DSPDeviceMIMOEngine::addMIMOChannel(MIMOChannel *channel)
{
	qDebug() << "DSPDeviceMIMOEngine::addMIMOChannel: "
        << channel->objectName().toStdString().c_str();
    AddMIMOChannel cmd(channel);
    m_syncMessenger.sendWait(cmd);
}

void DSPDeviceMIMOEngine::removeMIMOChannel(MIMOChannel *channel)
{
	qDebug() << "DSPDeviceMIMOEngine::removeMIMOChannel: "
        << channel->objectName().toStdString().c_str();
    RemoveMIMOChannel cmd(channel);
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

void DSPDeviceMIMOEngine::setSpectrumSinkInput(bool sourceElseSink, int index)
{
	qDebug() << "DSPDeviceSinkEngine::setSpectrumSinkInput: "
        << " sourceElseSink: " << sourceElseSink
        << " index: " << index;
    SetSpectrumSinkInput cmd(sourceElseSink, index);
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

void DSPDeviceMIMOEngine::workSampleSinkFifos()
{
    SampleMIFifo* sampleFifo = m_deviceSampleMIMO->getSampleMIFifo();

    if (!sampleFifo) {
        return;
    }

    unsigned int iPart1Begin;
    unsigned int iPart1End;
    unsigned int iPart2Begin;
    unsigned int iPart2End;
    const std::vector<SampleVector>& data = sampleFifo->getData();
    //unsigned int samplesDone = 0;

    while ((sampleFifo->fillSync() > 0) && (m_inputMessageQueue.size() == 0))
    {
        //unsigned int count = sampleFifo->readSync(sampleFifo->fillSync(), iPart1Begin, iPart1End, iPart2Begin, iPart2End);
        sampleFifo->readSync(iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End)
        {
            for (unsigned int stream = 0; stream < data.size(); stream++) {
                workSamples(data[stream].begin() + iPart1Begin, data[stream].begin() + iPart1End, stream);
            }
        }

        if (iPart2Begin != iPart2End)
        {
            for (unsigned int stream = 0; stream < data.size(); stream++) {
                workSamples(data[stream].begin() + iPart2Begin, data[stream].begin() + iPart2End, stream);
            }
        }

        // for (unsigned int stream = 0; stream < data.size(); stream++)
        // {
        //     SampleVector::const_iterator begin = data[stream].begin();

        //     if (iPart1Begin != iPart1End) {
        //         m_vectorBuffer.write(data[stream].begin() + iPart1Begin, data[stream].begin() + iPart1End, false);
        //     }

        //     if (iPart2Begin != iPart2End) {
        //         m_vectorBuffer.write(data[stream].begin() + iPart2Begin, data[stream].begin() + iPart2End, false);
        //     }

        //     SampleVector::iterator vbegin, vend;
        //     m_vectorBuffer.read(vbegin, vend);
        //     workSamples(vbegin, vend, stream);
        // }

        //sampleFifo->readCommitSync(count);
        //samplesDone += count;
    }
}

void DSPDeviceMIMOEngine::workSampleSinkFifo(unsigned int stream)
{
    SampleMIFifo* sampleFifo = m_deviceSampleMIMO->getSampleMIFifo();

    if (!sampleFifo) {
        return;
    }

    SampleVector::const_iterator part1begin;
    SampleVector::const_iterator part1end;
    SampleVector::const_iterator part2begin;
    SampleVector::const_iterator part2end;

    while ((sampleFifo->fillAsync(stream) > 0) && (m_inputMessageQueue.size() == 0))
    {
        //unsigned int count = sampleFifo->readAsync(sampleFifo->fillAsync(stream), &part1begin, &part1end, &part2begin, &part2end, stream);
        sampleFifo->readAsync(&part1begin, &part1end, &part2begin, &part2end, stream);

        if (part1begin != part1end) { // first part of FIFO data
            m_vectorBuffer.write(part1begin, part1end, false);
        }

        if (part2begin != part2end) { // second part of FIFO data (used when block wraps around)
            m_vectorBuffer.append(part2begin, part2end);
        }

        SampleVector::iterator vbegin, vend;
        m_vectorBuffer.read(vbegin, vend);
        workSamples(vbegin, vend, stream);

        //sampleFifo->readCommitAsync(count, stream);
    }
}

/**
 * Routes samples from device source FIFO to sink channels that are registered for the FIFO
 * Routes samples from source channels registered for the FIFO to the device sink FIFO
 */
void DSPDeviceMIMOEngine::workSamples(const SampleVector::const_iterator& vbegin, const SampleVector::const_iterator& vend, unsigned int sinkIndex)
{
	bool positiveOnly = false;
    // DC and IQ corrections
    // if (m_sourcesCorrections[sinkIndex].m_dcOffsetCorrection) {
    //     iqCorrections(vbegin, vend, sinkIndex, m_sourcesCorrections[sinkIndex].m_iqImbalanceCorrection);
    // }

    // feed data to direct sinks
    if (sinkIndex < m_basebandSampleSinks.size())
    {
        for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[sinkIndex].begin(); it != m_basebandSampleSinks[sinkIndex].end(); ++it) {
            (*it)->feed(vbegin, vend, positiveOnly);
        }
    }

    // possibly feed data to spectrum sink
    if ((m_spectrumSink) && (m_spectrumInputSourceElseSink) && (sinkIndex == m_spectrumInputIndex)) {
        m_spectrumSink->feed(vbegin, vend, positiveOnly);
    }

    // feed data to threaded sinks
    for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks[sinkIndex].begin(); it != m_threadedBasebandSampleSinks[sinkIndex].end(); ++it)
    {
        (*it)->feed(vbegin, vend, positiveOnly);
    }

    // feed data to MIMO channels
    for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it) {
        (*it)->feed(vbegin, vend, sinkIndex);
    }
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
        for (BasebandSampleSinks::const_iterator it = vbit->begin(); it != vbit->end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping BasebandSampleSink: " << (*it)->objectName().toStdString().c_str();
		    (*it)->stop();
        }
	}

    std::vector<ThreadedBasebandSampleSinks>::const_iterator vtSinkIt = m_threadedBasebandSampleSinks.begin();

    for (; vtSinkIt != m_threadedBasebandSampleSinks.end(); vtSinkIt++)
    {
        for (ThreadedBasebandSampleSinks::const_iterator it = vtSinkIt->begin(); it != vtSinkIt->end(); ++it)
        {
    		qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping ThreadedBasebandSampleSource(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
            (*it)->stop();
        }
    }

    std::vector<ThreadedBasebandSampleSources>::const_iterator vtSourceIt = m_threadedBasebandSampleSources.begin();

    for (; vtSourceIt != m_threadedBasebandSampleSources.end(); vtSourceIt++)
    {
        for (ThreadedBasebandSampleSources::const_iterator it = vtSourceIt->begin(); it != vtSourceIt->end(); ++it)
        {
    		qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping ThreadedBasebandSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
            (*it)->stop();
        }
    }

    for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
    {
        qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping MIMOChannel: " << (*it)->objectName().toStdString().c_str();
        (*it)->stop();
    }

	m_deviceSampleMIMO->stop();
	m_deviceDescription.clear();

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

	qDebug() << "DSPDeviceMIMOEngine::gotoInit:"
	        << "m_deviceDescription: " << m_deviceDescription.toStdString().c_str();

    // Rx

    for (unsigned int isource = 0; isource < m_deviceSampleMIMO->getNbSinkFifos(); isource++)
    {
        if (isource < m_sourcesCorrections.size())
        {
            m_sourcesCorrections[isource].m_iOffset = 0;
            m_sourcesCorrections[isource].m_qOffset = 0;
            m_sourcesCorrections[isource].m_iRange = 1 << 16;
            m_sourcesCorrections[isource].m_qRange = 1 << 16;
        }

        quint64 sourceCenterFrequency = m_deviceSampleMIMO->getSourceCenterFrequency(isource);
        int sourceStreamSampleRate = m_deviceSampleMIMO->getSourceSampleRate(isource);

        qDebug("DSPDeviceMIMOEngine::gotoInit: m_sourceCenterFrequencies[%d] = %llu", isource,  sourceCenterFrequency);
        qDebug("DSPDeviceMIMOEngine::gotoInit: m_sourceStreamSampleRates[%d] = %d", isource,  sourceStreamSampleRate);

        DSPSignalNotification notif(sourceStreamSampleRate, sourceCenterFrequency);

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

        // Probably not necessary
        // // possibly forward to spectrum sink
        // if ((m_spectrumSink) && (m_spectrumInputSourceElseSink) && (isource == m_spectrumInputIndex)) {
        //     m_spectrumSink->handleMessage(notif);
        // }

        // // forward changes to MIMO GUI input queue
        // MessageQueue *guiMessageQueue = m_deviceSampleMIMO->getMessageQueueToGUI();

        // if (guiMessageQueue) {
        //     DSPMIMOSignalNotification* rep = new DSPMIMOSignalNotification(sourceStreamSampleRate, sourceCenterFrequency, true, isource); // make a copy for the MIMO GUI
        //     guiMessageQueue->push(rep);
        // }
    }

    //TODO: Tx

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

	qDebug() << "DSPDeviceMIMOEngine::gotoRunning:" << m_deviceDescription.toStdString().c_str() << "started";

	// Start everything

	if (!m_deviceSampleMIMO->start()) {
		return gotoError("Could not start sample source");
	}

    std::vector<BasebandSampleSinks>::const_iterator vbit = m_basebandSampleSinks.begin();

	for (; vbit != m_basebandSampleSinks.end(); ++vbit)
	{
        for (BasebandSampleSinks::const_iterator it = vbit->begin(); it != vbit->end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting BasebandSampleSink: " << (*it)->objectName().toStdString().c_str();
		    (*it)->start();
        }
	}

    std::vector<ThreadedBasebandSampleSinks>::const_iterator vtSinkIt = m_threadedBasebandSampleSinks.begin();

    for (; vtSinkIt != m_threadedBasebandSampleSinks.end(); vtSinkIt++)
    {
        for (ThreadedBasebandSampleSinks::const_iterator it = vtSinkIt->begin(); it != vtSinkIt->end(); ++it)
        {
    		qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting ThreadedBasebandSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
            (*it)->start();
        }
    }

    std::vector<ThreadedBasebandSampleSources>::const_iterator vtSourceIt = m_threadedBasebandSampleSources.begin();

    for (; vtSourceIt != m_threadedBasebandSampleSources.end(); vtSourceIt++)
    {
        for (ThreadedBasebandSampleSources::const_iterator it = vtSourceIt->begin(); it != vtSourceIt->end(); ++it)
        {
    		qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting ThreadedBasebandSampleSource(" << (*it)->getSampleSourceObjectName().toStdString().c_str() << ")";
            (*it)->start();
        }
    }

    for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
    {
        qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting MIMOChannel: " << (*it)->objectName().toStdString().c_str();
        (*it)->start();
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

void DSPDeviceMIMOEngine::handleDataRxSync()
{
	if (m_state == StRunning) {
        workSampleSinkFifos();
	}
}

void DSPDeviceMIMOEngine::handleDataRxAsync(int sinkIndex)
{
	if (m_state == StRunning) {
		workSampleSinkFifo(sinkIndex);
	}
}

void DSPDeviceMIMOEngine::handleSetMIMO(DeviceSampleMIMO* mimo)
{
    m_deviceSampleMIMO = mimo;

    if (!mimo) { // Early leave
        return;
    }

    for (int i = 0; i < m_deviceSampleMIMO->getNbSinkFifos(); i++)
    {
        m_basebandSampleSinks.push_back(BasebandSampleSinks());
        m_threadedBasebandSampleSinks.push_back(ThreadedBasebandSampleSinks());
        m_sourcesCorrections.push_back(SourceCorrection());
    }

    for (int i = 0; i < m_deviceSampleMIMO->getNbSourceFifos(); i++) {
        m_threadedBasebandSampleSources.push_back(ThreadedBasebandSampleSources());
    }

    if (m_deviceSampleMIMO->getMIMOType() == DeviceSampleMIMO::MIMOHalfSynchronous) // synchronous FIFOs on Rx and not with Tx
    {
        qDebug("DSPDeviceMIMOEngine::handleSetMIMO: synchronous sources set %s", qPrintable(mimo->getDeviceDescription()));
        // connect(m_deviceSampleMIMO->getSampleSinkFifo(m_sampleSinkConnectionIndexes[0]), SIGNAL(dataReady()), this, SLOT(handleData()), Qt::QueuedConnection);
        QObject::connect(
            m_deviceSampleMIMO->getSampleMIFifo(),
            &SampleMIFifo::dataSyncReady,
            this,
            &DSPDeviceMIMOEngine::handleDataRxSync,
            Qt::QueuedConnection
        );
    }
    else if (m_deviceSampleMIMO->getMIMOType() == DeviceSampleMIMO::MIMOAsynchronous) // asynchronous FIFOs
    {
        for (unsigned int stream = 0; stream < m_deviceSampleMIMO->getNbSourceStreams(); stream++)
        {
            qDebug("DSPDeviceMIMOEngine::handleSetMIMO: asynchronous sources set %s channel %u",
                qPrintable(mimo->getDeviceDescription()), stream);
            QObject::connect(
                m_deviceSampleMIMO->getSampleMIFifo(),
                &SampleMIFifo::dataAsyncReady,
                this,
                &DSPDeviceMIMOEngine::handleDataRxAsync,
                Qt::QueuedConnection
            );
            // QObject::connect(
            //     m_deviceSampleMIMO->getSampleSinkFifo(stream),
            //     &SampleSinkFifo::dataReady,
            //     this,
            //     [=](){ this->handleDataRxAsync(stream); },
            //     Qt::QueuedConnection
            // );
        }
    }

    // TODO: Tx
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

        if (isource < m_basebandSampleSinks.size())
        {
            m_basebandSampleSinks[isource].push_back(sink);
            // initialize sample rate and center frequency in the sink:
            int sourceStreamSampleRate = m_deviceSampleMIMO->getSourceSampleRate(isource);
            quint64 sourceCenterFrequency = m_deviceSampleMIMO->getSourceCenterFrequency(isource);
            DSPSignalNotification msg(sourceStreamSampleRate, sourceCenterFrequency);
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

        if (isource < m_threadedBasebandSampleSinks.size())
        {
		    m_threadedBasebandSampleSinks[isource].push_back(threadedSink);
            // initialize sample rate and center frequency in the sink:
            int sourceStreamSampleRate = m_deviceSampleMIMO->getSourceSampleRate(isource);
            quint64 sourceCenterFrequency = m_deviceSampleMIMO->getSourceCenterFrequency(isource);
            DSPSignalNotification msg(sourceStreamSampleRate, sourceCenterFrequency);
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

        if (isink < m_threadedBasebandSampleSources.size())
        {
		    m_threadedBasebandSampleSources[isink].push_back(threadedSource);
            // initialize sample rate and center frequency in the sink:
            int sinkStreamSampleRate = m_deviceSampleMIMO->getSinkSampleRate(isink);
            quint64 sinkCenterFrequency = m_deviceSampleMIMO->getSinkCenterFrequency(isink);
            DSPSignalNotification msg(sinkStreamSampleRate, sinkCenterFrequency);
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
    else if (AddMIMOChannel::match(*message))
    {
        const AddMIMOChannel *msg = (AddMIMOChannel *) message;
        MIMOChannel *channel = msg->getChannel();
        m_mimoChannels.push_back(channel);

        for (int isource = 0; isource < m_deviceSampleMIMO->getNbSourceStreams(); isource++)
        {
            DSPMIMOSignalNotification notif(
                m_deviceSampleMIMO->getSourceSampleRate(isource),
                m_deviceSampleMIMO->getSourceCenterFrequency(isource),
                true,
                isource
            );
            channel->handleMessage(notif);
        }

        for (int isink = 0; isink < m_deviceSampleMIMO->getNbSinkStreams(); isink++)
        {
            DSPMIMOSignalNotification notif(
                m_deviceSampleMIMO->getSourceSampleRate(isink),
                m_deviceSampleMIMO->getSourceCenterFrequency(isink),
                false,
                isink
            );
            channel->handleMessage(notif);
        }

        if (m_state == StRunning) {
            channel->start();
        }
    }
    else if (RemoveMIMOChannel::match(*message))
    {
        const RemoveMIMOChannel *msg = (RemoveMIMOChannel *) message;
        MIMOChannel *channel = msg->getChannel();
        channel->stop();
        m_mimoChannels.remove(channel);
    }
	else if (AddSpectrumSink::match(*message))
	{
		m_spectrumSink = ((AddSpectrumSink*) message)->getSampleSink();
	}
    else if (RemoveSpectrumSink::match(*message))
    {
        BasebandSampleSink* spectrumSink = ((DSPRemoveSpectrumSink*) message)->getSampleSink();
        spectrumSink->stop();

        if (!m_spectrumInputSourceElseSink && m_deviceSampleMIMO && (m_spectrumInputIndex <  m_deviceSampleMIMO->getNbSinkStreams()))
        {
            SampleSourceFifo *inputFIFO = m_deviceSampleMIMO->getSampleSourceFifo(m_spectrumInputIndex);
            disconnect(inputFIFO, SIGNAL(dataRead(int)), this, SLOT(handleForwardToSpectrumSink(int)));
        }

        m_spectrumSink = nullptr;
    }
    else if (SetSpectrumSinkInput::match(*message))
    {
        const SetSpectrumSinkInput *msg = (SetSpectrumSinkInput *) message;
        bool spectrumInputSourceElseSink = msg->getSourceElseSink();
        unsigned int spectrumInputIndex = msg->getIndex();

        if ((spectrumInputSourceElseSink != m_spectrumInputSourceElseSink) || (spectrumInputIndex != m_spectrumInputIndex))
        {
            if (!m_spectrumInputSourceElseSink) // remove the source listener
            {
                SampleSourceFifo *inputFIFO = m_deviceSampleMIMO->getSampleSourceFifo(m_spectrumInputIndex);
                disconnect(inputFIFO, SIGNAL(dataRead(int)), this, SLOT(handleForwardToSpectrumSink(int)));
            }

            if ((!spectrumInputSourceElseSink) && (spectrumInputIndex <  m_deviceSampleMIMO->getNbSinkStreams())) // add the source listener
            {
                SampleSourceFifo *inputFIFO = m_deviceSampleMIMO->getSampleSourceFifo(spectrumInputIndex);
                connect(inputFIFO, SIGNAL(dataRead(int)), this, SLOT(handleForwardToSpectrumSink(int)));

                if (m_spectrumSink)
                {
                    DSPSignalNotification notif(
                        m_deviceSampleMIMO->getSinkSampleRate(spectrumInputIndex),
                        m_deviceSampleMIMO->getSinkCenterFrequency(spectrumInputIndex));
                    m_spectrumSink->handleMessage(notif);
                }
            }

            if (m_spectrumSink && (spectrumInputSourceElseSink) && (spectrumInputIndex <  m_deviceSampleMIMO->getNbSinkFifos()))
            {
                DSPSignalNotification notif(
                    m_deviceSampleMIMO->getSourceSampleRate(spectrumInputIndex),
                    m_deviceSampleMIMO->getSourceCenterFrequency(spectrumInputIndex));
                m_spectrumSink->handleMessage(notif);
            }

            m_spectrumInputSourceElseSink = spectrumInputSourceElseSink;
            m_spectrumInputIndex = spectrumInputIndex;
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
                m_sourcesCorrections[isource].m_iBeta.reset();
                m_sourcesCorrections[isource].m_qBeta.reset();
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
		else if (DSPMIMOSignalNotification::match(*message))
		{
			DSPMIMOSignalNotification *notif = (DSPMIMOSignalNotification *) message;

			// update DSP values

            bool sourceElseSink = notif->getSourceOrSink();
            unsigned int istream = notif->getIndex();
			int sampleRate = notif->getSampleRate();
			qint64 centerFrequency = notif->getCenterFrequency();

			qDebug() << "DeviceMIMOEngine::handleInputMessages: DSPMIMOSignalNotification:"
                << " sourceElseSink: " << sourceElseSink
                << " istream: " << istream
				<< " sampleRate: " << sampleRate
				<< " centerFrequency: " << centerFrequency;

            for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
            {
                DSPMIMOSignalNotification *message = new DSPMIMOSignalNotification(*notif);
                (*it)->handleMessage(*message);
            }

            if (sourceElseSink)
            {
                if ((istream < m_deviceSampleMIMO->getNbSourceStreams()))
                {
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

                    // forward changes to MIMO GUI input queue
                    MessageQueue *guiMessageQueue = m_deviceSampleMIMO->getMessageQueueToGUI();
                    qDebug("DeviceMIMOEngine::handleInputMessages: DSPMIMOSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    if (guiMessageQueue) {
                        DSPMIMOSignalNotification* rep = new DSPMIMOSignalNotification(*notif); // make a copy for the MIMO GUI
                        guiMessageQueue->push(rep);
                    }

                    // forward changes to spectrum sink if currently active
                    if (m_spectrumSink && m_spectrumInputSourceElseSink && (m_spectrumInputIndex == istream))
                    {
                        DSPSignalNotification spectrumNotif(sampleRate, centerFrequency);
                        m_spectrumSink->handleMessage(spectrumNotif);
                    }
                }
            }
            else
            {
                if ((istream < m_deviceSampleMIMO->getNbSinkStreams()))
                {
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

                    // forward changes to MIMO GUI input queue
                    MessageQueue *guiMessageQueue = m_deviceSampleMIMO->getMessageQueueToGUI();
                    qDebug("DSPDeviceMIMOEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    if (guiMessageQueue) {
                        DSPMIMOSignalNotification* rep = new DSPMIMOSignalNotification(*notif); // make a copy for the source GUI
                        guiMessageQueue->push(rep);
                    }

                    // forward changes to spectrum sink if currently active
                    if (m_spectrumSink && !m_spectrumInputSourceElseSink && (m_spectrumInputIndex == istream))
                    {
                        DSPSignalNotification spectrumNotif(sampleRate, centerFrequency);
                        m_spectrumSink->handleMessage(spectrumNotif);
                    }
                }
            }

            delete message;
		}
	}
}

void DSPDeviceMIMOEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection, int isource)
{
	qDebug() << "DSPDeviceMIMOEngine::configureCorrections";
	ConfigureCorrection* cmd = new ConfigureCorrection(dcOffsetCorrection, iqImbalanceCorrection, isource);
	m_inputMessageQueue.push(cmd);
}

// This is used for the Tx (sink streams) side
void DSPDeviceMIMOEngine::handleForwardToSpectrumSink(int nbSamples)
{
	if ((m_spectrumSink) && (m_spectrumInputIndex < m_deviceSampleMIMO->getNbSinkStreams()))
	{
		SampleSourceFifo* sampleFifo = m_deviceSampleMIMO->getSampleSourceFifo(m_spectrumInputIndex);
		SampleVector::iterator readUntil;
		sampleFifo->getReadIterator(readUntil);
		m_spectrumSink->feed(readUntil - nbSamples, readUntil, false);
	}
}

void DSPDeviceMIMOEngine::iqCorrections(SampleVector::iterator begin, SampleVector::iterator end, int isource, bool imbalanceCorrection)
{
    for(SampleVector::iterator it = begin; it < end; it++)
    {
        m_sourcesCorrections[isource].m_iBeta(it->real());
        m_sourcesCorrections[isource].m_qBeta(it->imag());

        if (imbalanceCorrection)
        {
#if IMBALANCE_INT
            // acquisition
            int64_t xi = (it->m_real - (int32_t) m_sourcesCorrections[isource].m_iBeta) << 5;
            int64_t xq = (it->m_imag - (int32_t) m_sourcesCorrections[isource].m_qBeta) << 5;

            // phase imbalance
            m_sourcesCorrections[isource].m_avgII((xi*xi)>>28); // <I", I">
            m_sourcesCorrections[isource].m_avgIQ((xi*xq)>>28); // <I", Q">

            if ((int64_t) m_sourcesCorrections[isource].m_avgII != 0)
            {
                int64_t phi = (((int64_t) m_sourcesCorrections[isource].m_avgIQ)<<28) / (int64_t) m_sourcesCorrections[isource].m_avgII;
                m_sourcesCorrections[isource].m_avgPhi(phi);
            }

            int64_t corrPhi = (((int64_t) m_sourcesCorrections[isource].m_avgPhi) * xq) >> 28;  //(m_avgPhi.asDouble()/16777216.0) * ((double) xq);

            int64_t yi = xi - corrPhi;
            int64_t yq = xq;

            // amplitude I/Q imbalance
            m_sourcesCorrections[isource].m_avgII2((yi*yi)>>28); // <I, I>
            m_sourcesCorrections[isource].m_avgQQ2((yq*yq)>>28); // <Q, Q>

            if ((int64_t) m_sourcesCorrections[isource].m_avgQQ2 != 0)
            {
                int64_t a = (((int64_t) m_sourcesCorrections[isource].m_avgII2)<<28) / (int64_t) m_sourcesCorrections[isource].m_avgQQ2;
                Fixed<int64_t, 28> fA(Fixed<int64_t, 28>::internal(), a);
                Fixed<int64_t, 28> sqrtA = sqrt((Fixed<int64_t, 28>) fA);
                m_sourcesCorrections[isource].m_avgAmp(sqrtA.as_internal());
            }

            int64_t zq = (((int64_t) m_sourcesCorrections[isource].m_avgAmp) * yq) >> 28;

            it->m_real = yi >> 5;
            it->m_imag = zq >> 5;

#else
            // DC correction and conversion
            float xi = (it->m_real - (int32_t) m_sourcesCorrections[isource].m_iBeta) / SDR_RX_SCALEF;
            float xq = (it->m_imag - (int32_t) m_sourcesCorrections[isource].m_qBeta) / SDR_RX_SCALEF;

            // phase imbalance
            m_sourcesCorrections[isource].m_avgII(xi*xi); // <I", I">
            m_sourcesCorrections[isource].m_avgIQ(xi*xq); // <I", Q">


            if (m_sourcesCorrections[isource].m_avgII.asDouble() != 0) {
                m_sourcesCorrections[isource].m_avgPhi(m_sourcesCorrections[isource].m_avgIQ.asDouble()/m_sourcesCorrections[isource].m_avgII.asDouble());
            }

            float& yi = xi; // the in phase remains the reference
            float yq = xq - m_sourcesCorrections[isource].m_avgPhi.asDouble()*xi;

            // amplitude I/Q imbalance
            m_sourcesCorrections[isource].m_avgII2(yi*yi); // <I, I>
            m_sourcesCorrections[isource].m_avgQQ2(yq*yq); // <Q, Q>

            if (m_sourcesCorrections[isource].m_avgQQ2.asDouble() != 0) {
                m_sourcesCorrections[isource].m_avgAmp(sqrt(m_sourcesCorrections[isource].m_avgII2.asDouble() / m_sourcesCorrections[isource].m_avgQQ2.asDouble()));
            }

            // final correction
            float& zi = yi; // the in phase remains the reference
            float zq = m_sourcesCorrections[isource].m_avgAmp.asDouble() * yq;

            // convert and store
            it->m_real = zi * SDR_RX_SCALEF;
            it->m_imag = zq * SDR_RX_SCALEF;
#endif
        }
        else
        {
            // DC correction only
            it->m_real -= (int32_t) m_sourcesCorrections[isource].m_iBeta;
            it->m_imag -= (int32_t) m_sourcesCorrections[isource].m_qBeta;
        }
    }
}
