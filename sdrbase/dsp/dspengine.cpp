///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
#include "dsp/dspengine.h"
#include "dsp/channelizer.h"
#include "dsp/samplefifo.h"
#include "dsp/samplesink.h"
#include "dsp/dspcommands.h"
#include "dsp/samplesource/samplesource.h"

DSPEngine::DSPEngine(MessageQueue* reportQueue, QObject* parent) :
	QThread(parent),
	m_messageQueue(),
	m_reportQueue(reportQueue),
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

void DSPEngine::start()
{
	DSPPing cmd;
	QThread::start();
	cmd.execute(&m_messageQueue);
}

void DSPEngine::stop()
{
	DSPExit cmd;
	cmd.execute(&m_messageQueue);
}

bool DSPEngine::startAcquisition()
{
	DSPAcquisitionStart cmd;
	return cmd.execute(&m_messageQueue) == StRunning;
}

void DSPEngine::stopAcquistion()
{
	DSPAcquisitionStop cmd;
	cmd.execute(&m_messageQueue);
}

void DSPEngine::setSource(SampleSource* source)
{
	DSPSetSource cmd(source);
	cmd.execute(&m_messageQueue);
}

void DSPEngine::addSink(SampleSink* sink)
{
	DSPAddSink cmd(sink);
	cmd.execute(&m_messageQueue);
}

void DSPEngine::removeSink(SampleSink* sink)
{
	DSPRemoveSink cmd(sink);
	cmd.execute(&m_messageQueue);
}

void DSPEngine::addAudioSource(AudioFifo* audioFifo)
{
	DSPAddAudioSource cmd(audioFifo);
	cmd.execute(&m_messageQueue);
}

void DSPEngine::removeAudioSource(AudioFifo* audioFifo)
{
	DSPRemoveAudioSource cmd(audioFifo);
	cmd.execute(&m_messageQueue);
}

void DSPEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection)
{
	Message* cmd = DSPConfigureCorrection::create(dcOffsetCorrection, iqImbalanceCorrection);
	cmd->submit(&m_messageQueue);
}

QString DSPEngine::errorMessage()
{
	DSPGetErrorMessage cmd;
	cmd.execute(&m_messageQueue);
	return cmd.getErrorMessage();
}

QString DSPEngine::deviceDescription()
{
	DSPGetDeviceDescription cmd;
	cmd.execute(&m_messageQueue);
	return cmd.getDeviceDescription();
}

void DSPEngine::run()
{
	connect(&m_messageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()), Qt::QueuedConnection);

	m_state = StIdle;

	handleMessages();
	exec();
}

void DSPEngine::dcOffset(SampleVector::iterator begin, SampleVector::iterator end)
{
	int count = end - begin;
	int io = 0;
	int qo = 0;

	// sum all sample components
	for(SampleVector::iterator it = begin; it < end; it++) {
		io += it->real();
		qo += it->imag();
	}

	// build a sliding average (el cheapo style)
	m_iOffset = (m_iOffset * 3 + io / count) >> 2;
	m_qOffset = (m_qOffset * 3 + qo / count) >> 2;

	// correct samples
	Sample corr(m_iOffset, m_qOffset);
	for(SampleVector::iterator it = begin; it < end; it++)
		*it -= corr;
}

