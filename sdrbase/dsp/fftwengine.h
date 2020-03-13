#ifndef INCLUDE_FFTWENGINE_H
#define INCLUDE_FFTWENGINE_H

#include <QMutex>
#include <QString>

#include <fftw3.h>
#include <list>
#include "dsp/fftengine.h"
#include "export.h"

class SDRBASE_API FFTWEngine : public FFTEngine {
public:
	FFTWEngine(const QString& fftWisdomFileName);
	virtual ~FFTWEngine();

	virtual void configure(int n, bool inverse);
	virtual void transform();

	virtual Complex* in();
	virtual Complex* out();

    virtual void setReuse(bool reuse) { m_reuse = reuse; }

protected:
	static QMutex m_globalPlanMutex;
    QString m_fftWisdomFileName;

	struct Plan {
		int n;
		bool inverse;
		fftwf_plan plan;
		fftwf_complex* in;
		fftwf_complex* out;
	};
	typedef std::list<Plan*> Plans;
	Plans m_plans;
	Plan* m_currentPlan;
    bool m_reuse;

	void freeAll();
};

#endif // INCLUDE_FFTWENGINE_H
