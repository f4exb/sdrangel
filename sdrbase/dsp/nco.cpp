///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2016-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include <QtGlobal>
#include <cstdio>
#include <cmath>
#include "dsp/nco.h"

Real NCO::m_table[NCO::TableSize];
bool NCO::m_tableInitialized = false;

void NCO::initTable()
{
	if(m_tableInitialized)
		return;

	for(int i = 0; i < TableSize; i++)
		m_table[i] = cos((2.0 * M_PI * i) / TableSize);

	m_tableInitialized = true;
}

NCO::NCO()
{
	initTable();
	m_phase = 0;
	m_phaseIncrement = 0;
}

void NCO::setFreq(Real freq, Real sampleRate)
{
	m_phaseIncrement = (unsigned) std::round((freq * 4294967296.0) / sampleRate);
	qDebug("NCO freq: %f phase inc: %u sr: %f", freq, m_phaseIncrement, sampleRate);
}

Real NCO::next()
{
	nextPhase();
	return get();
}

Complex NCO::nextIQ()
{
	nextPhase();
	return getIQ();
}

Complex NCO::nextQI()
{
	Complex iq = nextIQ();
	return Complex(iq.imag(), iq.real());
}

void NCO::nextIQMul(Real& i, Real& q)
{
    Real x = i;
    Real y = q;
	Complex iq = nextIQ();
    i = x*iq.real() - y*iq.imag();
    q = x*iq.imag() + y*iq.real();
}

Real NCO::get()
{
	unsigned intBits = (m_phase >> IntShift) & IntMask;
	unsigned fracBits = m_phase & FracMask;
	unsigned i = intBits;
	unsigned j = (i + 1) & IntMask;
	Real frac = ((Real) fracBits) / Denom;
	return m_table[i] + frac * (m_table[j] - m_table[i]); // Linear interpolation
}

Complex NCO::getIQ()
{
	unsigned intBits = (m_phase >> IntShift) & IntMask;
	unsigned fracBits = m_phase & FracMask;
	unsigned i = intBits;
	unsigned j = (i + 1) & IntMask;
	unsigned k = (i + TableSize / 4) & IntMask;
	unsigned l = (j + TableSize / 4) & IntMask;
	Real frac = ((Real) fracBits) / Denom;
	Real s = m_table[i] + frac * (m_table[j] - m_table[i]); // Linear interpolation for sin
	Real c = m_table[k] + frac * (m_table[l] - m_table[k]); // Linear interpolation for cos
	return Complex(s, -c);
}

void NCO::getIQ(Complex& c)
{
	c = getIQ();
}

Complex NCO::getQI()
{
	Complex iq = getIQ();
	return Complex(iq.imag(), iq.real());
}

void NCO::getQI(Complex& c)
{
	c = getQI();
}
