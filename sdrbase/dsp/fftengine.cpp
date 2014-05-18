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
	qDebug("FFT: using FFTW engine");
	return new FFTWEngine;
#endif // USE_FFTW
#ifdef USE_KISSFFT
	qDebug("FFT: using KissFFT engine");
	return new KissEngine;
#endif // USE_KISSFFT

	qCritical("FFT: no engine built");
	return NULL;
}
