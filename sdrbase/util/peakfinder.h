///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
//                                                                               //
// Find peaks in a series of real values                                         //
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

#ifndef SDRBASE_UTIL_PEAKFINDER_H_
#define SDRBASE_UTIL_PEAKFINDER_H_

#include <vector>
#include <utility>

#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API PeakFinder {
public:
    PeakFinder();
    ~PeakFinder();

    void init(Real value);
    void push(Real value, bool last=false);
    void sortPeaks();
    const std::vector<std::pair<Real, int>>& getPeaks() { return m_peaks; }

private:
    Real m_prevValue;
    int m_index;
    std::vector<std::pair<Real, int>> m_peaks; //!< index, value

};


#endif // SDRBASE_UTIL_PEAKFINDER_H_
