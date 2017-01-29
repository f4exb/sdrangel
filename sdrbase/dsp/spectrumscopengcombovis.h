#ifndef INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H
#define INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H

#include <dsp/basebandsamplesink.h>
#include "dsp/spectrumvis.h"
#include "dsp/scopevisng.h"
#include "util/export.h"

class Message;

class SDRANGEL_API SpectrumScopeNGComboVis : public BasebandSampleSink {
public:

    SpectrumScopeNGComboVis(SpectrumVis* spectrumVis, ScopeVisNG* scopeVis);
	virtual ~SpectrumScopeNGComboVis();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

private:
	SpectrumVis* m_spectrumVis;
	ScopeVisNG* m_scopeVis;
};

#endif // INCLUDE_SPECTRUMSCOPENGCOMBOVIS_H
