#ifndef INCLUDE_SPECTRUMSCOPECOMBOVIS_H
#define INCLUDE_SPECTRUMSCOPECOMBOVIS_H

#include "dsp/samplesink.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "util/export.h"

class Message;

class SDRANGELOVE_API SpectrumScopeComboVis : public SampleSink {
public:

	SpectrumScopeComboVis(SpectrumVis* spectrumVis, ScopeVis* scopeVis);
	virtual ~SpectrumScopeComboVis();

	virtual bool init(const Message& cmd);
	virtual void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;
};

#endif // INCLUDE_SPECTRUMSCOPECOMBOVIS_H
