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

#include "dspdevicesourceengine.h"

#include <dsp/basebandsamplesink.h>
#include <dsp/devicesamplesource.h>
#include <dsp/downchannelizer.h>
#include <stdio.h>
#include <QDebug>
#include "dsp/dspcommands.h"
#include "samplesinkfifo.h"
#include "threadedbasebandsamplesink.h"

DSPDeviceSourceEngine::DSPDeviceSourceEngine(uint uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
	m_state(StNotStarted),
	m_deviceSampleSource(0),
	m_sampleSourceSequence(0),
	m_basebandSampleSinks(),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_dcOffsetCorrection(false),
	m_iqImbalanceCorrection(false),
	m_iOffset(0),
	m_qOffset(0),
	m_iRange(1 << 16),
	m_qRange(1 << 16),
	m_imbalance(65536)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	connect(&m_syncMessenger, SIGNAL(messageSent()), this, SLOT(handleSynchronousMessages()), Qt::QueuedConnection);

	moveToThread(this);
}

DSPDeviceSourceEngine::~DSPDeviceSourceEngine()
{
	wait();
}

void DSPDeviceSourceEngine::run()
{
	qDebug() << "DSPDeviceSourceEngine::run";

	m_state = StIdle;

    m_syncMessenger.done(); // Release start() that is waiting in main thread
	exec();
}

void DSPDeviceSourceEngine::start()
{
	qDebug() << "DSPDeviceSourceEngine::start";
	QThread::start();
}

void DSPDeviceSourceEngine::stop()
{
	qDebug() << "DSPDeviceSourceEngine::stop";
    gotoIdle();
    m_state = StNotStarted;
	QThread::exit();
//	DSPExit cmd;
//	m_syncMessenger.sendWait(cmd);
}

bool DSPDeviceSourceEngine::initAcquisition()
{
	qDebug() << "DSPDeviceSourceEngine::initAcquisition";
	DSPAcquisitionInit cmd;

	return m_syncMessenger.sendWait(cmd) == StReady;
}

bool DSPDeviceSourceEngine::startAcquisition()
{
	qDebug() << "DSPDeviceSourceEngine::startAcquisition";
	DSPAcquisitionStart cmd;

	return m_syncMessenger.sendWait(cmd) == StRunning;
}

void DSPDeviceSourceEngine::stopAcquistion()
{
	qDebug() << "DSPDeviceSourceEngine::stopAcquistion";
	DSPAcquisitionStop cmd;
	m_syncMessenger.storeMessage(cmd);
	handleSynchronousMessages();

	if(m_dcOffsetCorrection)
	{
		qDebug("DC offset:%f,%f", m_iOffset, m_qOffset);
	}
}

