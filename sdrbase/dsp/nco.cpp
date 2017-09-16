///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <QtGlobal>
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "dsp/nco.h"

#undef M_PI
#define M_PI		3.14159265358979323846

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
}

void NCO::setFreq(Real freq, Real sampleRate)
{
	m_phaseIncrement = (freq * TableSize) / sampleRate;
	qDebug("NCO freq: %f phase inc %d", freq, m_phaseIncrement);
}

float NCO::next()
{
	nextPhase();
	return m_table[m_phase];
}

Complex NCO::nextIQ()
{
	nextPhase();
	return Complex(m_table[m_phase], -m_table[(m_phase + TableSize / 4) % TableSize]);
}

Complex NCO::nextQI()
{
	nextPhase();
	return Complex(-m_table[(m_phase + TableSize / 4) % TableSize], m_table[m_phase]);
}

void NCO::nextIQMul(Real& i, Real& q)
{
    nextPhase();
    Real x = i;
    Real y = q;
    const Real& u = m_table[m_phase];
    const Real& v = -m_table[(m_phase + TableSize / 4) % TableSize];
    i = x*u - y*v;
    q = x*v + y*u;
}

float NCO::get()
{
	return m_table[m_phase];
}

Complex NCO::getIQ()
{
	return Complex(m_table[m_phase], -m_table[(m_phase + TableSize / 4) % TableSize]);
}

void NCO::getIQ(Complex& c)
{
	c.real(m_table[m_phase]);
	c.imag(-m_table[(m_phase + TableSize / 4) % TableSize]);
}

Complex NCO::getQI()
{
	return Complex(-m_table[(m_phase + TableSize / 4) % TableSize], m_table[m_phase]);
}

void NCO::getQI(Complex& c)
{
	c.imag(m_table[m_phase]);
	c.real(-m_table[(m_phase + TableSize / 4) % TableSize]);
}
