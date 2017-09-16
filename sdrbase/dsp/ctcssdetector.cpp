/*
 * ctcssdetector.cpp
 *
 *  Created on: Jun 16, 2015
 *      Author: f4exb
 */
#include <math.h>
#include "dsp/ctcssdetector.h"

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
	nTones = 32;
	k = new Real[nTones];
	coef = new Real[nTones];
	toneSet = new Real[nTones];
	u0 = new Real[nTones];
	u1 = new Real[nTones];
	power = new Real[nTones];

	// The 32 EIA standard tones
	toneSet[0]  = 67.0;
	toneSet[1]  = 71.9;
	toneSet[2]  = 74.4;
	toneSet[3]  = 77.0;
	toneSet[4]  = 79.7;
	toneSet[5]  = 82.5;
	toneSet[6]  = 85.4;
	toneSet[7]  = 88.5;
	toneSet[8]  = 91.5;
	toneSet[9]  = 94.8;
	toneSet[10]  = 97.4;
	toneSet[11] = 100.0;
	toneSet[12] = 103.5;
	toneSet[13] = 107.2;
	toneSet[14] = 110.9;
	toneSet[15] = 114.8;
	toneSet[16] = 118.8;
	toneSet[17] = 123.0;
	toneSet[18] = 127.3;
	toneSet[19] = 131.8;
	toneSet[20] = 136.5;
	toneSet[21] = 141.3;
	toneSet[22] = 146.2;
	toneSet[23] = 151.4;
	toneSet[24] = 156.7;
	toneSet[25] = 162.2;
	toneSet[26] = 167.9;
	toneSet[27] = 173.8;
	toneSet[28] = 179.9;
	toneSet[29] = 186.2;
	toneSet[30] = 192.8;
	toneSet[31] = 203.5;
}

CTCSSDetector::CTCSSDetector(int _nTones, Real *tones) :
			N(0),
			sampleRate(0),
			samplesProcessed(0),
			maxPowerIndex(0),
			toneDetected(false),
			maxPower(0.0)
{
	nTones = _nTones;
	k = new Real[nTones];
	coef = new Real[nTones];
	toneSet = new Real[nTones];
	u0 = new Real[nTones];
	u1 = new Real[nTones];
	power = new Real[nTones];

	for (int j = 0; j < nTones; ++j)
	{
		toneSet[j] = tones[j];
	}
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