void DSPDeviceSourceEngine::setSource(DeviceSampleSource* source)
{
	qDebug() << "DSPDeviceSourceEngine::setSource";
	DSPSetSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::setSourceSequence(int sequence)
{
	qDebug("DSPDeviceSourceEngine::setSourceSequence: seq: %d", sequence);
	m_sampleSourceSequence = sequence;
}

void DSPDeviceSourceEngine::addSink(BasebandSampleSink* sink)
{
	qDebug() << "DSPDeviceSourceEngine::addSink: " << sink->objectName().toStdString().c_str();
	DSPAddSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::removeSink(BasebandSampleSink* sink)
{
	qDebug() << "DSPDeviceSourceEngine::removeSink: " << sink->objectName().toStdString().c_str();
	DSPRemoveSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::addThreadedSink(ThreadedBasebandSampleSink* sink)
{
	qDebug() << "DSPDeviceSourceEngine::addThreadedSink: " << sink->objectName().toStdString().c_str();
	DSPAddThreadedSampleSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::removeThreadedSink(ThreadedBasebandSampleSink* sink)
{
	qDebug() << "DSPDeviceSourceEngine::removeThreadedSink: " << sink->objectName().toStdString().c_str();
	DSPRemoveThreadedSampleSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
	qDebug() << "DSPDeviceSourceEngine::configureCorrections";
	DSPConfigureCorrection* cmd = new DSPConfigureCorrection(dcOffsetCorrection, iqImbalanceCorrection);
	m_inputMessageQueue.push(cmd);
}

QString DSPDeviceSourceEngine::errorMessage()
{
	qDebug() << "DSPDeviceSourceEngine::errorMessage";
	DSPGetErrorMessage cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getErrorMessage();
}

QString DSPDeviceSourceEngine::sourceDeviceDescription()
{
	qDebug() << "DSPDeviceSourceEngine::sourceDeviceDescription";
	DSPGetSourceDeviceDescription cmd;
	m_syncMessenger.sendWait(cmd);
	return cmd.getDeviceDescription();
}

void DSPDeviceSourceEngine::dcOffset(SampleVector::iterator begin, SampleVector::iterator end)
{
	double count;
	int io = 0;
	int qo = 0;
	Sample corr((qint16)m_iOffset, (qint16)m_qOffset);

	// sum and correct in one pass
	for(SampleVector::iterator it = begin; it < end; it++)
	{
		io += it->real();
		qo += it->imag();
		*it -= corr;
	}

	// moving average
	count = end - begin;
	m_iOffset = (15.0 * m_iOffset + (double)io / count) / 16.0;
	m_qOffset = (15.0 * m_qOffset + (double)qo / count) / 16.0;
}

void DSPDeviceSourceEngine::imbalance(SampleVector::iterator begin, SampleVector::iterator end)
{
	int iMin = 0;
	int iMax = 0;
	int qMin = 0;
	int qMax = 0;

	// find value ranges for both I and Q
	// both intervals should be same same size (for a perfect circle)
	for (SampleVector::iterator it = begin; it < end; it++)
	{
		if (it != begin)
		{
			if (it->real() < iMin) {
				iMin = it->real();
			} else if (it->real() > iMax) {
				iMax = it->real();
			}

			if (it->imag() < qMin) {
				qMin = it->imag();
			} else if (it->imag() > qMax) {
				qMax = it->imag();
			}
		}
		else
		{
			iMin = it->real();
			iMax = it->real();
			qMin = it->imag();
			qMax = it->imag();
		}
	}

	// sliding average (el cheapo again)
	m_iRange = (m_iRange * 15 + (iMax - iMin)) >> 4;
	m_qRange = (m_qRange * 15 + (qMax - qMin)) >> 4;

	// calculate imbalance as Q15.16
	if(m_qRange != 0) {
		m_imbalance = ((uint)m_iRange << 16) / (uint)m_qRange;
	}

	// correct imbalance and convert back to signed int 16
	for(SampleVector::iterator it = begin; it < end; it++) {
		it->m_imag = (it->m_imag * m_imbalance) >> 16;
	}
}

void DSPDeviceSourceEngine::work()
{
	SampleSinkFifo* sampleFifo = m_deviceSampleSource->getSampleFifo();
	std::size_t samplesDone = 0;
	bool positiveOnly = false;

	while ((sampleFifo->fill() > 0) && (m_inputMessageQueue.size() == 0) && (samplesDone < m_sampleRate))
	{
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		std::size_t count = sampleFifo->readBegin(sampleFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if (part1begin != part1end)
		{
			// correct stuff
			if (m_dcOffsetCorrection)
			{
				dcOffset(part1begin, part1end);
			}

			if (m_iqImbalanceCorrection)
			{
				imbalance(part1begin, part1end);
			}

			// feed data to direct sinks
			for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); ++it)
			{
				(*it)->feed(part1begin, part1end, positiveOnly);
			}

			// feed data to threaded sinks
			for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); ++it)
			{
				(*it)->feed(part1begin, part1end, positiveOnly);
			}
		}

		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end)
		{
			// correct stuff
			if (m_dcOffsetCorrection)
			{
				dcOffset(part2begin, part2end);
			}

			if (m_iqImbalanceCorrection)
			{
				imbalance(part2begin, part2end);
			}

			// feed data to direct sinks
			for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
			{
				(*it)->feed(part2begin, part2end, positiveOnly);
			}

			// feed data to threaded sinks
			for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); ++it)
			{
				(*it)->feed(part2begin, part2end, positiveOnly);
			}
		}

		// adjust FIFO pointers
		sampleFifo->readCommit((unsigned int) count);
		samplesDone += count;
	}
}

// notStarted -> idle -> init -> running -+
//                ^                       |
//                +-----------------------+

DSPDeviceSourceEngine::State DSPDeviceSourceEngine::gotoIdle()
{
	qDebug() << "DSPDeviceSourceEngine::gotoIdle";

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

	if(m_deviceSampleSource == 0)
	{
		return StIdle;
	}

	// stop everything

	for(BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
	{
		(*it)->stop();
	}

    for(ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); it++)
    {
        (*it)->stop();
    }

	m_deviceSampleSource->stop();
	m_deviceDescription.clear();
	m_sampleRate = 0;

	return StIdle;
}

