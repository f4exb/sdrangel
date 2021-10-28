///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef DATVMETER_H
#define DATVMETER_H

#include <QString>

#include "leansdr/framework.h"
#include "leansdr/sdr.h"

namespace leansdr {

struct datvmeter: runnable
{
    leansdr::pipereader<leansdr::f32> m_in;
    float m_avg;
    float m_rms;
    float m_peak;
    static const int m_nbAvg = 10;
    leansdr::f32 m_samples[m_nbAvg];
    leansdr::f32 m_sum;
    int m_index;

    datvmeter(
        scheduler *sch,
        leansdr::pipebuf<leansdr::f32> &in,
        const char *_name = nullptr
    ) :
        runnable(sch, _name ? _name : in.name),
        m_in(in)
    {
        std::fill(m_samples, m_samples+m_nbAvg, 0);
        m_avg = 0.0f;
        m_rms = 0.0f;
        m_peak = 0.0f;
        m_sum = 0;
        m_index = 0;
    }

    virtual void run()
    {
        while (m_in.readable() >= 1)
        {
            leansdr::f32 *p = m_in.rd();
            leansdr::f32& oldest = m_samples[m_index++];
            m_sum += *p - oldest;
            oldest = *p;
            leansdr::f32 avg = m_sum/m_nbAvg;

            m_avg = avg;
            m_rms = avg/30;
            m_peak = *p/30;
            m_in.read(1);

            if (m_index == m_nbAvg) {
                m_index = 0;
            }
        }
    }
};

} // namespace leansdr

#endif // DATVGAUGE_H
