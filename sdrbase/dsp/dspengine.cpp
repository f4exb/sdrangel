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

#include <stdio.h>
#include <QDebug>
#include "dsp/dspengine.h"
#include "dsp/channelizer.h"
#include "dsp/samplefifo.h"
#include "dsp/samplesink.h"
#include "dsp/dspcommands.h"
#include "dsp/samplesource/samplesource.h"

DSPEngine::DSPEngine(QObject* parent) :
	QThread(parent),
	m_state(StNotStarted),
	m_sampleSource(NULL),
	m_sampleSinks(),
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
	moveToThread(this);
}

DSPEngine::~DSPEngine()
{
	wait();
}

Q_GLOBAL_STATIC(DSPEngine, dspEngine)
DSPEngine *DSPEngine::instance()
{
	return dspEngine;
}

void DSPEngine::run()
{
	qDebug() << "DSPEngine::run";

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

	m_state = StIdle;

	handleInputMessages();
	exec();
}

void DSPEngine::start()
{
	qDebug() << "DSPEngine::start";
	DSPPing cmd;
	QThread::start();
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::stop()
{
	qDebug() << "DSPEngine::stop";
	DSPExit cmd;
	cmd.execute(&m_inputMessageQueue);
}

bool DSPEngine::initAcquisition()
{
	DSPAcquisitionInit cmd;

	return cmd.execute(&m_inputMessageQueue) == StReady;
}

bool DSPEngine::startAcquisition()
{
	DSPAcquisitionStart cmd;

	return cmd.execute(&m_inputMessageQueue) == StRunning;
}

void DSPEngine::stopAcquistion()
{
	DSPAcquisitionStop cmd;
	cmd.execute(&m_inputMessageQueue);

	if(m_dcOffsetCorrection)
	{
		qDebug("DC offset:%f,%f", m_iOffset, m_qOffset);
	}
}

void DSPEngine::setSource(SampleSource* source)
{
	DSPSetSource cmd(source);
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::addSink(SampleSink* sink)
{
	qDebug() << "DSPEngine::addSink: " << sink->objectName().toStdString().c_str();
	DSPAddSink cmd(sink);
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::removeSink(SampleSink* sink)
{
	DSPRemoveSink cmd(sink);
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::addAudioSink(AudioFifo* audioFifo)
{
	DSPAddAudioSink cmd(audioFifo);
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::removeAudioSink(AudioFifo* audioFifo)
{
	DSPRemoveAudioSink cmd(audioFifo);
	cmd.execute(&m_inputMessageQueue);
}

void DSPEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
	Message* cmd = DSPConfigureCorrection::create(dcOffsetCorrection, iqImbalanceCorrection);
	cmd->submit(&m_inputMessageQueue);
}

QString DSPEngine::errorMessage()
{
	DSPGetErrorMessage cmd;
	cmd.execute(&m_inputMessageQueue);
	return cmd.getErrorMessage();
}

QString DSPEngine::sourceDeviceDescription()
{
	DSPGetSourceDeviceDescription cmd;
	cmd.execute(&m_inputMessageQueue);
	return cmd.getDeviceDescription();
}

void DSPEngine::dcOffset(SampleVector::iterator begin, SampleVector::iterator end)
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

void DSPEngine::imbalance(SampleVector::iterator begin, SampleVector::iterator end)
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

void DSPEngine::work()
{
	SampleFifo* sampleFifo = m_sampleSource->getSampleFifo();
	size_t samplesDone = 0;
	bool positiveOnly = false;

	while ((sampleFifo->fill() > 0) && (m_inputMessageQueue.countPending() == 0) && (samplesDone < m_sampleRate))
	{
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		size_t count = sampleFifo->readBegin(sampleFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if (part1begin != part1end)
		{
			// correct stuff
			if (m_dcOffsetCorrection) {
				dcOffset(part1begin, part1end);
			}

			if (m_iqImbalanceCorrection) {
				imbalance(part1begin, part1end);
			}

			// feed data to handlers
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++) {
				(*it)->feed(part1begin, part1end, positiveOnly);
			}
		}

		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end)
		{
			// correct stuff
			if(m_dcOffsetCorrection) {
				dcOffset(part2begin, part2end);
			}

			if(m_iqImbalanceCorrection) {
				imbalance(part2begin, part2end);
			}

			// feed data to handlers
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++) {
				(*it)->feed(part2begin, part2end, positiveOnly);
			}
		}

		// adjust FIFO pointers
		sampleFifo->readCommit(count);
		samplesDone += count;
	}
}

// notStarted -> idle -> init -> running -+
//                ^                       |
//                +-----------------------+

DSPEngine::State DSPEngine::gotoIdle()
{
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

	if(m_sampleSource == 0)
	{
		return StIdle;
	}

	// stop everything

	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
	{
		(*it)->stop();
	}

	m_sampleSource->stopInput();
	m_deviceDescription.clear();
	m_audioSink.stop();
	m_sampleRate = 0;

	return StIdle;
}

DSPEngine::State DSPEngine::gotoInit()
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

	if (m_sampleSource == 0)
	{
		return gotoError("No sample source configured");
	}

	// init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion

	m_iOffset = 0;
	m_qOffset = 0;
	m_iRange = 1 << 16;
	m_qRange = 1 << 16;

	m_deviceDescription = m_sampleSource->getDeviceDescription();
	m_centerFrequency = m_sampleSource->getCenterFrequency();
	m_sampleRate = m_sampleSource->getSampleRate();

	qDebug() << "DSPEngine::gotoInit: " << m_deviceDescription.toStdString().c_str() << ": "
			<< " sampleRate: " << m_sampleRate
			<< " centerFrequency: " << m_centerFrequency;

	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
	{
		qDebug() << "  - initializing " << (*it)->objectName().toStdString().c_str();
		DSPSignalNotification* notif = DSPSignalNotification::create(m_sampleRate, m_centerFrequency);
		(*it)->init(notif);
	}

	// pass sample rate to main window

	Message* rep = DSPEngineReport::create(m_sampleRate, m_centerFrequency);
	rep->submit(&m_outputMessageQueue);

	return StReady;
}

DSPEngine::State DSPEngine::gotoRunning()
{
    qDebug() << "DSPEngine::gotoRunning";
    
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

	if(m_sampleSource == NULL) {
		return gotoError("No sample source configured");
	}

	qDebug() << "  - " << m_deviceDescription.toStdString().c_str() << " started";

	// Start everything

	if(!m_sampleSource->startInput(0))
	{
		return gotoError("Could not start sample source");
	}

	m_audioSink.start(0, 48000);

	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
	{
        qDebug() << "  - starting " << (*it)->objectName().toStdString().c_str();
		(*it)->start();
	}

	qDebug() << "  - input message queue pending: " << m_inputMessageQueue.countPending();

	return StRunning;
}

DSPEngine::State DSPEngine::gotoError(const QString& errorMessage)
{
	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	m_state = StError;
	return StError;
}

void DSPEngine::handleSetSource(SampleSource* source)
{
	qDebug() << "DSPEngine::handleSetSource: " << source->getDeviceDescription().toStdString().c_str();

	gotoIdle();

	if(m_sampleSource != 0)
	{
		disconnect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()));
	}

	m_sampleSource = source;

	if(m_sampleSource != 0)
	{
		qDebug() << "  - connect";
		connect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()), Qt::QueuedConnection);
	}
}