DSPDeviceSourceEngine::State DSPDeviceSourceEngine::gotoInit()
{
	qDebug() << "DSPDeviceSourceEngine::gotoInit";

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

	if (m_deviceSampleSource == 0)
	{
		return gotoError("No sample source configured");
	}

	// init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion

	m_iOffset = 0;
	m_qOffset = 0;
	m_iRange = 1 << 16;
	m_qRange = 1 << 16;

	m_deviceDescription = m_deviceSampleSource->getDeviceDescription();
	m_centerFrequency = m_deviceSampleSource->getCenterFrequency();
	m_sampleRate = m_deviceSampleSource->getSampleRate();

	qDebug() << "DSPDeviceSourceEngine::gotoInit: " << m_deviceDescription.toStdString().c_str() << ": "
			<< " sampleRate: " << m_sampleRate
			<< " centerFrequency: " << m_centerFrequency;

	DSPSignalNotification notif(m_sampleRate, m_centerFrequency);

	for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); ++it)
	{
		qDebug() << "DSPDeviceSourceEngine::gotoInit: initializing " << (*it)->objectName().toStdString().c_str();
		(*it)->handleMessage(notif);
	}

	for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); ++it)
	{
		qDebug() << "DSPDeviceSourceEngine::gotoInit: initializing ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
		(*it)->handleSinkMessage(notif);
	}

	// pass data to listeners
	if (m_deviceSampleSource->getMessageQueueToGUI())
	{
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy for the output queue
        m_deviceSampleSource->getMessageQueueToGUI()->push(rep);
	}

	return StReady;
}

DSPDeviceSourceEngine::State DSPDeviceSourceEngine::gotoRunning()
{
	qDebug() << "DSPDeviceSourceEngine::gotoRunning";

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

	if(m_deviceSampleSource == NULL) {
		return gotoError("DSPDeviceSourceEngine::gotoRunning: No sample source configured");
	}

	qDebug() << "DSPDeviceSourceEngine::gotoRunning: " << m_deviceDescription.toStdString().c_str() << " started";

	// Start everything

	if(!m_deviceSampleSource->start())
	{
		return gotoError("Could not start sample source");
	}

	for(BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
	{
        qDebug() << "DSPDeviceSourceEngine::gotoRunning: starting " << (*it)->objectName().toStdString().c_str();
		(*it)->start();
	}

	for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); ++it)
	{
		qDebug() << "DSPDeviceSourceEngine::gotoRunning: starting ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
		(*it)->start();
	}

	qDebug() << "DSPDeviceSourceEngine::gotoRunning:input message queue pending: " << m_inputMessageQueue.size();

	return StRunning;
}

DSPDeviceSourceEngine::State DSPDeviceSourceEngine::gotoError(const QString& errorMessage)
{
	qDebug() << "DSPDeviceSourceEngine::gotoError: " << errorMessage;

	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	m_state = StError;
	return StError;
}

void DSPDeviceSourceEngine::handleSetSource(DeviceSampleSource* source)
{
	gotoIdle();

//	if(m_sampleSource != 0)
//	{
//		disconnect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()));
//	}

	m_deviceSampleSource = source;

	if(m_deviceSampleSource != 0)
	{
		qDebug("DSPDeviceSourceEngine::handleSetSource: set %s", qPrintable(source->getDeviceDescription()));
		connect(m_deviceSampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()), Qt::QueuedConnection);
	}
	else
	{
		qDebug("DSPDeviceSourceEngine::handleSetSource: set none");
	}
}

void DSPDeviceSourceEngine::handleData()
{
	if(m_state == StRunning)
	{
		work();
	}
}

