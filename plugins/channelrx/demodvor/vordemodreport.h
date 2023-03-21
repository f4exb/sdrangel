///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_VORDEMODSCREPORT_H
#define INCLUDE_VORDEMODSCREPORT_H

#include <QObject>

#include "util/message.h"

class VORDemodReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportRadial : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getRadial() const { return m_radial; }
        float getRefMag() const { return m_refMag; }
        float getVarMag() const { return m_varMag; }

        static MsgReportRadial* create(float radial, float refMag, float varMag) {
            return new MsgReportRadial(radial, refMag, varMag);
        }

    private:
        float m_radial;
        float m_refMag;
        float m_varMag;

        MsgReportRadial(float radial, float refMag, float varMag) :
            Message(),
            m_radial(radial),
            m_refMag(refMag),
            m_varMag(varMag)
        {
        }
    };

public:
    VORDemodReport() {}
    ~VORDemodReport() {}
};

#endif // INCLUDE_VORDEMODSCREPORT_H
