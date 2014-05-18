#ifndef INCLUDE_LOWPASS_H
#define INCLUDE_LOWPASS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include "dsp/dsptypes.h"

template <class Type> class Lowpass {
public:
	Lowpass() { }

	void create(int nTaps, double sampleRate, double cutoff)
	{
		double wc = 2.0 * M_PI * cutoff;
		double Wc = wc / sampleRate;
		int i;

		// check constraints
		if(!(nTaps & 1)) {
			qDebug("Lowpass filter has to have an odd number of taps");
			nTaps++;
		}

		// make room
		m_samples.resize(nTaps);
		for(int i = 0; i < nTaps; i++)
			m_samples[i] = 0;
		m_ptr = 0;
		m_taps.resize(nTaps / 2 + 1);

		// generate Sinc filter core
		for(i = 0; i < nTaps / 2 + 1; i++) {
			if(i == (nTaps - 1) / 2)
				m_taps[i] = Wc / M_PI;
			else
				m_taps[i] = sin(((double)i - ((double)nTaps - 1.0) / 2.0) * Wc) / (((double)i - ((double)nTaps - 1.0) / 2.0) * M_PI);
		}

		// apply Hamming window
		for(i = 0; i < nTaps / 2 + 1; i++)
			m_taps[i] *= 0.54 + 0.46 * cos((2.0 * M_PI * ((double)i - ((double)nTaps - 1.0) / 2.0)) / (double)nTaps);

		// normalize
		Real sum = 0;
		for(i = 0; i < (int)m_taps.size() - 1; i++)
			sum += m_taps[i] * 2;
		sum += m_taps[i];
		for(i = 0; i < (int)m_taps.size(); i++)
			m_taps[i] /= sum;
	}

	Type filter(Type sample)
	{
		Type acc = 0;
		int a = m_ptr;
		int b = a - 1;
		int i;

		m_samples[m_ptr] = sample;

		while(b < 0)
			b += m_samples.size();

		for(i = 0; i < (int)m_taps.size() - 1; i++) {
			acc +=  (m_samples[a] + m_samples[b]) * m_taps[i];
			a++;
			while(a >= (int)m_samples.size())
				a -= m_samples.size();
			b--;
			while(b < 0)
				b += m_samples.size();
		}
		acc += m_samples[a] * m_taps[i];

		m_ptr++;
		while(m_ptr >= (int)m_samples.size())
			m_ptr -= m_samples.size();

		return acc;
	}

private:
	std::vector<Real> m_taps;
	std::vector<Type> m_samples;
	int m_ptr;
};

#endif // INCLUDE_LOWPASS_H
