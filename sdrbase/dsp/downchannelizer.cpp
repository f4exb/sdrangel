///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 F4EXB                                                 //
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

#include <dsp/downchannelizer.h>
#include "dsp/inthalfbandfilter.h"
#include "dsp/dspcommands.h"
#include "dsp/hbfilterchainconverter.h"

#include <QString>
#include <QDebug>

MESSAGE_CLASS_DEFINITION(DownChannelizer::MsgChannelizerNotification, Message)
MESSAGE_CLASS_DEFINITION(DownChannelizer::MsgSetChannelizer, Message)

DownChannelizer::DownChannelizer(BasebandSampleSink* sampleSink) :
    m_filterChainSetMode(false),
	m_sampleSink(sampleSink),
	m_inputSampleRate(0),
	m_requestedOutputSampleRate(0),
	m_requestedCenterFrequency(0),
	m_currentOutputSampleRate(0),
	m_currentCenterFrequency(0)
{
	QString name = "DownChannelizer(" + m_sampleSink->objectName() + ")";
	setObjectName(name);
}

DownChannelizer::~DownChannelizer()
{
	freeFilterChain();
}

void DownChannelizer::configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency)
{
	Message* cmd = new DSPConfigureChannelizer(sampleRate, centerFrequency);
	messageQueue->push(cmd);
}

void DownChannelizer::set(MessageQueue* messageQueue, unsigned int log2Decim, unsigned int filterChainHash)
{
    Message* cmd = new MsgSetChannelizer(log2Decim, filterChainHash);
    messageQueue->push(cmd);
}

void DownChannelizer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	if(m_sampleSink == 0) {
		m_sampleBuffer.clear();
		return;
	}

	if (m_filterStages.size() == 0) // optimization when no downsampling is done anyway
	{
		m_sampleSink->feed(begin, end, positiveOnly);
	}
	else
	{
		m_mutex.lock();

		for(SampleVector::const_iterator sample = begin; sample != end; ++sample)
		{
			Sample s(*sample);
			FilterStages::iterator stage = m_filterStages.begin();

			for (; stage != m_filterStages.end(); ++stage)
			{
#ifndef SDR_RX_SAMPLE_24BIT
                s.m_real /= 2; // avoid saturation on 16 bit samples
                s.m_imag /= 2;
#endif
				if(!(*stage)->work(&s))
				{
					break;
				}
			}

			if(stage == m_filterStages.end())
			{
#ifdef SDR_RX_SAMPLE_24BIT
			    s.m_real /= (1<<(m_filterStages.size())); // on 32 bit samples there is enough headroom to just divide the final result
			    s.m_imag /= (1<<(m_filterStages.size()));
#endif
				m_sampleBuffer.push_back(s);
			}
		}

		m_mutex.unlock();

		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), positiveOnly);
		m_sampleBuffer.clear();
	}
}

void DownChannelizer::start()
{
	if (m_sampleSink != 0)
	{
		qDebug() << "DownChannelizer::start: thread: " << thread()
				<< " m_inputSampleRate: " << m_inputSampleRate
				<< " m_requestedOutputSampleRate: " << m_requestedOutputSampleRate
				<< " m_requestedCenterFrequency: " << m_requestedCenterFrequency;
		m_sampleSink->start();
	}
}

void DownChannelizer::stop()
{
	if(m_sampleSink != 0)
		m_sampleSink->stop();
}

