#ifndef INCLUDE_KISSENGINE_H
#define INCLUDE_KISSENGINE_H

#include "dsp/fftengine.h"
#include "dsp/kissfft.h"

class KissEngine : public FFTEngine {
public:
	void configure(int n, bool inverse);
	void transform();

	Complex* in();
	Complex* out();

protected:
	typedef kissfft<Real, Complex> KissFFT;
	KissFFT m_fft;

	std::vector<Complex> m_in;
	std::vector<Complex> m_out;
};

#endif // INCLUDE_KISSENGINE_H
