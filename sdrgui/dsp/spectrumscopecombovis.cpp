#include "dsp/spectrumscopecombovis.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

SpectrumScopeComboVis::SpectrumScopeComboVis(SpectrumVis* spectrumVis, ScopeVis* scopeVis) :
	m_spectrumVis(spectrumVis),
	m_scopeVis(scopeVis)
{
	setObjectName("SpectrumScopeComboVis");
}

SpectrumScopeComboVis::~SpectrumScopeComboVis()
{
}

void SpectrumScopeComboVis::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	m_scopeVis->feed(begin, end, false);
	SampleVector::const_iterator triggerPoint = m_scopeVis->getTriggerPoint();
	m_spectrumVis->feedTriggered(triggerPoint, end, positiveOnly);
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

bool SpectrumScopeComboVis::handleMessage(const Message& message)
{
	bool spectDone = m_spectrumVis->handleMessage(message);
	bool scopeDone = m_scopeVis->handleMessage(message);

	return (spectDone || scopeDone);
}
