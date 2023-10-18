///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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
#include <algorithm>

#include "inthalfbandfilter.h"
#include "dspcommands.h"
#include "hbfilterchainconverter.h"
#include "upchannelizer.h"

UpChannelizer::UpChannelizer(ChannelSampleSource* sampleSource) :
    m_filterChainSetMode(false),
    m_sampleSource(sampleSource),
    m_basebandSampleRate(0),
    m_requestedInputSampleRate(0),
    m_requestedCenterFrequency(0),
    m_channelSampleRate(0),
    m_channelFrequencyOffset(0),
    m_log2Interp(0),
    m_filterChainHash(0)
{
}

UpChannelizer::~UpChannelizer()
{
    freeFilterChain();
}

void UpChannelizer::pullOne(Sample& sample)
{
    if (m_sampleSource == nullptr)
    {
        m_sampleBuffer.clear();
        return;
    }

    unsigned int log2Interp = m_filterStages.size();

    if (log2Interp == 0) // optimization when no downsampling is done anyway
    {
        m_sampleSource->pullOne(sample);
    }
    else
    {
        FilterStages::iterator stage = m_filterStages.begin();
        std::vector<Sample>::iterator stageSample = m_stageSamples.begin();

        for (; stage != m_filterStages.end(); ++stage, ++stageSample)
        {
            if(stage == m_filterStages.end() - 1)
            {
                if ((*stage)->work(&m_sampleIn, &(*stageSample)))
                {
                    m_sampleSource->pullOne(m_sampleIn); // get new input sample
                }
            }
            else
            {
                if (!(*stage)->work(&(*(stageSample+1)), &(*stageSample)))
                {
                    break;
                }
            }
        }

        sample = *m_stageSamples.begin();
    }
}

void UpChannelizer::pull(SampleVector::iterator begin, unsigned int nbSamples)
{
    if (m_sampleSource == nullptr)
    {
        m_sampleBuffer.clear();
        return;
    }

    unsigned int log2Interp = m_filterStages.size();

    if (log2Interp == 0) // optimization when no downsampling is done anyway
    {
        m_sampleSource->pull(begin, nbSamples);
    }
    else
    {
        std::for_each(
            begin,
            begin + nbSamples,
            [this](Sample& s) {
                pullOne(s);
            }
        );
    }
}

void UpChannelizer::prefetch(unsigned int nbSamples)
{
    unsigned int log2Interp = m_filterStages.size();
    m_sampleSource->prefetch(nbSamples/(1<<log2Interp)); // 2^n less samples will be produced by the source
}

void UpChannelizer::setChannelization(int requestedSampleRate, qint64 requestedCenterFrequency)
{
    m_requestedInputSampleRate = requestedSampleRate;
    m_requestedCenterFrequency = requestedCenterFrequency;
    applyChannelization();
}

void UpChannelizer::setBasebandSampleRate(int basebandSampleRate, bool interp)
{
    m_basebandSampleRate = basebandSampleRate;

    if (interp) {
        applyInterpolation();
    } else {
        applyChannelization();
    }
}

void UpChannelizer::applyChannelization()
{
    m_filterChainSetMode = false;

    if (m_basebandSampleRate == 0)
    {
        qDebug() << "UpChannelizer::applyConfiguration: aborting (out=0):"
                << " out:" << m_basebandSampleRate
                << " req:" << m_requestedInputSampleRate
                << " in:" << m_channelSampleRate
                << " fc:" << m_channelFrequencyOffset;
        return;
    }

    freeFilterChain();

    m_channelFrequencyOffset = createFilterChain(
        m_basebandSampleRate / -2, m_basebandSampleRate / 2,
        m_requestedCenterFrequency - m_requestedInputSampleRate / 2, m_requestedCenterFrequency + m_requestedInputSampleRate / 2);

    m_channelSampleRate = m_basebandSampleRate / (1 << m_filterStages.size());

    qDebug() << "UpChannelizer::applyConfiguration: done: "
            << " out:" << m_basebandSampleRate
            << " req:" << m_requestedInputSampleRate
            << " in:" << m_channelSampleRate
            << " fc:" << m_channelFrequencyOffset;
}

void UpChannelizer::setInterpolation(unsigned int log2Interp, unsigned int filterChainHash)
{
    m_log2Interp = log2Interp;
    m_filterChainHash = filterChainHash;
    applyInterpolation();
}

