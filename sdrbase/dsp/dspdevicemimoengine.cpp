///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <QDebug>

#include "dspcommands.h"
#include "basebandsamplesink.h"
#include "basebandsamplesource.h"
#include "devicesamplemimo.h"
#include "mimochannel.h"

#include "dspdevicemimoengine.h"

MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::SetSampleMIMO, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::AddBasebandSampleSource, Message)
MESSAGE_CLASS_DEFINITION(DSPDeviceMIMOEngine::RemoveBasebandSampleSource, Message)
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
	QObject(parent),
    m_uid(uid),
    m_stateRx(State::StNotStarted),
    m_stateTx(State::StNotStarted),
    m_deviceSampleMIMO(nullptr),
    m_spectrumInputSourceElseSink(true),
    m_spectrumInputIndex(0)
{
	setStateRx(State::StIdle);
	setStateTx(State::StIdle);
	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
}

DSPDeviceMIMOEngine::~DSPDeviceMIMOEngine()
{
    qDebug("DSPDeviceMIMOEngine::~DSPDeviceMIMOEngine");
}

void DSPDeviceMIMOEngine::setStateRx(State state)
{
    if (m_stateRx != state)
    {
        m_stateRx = state;
        emit stateChanged();
    }
}

void DSPDeviceMIMOEngine::setStateTx(State state)
{
    if (m_stateTx != state)
    {
        m_stateTx = state;
        emit stateChanged();
    }
}

bool DSPDeviceMIMOEngine::initProcess(int subsystemIndex)
{
	qDebug() << "DSPDeviceMIMOEngine::initProcess: subsystemIndex: " << subsystemIndex;

    if (subsystemIndex == 0) // Rx side
    {
        auto *cmd = new DSPAcquisitionInit();
        getInputMessageQueue()->push(cmd);
        return true;
    }
    else if (subsystemIndex == 1) // Tx side
    {
        auto *cmd = new DSPGenerationInit();
        getInputMessageQueue()->push(cmd);
        return true;
    }

    return false;
}

bool DSPDeviceMIMOEngine::startProcess(int subsystemIndex)
{
	qDebug() << "DSPDeviceMIMOEngine::startProcess: subsystemIndex: " << subsystemIndex;
    if (subsystemIndex == 0) // Rx side
    {
        auto *cmd = new DSPAcquisitionStart();
        getInputMessageQueue()->push(cmd);
        return true;
    }
    else if (subsystemIndex == 1) // Tx side
    {
	    auto *cmd = new DSPGenerationStart();
        getInputMessageQueue()->push(cmd);
        return true;
    }

    return false;
}

void DSPDeviceMIMOEngine::stopProcess(int subsystemIndex)
{
	qDebug() << "DSPDeviceMIMOEngine::stopProcess: subsystemIndex: " << subsystemIndex;

    if (subsystemIndex == 0) // Rx side
    {
        auto *cmd = new DSPAcquisitionStop();
        getInputMessageQueue()->push(cmd);
    }
    else if (subsystemIndex == 1) // Tx side
    {
        auto *cmd = new DSPGenerationStop();
        getInputMessageQueue()->push(cmd);
    }
}

