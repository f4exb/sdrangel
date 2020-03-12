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
	~FFTWEngine();

	void configure(int n, bool inverse);
	void transform();

	Complex* in();
	Complex* out();

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

	void freeAll();
};

#endif // INCLUDE_FFTWENGINE_H
