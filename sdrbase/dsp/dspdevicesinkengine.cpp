///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
	QObject(parent),
    m_uid(uid),
	m_state(State::StNotStarted),
	m_deviceSampleSink(nullptr),
	m_sampleSinkSequence(0),
	m_basebandSampleSources(),
	m_spectrumSink(nullptr),
	m_sampleRate(0),
	m_centerFrequency(0),
    m_realElseComplex(false)
{
    setState(State::StIdle);
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

DSPDeviceSinkEngine::~DSPDeviceSinkEngine()
{
    qDebug("DSPDeviceSinkEngine::~DSPDeviceSinkEngine");
}

void DSPDeviceSinkEngine::setState(State state)
{
    if (m_state != state)
    {
        m_state = state;
        emit stateChanged();
    }
}

bool DSPDeviceSinkEngine::initGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::initGeneration";
	auto *cmd = new DSPGenerationInit();
    getInputMessageQueue()->push(cmd);
	return true;
}

bool DSPDeviceSinkEngine::startGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::startGeneration";
	auto *cmd = new DSPGenerationStart();
    getInputMessageQueue()->push(cmd);
	return true;
}

void DSPDeviceSinkEngine::stopGeneration()
{
	qDebug() << "DSPDeviceSinkEngine::stopGeneration";
	auto *cmd = new DSPGenerationStop();
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceSinkEngine::setSink(DeviceSampleSink* sink)
{
	qDebug() << "DSPDeviceSinkEngine::setSink";
    m_deviceSampleSink = sink;
	auto *cmd = new DSPSetSink(sink);
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceSinkEngine::setSinkSequence(int sequence)
{
	qDebug("DSPDeviceSinkEngine::setSinkSequence: seq: %d", sequence);
	m_sampleSinkSequence = sequence;
}

void DSPDeviceSinkEngine::addChannelSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::addChannelSource: " << source->getSourceName().toStdString().c_str();
	auto *cmd = new DSPAddBasebandSampleSource(source);
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceSinkEngine::removeChannelSource(BasebandSampleSource* source)
{
	qDebug() << "DSPDeviceSinkEngine::removeChannelSource: " << source->getSourceName().toStdString().c_str();
	auto *cmd = new DSPRemoveBasebandSampleSource(source);
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceSinkEngine::addSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::addSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
    m_spectrumSink = spectrumSink;
}

void DSPDeviceSinkEngine::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::removeSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
	auto *cmd = new DSPRemoveSpectrumSink(spectrumSink);
	getInputMessageQueue()->push(cmd);
}

QString DSPDeviceSinkEngine::errorMessage() const
{
	qDebug() << "DSPDeviceSinkEngine::errorMessage";
    return m_errorMessage;
}

QString DSPDeviceSinkEngine::sinkDeviceDescription() const
{
	qDebug() << "DSPDeviceSinkEngine::sinkDeviceDescription";
    return m_deviceDescription;
}

void DSPDeviceSinkEngine::workSampleFifo()
{
    SampleSourceFifo *sourceFifo = m_deviceSampleSink->getSampleFifo();

    if (!sourceFifo) {
        return;
    }

    SampleVector& data = sourceFifo->getData();
    unsigned int iPart1Begin;
    unsigned int iPart1End;
    unsigned int iPart2Begin;
    unsigned int iPart2End;
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
        auto sBegin = m_sourceSampleBuffer.m_vector.begin();
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
                [this](const Sample& a, const Sample& b) -> Sample {
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
		case State::StNotStarted:
			return State::StNotStarted;

		case State::StIdle:
		case State::StError:
			return State::StIdle;

		case State::StReady:
		case State::StRunning:
			break;
	}

	if (!m_deviceSampleSink) {
		return State::StIdle;
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

	return State::StIdle;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoInit()
{
	switch(m_state) {
		case State::StNotStarted:
			return State::StNotStarted;

		case State::StRunning:
			return State::StRunning;

		case State::StReady:
			return State::StReady;

		case State::StIdle:
		case State::StError:
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
        auto* rep = new DSPSignalNotification(notif); // make a copy for the output queue
        m_deviceSampleSink->getMessageQueueToGUI()->push(rep);
	}

	return State::StReady;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoRunning()
{
	qDebug() << "DSPDeviceSinkEngine::gotoRunning";

	switch(m_state)
    {
		case State::StNotStarted:
			return State::StNotStarted;

		case State::StIdle:
			return State::StIdle;

		case State::StRunning:
			return State::StRunning;

		case State::StReady:
		case State::StError:
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

	return State::StRunning;
}

DSPDeviceSinkEngine::State DSPDeviceSinkEngine::gotoError(const QString& errorMessage)
{
	qDebug() << "DSPDeviceSinkEngine::gotoError";

	m_errorMessage = errorMessage;
	m_deviceDescription.clear();
	setState(State::StError);
	return State::StError;
}

void DSPDeviceSinkEngine::handleSetSink(const DeviceSampleSink*)
{
    if (!m_deviceSampleSink) { // Early leave
        return;
    }

    qDebug("DSPDeviceSinkEngine::handleSetSink: set %s", qPrintable(m_deviceSampleSink->getDeviceDescription()));

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
	if (m_state == State::StRunning) {
		workSampleFifo();
	}
}

bool DSPDeviceSinkEngine::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        auto& notif = (const DSPSignalNotification&) message;

        // update DSP values

        m_sampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        m_realElseComplex = notif.getRealElseComplex();

        qDebug() << "DSPDeviceSinkEngine::handleInputMessages: DSPSignalNotification:"
            << " m_sampleRate: " << m_sampleRate
            << " m_centerFrequency: " << m_centerFrequency
            << " m_realElseComplex" << m_realElseComplex;

        // forward source changes to sources with immediate execution

        for(BasebandSampleSources::const_iterator it = m_basebandSampleSources.begin(); it != m_basebandSampleSources.end(); it++)
        {
            auto *rep = new DSPSignalNotification(notif); // make a copy
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
                auto *rep = new DSPSignalNotification(notif); // make a copy for the output queue
                guiMessageQueue->push(rep);
            }
        }

        return true;
    }
    // From synchronous messages
	if (DSPGenerationInit::match(message))
	{
		setState(gotoIdle());

		if(m_state == State::StIdle) {
			setState(gotoInit()); // State goes ready if init is performed
		}

        return true;
	}
	else if (DSPGenerationStart::match(message))
	{
		if(m_state == State::StReady) {
			setState(gotoRunning());
		}

        return true;
	}
	else if (DSPGenerationStop::match(message))
	{
		setState(gotoIdle());
		emit generationStopped();
        return true;
	}
	else if (DSPSetSink::match(message))
    {
        const auto& cmd = (const DSPSetSink&) message;
		handleSetSink(cmd.getSampleSink());
		emit sampleSet();
        return true;
	}
	else if (DSPRemoveSpectrumSink::match(message))
	{
        auto& cmd = (const DSPRemoveSpectrumSink&) message;
		BasebandSampleSink* spectrumSink = cmd.getSampleSink();

		if(m_state == State::StRunning) {
			spectrumSink->stop();
		}

		m_spectrumSink = nullptr;
		emit spectrumSinkRemoved();
        return true;
	}
	else if (DSPAddBasebandSampleSource::match(message))
	{
        auto& cmd = (const DSPAddBasebandSampleSource&) message;
		BasebandSampleSource* source = cmd.getSampleSource();
		m_basebandSampleSources.push_back(source);
        auto *notif = new DSPSignalNotification(m_sampleRate, m_centerFrequency);
        source->pushMessage(notif);

        if (m_state == State::StRunning) {
            source->start();
        }

        return true;
	}
	else if (DSPRemoveBasebandSampleSource::match(message))
	{
        auto& cmd = (const DSPRemoveBasebandSampleSource&) message;
		BasebandSampleSource* source = cmd.getSampleSource();

		if(m_state == State::StRunning) {
			source->stop();
		}

		m_basebandSampleSources.remove(source);
        return true;
	}

    return false;
}

void DSPDeviceSinkEngine::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		qDebug("DSPDeviceSinkEngine::handleInputMessages: message: %s", message->getIdentifier());
        if (handleMessage(*message)) {
            delete message;
        }
	}
}
