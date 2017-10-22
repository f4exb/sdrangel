#include "dsp/spectrumscopengcombovis.h"
#include "dsp/dspcommands.h"
#include "util/messagequeue.h"

SpectrumScopeNGComboVis::SpectrumScopeNGComboVis(SpectrumVis* spectrumVis, ScopeVisNG* scopeVis) :
	m_spectrumVis(spectrumVis),
	m_scopeVis(scopeVis)
{
	setObjectName("SpectrumScopeNGComboVis");
}

SpectrumScopeNGComboVis::~SpectrumScopeNGComboVis()
{
}

void SpectrumScopeNGComboVis::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	m_scopeVis->feed(begin, end, false);
	SampleVector::const_iterator triggerPoint = m_scopeVis->getTriggerPoint();
	m_spectrumVis->feedTriggered(triggerPoint, end, positiveOnly);
}

void SpectrumScopeNGComboVis::start()
{
	m_spectrumVis->start();
	m_scopeVis->start();
}

void SpectrumScopeNGComboVis::stop()
{
	m_spectrumVis->stop();
	m_scopeVis->stop();
}

bool SpectrumScopeNGComboVis::handleMessage(const Message& message)
{
	bool spectDone = m_spectrumVis->handleMessage(message);
	bool scopeDone = m_scopeVis->handleMessage(message);

	return (spectDone || scopeDone);
}
