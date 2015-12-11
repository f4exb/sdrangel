///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include <QDebug>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "rdsdemod.h"

const Real RDSDemod::m_pllBeta = 50;

RDSDemod::RDSDemod()
{
	m_srate = 250000;
	m_fsc = 57000.0;
	m_subcarrPhi = 0;
	m_dPhiSc = 0;
	m_subcarrBB_1 = 0.0;
	m_rdsClockPhase = 0.0;
	m_rdsClockOffset = 0.0;
	m_rdsClockLO = 0.0;
	m_rdsClockLO_1 = 0.0;
	m_numSamples = 0;
	m_acc = 0.0;
	m_acc_1 = 0.0;
	m_counter = 0;
	m_readingFrame = 0;
	m_totErrors[0] = 0;
	m_totErrors[1] = 0;
	m_dbit = 0;
}

RDSDemod::~RDSDemod()
{
}

void RDSDemod::setSampleRate(int srate)
{
	m_srate = srate;
	m_fsc = 57000.0;
}

void RDSDemod::process(Real demod, Real pilotPhaseSample)
{
	/*
	// Subcarrier downmix & phase recovery
	m_subcarrPhi += 2 * M_PI * m_fsc * (1.0 / m_srate);
	m_subcarrBB[0] = filter_lp_2400_iq(demod * cos(m_subcarrPhi), 0);
	m_subcarrBB[1] = filter_lp_2400_iq(demod * sin(m_subcarrPhi), 1);

	m_dPhiSc = 2 * filter_lp_pll(m_subcarrBB[1] * m_subcarrBB[0]);
	m_subcarrPhi -= m_pllBeta * m_dPhiSc; //prev_loop;
	m_fsc -= .5 * m_pllBeta * m_dPhiSc; //prev_loop;

	// 1187.5 Hz clock

	m_rdsClockPhase = m_subcarrPhi / 48.0 + m_rdsClockOffset;
	m_rdsClockLO = (fmod(m_rdsClockPhase, 2 * M_PI) < M_PI ? 1 : -1);

	// Clock phase recovery

	if (sign(m_subcarrBB_1) != sign(m_subcarrBB[0]))
	{
		Real d_cphi = fmod(m_rdsClockPhase, M_PI);

		if (d_cphi >= M_PI_2)
		{
			d_cphi -= M_PI;
		}

		m_rdsClockOffset -= 0.005 * d_cphi;
	}

	// Decimate band-limited signal
	if (m_numSamples % 8 == 0)
	{
		// biphase symbol integrate & dump
		m_acc += m_subcarrBB[0] * m_rdsClockLO;

		if (sign(m_rdsClockLO) != sign(m_rdsClockLO_1))
		{
			biphase(m_acc, m_fsc);
			m_acc = 0;
		}

		m_rdsClockLO_1 = m_rdsClockLO;
	}

	m_subcarrBB_1 = m_subcarrBB[0];
	*/

	m_subcarrBB[0] = filter_lp_2400_iq(demod, 0); // working on real part only

	// 1187.5 Hz clock from 19 kHz pilot
	m_rdsClockPhase = (pilotPhaseSample / 16.0) + m_rdsClockOffset;
	m_rdsClockLO = (m_rdsClockPhase > 0.0 ? 1.0 : -1.0);

	// Clock phase recovery
	if (sign(m_subcarrBB_1) != sign(m_subcarrBB[0]))
	{
		Real d_cphi = fmod(m_rdsClockPhase, M_PI);

		if (d_cphi >= M_PI_2)
		{
			d_cphi -= M_PI;
		}

		m_rdsClockOffset -= 0.005 * d_cphi;
	}

	// Decimate band-limited signal
	if (m_numSamples % 8 == 0)
	{
		// biphase symbol integrate & dump
		m_acc += m_subcarrBB[0] * m_rdsClockLO;

		if (sign(m_rdsClockLO) != sign(m_rdsClockLO_1))
		{
			biphase(m_acc, 57000.0);
			m_acc = 0;
		}

		m_rdsClockLO_1 = m_rdsClockLO;
	}

	m_numSamples++;
}

Real RDSDemod::filter_lp_2400_iq(Real input, int iqIndex)
{
	/* Digital filter designed by mkfilter/mkshape/gencode A.J. Fisher
	 Command line: /www/usr/fisher/helpers/mkfilter -Bu -Lp -o 10
	 -a 4.8000000000e-03 0.0000000000e+00 -l */

	m_xv[iqIndex][0] = m_xv[iqIndex][1]; m_xv[iqIndex][1] = m_xv[iqIndex][2];
	m_xv[iqIndex][2] = input / 4.491730007e+03;
	m_yv[iqIndex][0] = m_yv[iqIndex][1]; m_yv[iqIndex][1] = m_yv[iqIndex][2];
	m_yv[iqIndex][2] =   (m_xv[iqIndex][0] + m_xv[iqIndex][2]) + 2 * m_xv[iqIndex][1]
	+ ( -0.9582451124 * m_yv[iqIndex][0]) + (  1.9573545869 * m_yv[iqIndex][1]);

	return m_yv[iqIndex][2];
}

Real RDSDemod::filter_lp_pll(Real input)
{
	  m_xw[0] = m_xw[1];
	  m_xw[1] = input / 3.716236217e+01;
	  m_yw[0] = m_yw[1];
	  m_yw[1] =   (m_xw[0] + m_xw[1])
	    + (  0.9461821078 * m_yw[0]);
	  return m_yw[1];
}

int RDSDemod::sign(Real a)
{
	return (a >= 0 ? 1 : 0);
}

void RDSDemod::biphase(Real acc, Real fsc)
{
	if (sign(acc) != sign(m_acc_1)) // two successive of different sign: error detected
	{
		m_totErrors[m_counter % 2]++;
	}

	if (m_counter % 2 == m_readingFrame) // two successive of the same sign: OK
	{
		print_delta(sign(acc + m_acc_1));
	}

	if (m_counter == 0)
	{
		if (m_totErrors[1 - m_readingFrame] < m_totErrors[m_readingFrame])
		{
			m_readingFrame = 1 - m_readingFrame;

			double qua = (1.0 * abs(m_totErrors[0] - m_totErrors[1]) /	(m_totErrors[0] + m_totErrors[1])) * 100;
			qDebug("RDSDemod::biphase: frame: %d  errs: %3d %3d  qual: %3.0f%% pll: %.1f (%.1f ppm)",
					m_readingFrame,
					m_totErrors[0],
					m_totErrors[1],
					qua,
					fsc,
					(57000.0-fsc)/57000.0*1000000);
		}

		m_totErrors[0] = 0;
		m_totErrors[1] = 0;
	}

	m_acc_1 = acc; // memorize (z^-1)
	m_counter = (m_counter + 1) % 800;
}

void RDSDemod::print_delta(char b)
{
	output_bit(b ^ m_dbit);
	m_dbit = b;
}

void RDSDemod::output_bit(char b)
{
	// TODO: return value instead of spitting out
	//printf("%d", b);
}

