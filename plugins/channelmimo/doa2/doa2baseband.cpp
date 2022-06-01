///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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
#include "dsp/basebandsamplesink.h"
#include "dsp/scopevis.h"
#include "dsp/dspcommands.h"

#include "doa2baseband.h"
#include "doa2settings.h"


MESSAGE_CLASS_DEFINITION(DOA2Baseband::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(DOA2Baseband::MsgSignalNotification, Message)
MESSAGE_CLASS_DEFINITION(DOA2Baseband::MsgConfigureCorrelation, Message)

DOA2Baseband::DOA2Baseband(int fftSize) :
    m_correlator(fftSize),
    m_correlationType(DOA2Settings::CorrelationFFT),
    m_fftSize(fftSize),
    m_samplesCount(0),
    m_magSum(0.0f),
    m_wphSum(0.0f),
    m_phi(0.0f),
    m_magThreshold(0.0f),
    m_fftAvg(1),
    m_fftAvgCount(0),
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
        &DOA2Baseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_lastStream = 0;
}

DOA2Baseband::~DOA2Baseband()
{
    for (int i = 0; i < 2; i++)
    {
        delete m_channelizers[i];
    }
}

void DOA2Baseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleMIFifo.reset();

    for (int i = 0; i < 2; i++) {
        m_sinks[i].reset();
    }
}

void DOA2Baseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    if (streamIndex == m_lastStream) {
        qWarning("DOA2Baseband::feed: twice same stream in a row: %u", streamIndex);
    }

    m_lastStream = streamIndex;
    m_vbegin[streamIndex] = begin;
    m_sizes[streamIndex] = end - begin;

    if (streamIndex == 1)
    {
        if (m_sizes[0] != m_sizes[1])
        {
            qWarning("DOA2Baseband::feed: unequal sizes: [0]: %d [1]: %d", m_sizes[0], m_sizes[1]);
            m_sampleMIFifo.writeSync(m_vbegin, std::min(m_sizes[0], m_sizes[1]));
        }
        else
        {
            m_sampleMIFifo.writeSync(m_vbegin, m_sizes[0]);
        }
    }
}

void DOA2Baseband::handleData()
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

void DOA2Baseband::processFifo(const std::vector<SampleVector>& data, unsigned int ibegin, unsigned int iend)
{
    for (unsigned int stream = 0; stream < 2; stream++) {
        m_channelizers[stream]->feed(data[stream].begin() + ibegin, data[stream].begin() + iend);
    }

    run();
}

void DOA2Baseband::run()
{
    if (m_correlator.performCorr(m_sinks[0].getData(), m_sinks[0].getSize(), m_sinks[1].getData(), m_sinks[1].getSize()))
    {
        if (m_correlationType == DOA2Settings::CorrelationType::CorrelationFFT) {
            processDOA(m_correlator.m_xcorr.begin(), m_correlator.m_processed);
        }

        if (m_scopeSink)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_correlator.m_tcorr.begin());
            m_scopeSink->feed(vbegin, m_correlator.m_processed);
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

void DOA2Baseband::handleInputMessages()
{
    qDebug("DOA2Baseband::handleInputMessage");
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool DOA2Baseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        int log2Decim = cfg.getLog2Decim();
        int filterChainHash = cfg.getFilterChainHash();

        qDebug() << "DOA2Baseband::handleMessage: MsgConfigureChannelizer:"
                << " log2Decim: " << log2Decim
                << " filterChainHash: " << filterChainHash;

        for (int i = 0; i < 2; i++)
        {
            m_channelizers[i]->setDecimation(log2Decim, filterChainHash);
            m_sinks[i].reset();
        }

        return true;
    }
    else if (MsgSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgSignalNotification& cfg = (MsgSignalNotification&) cmd;
        int inputSampleRate = cfg.getInputSampleRate();
        qint64 centerFrequency = cfg.getCenterFrequency();
        int streamIndex = cfg.getStreamIndex();

        qDebug() << "DOA2Baseband::handleMessage: MsgSignalNotification:"
                << " inputSampleRate: " << inputSampleRate
                << " centerFrequency: " << centerFrequency
                << " streamIndex: " << streamIndex;

        if (streamIndex < 2)
        {
            m_channelizers[streamIndex]->setBasebandSampleRate(inputSampleRate);
            m_sinks[streamIndex].reset();
        }

        return true;
    }
    else if (MsgConfigureCorrelation::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureCorrelation& cfg = (MsgConfigureCorrelation&) cmd;
        m_correlationType = cfg.getCorrelationType();

        qDebug() << "DOA2Baseband::handleMessage: MsgConfigureCorrelation:"
                << " correlationType: " << m_correlationType;

        m_correlator.setCorrType(m_correlationType);

        return true;
    }
    else
    {
        qDebug("DOA2Baseband::handleMessage: unhandled: %s", cmd.getIdentifier());
        return false;
    }
}

void DOA2Baseband ::setBasebandSampleRate(unsigned int sampleRate)
{
    for (int istream = 0; istream < 2; istream++)
    {
        m_channelizers[istream]->setBasebandSampleRate(sampleRate);
        m_sinks[istream].reset();
    }
}

void DOA2Baseband::setFFTAveraging(int nbFFT)
{
    qDebug("DOA2Baseband::setFFTAveraging: %d", nbFFT);
    m_fftAvg = nbFFT < 1 ? 1 : nbFFT;
    m_fftAvgCount = 0;
    m_magSum = 0;
    m_wphSum = 0;
    m_samplesCount = 0;
}

void DOA2Baseband::processDOA(const std::vector<Complex>::iterator& begin, int nbSamples, bool reverse)
{
    const std::vector<Complex>::iterator end = begin + nbSamples;

    for (std::vector<Complex>::iterator it = begin; it != end; ++it)
    {
        float ph = std::arg(*it);
        double mag = std::norm(*it);

        if (mag  > m_magThreshold)
        {
            m_magSum += mag;
            m_wphSum += mag*ph;
        }

        if (++m_samplesCount == m_fftSize)
        {
            if (m_wphSum != 0)
            {
                if (++m_fftAvgCount == m_fftAvg)
                {
                    m_phi = reverse ? -(m_wphSum / m_magSum) : (m_wphSum / m_magSum);
                    m_fftAvgCount = 0;
                }
            }

            m_magSum = 0;
            m_wphSum = 0;
            m_samplesCount = 0;
        }
    }
}
