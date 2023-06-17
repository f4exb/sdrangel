///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
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

#include <stdio.h>
#include <QDebug>
#include <QThread>

#include "dspdevicesinkengine.h"

#include "dsp/basebandsamplesource.h"
#include "dsp/basebandsamplesink.h"
#include "dsp/devicesamplesink.h"
#include "dsp/dspcommands.h"

DSPDeviceSinkEngine::DSPDeviceSinkEngine(uint32_t uid, QObject* parent) :
	QThread(parent),
    m_uid(uid),
	m_state(StNotStarted),
	m_deviceSampleSink(nullptr),
	m_sampleSinkSequence(0),
	m_basebandSampleSources(),
	m_spectrumSink(nullptr),
	m_sampleRate(0),
	m_centerFrequency(0),
    m_realElseComplex(false)
{
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
	connect(&m_syncMessenger, SIGNAL(messageSent()), this, SLOT(handleSynchronousMessages()), Qt::QueuedConnection);

	moveToThread(this);
}

DSPDeviceSinkEngine::~DSPDeviceSinkEngine()
{
    stop();
	wait();
}

void DSPDeviceSinkEngine::setState(State state)
{
    if (m_state != state)
    {
        m_state = state;
        emit stateChanged();
    }
}

void DSPDeviceSinkEngine::run()
{
	qDebug() << "DSPDeviceSinkEngine::run";
	setState(StIdle);
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
    gotoIdle();
    setState(StNotStarted);
    QThread::exit();
//	DSPExit cmd;
//	m_syncMessenger.sendWait(cmd);
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

void DSPDeviceSinkEngine::addChannelSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::addChannelSource: " << source->getSourceName().toStdString().c_str();
	DSPAddBasebandSampleSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::removeChannelSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::removeChannelSource: " << source->getSourceName().toStdString().c_str();
	DSPRemoveBasebandSampleSource cmd(source);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::addSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::addSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
	DSPAddSpectrumSink cmd(spectrumSink);
	m_syncMessenger.sendWait(cmd);
}

void DSPDeviceSinkEngine::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::removeSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
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

void DSPDeviceSinkEngine::workSampleFifo()
{
    SampleSourceFifo *sourceFifo = m_deviceSampleSink->getSampleFifo();

    if (!sourceFifo) {
        return;
    }

    SampleVector& data = sourceFifo->getData();
    unsigned int iPart1Begin, iPart1End, iPart2Begin, iPart2End;
    unsigned int remainder = sourceFifo->remainder();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        sourceFifo->write(remainder, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End) {
            workSamples(data, iPart1Begin, iPart1End);
        }

        if (iPart2Begin != iPart2End) {
            workSamples(data, iPart2Begin, iPart2End);
        }

        remainder = sourceFifo->remainder();
    }
}

