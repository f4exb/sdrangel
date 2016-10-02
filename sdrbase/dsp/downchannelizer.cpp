#include <dsp/downchannelizer.h>
#include "dsp/inthalfbandfilter.h"
#include "dsp/dspcommands.h"

#include <QString>
#include <QDebug>

MESSAGE_CLASS_DEFINITION(DownChannelizer::MsgChannelizerNotification, Message)

DownChannelizer::DownChannelizer(BasebandSampleSink* sampleSink) :
	m_sampleSink(sampleSink),
	m_inputSampleRate(0),
	m_requestedOutputSampleRate(0),
	m_requestedCenterFrequency(0),
	m_currentOutputSampleRate(0),
	m_currentCenterFrequency(0)
{
	QString name = "Channelizer(" + m_sampleSink->objectName() + ")";
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

void DownChannelizer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
	if(m_sampleSink == 0) {
		m_sampleBuffer.clear();
		return;
	}

	m_mutex.lock();

	for(SampleVector::const_iterator sample = begin; sample != end; ++sample)
	{
		Sample s(*sample);
		FilterStages::iterator stage = m_filterStages.begin();

		for (; stage != m_filterStages.end(); ++stage)
		{
			if(!(*stage)->work(&s))
			{
				break;
			}
		}

		if(stage == m_filterStages.end())
		{
			m_sampleBuffer.push_back(s);
		}
	}

	m_mutex.unlock();

	m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), positiveOnly);
	m_sampleBuffer.clear();
}

void DownChannelizer::start()
{
	if (m_sampleSink != 0)
	{
		qDebug() << "Channelizer::start: thread: " << thread()
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
	qDebug() << "Channelizer::handleMessage: " << cmd.getIdentifier();

	// TODO: apply changes only if input sample rate or requested output sample rate change. Change of center frequency has no impact.

	if (DSPSignalNotification::match(cmd))
	{
		DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
		m_inputSampleRate = notif.getSampleRate();
		qDebug() << "Channelizer::handleMessage: DSPSignalNotification: m_inputSampleRate: " << m_inputSampleRate;
		applyConfiguration();

		if (m_sampleSink != 0)
		{
			m_sampleSink->handleMessage(notif);
		}

		emit inputSampleRateChanged();
		return true;
	}
	else if (DSPConfigureChannelizer::match(cmd))
	{
		DSPConfigureChannelizer& chan = (DSPConfigureChannelizer&) cmd;
		m_requestedOutputSampleRate = chan.getSampleRate();
		m_requestedCenterFrequency = chan.getCenterFrequency();

		qDebug() << "Channelizer::handleMessage: DSPConfigureChannelizer:"
				<< " m_requestedOutputSampleRate: " << m_requestedOutputSampleRate
				<< " m_requestedCenterFrequency: " << m_requestedCenterFrequency;

		applyConfiguration();

		return true;
	}
	else
	{
		if (m_sampleSink != 0)
		{
			return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void DownChannelizer::applyConfiguration()
{
	if (m_inputSampleRate == 0)
	{
		qDebug() << "Channelizer::applyConfiguration: m_inputSampleRate=0 aborting";
		return;
	}

	m_mutex.lock();

	freeFilterChain();

	m_currentCenterFrequency = createFilterChain(
		m_inputSampleRate / -2, m_inputSampleRate / 2,
		m_requestedCenterFrequency - m_requestedOutputSampleRate / 2, m_requestedCenterFrequency + m_requestedOutputSampleRate / 2);

	m_mutex.unlock();

	m_currentOutputSampleRate = m_inputSampleRate / (1 << m_filterStages.size());

	qDebug() << "Channelizer::applyConfiguration in=" << m_inputSampleRate
			<< ", req=" << m_requestedOutputSampleRate
			<< ", out=" << m_currentOutputSampleRate
			<< ", fc=" << m_currentCenterFrequency;

	if (m_sampleSink != 0)
	{
		MsgChannelizerNotification notif(m_currentOutputSampleRate, m_currentCenterFrequency);
		m_sampleSink->handleMessage(notif);
	}
}

DownChannelizer::FilterStage::FilterStage(Mode mode) :
	m_filter(new IntHalfbandFilter),
	m_workFunction(0)
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
	Real safetyMargin = sigBw / 20;
	Real rot = sigBw / 4;

	safetyMargin = 0;

	//fprintf(stderr, "Channelizer::createFilterChain: ");
	//fprintf(stderr, "Signal [%.1f, %.1f] (BW %.1f), Channel [%.1f, %.1f], Rot %.1f, Safety %.1f\n", sigStart, sigEnd, sigBw, chanStart, chanEnd, rot, safetyMargin);
#if 1
	// check if it fits into the left half
	if(signalContainsChannel(sigStart + safetyMargin, sigStart + sigBw / 2.0 - safetyMargin, chanStart, chanEnd)) {
		//fprintf(stderr, "-> take left half (rotate by +1/4 and decimate by 2)\n");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeLowerHalf));
		return createFilterChain(sigStart, sigStart + sigBw / 2.0, chanStart, chanEnd);
	}

	// check if it fits into the right half
	if(signalContainsChannel(sigEnd - sigBw / 2.0f + safetyMargin, sigEnd - safetyMargin, chanStart, chanEnd)) {
		//fprintf(stderr, "-> take right half (rotate by -1/4 and decimate by 2)\n");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeUpperHalf));
		return createFilterChain(sigEnd - sigBw / 2.0f, sigEnd, chanStart, chanEnd);
	}

	// check if it fits into the center
	// Was: if(signalContainsChannel(sigStart + rot + safetyMargin, sigStart + rot + sigBw / 2.0f - safetyMargin, chanStart, chanEnd)) {
	if(signalContainsChannel(sigStart + rot + safetyMargin, sigEnd - rot - safetyMargin, chanStart, chanEnd)) {
		//fprintf(stderr, "-> take center half (decimate by 2)\n");
		m_filterStages.push_back(new FilterStage(FilterStage::ModeCenter));
		// Was: return createFilterChain(sigStart + rot, sigStart + sigBw / 2.0f + rot, chanStart, chanEnd);
		return createFilterChain(sigStart + rot, sigEnd - rot, chanStart, chanEnd);
	}
#endif
	Real ofs = ((chanEnd - chanStart) / 2.0 + chanStart) - ((sigEnd - sigStart) / 2.0 + sigStart);
	//fprintf(stderr, "-> complete (final BW %.1f, frequency offset %.1f)\n", sigBw, ofs);
	return ofs;
}

void DownChannelizer::freeFilterChain()
{
	for(FilterStages::iterator it = m_filterStages.begin(); it != m_filterStages.end(); ++it)
		delete *it;
	m_filterStages.clear();
}
