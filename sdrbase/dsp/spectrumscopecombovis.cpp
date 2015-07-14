#include "dsp/spectrumscopecombovis.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

SpectrumScopeComboVis::SpectrumScopeComboVis(SpectrumVis* spectrumVis, ScopeVis* scopeVis) :
	m_spectrumVis(spectrumVis),
	m_scopeVis(scopeVis)
{
}

void SpectrumScopeComboVis::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
	m_scopeVis->feed(begin, end, false);
	SampleVector::const_iterator triggerPoint = m_scopeVis->getTriggerPoint();
	m_spectrumVis->feedTriggered(triggerPoint, begin, end, positiveOnly);
}

void SpectrumScopeComboVis::start()
{
	m_spectrumVis->start();
	m_scopeVis->start();
}

void SpectrumScopeComboVis::stop()
{
	m_spectrumVis->stop();
	m_scopeVis->stop();
}

bool SpectrumScopeComboVis::handleMessage(Message* message)
{
	bool spectDone = m_spectrumVis->handleMessageKeep(message);
	bool scopeDone = m_scopeVis->handleMessageKeep(message);

	if (spectDone || scopeDone)
	{
		message->completed();
	}

	return (spectDone || scopeDone);
}
