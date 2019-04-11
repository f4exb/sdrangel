///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
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

#ifndef SDRBASE_UTIL_LIMITEDCOUNTER_H_
#define SDRBASE_UTIL_LIMITEDCOUNTER_H_

#include <stdint.h>

template<typename StoreType, uint32_t Limit>
class LimitedCounter
{
public:
    LimitedCounter() :
        m_counter(0)
    {}

    LimitedCounter(StoreType value) :
        m_counter(value < Limit ? value : Limit - 1)
    {}

    void operator=(const LimitedCounter& rhs) {
        m_counter = rhs.m_counter;
    }

    LimitedCounter& operator++() {
        m_counter++;
        if (m_counter >= Limit) {
            m_counter = 0;
        }
        return *this;
    }

    LimitedCounter operator++(int) {
        LimitedCounter temp = *this ;
        m_counter++;
        if (m_counter >= Limit) {
            m_counter = 0;
        }
        return temp;
    }

    LimitedCounter& operator+=(const uint32_t rhs) {
        m_counter += rhs;
        if (m_counter >= Limit) {
            m_counter -= Limit;
        }
        return *this;
    }

    StoreType value() const { return m_counter; }

    friend LimitedCounter operator-(const LimitedCounter lhs, const LimitedCounter& rhs)
    {
        LimitedCounter result;

        if (lhs.m_counter < rhs.m_counter) {
            result.m_counter = Limit - lhs.m_counter + rhs.m_counter + 1;
        } else {
            result.m_counter = lhs.m_counter - rhs.m_counter;
        }

        return result;
    }

private:
    StoreType m_counter;
};



#endif /* SDRBASE_UTIL_LIMITEDCOUNTER_H_ */