bool DSPEngine::distributeMessage(Message* message)
{
	if (m_sampleSource != 0)
	{
		if ((message->getDestination() == 0) || (message->getDestination() == m_sampleSource))
		{
			if (m_sampleSource->handleMessage(message))
			{
				return true;
			}
		}
	}

	for (SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
	{
		if ((message->getDestination() == NULL) || (message->getDestination() == *it))
		{
			if ((*it)->handleMessage(message))
			{
				return true;
			}
		}
	}

	return false;
}

void DSPEngine::handleData()
{
	if(m_state == StRunning)
	{
		work();
	}
}

void DSPEngine::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.accept()) != NULL)
	{
		qDebug("DSPEngine::handleInputMessages: Message: %s", message->getIdentifier());

		if (DSPPing::match(message))
		{
			message->completed(m_state);
		}
		else if (DSPExit::match(message))
		{
			gotoIdle();
			m_state = StNotStarted;
			exit();
			message->completed(m_state);
		}
		else if (DSPAcquisitionInit::match(message))
		{
			m_state = gotoIdle();

			if(m_state == StIdle) {
				m_state = gotoInit(); // State goes ready if init is performed
			}

			message->completed(m_state);
		}
		else if (DSPAcquisitionStart::match(message))
		{
			if(m_state == StReady) {
				m_state = gotoRunning();
			}

			message->completed(m_state);
		}
		else if (DSPAcquisitionStop::match(message))
		{
			m_state = gotoIdle();
			message->completed(m_state);
		}
		else if (DSPGetSourceDeviceDescription::match(message))
		{
			((DSPGetSourceDeviceDescription*)message)->setDeviceDescription(m_deviceDescription);
			message->completed();
		}
		else if (DSPGetErrorMessage::match(message))
		{
			((DSPGetErrorMessage*)message)->setErrorMessage(m_errorMessage);
			message->completed();
		}
		else if (DSPSetSource::match(message)) {
			handleSetSource(((DSPSetSource*)message)->getSampleSource());
			message->completed();
		}
		else if (DSPAddSink::match(message))
		{
			SampleSink* sink = ((DSPAddSink*)message)->getSampleSink();
			m_sampleSinks.push_back(sink);
			message->completed();
		}
		else if (DSPRemoveSink::match(message))
		{
			SampleSink* sink = ((DSPAddSink*)message)->getSampleSink();

			if(m_state == StRunning) {
				sink->stop();
			}

			m_sampleSinks.remove(sink);
			message->completed();
		}
		else if (DSPAddAudioSink::match(message))
		{
			m_audioSink.addFifo(((DSPAddAudioSink*)message)->getAudioFifo());
			message->completed();
		}
		else if (DSPRemoveAudioSink::match(message))
		{
			m_audioSink.removeFifo(((DSPAddAudioSink*)message)->getAudioFifo());
			message->completed();
		}
		else if (DSPConfigureCorrection::match(message))
		{
			DSPConfigureCorrection* conf = (DSPConfigureCorrection*)message;
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

			message->completed();
		}
		else
		{
			if (DSPSignalNotification::match(message))
			{
				DSPSignalNotification *conf = (DSPSignalNotification*)message;
				qDebug() << "  (" << conf->getSampleRate() << "," << conf->getFrequencyOffset() << ")";
			}
			if (!distributeMessage(message))
			{
				message->completed();
			}
		}
	}
}
