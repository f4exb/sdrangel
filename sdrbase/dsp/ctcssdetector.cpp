/*
 * ctcssdetector.cpp
 *
 *  Created on: Jun 16, 2015
 *      Author: f4exb
 */
#include <math.h>
#include "dsp/ctcssdetector.h"
#include "ctcssfrequencies.h"

#undef M_PI
#define M_PI		3.14159265358979323846

CTCSSDetector::CTCSSDetector() :
			N(0),
			sampleRate(0),
			samplesProcessed(0),
			maxPowerIndex(0),
			toneDetected(false),
			maxPower(0.0)
{
	nTones = CTCSSFrequencies::m_nbFreqs;
	k = new Real[nTones];
	coef = new Real[nTones];
	toneSet = new Real[nTones];
	u0 = new Real[nTones];
	u1 = new Real[nTones];
	power = new Real[nTones];
	toneSet = CTCSSFrequencies::m_Freqs;
}

CTCSSDetector::CTCSSDetector(int _nTones, Real *tones) :
			N(0),
			sampleRate(0),
			samplesProcessed(0),
			maxPowerIndex(0),
			toneDetected(false),
			maxPower(0.0)
{
	nTones = CTCSSFrequencies::m_nbFreqs;
	k = new Real[nTones];
	coef = new Real[nTones];
	toneSet = new Real[nTones];
	u0 = new Real[nTones];
	u1 = new Real[nTones];
	power = new Real[nTones];
	toneSet = CTCSSFrequencies::m_Freqs;
}


CTCSSDetector::~CTCSSDetector()
{
	delete[] k;
	delete[] coef;
	delete[] toneSet;
	delete[] u0;
	delete[] u1;
	delete[] power;
}


void CTCSSDetector::setCoefficients(int zN, int _samplerate )
{
	N = zN;                   // save the basic parameters for use during analysis
	sampleRate = _samplerate;

	// for each of the frequencies (tones) of interest calculate
	// k and the associated filter coefficient as per the Goertzel
	// algorithm. Note: we are using a real value (as apposed to
	// an integer as described in some references. k is retained
	// for later display. The tone set is specified in the
	// constructor. Notice that the resulting coefficients are
	// independent of N.
	for (int j = 0; j < nTones; ++j)
	{
		k[j] = ((double)N * toneSet[j]) / (double)sampleRate;
		coef[j] = 2.0 * cos((2.0 * M_PI * toneSet[j])/(double)sampleRate);
	}
}


// Analyze an input signal for the presence of CTCSS tones.
bool CTCSSDetector::analyze(Real *sample)
{

	feedback(*sample); // Goertzel feedback
	samplesProcessed += 1;

	if (samplesProcessed == N) // completed a block of N
	{
		feedForward(); // calculate the power at each tone
		samplesProcessed = 0;
		return true; // have a result
	}
	else
	{
		return false;
	}
}


void CTCSSDetector::feedback(Real in)
{
	Real t;

	// feedback for each tone
	for (int j = 0; j < nTones; ++j)
	{
		t = u0[j];
		u0[j] = in + (coef[j] * u0[j]) - u1[j];
		u1[j] = t;
	}
}


void CTCSSDetector::feedForward()
{
	initializePower();

	for (int j = 0; j < nTones; ++j)
	{
		power[j] = (u0[j] * u0[j]) + (u1[j] * u1[j]) - (coef[j] * u0[j] * u1[j]);
		u0[j] = u1[j] = 0.0; // reset for next block.
	}

	evaluatePower();
}


void CTCSSDetector::reset()
{
	for (int j = 0; j < nTones; ++j)
	{
		power[j] = u0[j] = u1[j] = 0.0; // reset
	}

	samplesProcessed = 0;
	maxPower = 0.0;
	maxPowerIndex = 0;
	toneDetected = false;
}


void CTCSSDetector::initializePower()
{
	for (int j = 0; j < nTones; ++j)
	{
		power[j] = 0.0; // reset
	}
}


void CTCSSDetector::evaluatePower()
{
	Real sumPower = 0.0;
	Real aboveAvg = 2.0; // Arbitrary max power above average threshold
	maxPower = 0.0;

	for (int j = 0; j < nTones; ++j)
	{
		sumPower += power[j];

		if (power[j] > maxPower)
		{
			maxPower = power[j];
			maxPowerIndex = j;
		}
	}

	toneDetected = (maxPower > (sumPower/nTones) + aboveAvg);
}
