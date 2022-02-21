///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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
#include <stdio.h>
#include <cmath>
#include "dsp/ncof.h"

Real NCOF::m_table[NCOF::TableSize+1];
bool NCOF::m_tableInitialized = false;
float NCOF::m_tableSizeLimit = (float) NCOF::TableSize;

void NCOF::initTable()
{
	if(m_tableInitialized) {
		return;
	}

	for(int i = 0; i <= TableSize; i++) {
		m_table[i] = cos((2.0 * M_PI * i) / TableSize);
	}

	m_tableInitialized = true;
}

NCOF::NCOF()
{
	initTable();
	m_phase = 0.0f;
	m_phaseIncrement = 0.0f;
}

void NCOF::setFreq(Real freq, Real sampleRate)
{
	m_phaseIncrement = sampleRate == 0 ? 0 : (freq * TableSize) / sampleRate;
	qDebug("NCOF::setFreq: freq: %f sr: %f m_phaseIncrement: %f", freq, sampleRate, m_phaseIncrement);
}

float NCOF::next()
{
	int phase = nextPhase();
	return m_table[phase];
}

Complex NCOF::nextIQ()
{
    int phase = nextPhase();
	return Complex(m_table[phase], -m_table[(phase + TableSize / 4) % TableSize]);
}

Complex NCOF::nextIQ(float imbalance)
{
    int phase = nextPhase();
    int phaseQ = imbalance < 0.0 ? phase + (int) (imbalance*TableSize) : phase;
    int phaseI = imbalance < 0.0 ? phase : phase + (int) (imbalance*TableSize);
    return Complex(m_table[phaseI % TableSize], -m_table[(phaseQ + TableSize / 4) % TableSize]);
}

Complex NCOF::nextQI()
{
    int phase = nextPhase();
	return Complex(-m_table[(phase + TableSize / 4) % TableSize], m_table[phase]);
}

float NCOF::get()
{
	return m_table[(int) m_phase];
}

Complex NCOF::getIQ()
{
	return Complex(m_table[(int) m_phase], -m_table[((int) m_phase + TableSize / 4) % TableSize]);
}

void NCOF::getIQ(Complex& c)
{
	c.real(m_table[(int) m_phase]);
	c.imag(-m_table[((int) m_phase + TableSize / 4) % TableSize]);
}

Complex NCOF::getQI()
{
	return Complex(-m_table[((int) m_phase + TableSize / 4) % TableSize], m_table[(int) m_phase]);
}

void NCOF::getQI(Complex& c)
{
	c.imag(m_table[(int) m_phase]);
	c.real(-m_table[((int) m_phase + TableSize / 4) % TableSize]);
}
