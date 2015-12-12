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
const int RDSDemod::m_udpSize = 1472;
const Real RDSDemod::m_fsc = 1187.5;

RDSDemod::RDSDemod()
{
	m_srate = 250000;

	m_parms.subcarr_phi = 0;
	m_parms.clock_offset = 0;
	m_parms.clock_phi = 0;
	m_parms.prev_clock_phi = 0;
	m_parms.lo_clock = 0;
	m_parms.prevclock = 0;
	m_parms.prev_bb = 0;
	m_parms.d_phi_sc = 0;
	m_parms.d_cphi = 0;
	m_parms.acc = 0;
	m_parms.subcarr_sample = 0;
	m_parms.c = 0;
	m_parms.fmfreq = 0;
	m_parms.bytesread;
	m_parms.numsamples = 0;
	m_parms.loop_out = 0;
	m_parms.prev_loop = 0;
	m_parms.prev_acc = 0;
	m_parms.counter = 0;
	m_parms.reading_frame = 0;

	m_socket = new QUdpSocket(this);
	m_sampleBufferIndex = 0;
}

RDSDemod::~RDSDemod()
{
	delete m_socket;
}

void RDSDemod::setSampleRate(int srate) /// FIXME: fix rate for now
{
}

void RDSDemod::process(Real demod, Real pilot)
{
	m_sampleBuffer[m_sampleBufferIndex] =  m_parms.lo_clock * m_parms.subcarr_bb[0]; // UDP debug

	if (m_sampleBufferIndex < m_udpSize)
	{
		m_sampleBufferIndex++;
	}
	else
	{
		m_socket->writeDatagram((const char*)&m_sampleBuffer[0], (qint64 ) (m_udpSize * sizeof(Real)), QHostAddress::LocalHost, 9995);
		m_sampleBufferIndex = 0;
	}

	// Subcarrier downmix & phase recovery

	m_parms.subcarr_bb[0] = filter_lp_2400_iq(demod, 0);

	// 1187.5 Hz clock

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

		if (sign(m_parms.lo_clock) != sign(m_parms.prevclock))
		{
			biphase(m_parms.acc, m_parms.clock_phi - m_parms.prev_clock_phi);
			m_parms.acc = 0;
		}

		m_parms.prevclock = m_parms.lo_clock;
	}

	m_parms.numsamples++;
	m_parms.prev_bb = m_parms.subcarr_bb[0];
	m_parms.prev_clock_phi = m_parms.clock_phi;
	m_prev = demod;
}

void RDSDemod::biphase(Real acc, Real d_cphi)
{

	if (sign(acc) != sign(m_parms.prev_acc)) // two successive of different sign: error detected
	{
		m_parms.tot_errs[m_parms.counter % 2]++;
	}

	if (m_parms.counter % 2 == m_parms.reading_frame) // two successive of the same sing: OK
	{
		// TODO: take action print_delta(sign(acc + prev_acc));
	}

	if (m_parms.counter == 0)
	{
		if (m_parms.tot_errs[1 - m_parms.reading_frame] < m_parms.tot_errs[m_parms.reading_frame])
		{
			m_parms.reading_frame = 1 - m_parms.reading_frame;
		}

		Real qua = (1.0 * abs(m_parms.tot_errs[0] - m_parms.tot_errs[1]) / (m_parms.tot_errs[0] + m_parms.tot_errs[1])) * 100;
		qDebug("RDSDemod::biphase: frame: %d  acc: %+6.3f errs: %3d %3d  qual: %3.0f%%  clk: %7.2f",
				m_parms.reading_frame,
				acc,
				m_parms.tot_errs[0],
				m_parms.tot_errs[1],
				qua,
				(d_cphi / (2 * M_PI)) * m_srate);

		m_parms.tot_errs[0] = 0;
		m_parms.tot_errs[1] = 0;
	}

	m_parms.prev_acc = acc; // memorize (z^-1)
	m_parms.counter = (m_parms.counter + 1) % 800;
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
