#include <cmath>
#include <vector>
#include "dsp/interpolator.h"


void Interpolator::createPolyphaseLowPass(
        std::vector<Real>& taps,
        int phaseSteps,
        double gain,
        double sampleRateHz,
        double cutoffFreqHz,
        double transitionWidthHz,
        double oobAttenuationdB)
{
    double nbTapsPerPhase = (oobAttenuationdB * sampleRateHz) / (22.0 * transitionWidthHz * phaseSteps);
    return createPolyphaseLowPass(taps, phaseSteps, gain, sampleRateHz, cutoffFreqHz, nbTapsPerPhase);
}


void Interpolator::createPolyphaseLowPass(
        std::vector<Real>& taps,
        int phaseSteps,
        double gain,
        double sampleRateHz,
        double cutoffFreqHz,
        double nbTapsPerPhase)
{
	int ntaps = (int)(nbTapsPerPhase * phaseSteps);
	qDebug("Interpolator::createPolyphaseLowPass: ntaps: %d", ntaps);

	if ((ntaps % 2) != 0) {
	    ntaps++;
	}

	ntaps *= phaseSteps;

	taps.resize(ntaps);
	std::vector<float> window(ntaps);

	for (int n = 0; n < ntaps; n++) {
	    window[n] = 0.54 - 0.46 * cos ((2 * M_PI * n) / (ntaps - 1));
	}

	int M = (ntaps - 1) / 2;
	double fwT0 = 2 * M_PI * cutoffFreqHz / sampleRateHz;

	for (int n = -M; n <= M; n++)
	{
		if (n == 0) {
		    taps[n + M] = fwT0 / M_PI * window[n + M];
		} else {
		    taps[n + M] =  sin (n * fwT0) / (n * M_PI) * window[n + M];
		}
	}

	double max = taps[0 + M];

	for (int n = 1; n <= M; n++) {
	    max += 2.0 * taps[n + M];
	}

	gain /= max;

	for (int i = 0; i < ntaps; i++) {
	    taps[i] *= gain;
	}
}

Interpolator::Interpolator() :
	m_taps(0),
	m_alignedTaps(0),
	m_taps2(0),
	m_alignedTaps2(0),
    m_ptr(0),
	m_phaseSteps(1),
    m_nTaps(1)
{
}

Interpolator::~Interpolator()
{
	free();
}

void Interpolator::create(int phaseSteps, double sampleRate, double cutoff, double nbTapsPerPhase)
{
	free();

	std::vector<Real> taps;

	createPolyphaseLowPass(
	    taps,
		phaseSteps, // number of polyphases
		1.0, // gain
		phaseSteps * sampleRate, // sampling frequency
		cutoff, // hz beginning of transition band
		nbTapsPerPhase);

	// init state
	m_ptr = 0;
	m_nTaps = taps.size() / phaseSteps;
	m_phaseSteps = phaseSteps;
	m_samples.resize(m_nTaps + 2);

	for (int i = 0; i < m_nTaps + 2; i++) {
	    m_samples[i] = 0;
	}

	// reorder into polyphase
	std::vector<Real> polyphase(taps.size());

	for (int phase = 0; phase < phaseSteps; phase++)
	{
		for (int i = 0; i < m_nTaps; i++) {
		    polyphase[phase * m_nTaps + i] = taps[i * phaseSteps + phase];
		}
	}

	// normalize phase filters
	for (int phase = 0; phase < phaseSteps; phase++)
	{
		Real sum = 0;

		for (int i = phase * m_nTaps; i < phase * m_nTaps + m_nTaps; i++) {
		    sum += polyphase[i];
		}

		for (int i = phase * m_nTaps; i < phase * m_nTaps + m_nTaps; i++) {
		    polyphase[i] /= sum;
		}
	}

	// move taps around to match sse storage requirements
	m_taps = new float[2 * taps.size() + 8];

	for (uint i = 0; i < 2 * taps.size() + 8; ++i) {
	    m_taps[i] = 0;
	}

	m_alignedTaps = (float*)((((quint64)m_taps) + 15) & ~15);

	for (uint i = 0; i < taps.size(); ++i)
	{
		m_alignedTaps[2 * i + 0] = polyphase[i];
		m_alignedTaps[2 * i + 1] = polyphase[i];
	}

	m_taps2 = new float[2 * taps.size() + 8];

	for (uint i = 0; i < 2 * taps.size() + 8; ++i) {
	    m_taps2[i] = 0;
	}

	m_alignedTaps2 = (float*)((((quint64)m_taps2) + 15) & ~15);

	for (uint i = 1; i < taps.size(); ++i)
	{
		m_alignedTaps2[2 * (i - 1) + 0] = polyphase[i];
		m_alignedTaps2[2 * (i - 1) + 1] = polyphase[i];
	}
}

void Interpolator::free()
{
	if (m_taps != NULL)
	{
		delete[] m_taps;
		m_taps = NULL;
		m_alignedTaps = NULL;
		delete[] m_taps2;
		m_taps2 = NULL;
		m_alignedTaps2 = NULL;
	}
}
