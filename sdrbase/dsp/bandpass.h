#ifndef INCLUDE_BANDPASS_H
#define INCLUDE_BANDPASS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include "dsp/dsptypes.h"

#undef M_PI
#define M_PI 3.14159265358979323846

template <class Type> class Bandpass {
public:
	Bandpass() : m_ptr(0) { }

	void create(int nTaps, double sampleRate, double lowCutoff, double highCutoff)
	{
		std::vector<Real> taps_lp;
		std::vector<Real> taps_hp;
		double wcl = 2.0 * M_PI * lowCutoff;
		double Wcl = wcl / sampleRate;
		double wch = 2.0 * M_PI * highCutoff;
		double Wch = wch / sampleRate;
		int i;

		// check constraints
		if(!(nTaps & 1)) {
			qDebug("Bandpass filter has to have an odd number of taps");
			nTaps++;
		}

		// make room
		m_samples.resize(nTaps);
		for(int i = 0; i < nTaps; i++)
			m_samples[i] = 0;
		m_ptr = 0;
		m_taps.resize(nTaps / 2 + 1);
		taps_lp.resize(nTaps / 2 + 1);
		taps_hp.resize(nTaps / 2 + 1);

		// generate Sinc filter core
		for(i = 0; i < nTaps / 2 + 1; i++) {
			if(i == (nTaps - 1) / 2) {
				taps_lp[i] = Wch / M_PI;
				taps_hp[i] = -(Wcl / M_PI);
			}
			else {
				taps_lp[i] = sin(((double)i - ((double)nTaps - 1.0) / 2.0) * Wch) / (((double)i - ((double)nTaps - 1.0) / 2.0) * M_PI);
				taps_hp[i] = -sin(((double)i - ((double)nTaps - 1.0) / 2.0) * Wcl) / (((double)i - ((double)nTaps - 1.0) / 2.0) * M_PI);
			}
		}

		taps_hp[(nTaps - 1) / 2] += 1;

		// apply Hamming window and combine lowpass and highpass
		for(i = 0; i < nTaps / 2 + 1; i++) {
			taps_lp[i] *= 0.54 + 0.46 * cos((2.0 * M_PI * ((double)i - ((double)nTaps - 1.0) / 2.0)) / (double)nTaps);
			taps_hp[i] *= 0.54 + 0.46 * cos((2.0 * M_PI * ((double)i - ((double)nTaps - 1.0) / 2.0)) / (double)nTaps);
			m_taps[i] = -(taps_lp[i]+taps_hp[i]);
		}

		m_taps[(nTaps - 1) / 2] += 1;

		// normalize
		Real sum = 0;

		for(i = 0; i < (int)m_taps.size() - 1; i++) {
			sum += m_taps[i] * 2;
		}

		sum += m_taps[i];

		for(i = 0; i < (int)m_taps.size(); i++) {
			m_taps[i] /= sum;
		}
	}

	Type filter(Type sample)
	{
		Type acc = 0;
		int a = m_ptr;
		int b = a - 1;
		int i, n_taps, size;

		m_samples[m_ptr] = sample;
		size = m_samples.size(); // Valgrind optim (2)

		while(b < 0)
		{
			b += size;
		}

		n_taps = m_taps.size() - 1; // Valgrind optim

		for(i = 0; i < n_taps; i++)
		{
			acc += (m_samples[a] + m_samples[b]) * m_taps[i];
			a++;

			while (a >= size)
			{
				a -= size;
			}

			b--;

			while(b < 0)
			{
				b += size;
			}
		}

		acc += m_samples[a] * m_taps[i];

		m_ptr++;

		while (m_ptr >= size)
		{
			m_ptr -= size;
		}

		return acc;
	}

private:
	std::vector<Real> m_taps;
	std::vector<Type> m_samples;
	int m_ptr;
};

#endif // INCLUDE_BANDPASS_H
