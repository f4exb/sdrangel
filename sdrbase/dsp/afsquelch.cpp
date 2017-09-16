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

#include <math.h>
#include "dsp/afsquelch.h"

#undef M_PI
#define M_PI		3.14159265358979323846

AFSquelch::AFSquelch() :
            m_nbAvg(128),
			m_N(0),
			m_sampleRate(0),
			m_samplesProcessed(0),
            m_samplesAvgProcessed(0),
			m_maxPowerIndex(0),
			m_nTones(2),
			m_samplesAttack(0),
			m_attackCount(0),
			m_samplesDecay(0),
			m_decayCount(0),
			m_squelchCount(0),
			m_isOpen(false),
			m_threshold(0.0)
{
	m_k = new double[m_nTones];
	m_coef = new double[m_nTones];
	m_toneSet = new double[m_nTones];
	m_u0 = new double[m_nTones];
	m_u1 = new double[m_nTones];
	m_power = new double[m_nTones];
	m_movingAverages.resize(m_nTones, MovingAverage<double>(m_nbAvg, 0.0f));

	m_toneSet[0]  = 2000.0;
	m_toneSet[1]  = 10000.0;

    for (unsigned int j = 0; j < m_nTones; ++j)
    {
        m_k[j] = ((double)m_N * m_toneSet[j]) / (double)m_sampleRate;
        m_coef[j] = 2.0 * cos((2.0 * M_PI * m_toneSet[j])/(double)m_sampleRate);
        m_u0[j] = 0.0;
        m_u1[j] = 0.0;
        m_power[j] = 0.0;
        m_movingAverages[j].fill(0.0);
    }
}

AFSquelch::AFSquelch(unsigned int nbTones, const double *tones) :
            m_nbAvg(128),
			m_N(0),
			m_sampleRate(0),
			m_samplesProcessed(0),
            m_samplesAvgProcessed(0),
			m_maxPowerIndex(0),
			m_nTones(nbTones),
			m_samplesAttack(0),
			m_attackCount(0),
			m_samplesDecay(0),
			m_decayCount(0),
			m_squelchCount(0),
			m_isOpen(false),
			m_threshold(0.0)
{
	m_k = new double[m_nTones];
	m_coef = new double[m_nTones];
	m_toneSet = new double[m_nTones];
	m_u0 = new double[m_nTones];
	m_u1 = new double[m_nTones];
	m_power = new double[m_nTones];
    m_movingAverages.resize(m_nTones, MovingAverage<double>(m_nbAvg, 0.0f));

    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		m_toneSet[j] = tones[j];
        m_k[j] = ((double)m_N * m_toneSet[j]) / (double)m_sampleRate;
        m_coef[j] = 2.0 * cos((2.0 * M_PI * m_toneSet[j])/(double)m_sampleRate);
        m_u0[j] = 0.0;
        m_u1[j] = 0.0;
        m_power[j] = 0.0;
        m_movingAverages[j].fill(0.0);
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


void AFSquelch::setCoefficients(
        unsigned int N,
        unsigned int nbAvg,
        unsigned int _samplerate,
        unsigned int _samplesAttack,
        unsigned int _samplesDecay)
{
	m_N = N;                   // save the basic parameters for use during analysis
	m_nbAvg = nbAvg;
	m_sampleRate = _samplerate;
	m_samplesAttack = _samplesAttack;
	m_samplesDecay = _samplesDecay;
	m_movingAverages.resize(m_nTones, MovingAverage<double>(m_nbAvg, 0.0));
	m_samplesProcessed = 0;
    m_samplesAvgProcessed = 0;
	m_maxPowerIndex = 0;
	m_attackCount = 0;
	m_decayCount = 0;
	m_squelchCount = 0;
	m_isOpen = false;
	m_threshold = 0.0;

	// for each of the frequencies (tones) of interest calculate
	// k and the associated filter coefficient as per the Goertzel
	// algorithm. Note: we are using a real value (as opposed to
	// an integer as described in some references. k is retained
	// for later display. The tone set is specified in the
	// constructor. Notice that the resulting coefficients are
	// independent of N.
    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		m_k[j] = ((double)m_N * m_toneSet[j]) / (double)m_sampleRate;
		m_coef[j] = 2.0 * cos((2.0 * M_PI * m_toneSet[j])/(double)m_sampleRate);
		m_u0[j] = 0.0;
		m_u1[j] = 0.0;
        m_power[j] = 0.0;
        m_movingAverages[j].fill(0.0);
	}
}