void DSPEngine::imbalance(SampleVector::iterator begin, SampleVector::iterator end)
{
	int iMin = 0;
	int iMax = 0;
	int qMin = 0;
	int qMax = 0;

	// find value ranges for both I and Q
	// both intervals should be same same size (for a perfect circle)
	for(SampleVector::iterator it = begin; it < end; it++) {
		if(it != begin) {
			if(it->real() < iMin)
				iMin = it->real();
			else if(it->real() > iMax)
				iMax = it->real();
			if(it->imag() < qMin)
				qMin = it->imag();
			else if(it->imag() > qMax)
				qMax = it->imag();

		} else {
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
	if(m_qRange != 0)
		m_imbalance = ((uint)m_iRange << 16) / (uint)m_qRange;

	// correct imbalance and convert back to signed int 16
	for(SampleVector::iterator it = begin; it < end; it++)
		it->m_imag = (it->m_imag * m_imbalance) >> 16;
}

void DSPEngine::work()
{
	SampleFifo* sampleFifo = m_sampleSource->getSampleFifo();
	size_t samplesDone = 0;
	bool firstOfBurst = true;

	while((sampleFifo->fill() > 0) && (m_messageQueue.countPending() == 0) && (samplesDone < m_sampleRate)) {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		size_t count = sampleFifo->readBegin(sampleFifo->fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if(part1begin != part1end) {
			// correct stuff
			if(m_dcOffsetCorrection)
				dcOffset(part1begin, part1end);
			if(m_iqImbalanceCorrection)
				imbalance(part1begin, part1end);
			// feed data to handlers
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
				(*it)->feed(part1begin, part1end, firstOfBurst);
			firstOfBurst = false;
		}
		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
			// correct stuff
			if(m_dcOffsetCorrection)
				dcOffset(part2begin, part2end);
			if(m_iqImbalanceCorrection)
				imbalance(part2begin, part2end);
			// feed data to handlers
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
				(*it)->feed(part2begin, part2end, firstOfBurst);
			firstOfBurst = false;
		}

		// adjust FIFO pointers
		sampleFifo->readCommit(count);
		samplesDone += count;
	}

#if 0
	size_t wus;
	size_t maxWorkUnitSize = 0;
	size_t samplesDone = 0;

	wus = m_spectrum.workUnitSize();
	if(wus > maxWorkUnitSize)
		maxWorkUnitSize = wus;
	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++) {
		wus = (*it)->workUnitSize();
		if(wus > maxWorkUnitSize)
			maxWorkUnitSize = wus;
	}

	while((m_sampleFifo.fill() > maxWorkUnitSize) && (m_commandQueue.countPending() == 0) && (samplesDone < m_sampleRate)) {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

		size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
		if(part1begin != part1end) {
			// correct stuff
			if(m_settings.dcOffsetCorrection())
				dcOffset(part1begin, part1end);
			if(m_settings.iqImbalanceCorrection())
				imbalance(part1begin, part1end);
			// feed data to handlers
			m_spectrum.feed(part1begin, part1end);
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
				(*it)->feed(part1begin, part1end);
		}
		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
			// correct stuff
			if(m_settings.dcOffsetCorrection())
				dcOffset(part2begin, part2end);
			if(m_settings.iqImbalanceCorrection())
				imbalance(part2begin, part2end);
			// feed data to handlers
			m_spectrum.feed(part2begin, part2end);
			for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
				(*it)->feed(part1begin, part1end);
		}

		// adjust FIFO pointers
		m_sampleFifo.readCommit(count);
		samplesDone += count;
	}

	// check if the center frequency has changed (has to be responsive)
	if(m_settings.isModifiedCenterFreq())
		m_sampleSource->setCenterFrequency(m_settings.centerFreq());
	// check if decimation has changed (needed to be done here, because to high a sample rate can clog the switch)
	if(m_settings.isModifiedDecimation()) {
		m_sampleSource->setDecimation(m_settings.decimation());
		m_sampleRate = 4000000 / (1 << m_settings.decimation());
		qDebug("New rate: %d", m_sampleRate);
	}
#endif
}

DSPEngine::State DSPEngine::gotoIdle()
{
	switch(m_state) {
		case StNotStarted:
			return StNotStarted;

		case StIdle:
		case StError:
			return StIdle;

		case StRunning:
			break;
	}

	if(m_sampleSource == NULL)
		return StIdle;

	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
		(*it)->stop();
	m_sampleSource->stopInput();
	m_deviceDescription.clear();
	m_audioOutput.stop();
	m_sampleRate = 0;

	return StIdle;
}

DSPEngine::State DSPEngine::gotoRunning()
{
	switch(m_state) {
		case StNotStarted:
			return StNotStarted;

		case StRunning:
			return StRunning;

		case StIdle:
		case StError:
			break;
	}

	if(m_sampleSource == NULL)
		return gotoError("No sample source configured");

	m_iOffset = 0;
	m_qOffset = 0;
	m_iRange = 1 << 16;
	m_qRange = 1 << 16;

	if(!m_sampleSource->startInput(0))
		return gotoError("Could not start sample source");
	m_deviceDescription = m_sampleSource->getDeviceDescription();

	m_audioOutput.start(0, 44100);
	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++)
		(*it)->start();
	m_sampleRate = 0; // make sure, report is sent
	generateReport();

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
	gotoIdle();
	if(m_sampleSource != NULL)
		disconnect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()));
	m_sampleSource = source;
	if(m_sampleSource != NULL)
		connect(m_sampleSource->getSampleFifo(), SIGNAL(dataReady()), this, SLOT(handleData()), Qt::QueuedConnection);
	generateReport();
}

