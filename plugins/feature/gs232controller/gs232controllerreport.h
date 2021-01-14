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

#ifndef INCLUDE_FEATURE_GS232CONTROLLERREPORT_H_
#define INCLUDE_FEATURE_GS232CONTROLLERREPORT_H_

#include <QObject>

#include "util/message.h"

class GS232ControllerReport : public QObject
{
    Q_OBJECT
public:

    class MsgReportAzAl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getAzimuth() const { return m_azimuth; }
        float getElevation() const { return m_elevation; }

        static MsgReportAzAl* create(float azimuth, float elevation)
        {
            return new MsgReportAzAl(azimuth, elevation);
        }

    private:
        float m_azimuth;
        float m_elevation;

        MsgReportAzAl(float azimuth, float elevation) :
            Message(),
            m_azimuth(azimuth),
            m_elevation(elevation)
        {
        }
    };

public:
    GS232ControllerReport() {}
    ~GS232ControllerReport() {}
};

#endif // INCLUDE_FEATURE_GS232CONTROLLERREPORT_H_
