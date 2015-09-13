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
            m_nbAvg(128),
			m_N(0),
			m_sampleRate(0),
			m_samplesProcessed(0),
			m_maxPowerIndex(0),
			m_nTones(2),
			m_samplesAttack(0),
			m_attackCount(0),
			m_samplesDecay(0),
			m_decayCount(0),
			m_isOpen(false),
			m_threshold(0.0)
{
	m_k = new double[m_nTones];
	m_coef = new double[m_nTones];
	m_toneSet = new Real[m_nTones];
	m_u0 = new double[m_nTones];
	m_u1 = new double[m_nTones];
	m_power = new double[m_nTones];
	m_movingAverages.resize(m_nTones, MovingAverage<Real>(m_nbAvg, 0.0));

	m_toneSet[0]  = 2000.0;
	m_toneSet[1]  = 10000.0;
}

AFSquelch::AFSquelch(unsigned int nbTones, const Real *tones) :
			m_N(0),
            m_nbAvg(0),
			m_sampleRate(0),
			m_samplesProcessed(0),
			m_maxPowerIndex(0),
			m_nTones(nbTones),
			m_samplesAttack(0),
			m_attackCount(0),
			m_samplesDecay(0),
			m_decayCount(0),
			m_isOpen(false),
			m_threshold(0.0)
{
	m_k = new double[m_nTones];
	m_coef = new double[m_nTones];
	m_toneSet = new Real[m_nTones];
	m_u0 = new double[m_nTones];
	m_u1 = new double[m_nTones];
	m_power = new double[m_nTones];

	for (int j = 0; j < m_nTones; ++j)
	{
		m_toneSet[j] = tones[j];
	}
}


AFSquelch::~AFSquelch()
{
	delete[] m_k;
	delete[] m_coef;
	delete[] m_toneSet;
	delete[] m_u0;
	delete[] m_u1;
	delete[] m_power;
}


void AFSquelch::setCoefficients(int _N, unsigned int nbAvg, int _samplerate, int _samplesAttack, int _samplesDecay )
{
	m_N = _N;                   // save the basic parameters for use during analysis
	m_nbAvg = nbAvg;
	m_sampleRate = _samplerate;
	m_samplesAttack = _samplesAttack;
	m_samplesDecay = _samplesDecay;
	m_movingAverages.resize(m_nTones, MovingAverage<Real>(m_nbAvg, 0.0));

	// for each of the frequencies (tones) of interest calculate
	// k and the associated filter coefficient as per the Goertzel
	// algorithm. Note: we are using a real value (as apposed to
	// an integer as described in some references. k is retained
	// for later display. The tone set is specified in the
	// constructor. Notice that the resulting coefficients are
	// independent of N.
	for (int j = 0; j < m_nTones; ++j)
	{
		m_k[j] = ((double)m_N * m_toneSet[j]) / (double)m_sampleRate;
		m_coef[j] = 2.0 * cos((2.0 * M_PI * m_toneSet[j])/(double)m_sampleRate);
	}
}


// Analyze an input signal
bool AFSquelch::analyze(Real sample)
{

	feedback(sample); // Goertzel feedback
	m_samplesProcessed += 1;

	if (m_samplesProcessed == m_N) // completed a block of N
	{
		feedForward(); // calculate the power at each tone
		m_samplesProcessed = 0;
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
	for (int j = 0; j < m_nTones; ++j)
	{
		t = m_u0[j];
		m_u0[j] = in + (m_coef[j] * m_u0[j]) - m_u1[j];
		m_u1[j] = t;
	}
}


void AFSquelch::feedForward()
{
	for (int j = 0; j < m_nTones; ++j)
	{
		m_power[j] = (m_u0[j] * m_u0[j]) + (m_u1[j] * m_u1[j]) - (m_coef[j] * m_u0[j] * m_u1[j]);
		m_movingAverages[j].feed(m_power[j]);
		m_u0[j] = m_u1[j] = 0.0; // reset for next block.
	}

	evaluate();
}


void AFSquelch::reset()
{
	for (int j = 0; j < m_nTones; ++j)
	{
		m_power[j] = m_u0[j] = m_u1[j] = 0.0; // reset
		m_movingAverages[j].fill(0.0);
	}

	m_samplesProcessed = 0;
	m_maxPowerIndex = 0;
	m_isOpen = false;
}


bool AFSquelch::evaluate()
{
	double maxPower = 0.0;
	double minPower;
	int minIndex = 0, maxIndex = 0;

	for (int j = 0; j < m_nTones; ++j)
	{
		if (m_movingAverages[j].sum() > maxPower)
		{
			maxPower = m_movingAverages[j].sum();
			maxIndex = j;
		}
	}

	minPower = maxPower;

	for (int j = 0; j < m_nTones; ++j)
	{
		if (m_movingAverages[j].sum() < minPower) {
			minPower = m_movingAverages[j].sum();
			minIndex = j;
		}
	}

	// principle is to open if power is uneven because noise gives even power
	bool open = (minPower/maxPower < m_threshold) && (minIndex > maxIndex);

	if (open)
	{
		if (m_samplesAttack && (m_attackCount < m_samplesAttack))
		{
			m_isOpen = false;
			m_attackCount++;
		}
		else
		{
			m_isOpen = true;
			m_decayCount = 0;
		}
	}
	else
	{
		if (m_samplesDecay && (m_decayCount < m_samplesDecay))
		{
			m_isOpen = true;
			m_decayCount++;
		}
		else
		{
			m_isOpen = false;
			m_attackCount = 0;
		}
	}

	return m_isOpen;
}

void AFSquelch::setThreshold(double threshold)
{
	qDebug("AFSquelch::setThreshold: threshold: %f", threshold);
	m_threshold = threshold;
}
