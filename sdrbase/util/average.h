///////////////////////////////////////////////////////////////////////////////////////
//                                                                                   //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                       //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#ifndef _UTIL_AVERAGE_H_
#define _UTIL_AVERAGE_H_

#include <algorithm>

template <typename T, typename Total>
class AverageUtil
{
  public:
    AverageUtil()
      : m_numSamples(0), m_total(0)
    { }

    AverageUtil(T sample)
      : m_numSamples(1), m_total(sample)
    { }

    void reset()
    {
        m_numSamples = 0;
        m_total = 0;
    }

    void operator()(T sample)
    {
        m_total += sample;
        m_numSamples++;
    }

    double asDouble() const { return ((double)m_total) / (m_numSamples == 0 ? 1 : m_numSamples); }
    float asFloat() const { return ((float)m_total) / (m_numSamples == 0 ? 1 : m_numSamples); }
    operator T() const { return  m_total / (m_numSamples == 0 ? 1 : m_numSamples); }
    T instantAverage() const { return m_total / (m_numSamples == 0 ? 1 : m_numSamples); }
    int getNumSamples() const { return m_numSamples; }

  private:
    int m_numSamples;
    Total m_total;
};

#endif /* _UTIL_AVERAGE_H_ */