// Analyze an input signal
bool AFSquelch::analyze(double sample)
{

	feedback(sample); // Goertzel feedback

	if (m_samplesProcessed < m_N) // completed a block of N
	{
	    m_samplesProcessed++;
        return false;
	}
	else
	{
        feedForward(); // calculate the power at each tone
        m_samplesProcessed = 0;

        if (m_samplesAvgProcessed < m_nbAvg)
        {
            m_samplesAvgProcessed++;
            return false;
        }
        else
        {
            return true; // have a result
        }
	}
}


void AFSquelch::feedback(double in)
{
	double t;

	// feedback for each tone
    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		t = m_u0[j];
		m_u0[j] = in + (m_coef[j] * m_u0[j]) - m_u1[j];
		m_u1[j] = t;
	}
}


void AFSquelch::feedForward()
{
    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		m_power[j] = (m_u0[j] * m_u0[j]) + (m_u1[j] * m_u1[j]) - (m_coef[j] * m_u0[j] * m_u1[j]);
		m_movingAverages[j].feed(m_power[j]);
		m_u0[j] = 0.0;
		m_u1[j] = 0.0; // reset for next block.
	}

	evaluate();
}


void AFSquelch::reset()
{
    for (unsigned int j = 0; j < m_nTones; ++j)
	{
        m_u0[j] = 0.0;
        m_u1[j] = 0.0;
        m_power[j] = 0.0;
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

    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		if (m_movingAverages[j].sum() > maxPower)
		{
			maxPower = m_movingAverages[j].sum();
			maxIndex = j;
		}
	}

    if (maxPower == 0.0)
    {
        return m_isOpen;
    }

	minPower = maxPower;

    for (unsigned int j = 0; j < m_nTones; ++j)
	{
		if (m_movingAverages[j].sum() < minPower) {
			minPower = m_movingAverages[j].sum();
			minIndex = j;
		}
	}

//    m_isOpen = ((minPower/maxPower < m_threshold) && (minIndex > maxIndex));

    if ((minPower/maxPower < m_threshold) && (minIndex > maxIndex)) // open condition
    {
        if (m_squelchCount < m_samplesAttack + m_samplesDecay)
        {
            m_squelchCount++;
        }
    }
    else
    {
        if (m_squelchCount > m_samplesAttack)
        {
            m_squelchCount--;
        }
        else
        {
            m_squelchCount = 0;
        }
    }

    m_isOpen = (m_squelchCount >= m_samplesAttack) ;

//	if ((minPower/maxPower < m_threshold) && (minIndex > maxIndex)) // open condition
//	{
//		if ((m_samplesAttack > 0) && (m_attackCount < m_samplesAttack))
//		{
//			m_isOpen = false;
//			m_attackCount++;
//		}
//		else
//		{
//			m_isOpen = true;
//			m_decayCount = 0;
//		}
//	}
//	else
//	{
//		if ((m_samplesDecay > 0) && (m_decayCount < m_samplesDecay))
//		{
//			m_isOpen = true;
//			m_decayCount++;
//		}
//		else
//		{
//			m_isOpen = false;
//			m_attackCount = 0;
//		}
//	}

	return m_isOpen;
}

void AFSquelch::setThreshold(double threshold)
{
	qDebug("AFSquelch::setThreshold: threshold: %f", threshold);
	m_threshold = threshold;
	reset();
}