void UpChannelizer::applyInterpolation()
{
    m_filterChainSetMode = true;
    std::vector<unsigned int> stageIndexes;
    m_channelFrequencyOffset = m_basebandSampleRate * HBFilterChainConverter::convertToIndexes(m_log2Interp, m_filterChainHash, stageIndexes);
    m_requestedCenterFrequency = m_channelFrequencyOffset;

    freeFilterChain();

    m_channelFrequencyOffset = m_basebandSampleRate * setFilterChain(stageIndexes);
    m_channelSampleRate = m_basebandSampleRate / (1 << m_filterStages.size());
    m_requestedInputSampleRate = m_channelSampleRate;

	qDebug() << "UpChannelizer::applyInterpolation:"
            << " m_log2Interp:" << m_log2Interp
            << " m_filterChainHash:" << m_filterChainHash
            << " out:" << m_basebandSampleRate
			<< " in:" << m_channelSampleRate
			<< " fc:" << m_channelFrequencyOffset;
}

#ifdef USE_SSE4_1
UpChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>),
    m_workFunction(0)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterEO1<UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateUpperHalf;
            break;
    }
}
#else
UpChannelizer::FilterStage::FilterStage(Mode mode) :
    m_filter(new IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>),
    m_workFunction(0)
{
    switch(mode) {
        case ModeCenter:
            m_workFunction = &IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateCenter;
            break;

        case ModeLowerHalf:
            m_workFunction = &IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateLowerHalf;
            break;

        case ModeUpperHalf:
            m_workFunction = &IntHalfbandFilterDB<qint32, UPCHANNELIZER_HB_FILTER_ORDER>::workInterpolateUpperHalf;
            break;
    }
}
#endif

UpChannelizer::FilterStage::~FilterStage()
{
    delete m_filter;
}

Real UpChannelizer::channelMinSpace(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
    Real leftSpace = chanStart - sigStart;
    Real rightSpace = sigEnd - chanEnd;
    return std::min(leftSpace, rightSpace);
}

Real UpChannelizer::createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
    Real sigBw = sigEnd - sigStart;
    Real chanBw = chanEnd - chanStart;
    Real rot = sigBw / 4;
    Sample s;

    std::array<Real, 3> filterMinSpaces; // Array of left, center and right filter min spaces respectively
    filterMinSpaces[0] = channelMinSpace(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
    filterMinSpaces[1] = channelMinSpace(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
    filterMinSpaces[2] = channelMinSpace(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
    auto maxIt = std::max_element(filterMinSpaces.begin(), filterMinSpaces.end());
    int maxIndex = maxIt - filterMinSpaces.begin();
    Real maxValue = *maxIt;

	qDebug("UpChannelizer::createFilterChain: Signal [%.1f, %.1f] (BW %.1f) Channel [%.1f, %.1f] (BW %.1f) Selected: %d (fit %.1f)",
        sigStart, sigEnd, sigBw, chanStart, chanEnd, chanBw, maxIndex, maxValue);

    if ((sigStart < sigEnd) && (chanStart < chanEnd) && (maxValue >= chanBw/8.0))
    {
        if (maxIndex == 0)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
            m_stageSamples.push_back(s);
            return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
        }

        if (maxIndex == 1)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
            m_stageSamples.push_back(s);
            return createFilterChain(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
        }

        if (maxIndex == 2)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
            m_stageSamples.push_back(s);
            return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
        }
    }

    Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);

    qDebug() << "UpChannelizer::createFilterChain: complete:"
            << " #stages: " << m_filterStages.size()
            << " BW: "  << sigBw
            << " ofs: " << ofs;

    return ofs;
}

double UpChannelizer::setFilterChain(const std::vector<unsigned int>& stageIndexes)
{
    // filters are described from lower to upper level but the chain is constructed the other way round
    std::vector<unsigned int>::const_reverse_iterator rit = stageIndexes.rbegin();
    double ofs = 0.0, ofs_stage = 0.25;
    Sample s;

    // Each index is a base 3 number with 0 = low, 1 = center, 2 = high
    // Functions at upper level will convert a number to base 3 to describe the filter chain. Common converting
    // algorithms will go from LSD to MSD. This explains the reverse order.
    for (; rit != stageIndexes.rend(); ++rit)
    {
        if (*rit == 0)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
            m_stageSamples.push_back(s);
            ofs -= ofs_stage;
            qDebug("UpChannelizer::setFilterChain: lower half: ofs: %f", ofs);
        }
        else if (*rit == 1)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
            m_stageSamples.push_back(s);
            qDebug("UpChannelizer::setFilterChain: center: ofs: %f", ofs);
        }
        else if (*rit == 2)
        {
            m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
            m_stageSamples.push_back(s);
            ofs += ofs_stage;
            qDebug("UpChannelizer::setFilterChain: upper half: ofs: %f", ofs);
        }

        ofs_stage /= 2;
    }

    qDebug() << "UpChannelizer::setFilterChain: complete:"
            << " #stages: " << m_filterStages.size()
            << " ofs: " << ofs;

    return ofs;
}

void UpChannelizer::freeFilterChain()
{
    for(FilterStages::iterator it = m_filterStages.begin(); it != m_filterStages.end(); ++it)
        delete *it;
    m_filterStages.clear();
    m_stageSamples.clear();
}




