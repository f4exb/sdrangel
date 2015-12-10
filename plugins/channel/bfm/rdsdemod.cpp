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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "rdsdemod.h"

RDSDemod::RDSDemod()
{
	m_rdsBB = 0.0;
	m_rdsBB_1 = 0.0;
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

void RDSDemod::process(Real rdsSample, Real pilotSample)
{
	m_rdsBB = filter_lp_2400_iq(rdsSample, 0); // working on real part only

	// 1187.5 Hz clock
	m_rdsClockPhase = (pilotSample / 48.0) + m_rdsClockOffset;
	m_rdsClockLO = (fmod(m_rdsClockPhase, 2 * M_PI) < M_PI ? 1 : -1);

	// Clock phase recovery
	if (sign(m_rdsBB_1) != sign(m_rdsBB))
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
		/* biphase symbol integrate & dump */
		m_acc += m_rdsBB * m_rdsClockLO;

		if (sign(m_rdsClockLO) != sign(m_rdsClockLO_1))
		{
			biphase(m_acc);
			m_acc = 0;
		}

		m_rdsClockLO_1 = m_rdsClockLO;
	}
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

int RDSDemod::sign(Real a)
{
	return (a >= 0 ? 1 : 0);
}

void RDSDemod::biphase(Real acc)
{
	static int reading_frame = 0;

	if (sign(acc) != sign(m_acc_1)) // two successive of different sign: error detected
	{
		m_totErrors[m_counter % 2]++;
	}

	if (m_counter % 2 == reading_frame) // two successive of the same sing: OK
	{
		print_delta(sign(acc + m_acc_1));
	}

	if (m_counter == 0)
	{
		if (m_totErrors[1 - reading_frame] < m_totErrors[reading_frame])
		{
			reading_frame = 1 - reading_frame;
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
	printf("%d", b);
}

