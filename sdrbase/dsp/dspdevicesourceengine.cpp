///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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

#include "dspdevicesourceengine.h"

#include <dsp/basebandsamplesink.h>
#include <dsp/devicesamplesource.h>
#include <stdio.h>
#include <QDebug>
#include "dsp/dspcommands.h"
#include "util/fixed.h"
#include "samplesinkfifo.h"

DSPDeviceSourceEngine::DSPDeviceSourceEngine(uint uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
	m_state(StNotStarted),
	m_deviceSampleSource(nullptr),
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
    stop();
    wait();
}

void DSPDeviceSourceEngine::run()
{
	qDebug() << "DSPDeviceSourceEngine::run";
	m_state = StIdle;
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
	qDebug() << "DSPDeviceSourceEngine::addSink: " << sink->getSinkName().toStdString().c_str();
	DSPAddBasebandSampleSink cmd(sink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSourceEngine::removeSink(BasebandSampleSink* sink)
{
	qDebug() << "DSPDeviceSourceEngine::removeSink: " << sink->getSinkName().toStdString().c_str();
	DSPRemoveBasebandSampleSink cmd(sink);
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

void DSPDeviceSourceEngine::iqCorrections(SampleVector::iterator begin, SampleVector::iterator end, bool imbalanceCorrection)
{
    for(SampleVector::iterator it = begin; it < end; it++)
    {
        m_iBeta(it->real());
        m_qBeta(it->imag());

        if (imbalanceCorrection)
        {
#if IMBALANCE_INT
            // acquisition
            int64_t xi = (it->m_real - (int32_t) m_iBeta) << 5;
            int64_t xq = (it->m_imag - (int32_t) m_qBeta) << 5;

            // phase imbalance
            m_avgII((xi*xi)>>28); // <I", I">
            m_avgIQ((xi*xq)>>28); // <I", Q">

            if ((int64_t) m_avgII != 0)
            {
                int64_t phi = (((int64_t) m_avgIQ)<<28) / (int64_t) m_avgII;
                m_avgPhi(phi);
            }

            int64_t corrPhi = (((int64_t) m_avgPhi) * xq) >> 28;  //(m_avgPhi.asDouble()/16777216.0) * ((double) xq);

            int64_t yi = xi - corrPhi;
            int64_t yq = xq;

            // amplitude I/Q imbalance
            m_avgII2((yi*yi)>>28); // <I, I>
            m_avgQQ2((yq*yq)>>28); // <Q, Q>

            if ((int64_t) m_avgQQ2 != 0)
            {
                int64_t a = (((int64_t) m_avgII2)<<28) / (int64_t) m_avgQQ2;
                Fixed<int64_t, 28> fA(Fixed<int64_t, 28>::internal(), a);
                Fixed<int64_t, 28> sqrtA = sqrt((Fixed<int64_t, 28>) fA);
                m_avgAmp(sqrtA.as_internal());
            }

            int64_t zq = (((int64_t) m_avgAmp) * yq) >> 28;

            it->m_real = yi >> 5;
            it->m_imag = zq >> 5;

#else
            // DC correction and conversion
            float xi = (it->m_real - (int32_t) m_iBeta) / SDR_RX_SCALEF;
            float xq = (it->m_imag - (int32_t) m_qBeta) / SDR_RX_SCALEF;

            // phase imbalance
            m_avgII(xi*xi); // <I", I">
            m_avgIQ(xi*xq); // <I", Q">


            if (m_avgII.asDouble() != 0) {
                m_avgPhi(m_avgIQ.asDouble()/m_avgII.asDouble());
            }

            float& yi = xi; // the in phase remains the reference
            float yq = xq - m_avgPhi.asDouble()*xi;

            // amplitude I/Q imbalance
            m_avgII2(yi*yi); // <I, I>
            m_avgQQ2(yq*yq); // <Q, Q>

            if (m_avgQQ2.asDouble() != 0) {
                m_avgAmp(sqrt(m_avgII2.asDouble() / m_avgQQ2.asDouble()));
            }

            // final correction
            float& zi = yi; // the in phase remains the reference
            float zq = m_avgAmp.asDouble() * yq;

            // convert and store
            it->m_real = zi * SDR_RX_SCALEF;
            it->m_imag = zq * SDR_RX_SCALEF;
#endif
        }
        else
        {
            // DC correction only
            it->m_real -= (int32_t) m_iBeta;
            it->m_imag -= (int32_t) m_qBeta;
        }
    }
}

void DSPDeviceSourceEngine::dcOffset(SampleVector::iterator begin, SampleVector::iterator end)
{
	// sum and correct in one pass
	for(SampleVector::iterator it = begin; it < end; it++)
	{
	    m_iBeta(it->real());
	    m_qBeta(it->imag());
	    it->m_real -= (int32_t) m_iBeta;
	    it->m_imag -= (int32_t) m_qBeta;
	}
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

	// calculate imbalance on 32 bit full scale
	if(m_qRange != 0) {
		m_imbalance = ((uint)m_iRange << (32-SDR_RX_SAMP_SZ)) / (uint)m_qRange;
	}

	// correct imbalance and convert back to sample size
	for(SampleVector::iterator it = begin; it < end; it++) {
		it->m_imag = (it->m_imag * m_imbalance) >> (32-SDR_RX_SAMP_SZ);
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
                iqCorrections(part1begin, part1end, m_iqImbalanceCorrection);
            }

			// feed data to direct sinks
			for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); ++it)
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
                iqCorrections(part2begin, part2end, m_iqImbalanceCorrection);
            }

			// feed data to direct sinks
			for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
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
	m_deviceSampleSource->stop();

	for(BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
	{
		(*it)->stop();
	}

	m_deviceDescription.clear();
	m_sampleRate = 0;

	return StIdle;
}

DSPDeviceSourceEngine::State DSPDeviceSourceEngine::gotoInit()
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

	qDebug() << "DSPDeviceSourceEngine::gotoInit: "
	        << " m_deviceDescription: " << m_deviceDescription.toStdString().c_str()
			<< " sampleRate: " << m_sampleRate
			<< " centerFrequency: " << m_centerFrequency;


	for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); ++it)
	{
		DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
		qDebug() << "DSPDeviceSourceEngine::gotoInit: initializing " << (*it)->getSinkName().toStdString().c_str();
		(*it)->pushMessage(notif);
	}

	// pass data to listeners
	if (m_deviceSampleSource->getMessageQueueToGUI())
	{
        DSPSignalNotification* rep = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
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
        qDebug() << "DSPDeviceSourceEngine::gotoRunning: starting " << (*it)->getSinkName().toStdString().c_str();
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

	if (DSPAcquisitionInit::match(*message))
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
	else if (DSPAddBasebandSampleSink::match(*message))
	{
		BasebandSampleSink* sink = ((DSPAddBasebandSampleSink*) message)->getSampleSink();
		m_basebandSampleSinks.push_back(sink);
        // initialize sample rate and center frequency in the sink:
        DSPSignalNotification *msg = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
        sink->pushMessage(msg);
        // start the sink:
        if(m_state == StRunning) {
            sink->start();
        }
	}
	else if (DSPRemoveBasebandSampleSink::match(*message))
	{
		BasebandSampleSink* sink = ((DSPRemoveBasebandSampleSink*) message)->getSampleSink();

		if(m_state == StRunning) {
			sink->stop();
		}

		m_basebandSampleSinks.remove(sink);
	}

	m_syncMessenger.done(m_state);
}

