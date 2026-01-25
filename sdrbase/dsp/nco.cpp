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

uint64_t NCO::prsg63()
{
	m_lfsr = m_lfsr << 32 | (m_lfsr << 1 ^ m_lfsr << 2) >> 32;
	m_lfsr = m_lfsr << 32 | (m_lfsr << 1 ^ m_lfsr << 2) >> 32;
	return m_lfsr;
}

void NCO::setFreq(Real freq, Real sampleRate, bool integerPhase, int ditherBits)
{
	m_phaseIncrement = (Phase) std::round((freq * pow(2.0, PhaseBits)) / sampleRate);
	if (integerPhase) {
		m_phaseIncrement &= ~FracMask;
	}
	m_ditherMask = (1ull << ditherBits) - 1;
	qDebug("NCO freq: %f phase inc: %u sr: %f dither: %d", freq, m_phaseIncrement, sampleRate, ditherBits);
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

Complex NCO::nextIQ(float imbalance)
{
	nextPhase();
	Phase phaseQ = imbalance < 0.0f ? m_phase + (Phase) (imbalance * powf(2.0f, PhaseBits)) : m_phase;
	Phase phaseI = imbalance < 0.0f ? m_phase : m_phase + (Phase) (imbalance * powf(2.0f, PhaseBits));
	Phase phaseIIntBits = (phaseI >> IntShift) & IntMask;
	Phase phaseIFracBits = phaseI & FracMask;
	Phase phaseQIntBits = (phaseQ >> IntShift) & IntMask;
	Phase phaseQFracBits = phaseQ & FracMask;
	unsigned i = phaseIIntBits;
	unsigned j = (i + 1) & IntMask;
	unsigned k = (phaseQIntBits + TableSize / 4) & IntMask;
	unsigned l = (k + 1) & IntMask;
	Frac fracI = ((Frac) phaseIFracBits) / Denom;
	Frac fracQ = ((Frac) phaseQFracBits) / Denom;
	Frac s = m_table[i] + fracI * (m_table[j] - m_table[i]); // Linear interpolation for sin
	Frac c = m_table[k] + fracQ * (m_table[l] - m_table[k]); // Linear interpolation for cos
	return Complex(s, -c);
}

void NCO::nextIQMul(Real& i, Real& q)
{
    Real x = i;
    Real y = q;
	Complex iq = nextIQ();
    i = x*iq.real() - y*iq.imag();
    q = x*iq.imag() + y*iq.real();
}

Real NCO::get() const
{
	Phase intBits = (m_phaseDithered >> IntShift) & IntMask;
	Phase fracBits = m_phaseDithered & FracMask;
	unsigned i = intBits;
	unsigned j = (i + 1) & IntMask;
	Frac frac = ((Frac) fracBits) / Denom;
	return m_table[i] + frac * (m_table[j] - m_table[i]); // Linear interpolation
}

Complex NCO::getIQ() const
{
	Phase intBits = (m_phaseDithered >> IntShift) & IntMask;
	Phase fracBits = m_phaseDithered & FracMask;
	unsigned i = intBits;
	unsigned j = (i + 1) & IntMask;
	unsigned k = (i + TableSize / 4) & IntMask;
	unsigned l = (j + TableSize / 4) & IntMask;
	Frac frac = ((Frac) fracBits) / Denom;
	Frac s = m_table[i] + frac * (m_table[j] - m_table[i]); // Linear interpolation for sin
	Frac c = m_table[k] + frac * (m_table[l] - m_table[k]); // Linear interpolation for cos
	return Complex(s, -c);
}

void NCO::getIQ(Complex& c) const
{
	c = getIQ();
}

Complex NCO::getQI() const
{
	Complex iq = getIQ();
	return Complex(iq.imag(), iq.real());
}

void NCO::getQI(Complex& c) const
{
	c = getQI();
}