void DSPDeviceSinkEngine::workSamples(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    unsigned int nbSamples = iEnd - iBegin;
    SampleVector::iterator begin = data.begin() + iBegin;

    if (m_basebandSampleSources.size() == 0)
    {
        m_sourceZeroBuffer.allocate(nbSamples, Sample{0,0});
        std::copy(
            m_sourceZeroBuffer.m_vector.begin(),
            m_sourceZeroBuffer.m_vector.begin() + nbSamples,
            data.begin() + iBegin
        );
    }
    else if (m_basebandSampleSources.size() == 1)
    {
        BasebandSampleSource *source = m_basebandSampleSources.front();
        source->pull(begin, nbSamples);
    }
    else
    {
        m_sourceSampleBuffer.allocate(nbSamples);
        SampleVector::iterator sBegin = m_sourceSampleBuffer.m_vector.begin();
        BasebandSampleSources::const_iterator srcIt = m_basebandSampleSources.begin();
        BasebandSampleSource *source = *srcIt;
        source->pull(begin, nbSamples);
        srcIt++;
        m_sumIndex = 1;

        for (; srcIt != m_basebandSampleSources.end(); ++srcIt, m_sumIndex++)
        {
            source = *srcIt;
            source->pull(sBegin, nbSamples);
            std::transform(
                sBegin,
                sBegin + nbSamples,
                data.begin() + iBegin,
                data.begin() + iBegin,
                [this](Sample& a, const Sample& b) -> Sample {
                    FixReal den = m_sumIndex + 1; // at each stage scale sum by n/n+1 and input by 1/n+1
                    FixReal nom = m_sumIndex;     // so that final sum is scaled by N (number of channels)
					FixReal x = a.real()/den + nom*(b.real()/den);
					FixReal y = a.imag()/den + nom*(b.imag()/den);
                    return Sample{x, y};
                }
            );
        }
    }

    // possibly feed data to spectrum sink
    if (m_spectrumSink) {
        m_spectrumSink->feed(data.begin() + iBegin, data.begin() + iEnd, m_realElseComplex);
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

	if (!m_deviceSampleSink) {
		return StIdle;
	}

	// stop everything
	m_deviceSampleSink->stop();

	for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
	{
        qDebug() << "DSPDeviceSinkEngine::gotoIdle: stopping " << (*it)->getSourceName().toStdString().c_str();
		(*it)->stop();
	}

	m_deviceDescription.clear();
	m_sampleRate = 0;

	return StIdle;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoInit()
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

	if (!m_deviceSampleSink) {
		return gotoError("DSPDeviceSinkEngine::gotoInit: No sample source configured");
	}

	// init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion

	m_deviceDescription = m_deviceSampleSink->getDeviceDescription();
	m_centerFrequency = m_deviceSampleSink->getCenterFrequency();
	m_sampleRate = m_deviceSampleSink->getSampleRate();

	qDebug() << "DSPDeviceSinkEngine::gotoInit: "
	        << " m_deviceDescription: " << m_deviceDescription.toStdString().c_str()
			<< " sampleRate: " << m_sampleRate
			<< " centerFrequency: " << m_centerFrequency;

	DSPSignalNotification notif(m_sampleRate, m_centerFrequency);

	for (BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); ++it)
	{
		qDebug() << "DSPDeviceSinkEngine::gotoInit: initializing " << (*it)->getSourceName().toStdString().c_str();
		(*it)->pushMessage(new DSPSignalNotification(notif));
	}

	if (m_spectrumSink) {
        m_spectrumSink->pushMessage(new DSPSignalNotification(notif));
	}

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

	if (!m_deviceSampleSink) {
		return gotoError("DSPDeviceSinkEngine::gotoRunning: No sample source configured");
	}

	qDebug() << "DSPDeviceSinkEngine::gotoRunning: " << m_deviceDescription.toStdString().c_str() << " started";

	// Start everything

	if (!m_deviceSampleSink->start()) {
		return gotoError("DSPDeviceSinkEngine::gotoRunning: Could not start sample sink");
	}

	for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
	{
        qDebug() << "DSPDeviceSinkEngine::gotoRunning: starting " << (*it)->getSourceName().toStdString().c_str();
		(*it)->start();
	}

	if (m_spectrumSink)
	{
        m_spectrumSink->start();
	}

	qDebug() << "DSPDeviceSinkEngine::gotoRunning: input message queue pending: " << m_inputMessageQueue.size();

	return StRunning;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoError(const QString& errorMessage)
{
	qDebug() << "DSPDeviceSinkEngine::gotoError";

	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	setState(StError);
	return StError;
}

void DSPDeviceSinkEngine::handleSetSink(DeviceSampleSink* sink)
{
	m_deviceSampleSink = sink;

    if (!m_deviceSampleSink) { // Early leave
        return;
    }

    qDebug("DSPDeviceSinkEngine::handleSetSink: set %s", qPrintable(sink->getDeviceDescription()));

    QObject::connect(
        m_deviceSampleSink->getSampleFifo(),
        &SampleSourceFifo::dataRead,
        this,
        &DSPDeviceSinkEngine::handleData,
        Qt::QueuedConnection
    );

}

void DSPDeviceSinkEngine::handleData()
{
	if (m_state == StRunning) {
		 workSampleFifo();
	}
}

void DSPDeviceSinkEngine::handleSynchronousMessages()
{
    Message *message = m_syncMessenger.getMessage();
	qDebug() << "DSPDeviceSinkEngine::handleSynchronousMessages: " << message->getIdentifier();

	if (DSPGenerationInit::match(*message))
	{
		setState(gotoIdle());

		if(m_state == StIdle) {
			setState(gotoInit()); // State goes ready if init is performed
		}
	}
	else if (DSPGenerationStart::match(*message))
	{
		if(m_state == StReady) {
			setState(gotoRunning());
		}
	}
	else if (DSPGenerationStop::match(*message))
	{
		setState(gotoIdle());
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

		m_spectrumSink = nullptr;
	}
	else if (DSPAddBasebandSampleSource::match(*message))
	{
		BasebandSampleSource* source = ((DSPAddBasebandSampleSource*) message)->getSampleSource();
		m_basebandSampleSources.push_back(source);
        DSPSignalNotification *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
        source->pushMessage(notif);

        if (m_state == StRunning)
        {
            source->start();
        }
	}
	else if (DSPRemoveBasebandSampleSource::match(*message))
	{
		BasebandSampleSource* source = ((DSPRemoveBasebandSampleSource*) message)->getSampleSource();

		if(m_state == StRunning) {
			source->stop();
		}

		m_basebandSampleSources.remove(source);
	}

	m_syncMessenger.done(m_state);
}

void DSPDeviceSinkEngine::handleInputMessages()
{
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
            m_realElseComplex = notif->getRealElseComplex();

			qDebug() << "DSPDeviceSinkEngine::handleInputMessages: DSPSignalNotification:"
				<< " m_sampleRate: " << m_sampleRate
				<< " m_centerFrequency: " << m_centerFrequency
                << " m_realElseComplex" << m_realElseComplex;

            // forward source changes to sources with immediate execution

			for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
			{
				DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy
				qDebug() << "DSPDeviceSinkEngine::handleInputMessages: forward message to " << (*it)->getSourceName().toStdString().c_str();
				(*it)->pushMessage(rep);
			}

			// forward changes to listeners on DSP output queue
            if (m_deviceSampleSink)
            {
                MessageQueue *guiMessageQueue = m_deviceSampleSink->getMessageQueueToGUI();
                qDebug("DSPDeviceSinkEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                if (guiMessageQueue)
                {
                    DSPSignalNotification* rep = new DSPSignalNotification(*notif); // make a copy for the output queue
                    guiMessageQueue->push(rep);
                }
            }

			delete message;
		}
	}
}
