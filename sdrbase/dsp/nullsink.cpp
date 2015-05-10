#include "dsp/nullsink.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

NullSink::NullSink()
{
}

void NullSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
}

void NullSink::start()
{
}

void NullSink::stop()
{
}

bool NullSink::handleMessage(Message* message)
{
	message->completed();
	return true;
	/*
	if(DSPSignalNotification::match(message)) {
		DSPSignalNotification* signal = (DSPSignalNotification*)message;
		m_sampleRate = signal->getSampleRate();
		message->completed();
		return true;
	} else if(DSPConfigureScopeVis::match(message)) {
		DSPConfigureScopeVis* conf = (DSPConfigureScopeVis*)message;
		m_triggerState = Untriggered;
		m_triggerChannel = (TriggerChannel)conf->getTriggerChannel();
		m_triggerLevelHigh = conf->getTriggerLevelHigh() * 32767;
		m_triggerLevelLow = conf->getTriggerLevelLow() * 32767;
		message->completed();
		return true;
	} else {
		return false;
	}
	*/
}
