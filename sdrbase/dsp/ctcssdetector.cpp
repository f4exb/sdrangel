//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                                                         //
//                                                                                                          //
// See: http://www.embedded.com/design/connectivity/4025660/Detecting-CTCSS-tones-with-Goertzel-s-algorithm //
//                                                                                                          //
// This program is free software; you can redistribute it and/or modify                                     //
// it under the terms of the GNU General Public License as published by                                     //
// the Free Software Foundation as version 3 of the License, or                                             //
// (at your option) any later version.                                                                      //
//                                                                                                          //
// This program is distributed in the hope that it will be useful,                                          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                                           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                             //
// GNU General Public License V3 for more details.                                                          //
//                                                                                                          //
// You should have received a copy of the GNU General Public License                                        //
// along with this program. If not, see <http://www.gnu.org/licenses/>.                                     //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <cmath>

#include "dsp/ctcssdetector.h"

CTCSSDetector::CTCSSDetector() :
			m_N(0),
			m_sampleRate(0),
			m_samplesProcessed(0),
			m_maxPowerIndex(0),
			m_toneDetected(false),
			m_maxPower(0.0)
{
	m_k = new Real[CTCSSFrequencies::m_nbFreqs];
	m_coef = new Real[CTCSSFrequencies::m_nbFreqs];
	m_u0 = new Real[CTCSSFrequencies::m_nbFreqs];
	m_u1 = new Real[CTCSSFrequencies::m_nbFreqs];
	m_power = new Real[CTCSSFrequencies::m_nbFreqs];
}

CTCSSDetector::~CTCSSDetector()
{
	delete[] m_k;
	delete[] m_coef;
	delete[] m_u0;
	delete[] m_u1;
	delete[] m_power;
}


void CTCSSDetector::setCoefficients(int N, int sampleRate)
{
	m_N = N;                   // save the basic parameters for use during analysis
	m_sampleRate = sampleRate;

	// for each of the frequencies (tones) of interest calculate
	// k and the associated filter coefficient as per the Goertzel
	// algorithm. Note: we are using a real value (as apposed to
	// an integer as described in some references. k is retained
	// for later display. The tone set is specified in the
	// constructor. Notice that the resulting coefficients are
	// independent of N.
	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		m_k[j] = ((double) m_N * CTCSSFrequencies::m_Freqs[j]) / (double)m_sampleRate;
		m_coef[j] = 2.0 * cos((2.0 * M_PI * CTCSSFrequencies::m_Freqs[j])/(double)m_sampleRate);
	}
}


// Analyze an input signal for the presence of CTCSS tones.
bool CTCSSDetector::analyze(Real *sample)
{

	feedback(*sample); // Goertzel feedback
	m_samplesProcessed += 1;

	if (m_samplesProcessed == m_N) // completed a block of N
	{
		feedForward(); // calculate the m_power at each tone
		m_samplesProcessed = 0;
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
	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		t = m_u0[j];
		m_u0[j] = in + (m_coef[j] * m_u0[j]) - m_u1[j];
		m_u1[j] = t;
	}
}


void CTCSSDetector::feedForward()
{
	initializePower();

	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		m_power[j] = (m_u0[j] * m_u0[j]) + (m_u1[j] * m_u1[j]) - (m_coef[j] * m_u0[j] * m_u1[j]);
		m_u0[j] = m_u1[j] = 0.0; // reset for next block.
	}

	evaluatePower();
}


void CTCSSDetector::reset()
{
	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		m_power[j] = m_u0[j] = m_u1[j] = 0.0; // reset
	}

	m_samplesProcessed = 0;
	m_maxPower = 0.0;
	m_maxPowerIndex = 0;
	m_toneDetected = false;
}


void CTCSSDetector::initializePower()
{
	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		m_power[j] = 0.0; // reset
	}
}


void CTCSSDetector::evaluatePower()
{
	Real sumPower = 0.0;
	Real aboveAvg = 2.0; // Arbitrary max m_power above average threshold
	m_maxPower = 0.0;

	for (int j = 0; j < CTCSSFrequencies::m_nbFreqs; ++j)
	{
		sumPower += m_power[j];

		if (m_power[j] > m_maxPower)
		{
			m_maxPower = m_power[j];
			m_maxPowerIndex = j;
		}
	}

	m_toneDetected = (m_maxPower > (sumPower/CTCSSFrequencies::m_nbFreqs) + aboveAvg);
}