bool DownChannelizer::handleMessage(const Message& cmd)
{
	// TODO: apply changes only if input sample rate or requested output sample rate change. Change of center frequency has no impact.

	if (DSPSignalNotification::match(cmd))
	{
		DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
		m_inputSampleRate = notif.getSampleRate();
		qDebug() << "DownChannelizer::handleMessage: DSPSignalNotification: m_inputSampleRate: " << m_inputSampleRate;

        if (!m_filterChainSetMode) {
		    applyConfiguration();
        }

		if (m_sampleSink != 0)
		{
		    DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
			m_sampleSink->getInputMessageQueue()->push(rep);
		}

		emit inputSampleRateChanged();
		return true;
	}
	else if (DSPConfigureChannelizer::match(cmd))
	{
		DSPConfigureChannelizer& chan = (DSPConfigureChannelizer&) cmd;
		m_requestedOutputSampleRate = chan.getSampleRate();
		m_requestedCenterFrequency = chan.getCenterFrequency();

            // qDebug() << "DownChannelizer::handleMessage: DSPConfigureChannelizer:"
            // 		<< " m_requestedOutputSampleRate: " << m_requestedOutputSampleRate
            // 		<< " m_requestedCenterFrequency: " << m_requestedCenterFrequency;

		applyConfiguration();

		return true;
	}
    else if (MsgSetChannelizer::match(cmd))
    {
        MsgSetChannelizer& chan = (MsgSetChannelizer&) cmd;
        qDebug() << "DownChannelizer::handleMessage: MsgSetChannelizer";
        applySetting(chan.getLog2Decim(), chan.getFilterChainHash());

        return true;
    }
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        qDebug() << "DownChannelizer::handleMessage: MsgThreadedSink: forwarded to demod";
        return m_sampleSink->handleMessage(cmd); // this message is passed to the demod
    }
	else
	{
        qDebug() << "DownChannelizer::handleMessage: " << cmd.getIdentifier() << " unhandled";
	    return false;
	}
}

void DownChannelizer::applyConfiguration()
{
    m_filterChainSetMode = false;

	if (m_inputSampleRate == 0)
	{
		qDebug() << "DownChannelizer::applyConfiguration: m_inputSampleRate=0 aborting";
		return;
	}

	m_mutex.lock();

	freeFilterChain();

	m_currentCenterFrequency = createFilterChain(
		m_inputSampleRate / -2, m_inputSampleRate / 2,
		m_requestedCenterFrequency - m_requestedOutputSampleRate / 2, m_requestedCenterFrequency + m_requestedOutputSampleRate / 2);

	m_mutex.unlock();

	//debugFilterChain();

	m_currentOutputSampleRate = m_inputSampleRate / (1 << m_filterStages.size());

	qDebug() << "DownChannelizer::applyConfiguration in=" << m_inputSampleRate
			<< ", req=" << m_requestedOutputSampleRate
			<< ", out=" << m_currentOutputSampleRate
			<< ", fc=" << m_currentCenterFrequency;

	if (m_sampleSink != 0)
	{
		MsgChannelizerNotification *notif = MsgChannelizerNotification::create(m_currentOutputSampleRate, m_currentCenterFrequency);
		m_sampleSink->getInputMessageQueue()->push(notif);
	}
}

void DownChannelizer::applySetting(unsigned int log2Decim, unsigned int filterChainHash)
{
    m_filterChainSetMode = true;
    std::vector<unsigned int> stageIndexes;
    m_currentCenterFrequency = m_inputSampleRate * HBFilterChainConverter::convertToIndexes(log2Decim, filterChainHash, stageIndexes);
    m_requestedCenterFrequency = m_currentCenterFrequency;

    m_mutex.lock();
    freeFilterChain();
    setFilterChain(stageIndexes);
    m_mutex.unlock();

    m_currentOutputSampleRate = m_inputSampleRate / (1 << m_filterStages.size());
    m_requestedOutputSampleRate = m_currentOutputSampleRate;

	qDebug() << "DownChannelizer::applySetting inputSampleRate:" << m_inputSampleRate
			<< " currentOutputSampleRate: " << m_currentOutputSampleRate
			<< " currentCenterFrequency: " << m_currentCenterFrequency
            << " nb_filters: " << stageIndexes.size()
            << " nb_stages: " << m_filterStages.size();

	if (m_sampleSink != 0)
	{
		MsgChannelizerNotification *notif = MsgChannelizerNotification::create(m_currentOutputSampleRate, m_currentCenterFrequency);
		m_sampleSink->getInputMessageQueue()->push(notif);
	}
}

