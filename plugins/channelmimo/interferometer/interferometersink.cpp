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

#include <QMutexLocker>
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
    m_scopeSink(nullptr),
    m_mutex(QMutex::Recursive)
{
    m_sampleMIFifo.init(2, 96000 * 8);
    m_vbegin.resize(2);

    for (int i = 0; i < 2; i++)
    {
        m_sinks[i].setStreamIndex(i);
        m_channelizers[i] = new DownChannelizer(&m_sinks[i]);
        m_sizes[i] = 0;
    }

    QObject::connect(
        &m_sampleMIFifo,
        &SampleMIFifo::dataSyncReady,
        this,
        &InterferometerSink::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_lastStream = 0;
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
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleMIFifo.reset();

    for (int i = 0; i < 2; i++) {
        m_sinks[i].reset();
    }
}

void InterferometerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    if (streamIndex == m_lastStream) {
        qWarning("InterferometerSink::feed: twice same stream in a row: %u", streamIndex);
    }

    m_lastStream = streamIndex;
    m_vbegin[streamIndex] = begin;
    m_sizes[streamIndex] = end - begin;

    if (streamIndex == 1)
    {
        if (m_sizes[0] != m_sizes[1])
        {
            qWarning("InterferometerSink::feed: unequal sizes: [0]: %d [1]: %d", m_sizes[0], m_sizes[1]);
            m_sampleMIFifo.writeSync(m_vbegin, std::min(m_sizes[0], m_sizes[1]));
        }
        else
        {
            m_sampleMIFifo.writeSync(m_vbegin, m_sizes[0]);
        }
    }
}

void InterferometerSink::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    const std::vector<SampleVector>& data = m_sampleMIFifo.getData();

    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;

    while ((m_sampleMIFifo.fillSync() > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleMIFifo.readSync(ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }
    }
}

void InterferometerSink::processFifo(const std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend)
{
    for (unsigned int stream = 0; stream < 2; stream++) {
        m_channelizers[stream]->feed(data[stream].begin() + ibegin, data[stream].begin() + iend, false);
    }

    run();
}

void InterferometerSink::run()
{
    if (m_correlator.performCorr(m_sinks[0].getData(), m_sinks[0].getSize(), m_sinks[1].getData(), m_sinks[1].getSize()))
    {
        if (m_scopeSink) {
            m_scopeSink->feed(m_correlator.m_tcorr.begin(), m_correlator.m_tcorr.begin() + m_correlator.m_processed, false);
        }

        if (m_spectrumSink)
        {
            if ((m_correlator.getCorrType() == InterferometerSettings::CorrelationFFT)
             || (m_correlator.getCorrType() == InterferometerSettings::CorrelationIFFT)
             || (m_correlator.getCorrType() == InterferometerSettings::CorrelationIFFT2)
             || (m_correlator.getCorrType() == InterferometerSettings::CorrelationIFFTStar))
            {
                m_spectrumSink->feed(m_correlator.m_scorr.begin(), m_correlator.m_scorr.begin() + m_correlator.m_processed, false);
            }
            else
            {
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
        QMutexLocker mutexLocker(&m_mutex);
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
            m_sinks[i].reset();
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