void DSPDeviceSourceEngine::handleInputMessages()
{
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

			m_avgAmp.reset();
			m_avgII.reset();
			m_avgII2.reset();
			m_avgIQ.reset();
			m_avgPhi.reset();
			m_avgQQ2.reset();
			m_iBeta.reset();
			m_qBeta.reset();

			delete message;
		}
		else if (DSPSignalNotification::match(*message))
		{
			DSPSignalNotification *notif = (DSPSignalNotification *) message;

			// update DSP values

			m_sampleRate = notif->getSampleRate();
			m_centerFrequency = notif->getCenterFrequency();

			qDebug() << "DSPDeviceSourceEngine::handleInputMessages: DSPSignalNotification:"
				<< " m_sampleRate: " << m_sampleRate
				<< " m_centerFrequency: " << m_centerFrequency;

			// forward source changes to channel sinks with immediate execution (no queuing)

			for(BasebandSampleSinks::const_iterator it = m_basebandSampleSinks.begin(); it != m_basebandSampleSinks.end(); it++)
			{
				DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy
				qDebug() << "DSPDeviceSourceEngine::handleInputMessages: forward message to " << (*it)->getSinkName().toStdString().c_str();
				(*it)->pushMessage(rep);
			}

			// forward changes to source GUI input queue

			MessageQueue *guiMessageQueue = m_deviceSampleSource->getMessageQueueToGUI();
			qDebug("DSPDeviceSourceEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

			if (guiMessageQueue)
			{
			    DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the source GUI
                guiMessageQueue->push(rep);
			}

			//m_outputMessageQueue.push(rep);

			delete message;
		}
	}
}
