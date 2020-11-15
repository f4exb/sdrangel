#ifndef INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H
#define INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H

#include <dsp/basebandsamplesink.h>
#include "dsp/spectrumvis.h"
#include "export.h"

class Message;
class ScopeVis;

class SDRGUI_API SpectrumScopeComboVis : public BasebandSampleSink {
public:

    SpectrumScopeComboVis(SpectrumVis* spectrumVis, ScopeVis* scopeVis);
	virtual ~SpectrumScopeComboVis();

	using BasebandSampleSink::feed;
	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;
};

#endif // INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H
