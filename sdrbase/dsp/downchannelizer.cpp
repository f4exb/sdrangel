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

#include <array>

#include <QString>
#include <QDebug>

#include "dsp/inthalfbandfilter.h"
#include "dsp/dspcommands.h"
#include "dsp/hbfilterchainconverter.h"
#include "downchannelizer.h"

DownChannelizer::DownChannelizer(ChannelSampleSink* sampleSink) :
    m_filterChainSetMode(false),
	m_sampleSink(sampleSink),
	m_basebandSampleRate(0),
	m_requestedOutputSampleRate(0),
	m_requestedCenterFrequency(0),
    m_channelSampleRate(0),
	m_channelFrequencyOffset(0),
    m_log2Decim(0),
    m_filterChainHash(0)
{
}

DownChannelizer::~DownChannelizer()
{
	freeFilterChain();
}

void DownChannelizer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	if (m_sampleSink == 0)
    {
		m_sampleBuffer.clear();
		return;
	}

	if (m_filterStages.size() == 0) // optimization when no downsampling is done anyway
	{
		m_sampleSink->feed(begin, end);
	}
	else
	{
		for (SampleVector::const_iterator sample = begin; sample != end; ++sample)
		{
			Sample s(*sample);
			FilterStages::iterator stage = m_filterStages.begin();

			for (; stage != m_filterStages.end(); ++stage)
			{
#ifndef SDR_RX_SAMPLE_24BIT
                s.m_real /= 2; // avoid saturation on 16 bit samples
                s.m_imag /= 2;
#endif
				if (!(*stage)->work(&s)) {
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

		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end());
		m_sampleBuffer.clear();
	}
}

void DownChannelizer::setChannelization(int requestedSampleRate, qint64 requestedCenterFrequency)
{
    if (requestedSampleRate < 0)
    {
        qWarning("DownChannelizer::setChannelization: wrong sample rate requested: %d", requestedSampleRate);
        return;
    }

    m_requestedOutputSampleRate = requestedSampleRate;
    m_requestedCenterFrequency = requestedCenterFrequency;
    applyChannelization();
}

void DownChannelizer::setBasebandSampleRate(int basebandSampleRate, bool decim)
{
    m_basebandSampleRate = basebandSampleRate;

    if (decim) {
        applyDecimation();
    } else {
        applyChannelization();
    }
}

void DownChannelizer::applyChannelization()
{
    m_filterChainSetMode = false;

	if (m_basebandSampleRate == 0)
	{
		qDebug() << "DownChannelizer::applyChannelization: aborting (in=0)"
            << " in (baseband):" << m_basebandSampleRate
            << " req:" << m_requestedOutputSampleRate
            << " out (channel):" << m_channelSampleRate
            << " fc:" << m_channelFrequencyOffset;
        return;
	}

	freeFilterChain();

	m_channelFrequencyOffset = createFilterChain(
		m_basebandSampleRate / -2, m_basebandSampleRate / 2,
		m_requestedCenterFrequency - m_requestedOutputSampleRate / 2, m_requestedCenterFrequency + m_requestedOutputSampleRate / 2);

	m_channelSampleRate = m_basebandSampleRate / (1 << m_filterStages.size());

	qDebug() << "DownChannelizer::applyChannelization done:"
        << " nb stages:" << m_filterStages.size()
        << " in (baseband):" << m_basebandSampleRate
		<< " req:" << m_requestedOutputSampleRate
		<< " out (channel):" << m_channelSampleRate
		<< " fc:" << m_channelFrequencyOffset;
}

void DownChannelizer::setDecimation(unsigned int log2Decim, unsigned int filterChainHash)
{
    m_log2Decim = log2Decim;
    m_filterChainHash = filterChainHash;
    applyDecimation();
}

void DownChannelizer::applyDecimation()
{
    m_filterChainSetMode = true;
    std::vector<unsigned int> stageIndexes;
    m_channelFrequencyOffset = m_basebandSampleRate * HBFilterChainConverter::convertToIndexes(m_log2Decim, m_filterChainHash, stageIndexes);
    m_requestedCenterFrequency = m_channelFrequencyOffset;

    freeFilterChain();

    m_channelFrequencyOffset = m_basebandSampleRate * setFilterChain(stageIndexes);
    m_channelSampleRate = m_basebandSampleRate / (1 << m_filterStages.size());
    m_requestedOutputSampleRate = m_channelSampleRate;

	qDebug() << "DownChannelizer::applyDecimation:"
            << " m_log2Decim:" << m_log2Decim
            << " m_filterChainHash:" << m_filterChainHash
            << " out:" << m_basebandSampleRate
			<< " in:" << m_channelSampleRate
			<< " fc:" << m_channelFrequencyOffset;
}

#ifdef SDR_RX_SAMPLE_24BIT
DownChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>),
    m_workFunction(0),
    m_mode(mode),
    m_sse(true)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterEO<qint64, qint64, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateUpperHalf;
            break;
    }
}
#else
DownChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>),
    m_workFunction(0),
    m_mode(mode),
    m_sse(true)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterEO<qint32, qint32, DOWNCHANNELIZER_HB_FILTER_ORDER, true>::workDecimateUpperHalf;
            break;
    }
}
#endif

