///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SAMPLESIMPLEFIFO_H
#define INCLUDE_SAMPLESIMPLEFIFO_H

#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SampleSimpleFifo {
public:
	SampleSimpleFifo();
	SampleSimpleFifo(int size);
    SampleSimpleFifo(const SampleSimpleFifo& other);
    ~SampleSimpleFifo();

	bool setSize(int size);
    void reset();
    unsigned int getSize() { return m_size; }
    unsigned int write(SampleVector::const_iterator begin, SampleVector::const_iterator end);
	unsigned int readBegin(unsigned int count,
		SampleVector::iterator* part1Begin, SampleVector::iterator* part1End,
		SampleVector::iterator* part2Begin, SampleVector::iterator* part2End);
    unsigned int readCommit(unsigned int count);

private:
    SampleVector m_data; //!< FIFO container
	unsigned int m_size; //!< size of FIFO (above)
	unsigned int m_fill; //!< number of samples in FIFO
	unsigned int m_head; //!< read point
	unsigned int m_tail; //!< write point

    void create(unsigned int s);
};

#endif // INCLUDE_SAMPLESIMPLEFIFO_H