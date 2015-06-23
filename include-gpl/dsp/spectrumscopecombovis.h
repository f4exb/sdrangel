#ifndef INCLUDE_SPECTRUMSCOPECOMBOVIS_H
#define INCLUDE_SPECTRUMSCOPECOMBOVIS_H

#include "dsp/samplesink.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "util/export.h"

class MessageQueue;

class SDRANGELOVE_API SpectrumScopeComboVis : public SampleSink {
public:

	SpectrumScopeComboVis(SpectrumVis* spectrumVis, ScopeVis* scopeVis);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
	bool handleMessage(Message* message);

private:
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;
};

#endif // INCLUDE_SPECTRUMSCOPECOMBOVIS_H
