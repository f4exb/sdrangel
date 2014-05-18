#ifndef INCLUDE_INTERPOLATOR_H
#define INCLUDE_INTERPOLATOR_H

#include <immintrin.h>
#include "dsp/dsptypes.h"
#include "util/export.h"
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

class SDRANGELOVE_API Interpolator {
public:
	Interpolator();
	~Interpolator();

	void create(int phaseSteps, double sampleRate, double cutoff);
	void free();

	bool interpolate(Real* distance, const Complex& next, bool* consumed, Complex* result)
	{
		while(*distance >= 1.0) {
			if(!(*consumed)) {
				advanceFilter(next);
				*distance -= 1.0;
				*consumed = true;
			} else {
				return false;
			}
		}
		doInterpolate((int)floor(*distance * (Real)m_phaseSteps), result);
		return true;
	}


private:
	float* m_taps;
	float* m_alignedTaps;
	float* m_taps2;
	float* m_alignedTaps2;
	std::vector<Complex> m_samples;
	int m_ptr;
	int m_phaseSteps;
	int m_nTaps;

	void createTaps(int nTaps, double sampleRate, double cutoff, std::vector<Real>* taps);

	void advanceFilter(const Complex& next)
	{
		m_ptr--;
		if(m_ptr < 0)
			m_ptr = m_nTaps - 1;
		m_samples[m_ptr] = next;
	}

	void doInterpolate(int phase, Complex* result)
	{
#if 1
		// beware of the ringbuffer
		if(m_ptr == 0) {
			// only one straight block
			const float* src = (const float*)&m_samples[0];
			const __m128* filter = (const __m128*)&m_alignedTaps[phase * m_nTaps * 2];
			__m128 sum = _mm_setzero_ps();
			int todo = m_nTaps / 2;

			for(int i = 0; i < todo; i++) {
				sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(src), *filter));
				src += 4;
				filter += 1;
			}

			// add upper half to lower half and store
			_mm_storel_pi((__m64*)result, _mm_add_ps(sum, _mm_shuffle_ps(sum, _mm_setzero_ps(), _MM_SHUFFLE(1, 0, 3, 2))));
		} else {
			// two blocks
			const float* src = (const float*)&m_samples[m_ptr];
			const __m128* filter = (const __m128*)&m_alignedTaps[phase * m_nTaps * 2];
			__m128 sum = _mm_setzero_ps();

			// first block
			int block = m_nTaps - m_ptr;
			int todo = block / 2;
			if(block & 1)
				todo++;
			for(int i = 0; i < todo; i++) {
				sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(src), *filter));
				src += 4;
				filter += 1;
			}
			if(block & 1) {
				// one sample beyond the end -> switch coefficient table
				filter = (const __m128*)&m_alignedTaps2[phase * m_nTaps * 2 + todo * 4 - 4];
			}
			// second block
			src = (const float*)&m_samples[0];
			block = m_ptr;
			todo = block / 2;
			for(int i = 0; i < todo; i++) {
				sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(src), *filter));
				src += 4;
				filter += 1;
			}
			if(block & 1) {
				// one sample remaining
				sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadl_pi(_mm_setzero_ps(), (const __m64*)src), filter[0]));
			}

			// add upper half to lower half and store
			_mm_storel_pi((__m64*)result, _mm_add_ps(sum, _mm_shuffle_ps(sum, _mm_setzero_ps(), _MM_SHUFFLE(1, 0, 3, 2))));
		}
#else
		int sample = m_ptr;
		const Real* coeff = &m_alignedTaps[phase * m_nTaps * 2];
		Real rAcc = 0;
		Real iAcc = 0;

		for(int i = 0; i < m_nTaps; i++) {
			rAcc += *coeff * m_samples[sample].real();
			iAcc += *coeff * m_samples[sample].imag();
			sample = (sample + 1) % m_nTaps;
			coeff += 2;
		}
		*result = Complex(rAcc, iAcc);
#endif

	}
};

#endif // INCLUDE_INTERPOLATOR_H
