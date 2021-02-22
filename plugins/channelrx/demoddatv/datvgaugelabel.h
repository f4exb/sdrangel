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

#ifndef DATVGAUGELABEL_H
#define DATVGAUGELABEL_H

#include <QLabel>
#include <QString>

#include "leansdr/framework.h"
#include "leansdr/sdr.h"

namespace leansdr {

struct datvgaugelabel: runnable
{
    leansdr::pipereader<leansdr::f32> m_in;
    QLabel *m_label;
    datvgaugelabel(
        scheduler *sch,
        leansdr::pipebuf<leansdr::f32> &in,
        QLabel *label = nullptr,
        const char *_name = nullptr
    ) :
        runnable(sch, _name ? _name : in.name),
        m_in(in),
        m_label(label)
    {}

    virtual void run()
    {
        while (m_in.readable() >= 1)
        {
            leansdr::f32 *p = m_in.rd();
            m_label->setText(QString("%1").arg(*p, 0, 'f', 1));
            m_in.read(1);
        }
    }
};

} // namespace leansdr

#endif // DATVGAUGELABEL_H
