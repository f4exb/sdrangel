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
	m_subcarrPhi_1 = 0;
	m_dPhiSc = 0;
	m_subcarrBB_1 = 0.0;
	m_rdsClockPhase = 0.0;
	m_rdsClockPhase_1 = 0.0;
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
	pilotPhaseSample = fmod(pilotPhaseSample, M_PI);

	m_dPhiSc = pilotPhaseSample - m_subcarrPhi_1;

	if (m_dPhiSc < 0)
	{
		m_dPhiSc += M_PI;
	}

	m_subcarrBB[0] = filter_lp_2400_iq(demod, 0); // working on real part only

	// 1187.5 Hz clock from 19 kHz pilot

	m_rdsClockPhase += (m_dPhiSc / 16.0) + m_rdsClockOffset;
	m_rdsClockPhase = fmod(m_rdsClockPhase, M_PI);
	m_rdsClockLO = (m_rdsClockPhase < 0.0 ? -1.0 : 1.0);

	// Clock phase recovery

	if (sign(m_subcarrBB_1) != sign(m_subcarrBB[0]))
	{
		Real d_cphi = m_rdsClockPhase;

		if (d_cphi >= M_PI_2)
		{
			d_cphi -= M_PI;
		}

		m_rdsClockOffset -= 0.005 * d_cphi;
	}

	biphase(m_acc, m_rdsClockPhase - m_rdsClockPhase_1);

	m_numSamples++;
	m_subcarrPhi_1 = pilotPhaseSample;
	m_rdsClockPhase_1 = m_rdsClockPhase;
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

void RDSDemod::biphase(Real acc, Real clockDPhi)
{
	if (clockDPhi < 0)
	{
		clockDPhi += M_PI;
	}

	double fsc = (m_dPhiSc / (2.0 * M_PI)) * m_srate;
	double fclk = (clockDPhi / (2.0 * M_PI)) * m_srate;

	if (m_counter == 0)
	{
		qDebug("RDSDemod::biphase: frame: %d pll: %.3f (ppm: %+8.3f) clk: %8.1f",
				m_readingFrame,
				fsc,
				((fsc - 19000.0) / 19000.0) * 1000000,
				fclk);
	}

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

