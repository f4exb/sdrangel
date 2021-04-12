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

#ifndef DATVGAUGE_H
#define DATVGAUGE_H

#include <QLabel>
#include <QString>

#include "leansdr/framework.h"
#include "leansdr/sdr.h"

#include "gui/levelmeter.h"

namespace leansdr {

struct datvgauge: runnable
{
    leansdr::pipereader<leansdr::f32> m_in;
    QLabel *m_label;
    LevelMeterSignalDB *m_levelMeter;
    static const int m_nbAvg = 10;
    leansdr::f32 m_samples[m_nbAvg];
    leansdr::f32 m_sum;
    int m_index;

    datvgauge(
        scheduler *sch,
        leansdr::pipebuf<leansdr::f32> &in,
        QLabel *label = nullptr,
        LevelMeterSignalDB *levelMeter = nullptr,
        const char *_name = nullptr
    ) :
        runnable(sch, _name ? _name : in.name),
        m_in(in),
        m_label(label),
        m_levelMeter(levelMeter)
    {
        std::fill(m_samples, m_samples+m_nbAvg, 0);
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

            m_levelMeter->levelChanged(avg/30.0, *p/30.0, m_nbAvg);
            m_label->setText(QString("%1").arg(avg, 0, 'f', 1));
            m_in.read(1);

            if (m_index == m_nbAvg) {
                m_index = 0;
            }
        }
    }
};

} // namespace leansdr

#endif // DATVGAUGE_H
