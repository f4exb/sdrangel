///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include <algorithm>

#include "peakfinder.h"

PeakFinder::PeakFinder() :
    m_prevValue(0.0),
    m_index(0)
{}

PeakFinder::~PeakFinder()
{}

void PeakFinder::init(Real value)
{
    m_prevValue = value;
    m_peaks.clear();
    m_index = 0;
}

void PeakFinder::push(Real value, bool last)
{
    Real diff = value - m_prevValue;

    if (diff < 0) {
        m_peaks.push_back({m_prevValue, m_index});
    } else if (last) {
        m_peaks.push_back({value, m_index});
    }

    m_prevValue = value;
    m_index++;
}

void PeakFinder::sortPeaks()
{
    std::sort(m_peaks.rbegin(), m_peaks.rend()); // descending order of values
}
