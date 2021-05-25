///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_STARTRACKERREPORT_H_
#define INCLUDE_FEATURE_STARTRACKERREPORT_H_

#include <QObject>
#include <QList>

#include "util/message.h"
#include "util/astronomy.h"

class StarTrackerReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportAzAl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getAzimuth() const { return m_azimuth; }
        double getElevation() const { return m_elevation; }

        static MsgReportAzAl* create(double azimuth, double elevation)
        {
            return new MsgReportAzAl(azimuth, elevation);
        }

    private:
        double m_azimuth;
        double m_elevation;

        MsgReportAzAl(double azimuth, double elevation) :
            Message(),
            m_azimuth(azimuth),
            m_elevation(elevation)
        {
        }
    };

    class MsgReportRADec : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getRA() const { return m_ra; }
        double getDec() const { return m_dec; }

        static MsgReportRADec* create(double ra, double dec)
        {
            return new MsgReportRADec(ra, dec);
        }

    private:
        double m_ra;
        double m_dec;

        MsgReportRADec(double ra, double dec) :
            Message(),
            m_ra(ra),
            m_dec(dec)
        {
        }
    };

public:
    StarTrackerReport() {}
    ~StarTrackerReport() {}
};

#endif // INCLUDE_FEATURE_STARTRACKERREPORT_H_