void DSPEngine::generateReport()
{
	bool needReport = false;
	int sampleRate;
	quint64 centerFrequency;

	if(m_sampleSource != NULL) {
		sampleRate = m_sampleSource->getSampleRate();
		centerFrequency = m_sampleSource->getCenterFrequency();
	} else {
		sampleRate = 100000;
		centerFrequency = 100000000;
	}

	if(sampleRate != m_sampleRate) {
		m_sampleRate = sampleRate;
		needReport = true;
		for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++) {
			DSPSignalNotification* signal = DSPSignalNotification::create(m_sampleRate, 0);
			signal->submit(&m_messageQueue, *it);
		}
	}
	if(centerFrequency != m_centerFrequency) {
		m_centerFrequency = centerFrequency;
		needReport = true;
	}

	if(needReport) {
		Message* rep = DSPEngineReport::create(m_sampleRate, m_centerFrequency);
		rep->submit(m_reportQueue);
	}
}

bool DSPEngine::distributeMessage(Message* message)
{
	if(m_sampleSource != NULL) {
		if((message->getDestination() == NULL) || (message->getDestination() == m_sampleSource)) {
			if(m_sampleSource->handleMessage(message)) {
				generateReport();
				return true;
			}
		}
	}
	for(SampleSinks::const_iterator it = m_sampleSinks.begin(); it != m_sampleSinks.end(); it++) {
		if((message->getDestination() == NULL) || (message->getDestination() == *it)) {
			if((*it)->handleMessage(message))
				return true;
		}
	}
	return false;
}

void DSPEngine::handleData()
{
	if(m_state == StRunning)
		work();
}

void DSPEngine::handleMessages()
{
	Message* message;
	while((message = m_messageQueue.accept()) != NULL) {
		qDebug("Message: %s", message->getIdentifier());

		if(DSPPing::match(message)) {
			message->completed(m_state);
		} else if(DSPExit::match(message)) {
			gotoIdle();
			m_state = StNotStarted;
			exit();
			message->completed(m_state);
		} else if(DSPAcquisitionStart::match(message)) {
			m_state = gotoIdle();
			if(m_state == StIdle)
				m_state = gotoRunning();
			message->completed(m_state);
		} else if(DSPAcquisitionStop::match(message)) {
			m_state = gotoIdle();
			message->completed(m_state);
		} else if(DSPGetDeviceDescription::match(message)) {
			((DSPGetDeviceDescription*)message)->setDeviceDescription(m_deviceDescription);
			message->completed();
		} else if(DSPGetErrorMessage::match(message)) {
			((DSPGetErrorMessage*)message)->setErrorMessage(m_errorMessage);
			message->completed();
		} else if(DSPSetSource::match(message)) {
			handleSetSource(((DSPSetSource*)message)->getSampleSource());
			message->completed();
		} else if(DSPAddSink::match(message)) {
			SampleSink* sink = ((DSPAddSink*)message)->getSampleSink();
			if(m_state == StRunning) {
				DSPSignalNotification* signal = DSPSignalNotification::create(m_sampleRate, 0);
				signal->submit(&m_messageQueue, sink);
				sink->start();
			}
			m_sampleSinks.push_back(sink);
			message->completed();
		} else if(DSPRemoveSink::match(message)) {
			SampleSink* sink = ((DSPAddSink*)message)->getSampleSink();
			if(m_state == StRunning)
				sink->stop();
			m_sampleSinks.remove(sink);
			message->completed();
		} else if(DSPAddAudioSource::match(message)) {
			m_audioOutput.addFifo(((DSPAddAudioSource*)message)->getAudioFifo());
			message->completed();
		} else if(DSPRemoveAudioSource::match(message)) {
			m_audioOutput.removeFifo(((DSPAddAudioSource*)message)->getAudioFifo());
			message->completed();
		} else if(DSPConfigureCorrection::match(message)) {
			DSPConfigureCorrection* conf = (DSPConfigureCorrection*)message;
			m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();
			if(m_dcOffsetCorrection != conf->getDCOffsetCorrection()) {
				m_dcOffsetCorrection = conf->getDCOffsetCorrection();
				m_iOffset = 0;
				m_qOffset = 0;
			}
			if(m_iqImbalanceCorrection != conf->getIQImbalanceCorrection()) {
				m_iqImbalanceCorrection = conf->getIQImbalanceCorrection();
				m_iRange = 1 << 16;
				m_qRange = 1 << 16;
				m_imbalance = 65536;
			}
			message->completed();
		} else {
			if(!distributeMessage(message))
				message->completed();
		}
	}
}