#ifdef SDR_RX_SAMPLE_24BIT
DownChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>),
    m_workFunction(0),
    m_mode(mode),
    m_sse(true)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateUpperHalf;
            break;
    }
}
#else
DownChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>),
    m_workFunction(0),
    m_mode(mode),
    m_sse(true)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER>::workDecimateUpperHalf;
            break;
    }
}
#endif

DownChannelizer::FilterStage::~FilterStage()
{
	delete m_filter;
}

bool DownChannelizer::signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const
{
	//qDebug("   testing signal [%f, %f], channel [%f, %f]", sigStart, sigEnd, chanStart, chanEnd);
	if(sigEnd <= sigStart)
		return false;
	if(chanEnd <= chanStart)
		return false;
	return (sigStart <= chanStart) && (sigEnd >= chanEnd);
}

Real DownChannelizer::createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
	Real sigBw = sigEnd - sigStart;
	Real rot = sigBw / 4;

	//qDebug("DownChannelizer::createFilterChain: Signal [%.1f, %.1f] (BW %.1f), Channel [%.1f, %.1f], Rot %.1f", sigStart, sigEnd, sigBw, chanStart, chanEnd, rot);

	// check if it fits into the left half
	if(signalContainsChannel(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd))
    {
		//qDebug("DownChannelizer::createFilterChain: -> take left half (rotate by +1/4 and decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
		return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
	}

	// check if it fits into the right half
	if(signalContainsChannel(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd))
    {
		//qDebug("DownChannelizer::createFilterChain: -> take right half (rotate by -1/4 and decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
		return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
	}

	// check if it fits into the center
	if(signalContainsChannel(sigStart + rot, sigEnd - rot, chanStart, chanEnd))
    {
		//qDebug("DownChannelizer::createFilterChain: -> take center half (decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
		return createFilterChain(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
	}

	Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);
	//qDebug("DownChannelizer::createFilterChain: -> complete (final BW %.1f, frequency offset %.1f)", sigBw, ofs);
	return ofs;
}

void DownChannelizer::setFilterChain(const std::vector<unsigned int>& stageIndexes)
{
    // filters are described from lower to upper level but the chain is constructed the other way round
    std::vector<unsigned int>::const_reverse_iterator rit = stageIndexes.rbegin();

    // Each index is a base 3 number with 0 = low, 1 = center, 2 = high
    // Functions at upper level will convert a number to base 3 to describe the filter chain. Common converting
    // algorithms will go from LSD to MSD. This explains the reverse order.
    for (; rit != stageIndexes.rend(); ++rit)
    {
        if (*rit == 0) {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
        } else if (*rit == 1) {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
        } else if (*rit == 2) {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
        }
    }
}

void DownChannelizer::freeFilterChain()
{
	for(FilterStages::iterator it = m_filterStages.begin(); it != m_filterStages.end(); ++it)
		delete *it;
	m_filterStages.clear();
}

void DownChannelizer::debugFilterChain()
{
    qDebug("DownChannelizer::debugFilterChain: %lu stages", m_filterStages.size());

    for(FilterStages::iterator it = m_filterStages.begin(); it != m_filterStages.end(); ++it)
    {
        switch ((*it)->m_mode)
        {
        case FilterStage::ModeCenter:
            qDebug("DownChannelizer::debugFilterChain: center %s", (*it)->m_sse ? "sse" : "no_sse");
            break;
        case FilterStage::ModeLowerHalf:
            qDebug("DownChannelizer::debugFilterChain: lower %s", (*it)->m_sse ? "sse" : "no_sse");
            break;
        case FilterStage::ModeUpperHalf:
            qDebug("DownChannelizer::debugFilterChain: upper %s", (*it)->m_sse ? "sse" : "no_sse");
            break;
        default:
            qDebug("DownChannelizer::debugFilterChain: none %s", (*it)->m_sse ? "sse" : "no_sse");
            break;
        }
    }
}
