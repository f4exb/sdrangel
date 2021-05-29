#include "dsp/spectrumscopecombovis.h"
#include "dsp/dspcommands.h"
#include "dsp/scopevis.h"
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

void SpectrumScopeComboVis::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
	std::vector<SampleVector::const_iterator> vbegin;
	vbegin.push_back(begin);
	m_scopeVis->feed(vbegin, end - begin);
	//SampleVector::const_iterator triggerPoint = m_scopeVis->getTriggerPoint();
	//m_spectrumVis->feedTriggered(triggerPoint, end, positiveOnly);
    int triggerPointLocation = m_scopeVis->getTriggerLocation();

    if (m_scopeVis->getFreeRun()) {
        m_spectrumVis->feed(begin, end, positiveOnly);
    } else if ((triggerPointLocation >= 0) && (triggerPointLocation <= end - begin)) {
        m_spectrumVis->feedTriggered(end - triggerPointLocation, end, positiveOnly);
    } else {
        m_spectrumVis->feedTriggered(begin, end, positiveOnly);
    }
}

void SpectrumScopeComboVis::start()
{
	m_spectrumVis->start();
}

void SpectrumScopeComboVis::stop()
{
	m_spectrumVis->stop();
}

bool SpectrumScopeComboVis::handleMessage(const Message& message)
{
	bool spectDone = m_spectrumVis->handleMessage(message);
	bool scopeDone = m_scopeVis->handleMessage(message);

	return (spectDone || scopeDone);
}
