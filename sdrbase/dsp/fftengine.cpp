#include "dsp/fftengine.h"
#ifdef USE_KISSFFT
#include "dsp/kissengine.h"
#endif
#ifdef USE_FFTW
#include "dsp/fftwengine.h"
#endif // USE_FFTW

FFTEngine::~FFTEngine()
{
}

FFTEngine* FFTEngine::create()
{
#ifdef USE_FFTW
	qDebug("FFTEngine::create: using FFTW engine");
	return new FFTWEngine;
#endif // USE_FFTW
#ifdef USE_KISSFFT
	qDebug("FFTEngine::create: using KissFFT engine");
	return new KissEngine;
#endif // USE_KISSFFT

	qCritical("FFTEngine::create: no engine built");
	return NULL;
}
