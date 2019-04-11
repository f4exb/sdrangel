///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

class SDRBASE_API NCO {
private:
	enum {
		TableSize = (1 << 12),
	};
	static Real m_table[TableSize];
	static bool m_tableInitialized;

	static void initTable();

	int m_phaseIncrement;
	int m_phase;

public:
	NCO();

	void setFreq(Real freq, Real sampleRate);
	void setPhase(int phase) { m_phase = phase; }

	void nextPhase()        //!< Increment phase
	{
		m_phase += m_phaseIncrement;
		while(m_phase >= TableSize)
			m_phase -= TableSize;
		while(m_phase < 0)
			m_phase += TableSize;
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
