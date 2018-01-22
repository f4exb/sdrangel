///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////


#include <dsp/upchannelizer.h>
#include "dsp/inthalfbandfilter.h"
#include "dsp/dspcommands.h"

#include <QString>
#include <QDebug>

MESSAGE_CLASS_DEFINITION(UpChannelizer::MsgChannelizerNotification, Message)

UpChannelizer::UpChannelizer(BasebandSampleSource* sampleSource) :
    m_sampleSource(sampleSource),
    m_outputSampleRate(0),
    m_requestedInputSampleRate(0),
    m_requestedCenterFrequency(0),
    m_currentInputSampleRate(0),
    m_currentCenterFrequency(0)
{
    QString name = "UpChannelizer(" + m_sampleSource->objectName() + ")";
    setObjectName(name);
}

UpChannelizer::~UpChannelizer()
{
    freeFilterChain();
}

void UpChannelizer::configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency)
{
    Message* cmd = new DSPConfigureChannelizer(sampleRate, centerFrequency);
    messageQueue->push(cmd);
}

void UpChannelizer::pull(Sample& sample)
{
    if(m_sampleSource == 0) {
        m_sampleBuffer.clear();
        return;
    }

    if (m_filterStages.size() == 0) // optimization when no downsampling is done anyway
    {
        m_sampleSource->pull(sample);
    }
    else
    {
        m_mutex.lock();

        FilterStages::iterator stage = m_filterStages.begin();
        std::vector<Sample>::iterator stageSample = m_stageSamples.begin();

        for (; stage != m_filterStages.end(); ++stage, ++stageSample)
        {
            if(stage == m_filterStages.end() - 1)
            {
                if ((*stage)->work(&m_sampleIn, &(*stageSample)))
                {
                    m_sampleSource->pull(m_sampleIn); // get new input sample
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

//		for (; stage != m_filterStages.end(); ++stage)
//		{
//			// let's make it work for one stage only (96 kS/s < SR < 192 kS/s)
//			if(stage == m_filterStages.end() - 1)
//			{
//				if ((*stage)->work(&m_sampleIn, &sample))
//				{
//					m_sampleSource->pull(m_sampleIn); // get new input sample
//				}
//			}
//		}

        m_mutex.unlock();
    }
}

void UpChannelizer::start()
{
    if (m_sampleSource != 0)
    {
        qDebug() << "UpChannelizer::start: thread: " << thread()
                << " m_outputSampleRate: " << m_outputSampleRate
                << " m_requestedInputSampleRate: " << m_requestedInputSampleRate
                << " m_requestedCenterFrequency: " << m_requestedCenterFrequency;
        m_sampleSource->start();
    }
}

void UpChannelizer::stop()
{
    if(m_sampleSource != 0)
        m_sampleSource->stop();
}

bool UpChannelizer::handleMessage(const Message& cmd)
{
    qDebug() << "UpChannelizer::handleMessage: " << cmd.getIdentifier();

    // TODO: apply changes only if input sample rate or requested output sample rate change. Change of center frequency has no impact.

    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_outputSampleRate = notif.getSampleRate();
        qDebug() << "UpChannelizer::handleMessage: DSPSignalNotification: m_outputSampleRate: " << m_outputSampleRate;
        applyConfiguration();

        if (m_sampleSource != 0)
        {
            DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
            m_sampleSource->getInputMessageQueue()->push(rep);
        }

        emit outputSampleRateChanged();
        return true;
    }
    else if (DSPConfigureChannelizer::match(cmd))
    {
        DSPConfigureChannelizer& chan = (DSPConfigureChannelizer&) cmd;
        m_requestedInputSampleRate = chan.getSampleRate();
        m_requestedCenterFrequency = chan.getCenterFrequency();

        qDebug() << "UpChannelizer::handleMessage: DSPConfigureChannelizer:"
                << " m_requestedInputSampleRate: " << m_requestedInputSampleRate
                << " m_requestedCenterFrequency: " << m_requestedCenterFrequency;

        applyConfiguration();

        return true;
    }
    else
    {
        return false;
//        if (m_sampleSource != 0)
//        {
//            return m_sampleSource->handleMessage(cmd);
//        }
//        else
//        {
//            return false;
//        }
    }
}

void UpChannelizer::applyConfiguration()
{
    if (m_outputSampleRate == 0)
    {
        qDebug() << "UpChannelizer::applyConfiguration: aborting (out=0):"
                << " out =" << m_outputSampleRate
                << ", req =" << m_requestedInputSampleRate
                << ", in =" << m_currentInputSampleRate
                << ", fc =" << m_currentCenterFrequency;
        return;
    }

    m_mutex.lock();

    freeFilterChain();

    m_currentCenterFrequency = createFilterChain(
        m_outputSampleRate / -2, m_outputSampleRate / 2,
        m_requestedCenterFrequency - m_requestedInputSampleRate / 2, m_requestedCenterFrequency + m_requestedInputSampleRate / 2);

    m_mutex.unlock();

    m_currentInputSampleRate = m_outputSampleRate / (1 << m_filterStages.size());

    qDebug() << "UpChannelizer::applyConfiguration:"
            << " out=" << m_outputSampleRate
            << ", req=" << m_requestedInputSampleRate
            << ", in=" << m_currentInputSampleRate
            << ", fc=" << m_currentCenterFrequency;

    if (m_sampleSource != 0)
    {
        MsgChannelizerNotification *notif = MsgChannelizerNotification::create(m_outputSampleRate, m_currentInputSampleRate, m_currentCenterFrequency);
        m_sampleSource->getInputMessageQueue()->push(notif);
    }
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

bool UpChannelizer::signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const
{
    //qDebug("   testing signal [%f, %f], channel [%f, %f]", sigStart, sigEnd, chanStart, chanEnd);
    if(sigEnd <= sigStart)
        return false;
    if(chanEnd <= chanStart)
        return false;
    return (sigStart <= chanStart) && (sigEnd >= chanEnd);
}

Real UpChannelizer::createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
    Real sigBw = sigEnd - sigStart;
    Real safetyMargin = sigBw / 20;
    Real rot = sigBw / 4;
    Sample s;

    safetyMargin = 0;

    qDebug() << "UpChannelizer::createFilterChain: start:"
            << " sig: ["  << sigStart << ":" << sigEnd << "]"
            << " BW: " << sigBw
            << " chan: [" << chanStart << ":" << chanEnd << "]"
            << " rot: " << rot
            << " safety: " << safetyMargin;

    // check if it fits into the left half
    if(signalContainsChannel(sigStart + safetyMargin, sigStart + sigBw / 2.0 - safetyMargin, chanStart, chanEnd))
    {
        qDebug() << "UpChannelizer::createFilterChain: take left half (rotate by +1/4 and decimate by 2):"
                << " [" << m_filterStages.size() << "]"
                << " sig: ["  << sigStart << ":" << sigStart + sigBw / 2.0 << "]";
        m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
        m_stageSamples.push_back(s);
        return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
    }

    // check if it fits into the right half
    if(signalContainsChannel(sigEnd - sigBw / 2.0f + safetyMargin, sigEnd - safetyMargin, chanStart, chanEnd))
    {
        qDebug() << "UpChannelizer::createFilterChain: take right half (rotate by -1/4 and decimate by 2):"
                << " [" << m_filterStages.size() << "]"
                << " sig: ["  << sigEnd - sigBw / 2.0f << ":" << sigEnd << "]";
        m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
        m_stageSamples.push_back(s);
        return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
    }

    // check if it fits into the center
    // Was: if(signalContainsChannel(sigStart + rot + safetyMargin, sigStart + rot + sigBw / 2.0f - safetyMargin, chanStart, chanEnd)) {
    if(signalContainsChannel(sigStart + rot + safetyMargin, sigEnd - rot - safetyMargin, chanStart, chanEnd))
    {
        qDebug() << "UpChannelizer::createFilterChain: take center half (decimate by 2):"
                << " [" << m_filterStages.size() << "]"
                << " sig: ["  << sigStart + rot << ":" << sigEnd - rot << "]";
        m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
        m_stageSamples.push_back(s);
        // Was: return createFilterChain(sigStart + rot, sigStart + sigBw / 2.0f + rot, chanStart, chanEnd);
        return createFilterChain(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
    }

    Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);

    qDebug() << "UpChannelizer::createFilterChain: complete:"
            << " #stages: " << m_filterStages.size()
            << " BW: "  << sigBw
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