DownChannelizer::FilterStage::~FilterStage()
{
	delete m_filter;
}

Real DownChannelizer::channelMinSpace(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
    Real leftSpace = chanStart - sigStart;
    Real rightSpace = sigEnd - chanEnd;
    return std::min(leftSpace, rightSpace);
}

Real DownChannelizer::createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
	Real sigBw = sigEnd - sigStart;
    Real chanBw = chanEnd - chanStart;
	Real rot = sigBw / 4;


    std::array<Real, 3> filterMinSpaces; // Array of left, center and right filter min spaces respectively
    filterMinSpaces[0] = channelMinSpace(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
    filterMinSpaces[1] = channelMinSpace(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
    filterMinSpaces[2] = channelMinSpace(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
    auto maxIt = std::max_element(filterMinSpaces.begin(), filterMinSpaces.end());
    int maxIndex = maxIt - filterMinSpaces.begin();
    Real maxValue = *maxIt;

	qDebug("DownChannelizer::createFilterChain: Signal [%.1f, %.1f] (BW %.1f) Channel [%.1f, %.1f] (BW %.1f) Selected: %d (fit %.1f)",
        sigStart, sigEnd, sigBw, chanStart, chanEnd, chanBw, maxIndex, maxValue);

    if ((sigStart < sigEnd) && (chanStart < chanEnd) && (maxValue >= chanBw/8.0))
    {
        if (maxIndex == 0)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
            return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
        }

        if (maxIndex == 1)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
            return createFilterChain(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
        }

        if (maxIndex == 2)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
            return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
        }
    }

	Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);
	qDebug("DownChannelizer::createFilterChain: -> complete (final BW %.1f, frequency offset %.1f)", sigBw, ofs);
	return ofs;
}

double DownChannelizer::setFilterChain(const std::vector<unsigned int>& stageIndexes)
{
    // filters are described from lower to upper level but the chain is constructed the other way round
    std::vector<unsigned int>::const_reverse_iterator rit = stageIndexes.rbegin();
    double ofs = 0.0, ofs_stage = 0.25;

    // Each index is a base 3 number with 0 = low, 1 = center, 2 = high
    // Functions at upper level will convert a number to base 3 to describe the filter chain. Common converting
    // algorithms will go from LSD to MSD. This explains the reverse order.
    for (; rit != stageIndexes.rend(); ++rit)
    {
        if (*rit == 0)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
            ofs -= ofs_stage;
            qDebug("DownChannelizer::setFilterChain: lower half: ofs: %f", ofs);
        }
        else if (*rit == 1)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
            qDebug("DownChannelizer::setFilterChain: center: ofs: %f", ofs);
        }
        else if (*rit == 2)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
            ofs += ofs_stage;
            qDebug("DownChannelizer::setFilterChain: upper half: ofs: %f", ofs);
        }
    }

    return ofs;
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
