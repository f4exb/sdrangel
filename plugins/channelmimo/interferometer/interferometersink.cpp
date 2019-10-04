///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/downchannelizer.h"
#include "dsp/dspcommands.h"

#include "interferometersink.h"


MESSAGE_CLASS_DEFINITION(InterferometerSink::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(InterferometerSink::MsgSignalNotification, Message)
MESSAGE_CLASS_DEFINITION(InterferometerSink::MsgConfigureCorrelation, Message)

InterferometerSink::InterferometerSink(int fftSize) :
    m_correlator(fftSize),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr)
{
    m_sampleMIFifo.init(2, 96000 * 4);

    for (int i = 0; i < 2; i++)
    {
        m_sinks[i].setStreamIndex(i);
        m_channelizers[i] = new DownChannelizer(&m_sinks[i]);
    }

    QObject::connect(
        &m_sampleMIFifo,
        &SampleMIFifo::dataAsyncReady,
        this,
        &InterferometerSink::handleDataAsync,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

InterferometerSink::~InterferometerSink()
{
    for (int i = 0; i < 2; i++)
    {
        delete m_channelizers[i];
    }
}

void InterferometerSink::reset()
{
    m_sampleMIFifo.reset();
}

void InterferometerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    m_sampleMIFifo.writeAsync(begin, end - begin, streamIndex);
}

void InterferometerSink::handleDataAsync(int sinkIndex)
{
    SampleVector::const_iterator part1begin;
    SampleVector::const_iterator part1end;
    SampleVector::const_iterator part2begin;
    SampleVector::const_iterator part2end;

    while ((m_sampleMIFifo.fillAsync(sinkIndex) > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleMIFifo.readAsync(&part1begin, &part1end, &part2begin, &part2end, sinkIndex);

        if (part1begin != part1end) { // first part of FIFO data
            //qDebug("InterferometerSink::handleSinkFifo: part1-stream: %u count: %u", sinkIndex, count);
            processFifo(part1begin, part1end, sinkIndex);
        }

        if (part2begin != part2end) { // second part of FIFO data (used when block wraps around)
            //qDebug("InterferometerSink::handleSinkFifo: part2-stream: %u count: %u", sinkIndex, count);
            processFifo(part2begin, part2end, sinkIndex);
        }

    }

    // int samplesDone = 0;

    // while ((m_sinkFifos[sinkIndex].fill() > 0)
    //     && (m_inputMessageQueue.size() == 0))
    //     //&& (samplesDone < m_channelizers[sinkIndex]->getInputSampleRate()))
    // {
	// 	SampleVector::iterator part1begin;
	// 	SampleVector::iterator part1end;
	// 	SampleVector::iterator part2begin;
	// 	SampleVector::iterator part2end;

    //     unsigned int count = m_sinkFifos[sinkIndex].readBegin(m_sinkFifos[sinkIndex].fill(), &part1begin, &part1end, &part2begin, &part2end);

    //     if (part1begin != part1end) { // first part of FIFO data
    //         //qDebug("InterferometerSink::handleSinkFifo: part1-stream: %u count: %u", sinkIndex, count);
    //         processFifo(part1begin, part1end, sinkIndex);
    //     }

    //     if (part2begin != part2end) { // second part of FIFO data (used when block wraps around)
    //         //qDebug("InterferometerSink::handleSinkFifo: part2-stream: %u count: %u", sinkIndex, count);
    //         processFifo(part2begin, part2end, sinkIndex);
    //     }

    //     m_sinkFifos[sinkIndex].readCommit((unsigned int) count); // adjust FIFO pointers
    //     samplesDone += count;
    // }

    //qDebug("InterferometerSink::handleSinkFifo: done");
}

void InterferometerSink::processFifo(const SampleVector::const_iterator& vbegin, const SampleVector::const_iterator& vend, unsigned int sinkIndex)
{
    // if (sinkIndex == 0) {
    //     m_count0 = vend - vbegin;
    // } else if (sinkIndex == 1) {
    //     m_count1 = vend - vbegin;
    //     if (m_count1 != m_count0) {
    //         qDebug("InterferometerSink::processFifo: c0: %d 1: %d", m_count0, m_count1);
    //     }
    // }

    m_channelizers[sinkIndex]->feed(vbegin, vend, false);

    if (sinkIndex == 1) {
        run();
    }
}

void InterferometerSink::run()
{
    // if (m_sinks[0].getSize() != m_sinks[1].getSize()) {
    //     qDebug("InterferometerSink::run: size0: %d, size1: %d", m_sinks[0].getSize(), m_sinks[1].getSize());
    // }

    if (m_correlator.performCorr(m_sinks[0].getData(), m_sinks[0].getSize(), m_sinks[1].getData(), m_sinks[1].getSize()))
    {
        if (m_scopeSink) {
            m_scopeSink->feed(m_correlator.m_tcorr.begin(), m_correlator.m_tcorr.begin() + m_correlator.m_processed, false);
        }

        if (m_spectrumSink)
        {
            if (m_correlator.getCorrType() == InterferometerSettings::CorrelationFFT) {
                m_spectrumSink->feed(m_correlator.m_scorr.begin(), m_correlator.m_scorr.begin() + m_correlator.m_processed, false);
            } else {
                m_spectrumSink->feed(m_correlator.m_tcorr.begin(), m_correlator.m_tcorr.begin() + m_correlator.m_processed, false);
            }
        }
    }

    for (int i = 0; i < 2; i++)
    {
        std::copy(
            m_sinks[i].getData().begin() + m_correlator.m_processed,
            m_sinks[i].getData().begin() + m_correlator.m_processed + m_correlator.m_remaining[i],
            m_sinks[i].getData().begin()
        );

        m_sinks[i].setDataStart(m_correlator.m_remaining[i]);
    }
}

void InterferometerSink::handleInputMessages()
{
    qDebug("InterferometerSink::handleInputMessage");
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool InterferometerSink::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        int log2Decim = cfg.getLog2Decim();
        int filterChainHash = cfg.getFilterChainHash();

        qDebug() << "InterferometerSink::handleMessage: MsgConfigureChannelizer:"
                << " log2Decim: " << log2Decim
                << " filterChainHash: " << filterChainHash;

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->set(m_channelizers[i]->getInputMessageQueue(),
                log2Decim,
                filterChainHash);
        }

        return true;
    }
    else if (MsgSignalNotification::match(cmd))
    {
        MsgSignalNotification& cfg = (MsgSignalNotification&) cmd;
        int inputSampleRate = cfg.getInputSampleRate();
        qint64 centerFrequency = cfg.getCenterFrequency();
        int streamIndex = cfg.getStreamIndex();

        qDebug() << "InterferometerSink::handleMessage: MsgSignalNotification:"
                << " inputSampleRate: " << inputSampleRate
                << " centerFrequency: " << centerFrequency
                << " streamIndex: " << streamIndex;

        if (streamIndex < 2)
        {
            DSPSignalNotification *notif = new DSPSignalNotification(inputSampleRate, centerFrequency);
            m_channelizers[streamIndex]->getInputMessageQueue()->push(notif);
        }

        return true;
    }
    else if (MsgConfigureCorrelation::match(cmd))
    {
        MsgConfigureCorrelation& cfg = (MsgConfigureCorrelation&) cmd;
        InterferometerSettings::CorrelationType correlationType = cfg.getCorrelationType();

        qDebug() << "InterferometerSink::handleMessage: MsgConfigureCorrelation:"
                << " correlationType: " << correlationType;

        m_correlator.setCorrType(correlationType);

        return true;
    }
    else
    {
        qDebug("InterferometerSink::handleMessage: unhandled: %s", cmd.getIdentifier());
        return false;
    }
}