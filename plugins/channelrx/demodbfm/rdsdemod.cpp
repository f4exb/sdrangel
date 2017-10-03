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

#include "../../channelrx/demodbfm/rdsdemod.h"

#include <QDebug>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#undef M_PI
#define M_PI 3.14159265358979323846
#undef M_PI_2
#define M_PI_2 1.57079632679489661923

const Real RDSDemod::m_pllBeta = 50;
const Real RDSDemod::m_fsc = 1187.5;

RDSDemod::RDSDemod()
	// : m_udpDebug(this, 1472, 9995) // UDP debug
{
	m_srate = 250000;

	m_parms.subcarr_phi = 0;
	m_parms.clock_offset = 0;
	m_parms.clock_phi = 0;
	m_parms.prev_clock_phi = 0;
	m_parms.lo_clock = 0;
	m_parms.prev_lo_clock = 0;
	m_parms.prev_bb = 0;
	m_parms.d_cphi = 0;
	m_parms.acc = 0;
	m_parms.numsamples = 0;
	m_parms.prev_acc = 0;
	m_parms.counter = 0;
	m_parms.reading_frame = 0;
	m_parms.dbit = 0;
	m_prev = 0.0f;
}

RDSDemod::~RDSDemod()
{
	//delete m_socket;
}

void RDSDemod::setSampleRate(int srate __attribute__((unused))) /// FIXME: fix rate for now
{
}

bool RDSDemod::process(Real demod, bool& bit)
{
	bool ret = false;

	//m_udpDebug.write(m_parms.lo_clock * m_parms.subcarr_bb[0]); // UDP debug

	// Subcarrier downmix & phase recovery

	m_parms.subcarr_bb[0] = filter_lp_2400_iq(demod, 0);

	// 1187.5 Hz clock

	/*
	if (m_parms.subcarr_phi > 1e9) // ~ every 37 hours =>  not really useful
	{
		qDebug("RDSDemod::process: reset 1187.5 Hz clock");
		m_parms.subcarr_phi = 0;
		m_parms.clock_offset = 0;
	}*/

    m_parms.subcarr_phi += (2 * M_PI * m_fsc) / (Real) m_srate;
	m_parms.clock_phi = m_parms.subcarr_phi + m_parms.clock_offset;

	// Clock phase recovery

	if (sign(m_parms.prev_bb) != sign(m_parms.subcarr_bb[0]))
	{
		m_parms.d_cphi = std::fmod(m_parms.clock_phi, M_PI);

		if (m_parms.d_cphi >= M_PI_2)
		{
			m_parms.d_cphi -= M_PI;
		}

		m_parms.clock_offset -= 0.005 * m_parms.d_cphi;
	}

	m_parms.clock_phi = std::fmod(m_parms.clock_phi, 2 * M_PI);
	m_parms.lo_clock = (m_parms.clock_phi < M_PI ? 1 : -1);

	/* Decimate band-limited signal */
	if (m_parms.numsamples % 8 == 0)
	{
		/* biphase symbol integrate & dump */
		m_parms.acc += m_parms.subcarr_bb[0] * m_parms.lo_clock;

		if (sign(m_parms.lo_clock) != sign(m_parms.prev_lo_clock))
		{
			ret = biphase(m_parms.acc, bit, m_parms.clock_phi - m_parms.prev_clock_phi);
			m_parms.acc = 0;
		}

		m_parms.prev_lo_clock = m_parms.lo_clock;
	}

	m_parms.numsamples++;
	m_parms.prev_bb = m_parms.subcarr_bb[0];
	m_parms.prev_clock_phi = m_parms.clock_phi;
	m_prev = demod;

	return ret;
}

bool RDSDemod::biphase(Real acc, bool& bit, Real d_cphi)
{
	bool ret = false;

	if (sign(acc) != sign(m_parms.prev_acc)) // two successive of different sign: error detected
	{
		m_parms.tot_errs[m_parms.counter % 2]++;
	}

	if (m_parms.counter % 2 == m_parms.reading_frame) // two successive of the same sing: OK
	{
		// new bit found
		int b = sign(m_parms.acc + m_parms.prev_acc);
		bit = b ^ m_parms.dbit;
		m_parms.dbit = b;
		ret = true;
	}

	if (m_parms.counter == 0)
	{
		if (m_parms.tot_errs[1 - m_parms.reading_frame] < m_parms.tot_errs[m_parms.reading_frame])
		{
			m_parms.reading_frame = 1 - m_parms.reading_frame;
		}

		m_report.qua = (1.0 * abs(m_parms.tot_errs[0] - m_parms.tot_errs[1]) / (m_parms.tot_errs[0] + m_parms.tot_errs[1])) * 100;
		m_report.acc = acc;
		m_report.fclk = (d_cphi / (2 * M_PI)) * m_srate;

		/*
		qDebug("RDSDemod::biphase: frame: %d  acc: %+6.3f errs: %3d %3d  qual: %3.0f%%  clk: %7.2f",
				m_parms.reading_frame,
				acc,
				m_parms.tot_errs[0],
				m_parms.tot_errs[1],
				m_report.qua,
				m_report.fclk);*/

		m_parms.tot_errs[0] = 0;
		m_parms.tot_errs[1] = 0;
	}

	m_parms.prev_acc = acc; // memorize (z^-1)
	m_parms.counter = (m_parms.counter + 1) % 800;

	return ret;
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
