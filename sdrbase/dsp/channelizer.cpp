#include "dsp/channelizer.h"
#include "dsp/inthalfbandfilter.h"
#include "dsp/dspcommands.h"

Channelizer::Channelizer(SampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_inputSampleRate(100000),
	m_requestedOutputSampleRate(100000),
	m_requestedCenterFrequency(0),
	m_currentOutputSampleRate(100000),
	m_currentCenterFrequency(0)
{
}

Channelizer::~Channelizer()
{
	freeFilterChain();
}

void Channelizer::configure(MessageQueue* messageQueue, int sampleRate, int centerFrequency)
{
	Message* cmd = DSPConfigureChannelizer::create(sampleRate, centerFrequency);
	cmd->submit(messageQueue, this);
}

void Channelizer::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool firstOfBurst)
{
	for(SampleVector::const_iterator sample = begin; sample != end; ++sample) {
		Sample s(*sample);
		FilterStages::iterator stage = m_filterStages.begin();
		while(stage != m_filterStages.end()) {
			if(!(*stage)->work(&s))
				break;
			++stage;
		}
		if(stage == m_filterStages.end())
			m_sampleBuffer.push_back(s);
	}

	if(m_sampleSink != NULL)
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), firstOfBurst);

	m_sampleBuffer.clear();
}

void Channelizer::start()
{
	if(m_sampleSink != NULL)
		m_sampleSink->start();
}

void Channelizer::stop()
{
	if(m_sampleSink != NULL)
		m_sampleSink->stop();
}

bool Channelizer::handleMessage(Message* cmd)
{
	if(DSPSignalNotification::match(cmd)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)cmd;
		m_inputSampleRate = signal->getSampleRate();
		applyConfiguration();
		cmd->completed();
		if(m_sampleSink != NULL) {
			signal = DSPSignalNotification::create(m_currentOutputSampleRate, m_currentCenterFrequency);
			if(!m_sampleSink->handleMessage(signal))
				signal->completed();
		}
		return true;
	} else if(DSPConfigureChannelizer::match(cmd)) {
		DSPConfigureChannelizer* chan = (DSPConfigureChannelizer*)cmd;
		m_requestedOutputSampleRate = chan->getSampleRate();
		m_requestedCenterFrequency = chan->getCenterFrequency();
		applyConfiguration();
		cmd->completed();
		if(m_sampleSink != NULL) {
			DSPSignalNotification* signal = DSPSignalNotification::create(m_currentOutputSampleRate, m_currentCenterFrequency);
			if(!m_sampleSink->handleMessage(signal))
				signal->completed();
		}
		return true;
	} else {
		if(m_sampleSink != NULL)
			return m_sampleSink->handleMessage(cmd);
		else return false;
	}
}

void Channelizer::applyConfiguration()
{
	freeFilterChain();
	m_currentCenterFrequency = createFilterChain(
		m_inputSampleRate / -2, m_inputSampleRate / 2,
		m_requestedCenterFrequency - m_requestedOutputSampleRate / 2, m_requestedCenterFrequency + m_requestedOutputSampleRate / 2);
	m_currentOutputSampleRate = m_inputSampleRate / (1 << m_filterStages.size());
}

Channelizer::FilterStage::FilterStage(Mode mode) :
	m_filter(new IntHalfbandFilter),
	m_workFunction(NULL)
{
	switch(mode) {
		case ModeCenter:
			m_workFunction = &IntHalfbandFilter::workDecimateCenter;
			break;

		case ModeLowerHalf:
			m_workFunction = &IntHalfbandFilter::workDecimateLowerHalf;
			break;

		case ModeUpperHalf:
			m_workFunction = &IntHalfbandFilter::workDecimateUpperHalf;
			break;
	}
}

Channelizer::FilterStage::~FilterStage()
{
	delete m_filter;
}

bool Channelizer::signalContainsChannel(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd) const
{
	//qDebug("   testing signal [%f, %f], channel [%f, %f]", sigStart, sigEnd, chanStart, chanEnd);
	if(sigEnd <= sigStart)
		return false;
	if(chanEnd <= chanStart)
		return false;
	return (sigStart <= chanStart) && (sigEnd >= chanEnd);
}

Real Channelizer::createFilterChain(Real sigStart, Real sigEnd, Real chanStart, Real chanEnd)
{
	Real sigBw = sigEnd - sigStart;
	Real safetyMargin = sigBw / 20;
	Real rot = sigBw / 4;

	safetyMargin = 0;

	//qDebug("Signal [%f, %f] (BW %f), Channel [%f, %f], Rot %f, Safety %f", sigStart, sigEnd, sigBw, chanStart, chanEnd, rot, safetyMargin);
#if 1
	// check if it fits into the left half
	if(signalContainsChannel(sigStart + safetyMargin, sigStart + sigBw / 2.0 - safetyMargin, chanStart, chanEnd)) {
		//qDebug("-> take left half (rotate by +1/4 and decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
		return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
	}

	// check if it fits into the right half
	if(signalContainsChannel(sigEnd - sigBw / 2.0f + safetyMargin, sigEnd - safetyMargin, chanStart, chanEnd)) {
		//qDebug("-> take right half (rotate by -1/4 and decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
		return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
	}

	// check if it fits into the center
	if(signalContainsChannel(sigStart + rot + safetyMargin, sigStart + rot + sigBw / 2.0f - safetyMargin, chanStart, chanEnd)) {
		//qDebug("-> take center half (decimate by 2)");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
		return createFilterChain(sigStart + rot, sigStart + sigBw / 2.0f + rot, chanStart, chanEnd);
	}
#endif
	Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);
	qDebug("-> complete (final BW %f, frequency offset %f)", sigBw, ofs);
	return ofs;
}

void Channelizer::freeFilterChain()
{
	for(FilterStages::iterator it = m_filterStages.begin(); it != m_filterStages.end(); ++it)
		delete *it;
	m_filterStages.clear();
}

///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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
