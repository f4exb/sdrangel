///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "dsp/afsquelch.h"

AFSquelch::AFSquelch() :
			N(0),
			sampleRate(0),
			samplesProcessed(0),
			maxPowerIndex(0),
			nTones(2),
			samplesAttack(0),
			attackCount(0),
			samplesDecay(0),
			decayCount(0),
			isOpen(false),
			threshold(0.0)
{
	k = new double[nTones];
	coef = new double[nTones];
	toneSet = new Real[nTones];
	u0 = new double[nTones];
	u1 = new double[nTones];
	power = new double[nTones];

	toneSet[0]  = 2000.0;
	toneSet[1]  = 10000.0;
}

AFSquelch::AFSquelch(unsigned int nbTones, const Real *tones) :
			N(0),
			sampleRate(0),
			samplesProcessed(0),
			maxPowerIndex(0),
			nTones(nbTones),
			samplesAttack(0),
			attackCount(0),
			samplesDecay(0),
			decayCount(0),
			isOpen(false),
			threshold(0.0)
{
	k = new double[nTones];
	coef = new double[nTones];
	toneSet = new Real[nTones];
	u0 = new double[nTones];
	u1 = new double[nTones];
	power = new double[nTones];

	for (int j = 0; j < nTones; ++j)
	{
		toneSet[j] = tones[j];
	}
}


AFSquelch::~AFSquelch()
{
	delete[] k;
	delete[] coef;
	delete[] toneSet;
	delete[] u0;
	delete[] u1;
	delete[] power;
}


void AFSquelch::setCoefficients(int _N, int _samplerate, int _samplesAttack, int _samplesDecay )
{
	N = _N;                   // save the basic parameters for use during analysis
	sampleRate = _samplerate;
	samplesAttack = _samplesAttack;
	samplesDecay = _samplesDecay;

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


// Analyze an input signal
bool AFSquelch::analyze(Real *sample)
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


void AFSquelch::feedback(Real in)
{
	double t;

	// feedback for each tone
	for (int j = 0; j < nTones; ++j)
	{
		t = u0[j];
		u0[j] = in + (coef[j] * u0[j]) - u1[j];
		u1[j] = t;
	}
}


void AFSquelch::feedForward()
{
	for (int j = 0; j < nTones; ++j)
	{
		power[j] = (u0[j] * u0[j]) + (u1[j] * u1[j]) - (coef[j] * u0[j] * u1[j]);
		u0[j] = u1[j] = 0.0; // reset for next block.
	}

	evaluate();
}


void AFSquelch::reset()
{
	for (int j = 0; j < nTones; ++j)
	{
		power[j] = u0[j] = u1[j] = 0.0; // reset
	}

	samplesProcessed = 0;
	maxPowerIndex = 0;
	isOpen = false;
}


void AFSquelch::evaluate()
{
	double maxPower = 0.0;
	double minPower;
	int minIndex = 0, maxIndex = 0;

	for (int j = 0; j < nTones; ++j)
	{
		if (power[j] > maxPower) {
			maxPower = power[j];
			maxIndex = j;
		}
	}

	minPower = maxPower;

	for (int j = 0; j < nTones; ++j)
	{
		if (power[j] < minPower) {
			minPower = power[j];
			minIndex = j;
		}
	}

	// principle is to open if power is uneven because noise gives even power
	bool open = ((maxPower - minPower) > threshold) && (minIndex > maxIndex);

	if (open)
	{
		if (attackCount < samplesAttack)
		{
			attackCount++;
		}
		else
		{
			isOpen = true;
			decayCount = 0;
		}
	}
	else
	{
		if (decayCount < samplesDecay)
		{
			decayCount++;
		}
		else
		{
			isOpen = false;
			attackCount = 0;
		}
	}
}
