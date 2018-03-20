#ifndef INCLUDE_FFTWENGINE_H
#define INCLUDE_FFTWENGINE_H

#include <QMutex>
#include <fftw3.h>
#include <list>
#include "dsp/fftengine.h"
#include "export.h"

class SDRBASE_API FFTWEngine : public FFTEngine {
public:
	FFTWEngine();
	~FFTWEngine();

	void configure(int n, bool inverse);
	void transform();

	Complex* in();
	Complex* out();

protected:
	static QMutex m_globalPlanMutex;

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

	void freeAll();
};

#endif // INCLUDE_FFTWENGINE_H
