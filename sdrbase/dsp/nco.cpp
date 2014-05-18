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
	qDebug("NCO phase inc %d", m_phaseIncrement);
}

float NCO::next()
{
	m_phase += m_phaseIncrement;
	while(m_phase >= TableSize)
		m_phase -= TableSize;
	while(m_phase < 0)
		m_phase += TableSize;

	return m_table[m_phase];
}

Complex NCO::nextIQ()
{
	m_phase += m_phaseIncrement;
	while(m_phase >= TableSize)
		m_phase -= TableSize;
	while(m_phase < 0)
		m_phase += TableSize;

	return Complex(m_table[m_phase], -m_table[(m_phase + TableSize / 4) % TableSize]);
}
