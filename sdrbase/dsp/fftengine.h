#ifndef INCLUDE_FFTENGINE_H
#define INCLUDE_FFTENGINE_H

#include <QString>

#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API FFTEngine {
public:
	virtual ~FFTEngine();

	virtual void configure(int n, bool inverse) = 0;
	virtual void transform() = 0;

	virtual Complex* in() = 0;
	virtual Complex* out() = 0;

    virtual void setReuse(bool reuse) = 0;

	static FFTEngine* create(const QString& fftWisdomFileName);
};

#endif // INCLUDE_FFTENGINE_H
