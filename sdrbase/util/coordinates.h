///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_COORDINATES_H
#define INCLUDE_COORDINATES_H

#include "export.h"

#include <QDateTime>
#include <QVector3D>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>

// Functions for transformations between geodetic and ECEF coordinates
class SDRBASE_API Coordinates {

public:

    static QVector3D geodeticToECEF(double longitude, double latitude, double height=0.0);
    static QVector3D geodeticRadiansToECEF(double longitude, double latitude, double height=0.0);
    static QMatrix4x4 eastNorthUpToECEF(QVector3D origin);
    static void ecefToGeodetic(double x, double y, double z, double &latitude, double &longitude, double &height);
    static void ecefVelToSpeedHeading(double latitude, double longitude,
                                      double velX, double velY, double velZ,
                                      double &speed, double &verticalRate, double &heading);
    static QQuaternion orientation(double longitude, double latitude, double altitude, double heading, double pitch, double roll);

protected:

    static QVector3D scaleToGeodeticSurface(QVector3D cartesian, QVector3D oneOverRadii, QVector3D oneOverRadiiSquared);
    static QVector3D normalized(QVector3D vec);
    static QQuaternion fromHeadingPitchRoll(double heading, double pitch, double roll);
    static QQuaternion fromRotation(QMatrix3x3 mat);

};

#endif // INCLUDE_COORDINATES_H
