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

// Numerically Controlled Oscillator (NCO), using 2^12 entry 32-bit LUT and Q12.20 fixed-point phase accumulator with linear interpolation
// With a 2^12 32-bit LUT, SFDR is 144 dBc. 2^13 would be 156 dBc.
// Fractional part can be set to 0 by setting integerPhase = true, to increase SFDR while decreasing frequency accuracy.
// Frequency accuracy = sampleRate / 2^PhaseBits.
// So:
//   48k / 2^32 = 0.00001 Hz
//   48k / 2^12 = 15.2 Hz (integer only)
//    2M / 2^32 = 0.00046 Hz
//    2M / 2^12 = 488 Hz (integer only)
class SDRBASE_API NCO {
private:

#ifdef NCO_64_BIT
	constexpr static unsigned PhaseBits = 64;
	typedef uint64_t Phase;
	typedef double Frac;
#else
	constexpr static unsigned PhaseBits = 32;
	typedef uint32_t Phase;
	typedef float Frac;
#endif

	constexpr static unsigned TableBits = 12;
	constexpr static unsigned TableSize = 1 << TableBits;

	constexpr static unsigned IntShift = PhaseBits - TableBits;

	constexpr static Phase IntMask = TableSize - 1u;
	constexpr static Phase FracMask = ((1ull << IntShift) - 1u);

	constexpr static Frac Denom = 1ull << IntShift;

	static Real m_table[TableSize];
	static bool m_tableInitialized;

	static void initTable();

	uint64_t prsg63();

	Phase m_phaseIncrement;
	Phase m_phase;
	Phase m_phaseDithered;

	uint64_t m_lfsr;			// Linear feedback shift register for psuedo random number generation
	Phase m_ditherMask;			// Bit mask to select bits from lfsr to use for phase dithering

public:
	NCO();

	void setFreq(Real freq, Real sampleRate, bool integerPhase = false, int ditherBits = 0);
	void setPhase(Phase phase) { m_phase = phase; }

	void nextPhase()        //!< Increment phase
	{
		m_phase += m_phaseIncrement; // No need to wrap, as that is handled by overflow
		m_phaseDithered = m_phase;
		if (m_ditherMask) {
			m_phaseDithered += prsg63() & m_ditherMask;
		}
	}

	Real next();            //!< Return next real sample
	Complex nextIQ();       //!< Return next complex sample
	Complex nextQI();       //!< Return next complex sample (reversed)
	Complex nextIQ(float imbalance);  //!< Return next complex sample with an imbalance factor on I
	void nextIQMul(Real& i, Real& q); //!< multiply I,Q separately with next sample
	Real get() const;                 //!< Return current real sample (no phase increment)
	Complex getIQ() const;            //!< Return current complex sample (no phase increment)
	void getIQ(Complex& c) const;     //!< Sets to the current complex sample (no phase increment)
	Complex getQI() const;            //!< Return current complex sample (no phase increment, reversed)
	void getQI(Complex& c) const;     //!< Sets to the current complex sample (no phase increment, reversed)
};

#endif // INCLUDE_NCO_H