void DSPDeviceSourceEngine::handleSynchronousMessages()
{
    Message *message = m_syncMessenger.getMessage();
	qDebug() << "DSPDeviceSourceEngine::handleSynchronousMessages: " << message->getIdentifier();

	if (DSPExit::match(*message))
	{
		gotoIdle();
		m_state = StNotStarted;
		exit();
	}
	else if (DSPAcquisitionInit::match(*message))
	{
		m_state = gotoIdle();

		if(m_state == StIdle) {
			m_state = gotoInit(); // State goes ready if init is performed
		}
	}
	else if (DSPAcquisitionStart::match(*message))
	{
		if(m_state == StReady) {
			m_state = gotoRunning();
		}
	}
	else if (DSPAcquisitionStop::match(*message))
	{
		m_state = gotoIdle();
	}
	else if (DSPGetSourceDeviceDescription::match(*message))
	{
		((DSPGetSourceDeviceDescription*) message)->setDeviceDescription(m_deviceDescription);
	}
	else if (DSPGetErrorMessage::match(*message))
	{
		((DSPGetErrorMessage*) message)->setErrorMessage(m_errorMessage);
	}
	else if (DSPSetSource::match(*message)) {
		handleSetSource(((DSPSetSource*) message)->getSampleSource());
	}
	else if (DSPAddSink::match(*message))
	{
		BasebandSampleSink* sink = ((DSPAddSink*) message)->getSampleSink();
		m_basebandSampleSinks.push_back(sink);
	}
	else if (DSPRemoveSink::match(*message))
	{
		BasebandSampleSink* sink = ((DSPRemoveSink*) message)->getSampleSink();

		if(m_state == StRunning) {
			sink->stop();
		}

		m_basebandSampleSinks.remove(sink);
	}
	else if (DSPAddThreadedSampleSink::match(*message))
	{
		ThreadedBasebandSampleSink *threadedSink = ((DSPAddThreadedSampleSink*) message)->getThreadedSampleSink();
		m_threadedBasebandSampleSinks.push_back(threadedSink);
		// initialize sample rate and center frequency in the sink:
		DSPSignalNotification msg(m_sampleRate, m_centerFrequency);
		threadedSink->handleSinkMessage(msg);
		// start the sink:
		threadedSink->start();
	}
	else if (DSPRemoveThreadedSampleSink::match(*message))
	{
		ThreadedBasebandSampleSink* threadedSink = ((DSPRemoveThreadedSampleSink*) message)->getThreadedSampleSink();
		threadedSink->stop();
		m_threadedBasebandSampleSinks.remove(threadedSink);
	}

	m_syncMessenger.done(m_state);
}

void DSPDeviceSourceEngine::handleInputMessages()
{
	qDebug() << "DSPDeviceSourceEngine::handleInputMessages";

	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		qDebug("DSPDeviceSourceEngine::handleInputMessages: message: %s", message->getIdentifier());

		if (DSPConfigureCorrection::match(*message))
		{
			DSPConfigureCorrection* conf = (DSPConfigureCorrection*) message;
			m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();

			if(m_dcOffsetCorrection != conf->getDCOffsetCorrection())
			{
				m_dcOffsetCorrection = conf->getDCOffsetCorrection();
				m_iOffset = 0;
				m_qOffset = 0;
			}

			if(m_iqImbalanceCorrection != conf->getIQImbalanceCorrection())
			{
				m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();
				m_iRange = 1 << 16;
				m_qRange = 1 << 16;
				m_imbalance = 65536;
			}

			delete message;
		}
		else if (DSPSignalNotification::match(*message))
		{
			DSPSignalNotification *notif = (DSPSignalNotification *) message;

			// update DSP values

			m_sampleRate = notif->getSampleRate();
			m_centerFrequency = notif->getCenterFrequency();

			qDebug() << "DSPDeviceSourceEngine::handleInputMessages: DSPSignalNotification(" << m_sampleRate << "," << m_centerFrequency << ")";

			// forward source changes to channel sinks with immediate execution (no queuing)

			for(BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
			{
				qDebug() << "DSPDeviceSourceEngine::handleInputMessages: forward message to " << (*it)->objectName().toStdString().c_str();
				(*it)->handleMessage(*message);
			}

			for (ThreadedBasebandSampleSinks::const_iterator it = m_threadedBasebandSampleSinks.begin(); it != m_threadedBasebandSampleSinks.end(); ++it)
			{
				qDebug() << "DSPDeviceSourceEngine::handleSourceMessages: forward message to ThreadedSampleSink(" << (*it)->getSampleSinkObjectName().toStdString().c_str() << ")";
				(*it)->handleSinkMessage(*message);
			}

			// forward changes to source GUI input queue

			MessageQueue *guiMessageQueue = m_deviceSampleSource->getMessageQueueToGUI();

			if (guiMessageQueue) {
			    DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the source GUI
                m_deviceSampleSource->getMessageQueueToGUI()->push(rep);
			}

			//m_outputMessageQueue.push(rep);

			delete message;
		}
	}
}
