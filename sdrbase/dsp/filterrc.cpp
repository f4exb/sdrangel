///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include "dsp/filterrc.h"

// Construct 1st order low-pass IIR filter.
LowPassFilterRC::LowPassFilterRC(Real timeconst) :
    m_timeconst(timeconst),
	m_y1(0)
{
	m_a1 = - exp(-1/m_timeconst);
	m_b0 = 1 + m_a1;
}

// Reconfigure
void LowPassFilterRC::configure(Real timeconst)
{
	m_timeconst = timeconst;
	m_y1 = 0;
	m_a1 = - exp(-1/m_timeconst);
	m_b0 = 1 + m_a1;

	qDebug() << "LowPassFilterRC::configure: t: " << m_timeconst
			<< " a1: " << m_a1
			<< " b0: " << m_b0;
}

// Process samples.
void LowPassFilterRC::process(const Real& sample_in, Real& sample_out)
{
    /*
     * Continuous domain:
     *   H(s) = 1 / (1 - s * timeconst)
     *
     * Discrete domain:
     *   H(z) = (1 - exp(-1/timeconst)) / (1 - exp(-1/timeconst) / z)
     */

	m_y1 = (sample_in * m_b0) - (m_y1 * m_a1);
	sample_out = m_y1;
}

// Construct 1st order high-pass IIR filter.
HighPassFilterRC::HighPassFilterRC(Real timeconst) :
     m_timeconst(timeconst),
     m_y1(0)
{
	m_a1 = 1 - exp(-1/m_timeconst);
	m_b0 = 1 + m_a1;
}

// Reconfigure
void HighPassFilterRC::configure(Real timeconst)
{
	m_timeconst = timeconst;
	m_y1 = 0;
	m_a1 = 1 - exp(-1/m_timeconst);
	m_b0 = 1 + m_a1;

	qDebug() << "HighPassFilterRC::configure: t: " << m_timeconst
			<< " a1: " << m_a1
			<< " b0: " << m_b0;
}

// Process samples.
void HighPassFilterRC::process(const Real& sample_in, Real& sample_out)
{
	m_y1 = (sample_in * m_b0) - (m_y1 * m_a1);
	sample_out = m_y1;
}
