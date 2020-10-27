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
    enum AzAlType {TARGET, ACTUAL};

    class MsgReportAzAl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getAzimuth() const { return m_azimuth; }
        float getElevation() const { return m_elevation; }
        AzAlType getType() const { return m_type; }

        static MsgReportAzAl* create(float azimuth, float elevation, AzAlType type)
        {
            return new MsgReportAzAl(azimuth, elevation, type);
        }

    private:
        float m_azimuth;
        float m_elevation;
        AzAlType m_type;

        MsgReportAzAl(float azimuth, float elevation, AzAlType type) :
            Message(),
            m_azimuth(azimuth),
            m_elevation(elevation),
            m_type(type)
        {
        }
    };

public:
    GS232ControllerReport() {}
    ~GS232ControllerReport() {}
};

#endif // INCLUDE_FEATURE_GS232CONTROLLERREPORT_H_
