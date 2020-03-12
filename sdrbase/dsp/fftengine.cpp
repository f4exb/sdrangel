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

FFTEngine* FFTEngine::create(const QString& fftWisdomFileName)
{
#ifdef USE_FFTW
	qDebug("FFTEngine::create: using FFTW engine");
	return new FFTWEngine(fftWisdomFileName);
#elif USE_KISSFFT
	qDebug("FFTEngine::create: using KissFFT engine");
    (void) fftWisdomFileName;
	return new KissEngine;
#else // USE_KISSFFT
	qCritical("FFTEngine::create: no engine built");
	return 0;
#endif
}
