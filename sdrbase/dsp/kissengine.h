#ifndef INCLUDE_KISSENGINE_H
#define INCLUDE_KISSENGINE_H

#include "dsp/fftengine.h"
#include "dsp/kissfft.h"
#include "export.h"

class SDRBASE_API KissEngine : public FFTEngine {
public:
	virtual void configure(int n, bool inverse);
	virtual void transform();

	virtual Complex* in();
	virtual Complex* out();

    virtual void setReuse(bool reuse);
    QString getName() const override;
    static const QString m_name;

protected:
	typedef kissfft<Real, Complex> KissFFT;
	KissFFT m_fft;

	std::vector<Complex> m_in;
	std::vector<Complex> m_out;
};

#endif // INCLUDE_KISSENGINE_H