void DSPDeviceMIMOEngine::setMIMO(DeviceSampleMIMO* mimo)
{
	qDebug() << "DSPDeviceMIMOEngine::setMIMO";
	auto *cmd = new SetSampleMIMO(mimo);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::setMIMOSequence(int sequence)
{
	qDebug("DSPDeviceMIMOEngine::setSinkSequence: seq: %d", sequence);
	m_sampleMIMOSequence = sequence;
}

void DSPDeviceMIMOEngine::addChannelSource(BasebandSampleSource* source, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::addChannelSource: "
        << source->getSourceName().toStdString().c_str()
        << " at: "
        << index;
	auto *cmd = new AddBasebandSampleSource(source, index);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::removeChannelSource(BasebandSampleSource* source, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::removeChannelSource: "
        << source->getSourceName().toStdString().c_str()
        << " at: "
        << index;
	auto *cmd = new RemoveBasebandSampleSource(source, index);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::addChannelSink(BasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::addChannelSink: "
        << sink->getSinkName().toStdString().c_str()
        << " at: "
        << index;
	auto *cmd = new AddBasebandSampleSink(sink, index);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::removeChannelSink(BasebandSampleSink* sink, int index)
{
	qDebug() << "DSPDeviceMIMOEngine::removeChannelSink: "
        << sink->getSinkName().toStdString().c_str()
        << " at: "
        << index;
	auto *cmd = new RemoveBasebandSampleSink(sink, index);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::addMIMOChannel(MIMOChannel *channel)
{
	qDebug() << "DSPDeviceMIMOEngine::addMIMOChannel: "
        << channel->getMIMOName().toStdString().c_str();
    auto *cmd = new AddMIMOChannel(channel);
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::removeMIMOChannel(MIMOChannel *channel)
{
	qDebug() << "DSPDeviceMIMOEngine::removeMIMOChannel: "
        << channel->getMIMOName().toStdString().c_str();
    auto *cmd = new RemoveMIMOChannel(channel);
    getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::addSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceMIMOEngine::addSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
	auto *cmd = new AddSpectrumSink(spectrumSink);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::removeSpectrumSink(BasebandSampleSink* spectrumSink)
{
	qDebug() << "DSPDeviceSinkEngine::removeSpectrumSink: " << spectrumSink->getSinkName().toStdString().c_str();
	auto *cmd = new RemoveSpectrumSink(spectrumSink);
	getInputMessageQueue()->push(cmd);
}

void DSPDeviceMIMOEngine::setSpectrumSinkInput(bool sourceElseSink, int index)
{
	qDebug() << "DSPDeviceSinkEngine::setSpectrumSinkInput: "
        << " sourceElseSink: " << sourceElseSink
        << " index: " << index;
    auto *cmd = new SetSpectrumSinkInput(sourceElseSink, index);
    getInputMessageQueue()->push(cmd);
}

QString DSPDeviceMIMOEngine::errorMessage(int subsystemIndex) const
{
	qDebug() << "DSPDeviceMIMOEngine::errorMessage: subsystemIndex:" << subsystemIndex;
    if (subsystemIndex == 0) {
        return m_errorMessageRx;
    } else if (subsystemIndex == 1) {
        return m_errorMessageTx;
    } else {
        return "Not implemented";
    }
}

QString DSPDeviceMIMOEngine::deviceDescription() const
{
	qDebug() << "DSPDeviceMIMOEngine::deviceDescription";
    return m_deviceDescription;
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

    while ((sampleFifo->fillSync() > 0) && (m_inputMessageQueue.size() == 0))
    {
        sampleFifo->readSync(iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        if (iPart1Begin != iPart1End)
        {
            for (unsigned int stream = 0; stream < data.size(); stream++) {
                workSamplesSink(data[stream].begin() + iPart1Begin, data[stream].begin() + iPart1End, stream);
            }
        }

        if (iPart2Begin != iPart2End)
        {
            for (unsigned int stream = 0; stream < data.size(); stream++) {
                workSamplesSink(data[stream].begin() + iPart2Begin, data[stream].begin() + iPart2End, stream);
            }
        }
    }
}

void DSPDeviceMIMOEngine::workSampleSourceFifos()
{
    SampleMOFifo* sampleFifo = m_deviceSampleMIMO->getSampleMOFifo();

    if (!sampleFifo) {
        return;
    }

    std::vector<SampleVector::iterator> vbegin;
    std::vector<SampleVector>& data = sampleFifo->getData();
    unsigned int iPart1Begin;
    unsigned int iPart1End;
    unsigned int iPart2Begin;
    unsigned int iPart2End;
    unsigned int remainder = sampleFifo->remainderSync();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        sampleFifo->writeSync(remainder, iPart1Begin, iPart1End, iPart2Begin, iPart2End);

        // pull samples from the sources by stream

        if (iPart1Begin != iPart1End)
        {
            for (unsigned int streamIndex = 0; streamIndex < sampleFifo->getNbStreams(); streamIndex++) {
                workSamplesSource(data[streamIndex], iPart1Begin, iPart1End, streamIndex);
            }
        }

        if (iPart2Begin != iPart2End)
        {
            for (unsigned int streamIndex = 0; streamIndex < sampleFifo->getNbStreams(); streamIndex++) {
                workSamplesSource(data[streamIndex], iPart2Begin, iPart2End, streamIndex);
            }
        }

        // get new remainder
        remainder = sampleFifo->remainderSync();
    }
}

void DSPDeviceMIMOEngine::workSampleSinkFifo(unsigned int streamIndex)
{
    SampleMIFifo* sampleFifo = m_deviceSampleMIMO->getSampleMIFifo();

    if (!sampleFifo) {
        return;
    }

    SampleVector::const_iterator part1begin;
    SampleVector::const_iterator part1end;
    SampleVector::const_iterator part2begin;
    SampleVector::const_iterator part2end;

    while ((sampleFifo->fillAsync(streamIndex) > 0) && (m_inputMessageQueue.size() == 0))
    {
        sampleFifo->readAsync(&part1begin, &part1end, &part2begin, &part2end, streamIndex);

        if (part1begin != part1end) { // first part of FIFO data
            workSamplesSink(part1begin, part1end, streamIndex);
        }

        if (part2begin != part2end) { // second part of FIFO data (used when block wraps around)
            workSamplesSink(part2begin, part2end, streamIndex);
        }
    }
}


void DSPDeviceMIMOEngine::workSampleSourceFifo(unsigned int streamIndex)
{
    SampleMOFifo* sampleFifo = m_deviceSampleMIMO->getSampleMOFifo();

    if (!sampleFifo) {
        return;
    }

    SampleVector& data = sampleFifo->getData(streamIndex);
    unsigned int iPart1Begin;
    unsigned int iPart1End;
    unsigned int iPart2Begin;
    unsigned int iPart2End;
    unsigned int amount = sampleFifo->remainderAsync(streamIndex);

    while ((amount > 0) && (m_inputMessageQueue.size() == 0))
    {
        sampleFifo->writeAsync(amount, iPart1Begin, iPart1End, iPart2Begin, iPart2End, streamIndex);
        // part1
        if (iPart1Begin != iPart1End) {
            workSamplesSource(data, iPart1Begin, iPart1End, streamIndex);
        }
        // part2
        if (iPart2Begin != iPart2End) {
            workSamplesSource(data, iPart2Begin, iPart2End, streamIndex);
        }
        // get new amount
        amount = sampleFifo->remainderAsync(streamIndex);
    }
}

/**
 * Routes samples from device source FIFO to sink channels that are registered for the FIFO
 * Routes samples from source channels registered for the FIFO to the device sink FIFO
 */
void DSPDeviceMIMOEngine::workSamplesSink(const SampleVector::const_iterator& vbegin, const SampleVector::const_iterator& vend, unsigned int streamIndex)
{
    std::map<int, bool>::const_iterator rcIt = m_rxRealElseComplex.find(streamIndex);
	bool positiveOnly = (rcIt == m_rxRealElseComplex.end() ? false : rcIt->second);

    // feed data to direct sinks
    if (streamIndex < m_basebandSampleSinks.size())
    {
        for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[streamIndex].begin(); it != m_basebandSampleSinks[streamIndex].end(); ++it) {
            (*it)->feed(vbegin, vend, positiveOnly);
        }
    }

    // possibly feed data to spectrum sink
    if (m_spectrumSink && m_spectrumInputSourceElseSink && (streamIndex == m_spectrumInputIndex)) {
        m_spectrumSink->feed(vbegin, vend, positiveOnly);
    }

    // feed data to MIMO channels
    for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it) {
        (*it)->feed(vbegin, vend, streamIndex);
    }
}

void DSPDeviceMIMOEngine::workSamplesSource(SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int streamIndex)
{
    unsigned int nbSamples = iEnd - iBegin;
    SampleVector::iterator begin = data.begin() + iBegin;

    // pull data from MIMO channels

    for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it) {
        (*it)->pull(begin, nbSamples, streamIndex);
    }

    if (m_mimoChannels.size() == 0) // Process single stream channels only if there are no MIMO channels
    {
        if (m_basebandSampleSources[streamIndex].size() == 0)
        {
            m_sourceZeroBuffers[streamIndex].allocate(nbSamples, Sample{0,0});
            std::copy(
                m_sourceZeroBuffers[streamIndex].m_vector.begin(),
                m_sourceZeroBuffers[streamIndex].m_vector.begin() + nbSamples,
                begin
            );
        }
        else if (m_basebandSampleSources[streamIndex].size() == 1)
        {
            BasebandSampleSource *sampleSource = m_basebandSampleSources[streamIndex].front();
            sampleSource->pull(begin, nbSamples);
        }
        else
        {
            m_sourceSampleBuffers[streamIndex].allocate(nbSamples);
            BasebandSampleSources::const_iterator srcIt = m_basebandSampleSources[streamIndex].begin();
            BasebandSampleSource *sampleSource = *srcIt;
            sampleSource->pull(begin, nbSamples);
            ++srcIt;
            m_sumIndex = 1;

            for (; srcIt != m_basebandSampleSources[streamIndex].end(); ++srcIt, m_sumIndex++)
            {
                sampleSource = *srcIt;
                auto aBegin = m_sourceSampleBuffers[streamIndex].m_vector.begin();
                sampleSource->pull(aBegin, nbSamples);
                std::transform(
                    aBegin,
                    aBegin + nbSamples,
                    begin,
                    begin,
                    [this](Sample const& a, const Sample& b) -> Sample {
                        FixReal den = m_sumIndex + 1; // at each stage scale sum by n/n+1 and input by 1/n+1
                        FixReal nom = m_sumIndex;     // so that final sum is scaled by N (number of channels)
                        FixReal x = a.real()/den + nom*(b.real()/den);
                        FixReal y = a.imag()/den + nom*(b.imag()/den);
                        return Sample{x, y};
                    }
                );
            }
        }
    }

    // possibly feed data to spectrum sink
    std::map<int, bool>::const_iterator rcIt = m_txRealElseComplex.find(streamIndex);
	bool positiveOnly = (rcIt == m_txRealElseComplex.end() ? false : rcIt->second);

    if (m_spectrumSink && (!m_spectrumInputSourceElseSink) && (streamIndex == m_spectrumInputIndex)) {
        m_spectrumSink->feed(begin, begin + nbSamples, positiveOnly);
    }
}

// notStarted -> idle -> init -> running -+
//                ^                       |
//                +-----------------------+

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoIdle(int subsystemIndex)
{
	qDebug() << "DSPDeviceMIMOEngine::gotoIdle: subsystemIndex:" << subsystemIndex;

	if (!m_deviceSampleMIMO) {
		return State::StIdle;
	}

    if (subsystemIndex == 0) // Rx
    {
        switch (m_stateRx) {
            case State::StNotStarted:
                return State::StNotStarted;

            case State::StIdle:
            case State::StError:
                return State::StIdle;

            case State::StReady:
            case State::StRunning:
                break;
        }

        m_deviceSampleMIMO->stopRx(); // stop everything

        std::vector<BasebandSampleSinks>::const_iterator vbit = m_basebandSampleSinks.begin();

        for (; vbit != m_basebandSampleSinks.end(); ++vbit)
        {
            for (auto it = vbit->begin(); it != vbit->end(); ++it)
            {
                qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping BasebandSampleSink: " << (*it)->getSinkName().toStdString().c_str();
                (*it)->stop();
            }
        }

        for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping MIMOChannel sinks: " << (*it)->getMIMOName().toStdString().c_str();
            (*it)->stopSinks();
        }
    }
    else if (subsystemIndex == 1) // Tx
    {
        switch (m_stateTx) {
            case State::StNotStarted:
                return State::StNotStarted;

            case State::StIdle:
            case State::StError:
                return State::StIdle;

            case State::StReady:
            case State::StRunning:
                break;
        }

        m_deviceSampleMIMO->stopTx(); // stop everything

        std::vector<BasebandSampleSources>::const_iterator vSourceIt = m_basebandSampleSources.begin();

        for (; vSourceIt != m_basebandSampleSources.end(); vSourceIt++)
        {
            for (auto it = vSourceIt->begin(); it != vSourceIt->end(); ++it)
            {
                qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping BasebandSampleSource(" << (*it)->getSourceName().toStdString().c_str() << ")";
                (*it)->stop();
            }
        }

        for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoIdle: stopping MIMOChannel sources: " << (*it)->getMIMOName().toStdString().c_str();
            (*it)->stopSources();
        }
    }
    else
    {
        return State::StIdle;
    }

	m_deviceDescription.clear();

	return State::StIdle;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoInit(int subsystemIndex)
{
	if (!m_deviceSampleMIMO) {
		return gotoError(subsystemIndex, "No sample MIMO configured");
	}

	m_deviceDescription = m_deviceSampleMIMO->getDeviceDescription();

	qDebug() << "DSPDeviceMIMOEngine::gotoInit:"
            << "subsystemIndex: " << subsystemIndex
	        << "m_deviceDescription: " << m_deviceDescription.toStdString().c_str();

    if (subsystemIndex == 0) // Rx
    {
        switch(m_stateRx) {
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

	    // init: pass sample rate and center frequency to all sample rate and/or center frequency dependent sinks and wait for completion
        for (unsigned int isource = 0; isource < m_deviceSampleMIMO->getNbSourceStreams(); isource++)
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
                    qDebug() << "DSPDeviceMIMOEngine::gotoInit: initializing " << (*it)->getSinkName().toStdString().c_str();
                    (*it)->pushMessage(new DSPSignalNotification(notif));
                }
            }
        }
    }
    else if (subsystemIndex == 1) // Tx
    {
        switch(m_stateTx) {
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

        for (unsigned int isink = 0; isink < m_deviceSampleMIMO->getNbSinkStreams(); isink++)
        {
            quint64 sinkCenterFrequency = m_deviceSampleMIMO->getSinkCenterFrequency(isink);
            int sinkStreamSampleRate = m_deviceSampleMIMO->getSinkSampleRate(isink);

            qDebug("DSPDeviceMIMOEngine::gotoInit: m_sinkCenterFrequencies[%d] = %llu", isink,  sinkCenterFrequency);
            qDebug("DSPDeviceMIMOEngine::gotoInit: m_sinkStreamSampleRates[%d] = %d", isink,  sinkStreamSampleRate);

            DSPSignalNotification notif(sinkStreamSampleRate, sinkCenterFrequency);

            if (isink < m_basebandSampleSources.size())
            {
                for (BasebandSampleSources::const_iterator it = m_basebandSampleSources[isink].begin(); it != m_basebandSampleSources[isink].end(); ++it)
                {
                    qDebug() << "DSPDeviceMIMOEngine::gotoInit: initializing BasebandSampleSource(" << (*it)->getSourceName().toStdString().c_str() << ")";
                    (*it)->pushMessage(new DSPSignalNotification(notif));
                }
            }
        }
    }

	return State::StReady;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoRunning(int subsystemIndex)
{
	qDebug() << "DSPDeviceMIMOEngine::gotoRunning: subsystemIndex:" << subsystemIndex;

	if (!m_deviceSampleMIMO) {
		return gotoError(subsystemIndex, "DSPDeviceMIMOEngine::gotoRunning: No sample source configured");
	}

	qDebug() << "DSPDeviceMIMOEngine::gotoRunning:" << m_deviceDescription.toStdString().c_str() << "started";

    if (subsystemIndex == 0) // Rx
    {
        switch (m_stateRx)
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

        if (!m_deviceSampleMIMO->startRx()) { // Start everything
            return gotoError(0, "Could not start sample source");
        }

        std::vector<BasebandSampleSinks>::const_iterator vbit = m_basebandSampleSinks.begin();

        for (; vbit != m_basebandSampleSinks.end(); ++vbit)
        {
            for (auto it = vbit->begin(); it != vbit->end(); ++it)
            {
                qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting BasebandSampleSink: " << (*it)->getSinkName().toStdString().c_str();
                (*it)->start();
            }
        }

        for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting MIMOChannel sinks: " << (*it)->getMIMOName().toStdString().c_str();
            (*it)->startSinks();
        }
    }
    else if (subsystemIndex == 1) // Tx
    {
        switch (m_stateTx)
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

        if (!m_deviceSampleMIMO->startTx()) { // Start everything
            return gotoError(1, "Could not start sample sink");
        }

        std::vector<BasebandSampleSources>::const_iterator vSourceIt = m_basebandSampleSources.begin();

        for (; vSourceIt != m_basebandSampleSources.end(); vSourceIt++)
        {
            for (auto it = vSourceIt->begin(); it != vSourceIt->end(); ++it)
            {
                qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting BasebandSampleSource(" << (*it)->getSourceName().toStdString().c_str() << ")";
                (*it)->start();
            }
        }

        for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
        {
            qDebug() << "DSPDeviceMIMOEngine::gotoRunning: starting MIMOChannel sources: " << (*it)->getMIMOName().toStdString().c_str();
            (*it)->startSources();
        }
    }

	qDebug() << "DSPDeviceMIMOEngine::gotoRunning:input message queue pending: " << m_inputMessageQueue.size();

	return State::StRunning;
}

DSPDeviceMIMOEngine::State DSPDeviceMIMOEngine::gotoError(int subsystemIndex, const QString& errorMessage)
{
	qDebug() << "DSPDeviceMIMOEngine::gotoError: "
        << " subsystemIndex: " << subsystemIndex
        <<  " errorMessage: " << errorMessage;


    if (subsystemIndex == 0)
    {
        m_errorMessageRx = errorMessage;
        setStateRx(State::StError);
    }
    else if (subsystemIndex == 1)
    {
        m_errorMessageTx = errorMessage;
        setStateTx(State::StError);
    }

    return State::StError;
}

void DSPDeviceMIMOEngine::handleDataRxSync()
{
	if (m_stateRx == State::StRunning) {
        workSampleSinkFifos();
	}
}

void DSPDeviceMIMOEngine::handleDataRxAsync(int streamIndex)
{
	if (m_stateRx == State::StRunning) {
		workSampleSinkFifo(streamIndex);
	}
}

void DSPDeviceMIMOEngine::handleDataTxSync()
{
	if (m_stateTx == State::StRunning) {
        workSampleSourceFifos();
	}
}

void DSPDeviceMIMOEngine::handleDataTxAsync(int streamIndex)
{
	if (m_stateTx == State::StRunning) {
		workSampleSourceFifo(streamIndex);
	}
}

void DSPDeviceMIMOEngine::handleSetMIMO(DeviceSampleMIMO* mimo)
{
    m_deviceSampleMIMO = mimo;

    if (!mimo) { // Early leave
        return;
    }

    for (unsigned int i = 0; i < m_deviceSampleMIMO->getNbSinkFifos(); i++)
    {
        m_basebandSampleSinks.emplace_back();
        m_sourcesCorrections.emplace_back();
    }

    for (unsigned int i = 0; i < m_deviceSampleMIMO->getNbSourceFifos(); i++)
    {
        m_basebandSampleSources.emplace_back();
        m_sourceSampleBuffers.emplace_back();
        m_sourceZeroBuffers.emplace_back();
    }

    if (m_deviceSampleMIMO->getMIMOType() == DeviceSampleMIMO::MIMOHalfSynchronous) // synchronous FIFOs on Rx and not with Tx
    {
        qDebug("DSPDeviceMIMOEngine::handleSetMIMO: synchronous sources set %s", qPrintable(mimo->getDeviceDescription()));
        QObject::connect(
            m_deviceSampleMIMO->getSampleMIFifo(),
            &SampleMIFifo::dataSyncReady,
            this,
            &DSPDeviceMIMOEngine::handleDataRxSync,
            Qt::QueuedConnection
        );
        QObject::connect(
            m_deviceSampleMIMO->getSampleMOFifo(),
            &SampleMOFifo::dataReadSync,
            this,
            &DSPDeviceMIMOEngine::handleDataTxSync,
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
            QObject::connect(
                m_deviceSampleMIMO->getSampleMOFifo(),
                &SampleMOFifo::dataReadAsync,
                this,
                &DSPDeviceMIMOEngine::handleDataTxAsync,
                Qt::QueuedConnection
            );
        }
    }
}

bool DSPDeviceMIMOEngine::handleMessage(const Message& message)
{
    if (ConfigureCorrection::match(message))
    {
        const auto& conf = (const ConfigureCorrection&) message;
        unsigned int isource = conf.getIndex();

        if (isource < m_sourcesCorrections.size())
        {
            m_sourcesCorrections[isource].m_iqImbalanceCorrection = conf.getIQImbalanceCorrection();

            if (m_sourcesCorrections[isource].m_dcOffsetCorrection != conf.getDCOffsetCorrection())
            {
                m_sourcesCorrections[isource].m_dcOffsetCorrection = conf.getDCOffsetCorrection();
                m_sourcesCorrections[isource].m_iOffset = 0;
                m_sourcesCorrections[isource].m_qOffset = 0;

                if (m_sourcesCorrections[isource].m_iqImbalanceCorrection != conf.getIQImbalanceCorrection())
                {
                    m_sourcesCorrections[isource].m_iqImbalanceCorrection = conf.getIQImbalanceCorrection();
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

        return true;
    }
    else if (DSPMIMOSignalNotification::match(message))
    {
        const auto& notif = (const DSPMIMOSignalNotification&) message;

        // update DSP values

        bool sourceElseSink = notif.getSourceOrSink();
        unsigned int istream = notif.getIndex();
        int sampleRate = notif.getSampleRate();
        qint64 centerFrequency = notif.getCenterFrequency();
        bool realElseComplex = notif.getRealElseComplex();

        qDebug() << "DeviceMIMOEngine::handleInputMessages: DSPMIMOSignalNotification:"
            << " sourceElseSink: " << sourceElseSink
            << " istream: " << istream
            << " sampleRate: " << sampleRate
            << " centerFrequency: " << centerFrequency
            << " realElseComplex" << realElseComplex;

        if (sourceElseSink) {
            m_rxRealElseComplex[istream] = realElseComplex;
        } else {
            m_txRealElseComplex[istream] = realElseComplex;
        }

        for (MIMOChannels::const_iterator it = m_mimoChannels.begin(); it != m_mimoChannels.end(); ++it)
        {
            auto *msg = new DSPMIMOSignalNotification(notif);
            (*it)->pushMessage(msg);
        }

        if (m_deviceSampleMIMO)
        {
            if (sourceElseSink)
            {
                if (istream < m_deviceSampleMIMO->getNbSourceStreams())
                {

                    // forward source changes to ancillary sinks
                    if (istream < m_basebandSampleSinks.size())
                    {
                        for (BasebandSampleSinks::const_iterator it = m_basebandSampleSinks[istream].begin(); it != m_basebandSampleSinks[istream].end(); ++it)
                        {
                            auto *msg = new DSPSignalNotification(sampleRate, centerFrequency);
                            qDebug() << "DSPDeviceMIMOEngine::handleInputMessages: starting " << (*it)->getSinkName().toStdString().c_str();
                            (*it)->pushMessage(msg);
                        }
                    }

                    // forward changes to MIMO GUI input queue
                    MessageQueue *guiMessageQueue = m_deviceSampleMIMO->getMessageQueueToGUI();
                    qDebug("DeviceMIMOEngine::handleInputMessages: DSPMIMOSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    if (guiMessageQueue) {
                        auto* rep = new DSPMIMOSignalNotification(notif); // make a copy for the MIMO GUI
                        guiMessageQueue->push(rep);
                    }

                    // forward changes to spectrum sink if currently active
                    if (m_spectrumSink && m_spectrumInputSourceElseSink && (m_spectrumInputIndex == istream))
                    {
                        auto *spectrumNotif = new DSPSignalNotification(sampleRate, centerFrequency);
                        m_spectrumSink->pushMessage(spectrumNotif);
                    }
                }
            }
            else
            {
                if (istream < m_deviceSampleMIMO->getNbSinkStreams())
                {

                    // forward source changes to channel sources with immediate execution (no queuing)
                    if (istream < m_basebandSampleSources.size())
                    {
                        for (BasebandSampleSources::const_iterator it = m_basebandSampleSources[istream].begin(); it != m_basebandSampleSources[istream].end(); ++it)
                        {
                            auto *msg = new DSPSignalNotification(sampleRate, centerFrequency);
                            qDebug() << "DSPDeviceMIMOEngine::handleSinkMessages: forward message to BasebandSampleSource(" << (*it)->getSourceName().toStdString().c_str() << ")";
                            (*it)->pushMessage(msg);
                        }
                    }

                    // forward changes to MIMO GUI input queue
                    MessageQueue *guiMessageQueue = m_deviceSampleMIMO->getMessageQueueToGUI();
                    qDebug("DSPDeviceMIMOEngine::handleInputMessages: DSPSignalNotification: guiMessageQueue: %p", guiMessageQueue);

                    if (guiMessageQueue) {
                        auto* rep = new DSPMIMOSignalNotification(notif); // make a copy for the source GUI
                        guiMessageQueue->push(rep);
                    }

                    // forward changes to spectrum sink if currently active
                    if (m_spectrumSink && !m_spectrumInputSourceElseSink && (m_spectrumInputIndex == istream))
                    {
                        auto *spectrumNotif = new DSPSignalNotification(sampleRate, centerFrequency);
                        m_spectrumSink->pushMessage(spectrumNotif);
                    }
                }
            }
        }

        return true;
    }
    // was in handleSynchronousMessages
	else if (DSPAcquisitionInit::match(message))
	{
		setStateRx(gotoIdle(0));

		if (m_stateRx == State::StIdle) {
			setStateRx(gotoInit(0)); // State goes ready if init is performed
		}

        return true;
	}
	else if (DSPAcquisitionStart::match(message))
	{
		if (m_stateRx == State::StReady) {
			setStateRx(gotoRunning(0));
		}

        return true;
	}
    else if (DSPAcquisitionStop::match(message))
	{
		setStateRx(gotoIdle(0));
        emit acquisitionStopped();
        return true;
	}
    else if (DSPGenerationInit::match(message))
	{
		setStateTx(gotoIdle(1));

		if (m_stateTx == State::StIdle) {
			setStateTx(gotoInit(1)); // State goes ready if init is performed
		}

        return true;
	}
	else if (DSPGenerationStart::match(message))
	{
		if (m_stateTx == State::StReady) {
			setStateTx(gotoRunning(1));
		}

        return true;
	}
	else if (DSPGenerationStop::match(message))
	{
		setStateTx(gotoIdle(1));
        emit generationStopped();
        return true;
	}
	else if (SetSampleMIMO::match(message)) {
        const auto& cmd = (const SetSampleMIMO&) message;
        handleSetMIMO(cmd.getSampleMIMO());
        emit sampleSet();
		return true;
	}
	else if (AddBasebandSampleSink::match(message))
	{
        const auto& msg = (const AddBasebandSampleSink&) message;
		BasebandSampleSink* sink = msg.getSampleSink();
        unsigned int isource = msg.getIndex();

        if (isource < m_basebandSampleSinks.size())
        {
            m_basebandSampleSinks[isource].push_back(sink);
            // initialize sample rate and center frequency in the sink:
            int sourceStreamSampleRate = m_deviceSampleMIMO->getSourceSampleRate(isource);
            quint64 sourceCenterFrequency = m_deviceSampleMIMO->getSourceCenterFrequency(isource);
            auto *msgToSink = new DSPSignalNotification(sourceStreamSampleRate, sourceCenterFrequency);
            sink->pushMessage(msgToSink);
            // start the sink:
            if (m_stateRx == State::StRunning) {
                sink->start();
            }
        }

        return true;
	}
	else if (RemoveBasebandSampleSink::match(message))
	{
        const auto& msg = (const RemoveBasebandSampleSink&) message;
		BasebandSampleSink* sink = msg.getSampleSink();
        unsigned int isource = msg.getIndex();

        if (isource < m_basebandSampleSinks.size())
        {
            if (m_stateRx == State::StRunning) {
                sink->stop();
            }

            m_basebandSampleSinks[isource].remove(sink);
        }

        return true;
	}
	else if (AddBasebandSampleSource::match(message))
	{
        const auto& msg = (const AddBasebandSampleSource&) message;
		BasebandSampleSource *sampleSource = msg.getSampleSource();
        unsigned int isink = msg.getIndex();

        if (isink < m_basebandSampleSources.size())
        {
            m_basebandSampleSources[isink].push_back(sampleSource);
            // initialize sample rate and center frequency in the sink:
            int sinkStreamSampleRate = m_deviceSampleMIMO->getSinkSampleRate(isink);
            quint64 sinkCenterFrequency = m_deviceSampleMIMO->getSinkCenterFrequency(isink);
            auto *msgToSource = new DSPSignalNotification(sinkStreamSampleRate, sinkCenterFrequency);
            sampleSource->pushMessage(msgToSource);
            // start the sink:
            if (m_stateTx == State::StRunning) {
                sampleSource->start();
            }
        }

        return true;
	}
	else if (RemoveBasebandSampleSource::match(message))
	{
        const auto& msg = (const RemoveBasebandSampleSource&) message;
		BasebandSampleSource* sampleSource = msg.getSampleSource();
        unsigned int isink = msg.getIndex();

        if (isink < m_basebandSampleSources.size())
        {
            sampleSource->stop();
            m_basebandSampleSources[isink].remove(sampleSource);
        }

        return true;
	}
    else if (AddMIMOChannel::match(message))
    {
        const auto& msg = (const AddMIMOChannel&) message;
        MIMOChannel *channel = msg.getChannel();
        m_mimoChannels.push_back(channel);

        for (unsigned int isource = 0; isource < m_deviceSampleMIMO->getNbSourceStreams(); isource++)
        {
            auto *notif = new DSPMIMOSignalNotification(
                m_deviceSampleMIMO->getSourceSampleRate(isource),
                m_deviceSampleMIMO->getSourceCenterFrequency(isource),
                true,
                isource
            );
            channel->pushMessage(notif);
        }

        for (unsigned int isink = 0; isink < m_deviceSampleMIMO->getNbSinkStreams(); isink++)
        {
            auto *notif = new DSPMIMOSignalNotification(
                m_deviceSampleMIMO->getSinkSampleRate(isink),
                m_deviceSampleMIMO->getSinkCenterFrequency(isink),
                false,
                isink
            );
            channel->pushMessage(notif);
        }

        if (m_stateRx == State::StRunning) {
            channel->startSinks();
        }

        if (m_stateTx == State::StRunning) {
            channel->startSources();
        }

        return true;
    }
    else if (RemoveMIMOChannel::match(message))
    {
        const auto& msg = (const RemoveMIMOChannel&) message;
        MIMOChannel *channel = msg.getChannel();
        channel->stopSinks();
        channel->stopSources();
        m_mimoChannels.remove(channel);
        return true;
    }
	else if (AddSpectrumSink::match(message))
	{
        const auto& msg = (const AddSpectrumSink&) message;
		m_spectrumSink = msg.getSampleSink();
        return true;
	}
    else if (RemoveSpectrumSink::match(message))
    {
        const auto& msg = (const RemoveSpectrumSink&) message;
        BasebandSampleSink* spectrumSink = msg.getSampleSink();
        spectrumSink->stop();
        m_spectrumSink = nullptr;
        emit spectrumSinkRemoved();
        return true;
    }
    else if (SetSpectrumSinkInput::match(message))
    {
        const auto& msg = (const SetSpectrumSinkInput&) message;
        bool spectrumInputSourceElseSink = msg.getSourceElseSink();
        unsigned int spectrumInputIndex = msg.getIndex();

        if ((spectrumInputSourceElseSink != m_spectrumInputSourceElseSink) || (spectrumInputIndex != m_spectrumInputIndex))
        {
            if ((!spectrumInputSourceElseSink)
            && (spectrumInputIndex <  m_deviceSampleMIMO->getNbSinkStreams())
            && m_spectrumSink) // add the source listener
            {
                auto *notif = new DSPSignalNotification(
                    m_deviceSampleMIMO->getSinkSampleRate(spectrumInputIndex),
                    m_deviceSampleMIMO->getSinkCenterFrequency(spectrumInputIndex));
                m_spectrumSink->pushMessage(notif);
            }

            if (m_spectrumSink && spectrumInputSourceElseSink && (spectrumInputIndex <  m_deviceSampleMIMO->getNbSinkFifos()))
            {
                auto *notif = new DSPSignalNotification(
                    m_deviceSampleMIMO->getSourceSampleRate(spectrumInputIndex),
                    m_deviceSampleMIMO->getSourceCenterFrequency(spectrumInputIndex));
                m_spectrumSink->pushMessage(notif);
            }

            m_spectrumInputSourceElseSink = spectrumInputSourceElseSink;
            m_spectrumInputIndex = spectrumInputIndex;
        }

        return true;
    }


    return false;
}

void DSPDeviceMIMOEngine::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		qDebug("DSPDeviceMIMOEngine::handleInputMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message)) {
            delete message;
        }
	}
}

void DSPDeviceMIMOEngine::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection, int isource)
{
	qDebug() << "DSPDeviceMIMOEngine::configureCorrections";
	auto* cmd = new ConfigureCorrection(dcOffsetCorrection, iqImbalanceCorrection, isource);
	m_inputMessageQueue.push(cmd);
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
            float xi = (float) (it->m_real - (int32_t) m_sourcesCorrections[isource].m_iBeta) / SDR_RX_SCALEF;
            float xq = (float) (it->m_imag - (int32_t) m_sourcesCorrections[isource].m_qBeta) / SDR_RX_SCALEF;

            // phase imbalance
            m_sourcesCorrections[isource].m_avgII(xi*xi); // <I", I">
            m_sourcesCorrections[isource].m_avgIQ(xi*xq); // <I", Q">


            if (m_sourcesCorrections[isource].m_avgII.asDouble() != 0) {
                m_sourcesCorrections[isource].m_avgPhi(m_sourcesCorrections[isource].m_avgIQ.asDouble()/m_sourcesCorrections[isource].m_avgII.asDouble());
            }

            const float& yi = xi; // the in phase remains the reference
            float yq = xq - (float) m_sourcesCorrections[isource].m_avgPhi.asDouble()*xi;

            // amplitude I/Q imbalance
            m_sourcesCorrections[isource].m_avgII2(yi*yi); // <I, I>
            m_sourcesCorrections[isource].m_avgQQ2(yq*yq); // <Q, Q>

            if (m_sourcesCorrections[isource].m_avgQQ2.asDouble() != 0) {
                m_sourcesCorrections[isource].m_avgAmp(sqrt(m_sourcesCorrections[isource].m_avgII2.asDouble() / m_sourcesCorrections[isource].m_avgQQ2.asDouble()));
            }

            // final correction
            const float& zi = yi; // the in phase remains the reference
            auto zq = (float) (m_sourcesCorrections[isource].m_avgAmp.asDouble() * yq);

            // convert and store
            it->m_real = (FixReal) (zi * SDR_RX_SCALEF);
            it->m_imag = (FixReal) (zq * SDR_RX_SCALEF);
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
