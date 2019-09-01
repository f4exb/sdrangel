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
#include "interferometersink.h"


MESSAGE_CLASS_DEFINITION(InterferometerSink::MsgConfigureChannelizer, Message)

InterferometerSink::InterferometerSink() :
    m_correlator(4096),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr)
{
    for (int i = 0; i < 2; i++)
    {
        m_sinks[i].setStreamIndex(i);
        m_channelizers[i] = new DownChannelizer(&m_sinks[i]);
        QObject::connect(
            &m_sinkBuffers[i],
            &SampleSinkVector::dataReady,
            this,
            [=](){ this->handleSinkBuffer(i); },
            Qt::QueuedConnection
        );
    }

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

InterferometerSink::~InterferometerSink()
{
    for (int i = 0; i < 2; i++)
    {
        delete m_channelizers[i];
    }
}

void InterferometerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int streamIndex)
{
    if (streamIndex > 1) {
        return;
    }

    m_sinkBuffers[streamIndex].write(begin, end);
}

void InterferometerSink::handleSinkBuffer(unsigned int sinkIndex)
{
    SampleVector::iterator vbegin;
    SampleVector::iterator vend;
    m_sinkBuffers[sinkIndex].read(vbegin, vend);
    m_channelizers[sinkIndex]->feed(vbegin, vend, false);

    if (sinkIndex == 1) {
        run();
    }
}

void InterferometerSink::run()
{
    m_correlator.performCorr(m_sinks[0].getData(), m_sinks[1].getData());

    if (m_scopeSink) {
        m_scopeSink->feed(m_correlator.m_tcorr.begin(), m_correlator.m_tcorr.begin() + m_correlator.m_processed, false);
    }

    if (m_spectrumSink)
    {
        if (m_correlator.getCorrType() == InterferometerSettings::CorrelationCorrelation) {
            m_spectrumSink->feed(m_correlator.m_scorr.begin(), m_correlator.m_scorr.begin() + m_correlator.m_processed, false);
        } else {
            m_spectrumSink->feed(m_correlator.m_tcorr.begin(), m_correlator.m_tcorr.begin() + m_correlator.m_processed, false);
        }
    }

    if (m_correlator.m_remaining != 0)
    {
        for (int i = 0; i < 2; i++)
        {
            std::copy(
                m_sinks[i].getData().begin() + m_correlator.m_processed,
                m_sinks[i].getData().begin() + m_correlator.m_processed + m_correlator.m_remaining,
                m_sinks[i].getData().begin()
            );
        }
    }

    m_sinks[0].setDataStart(m_correlator.m_remaining);
    m_sinks[1].setDataStart(m_correlator.m_remaining);
}

void InterferometerSink::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != 0)
	{
		if (handleMessage(*message))
		{
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
    else
    {
        return false;
    }
}