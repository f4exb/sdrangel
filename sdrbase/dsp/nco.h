///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_NCO_H
#define INCLUDE_NCO_H

#include "dsp/dsptypes.h"
#include "export.h"

// Numerically Controlled Oscillator (NCO), using 12-bit LUT and Q12.20 fixed-point phase accumulator with linear interpolation
class SDRBASE_API NCO {
private:

	constexpr static unsigned TableBits = 12;
	constexpr static unsigned TableSize = 1 << TableBits;

	constexpr static unsigned IntShift = 32 - TableBits;
	constexpr static unsigned IntMask = TableSize - 1u;
	constexpr static unsigned FracMask = ((1u << IntShift) - 1u);

	constexpr static float Denom = 1u << IntShift;

	static Real m_table[TableSize];
	static bool m_tableInitialized;

	static void initTable();

	unsigned m_phaseIncrement;
	unsigned m_phase;

public:
	NCO();

	void setFreq(Real freq, Real sampleRate);
	void setPhase(int phase) { m_phase = phase; }

	void nextPhase()        //!< Increment phase
	{
		m_phase += m_phaseIncrement; // No need to wrap, as that is handled by overflow
	}

	Real next();            //!< Return next real sample
	Complex nextIQ();       //!< Return next complex sample
	Complex nextQI();       //!< Return next complex sample (reversed)
	void nextIQMul(Real& i, Real& q); //!< multiply I,Q separately with next sample
	Real get();             //!< Return current real sample (no phase increment)
	Complex getIQ();        //!< Return current complex sample (no phase increment)
	void getIQ(Complex& c); //!< Sets to the current complex sample (no phase increment)
	Complex getQI();        //!< Return current complex sample (no phase increment, reversed)
	void getQI(Complex& c); //!< Sets to the current complex sample (no phase increment, reversed)
};

#endif // INCLUDE_NCO_H
