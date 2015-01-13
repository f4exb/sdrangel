/*
 * Filters from Fldigi. 
*/

#ifndef	_FFTFILT_H
#define	_FFTFILT_H

#include "complex.h"
#include "gfft.h"

//----------------------------------------------------------------------

class fftfilt {
enum {NONE, BLACKMAN, HAMMING, HANNING};

protected:
	int flen;
	int flen2;
	g_fft<float> *fft;
	g_fft<float> *ift;
	cmplx *filter;
	cmplx *data;
	cmplx *ovlbuf;
	cmplx *output;
	int inptr;
	int pass;
	int window;

	inline float fsinc(float fc, int i, int len) {
		return (i == len/2) ? 2.0 * fc: 
				sin(2 * M_PI * fc * (i - len/2)) / (M_PI * (i - len/2));
	}
	inline float _blackman(int i, int len) {
		return (0.42 - 
				 0.50 * cos(2.0 * M_PI * i / len) + 
				 0.08 * cos(4.0 * M_PI * i / len));
	}
	void init_filter();

public:
	fftfilt(float f1, float f2, int len);
	fftfilt(float f, int len);
	~fftfilt();
// f1 < f2 ==> bandpass
// f1 > f2 ==> band reject
	void create_filter(float f1, float f2);
	void rtty_filter(float);

	int runFilt(const cmplx& in, cmplx **out);
	int runSSB(const cmplx& in, cmplx **out, bool usb);
};



/* Sliding FFT filter from Fldigi */
class sfft {
#define K1 0.99999
private:
	int fftlen;
	int first;
	int last;
	int ptr;
	struct vrot_bins_pair;
	vrot_bins_pair *vrot_bins;
	cmplx *delay;
	float k2;
public:
	sfft(int len);
	~sfft();
	void run(const cmplx& input);
	void fetch(float *result);
};

#endif
