///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_AZ_EL_H
#define INCLUDE_AZ_EL_H

#include "export.h"

// Spherical coordinate
struct SDRBASE_API AzElSpherical
{
    double m_latitude;          // Degrees
    double m_longitude;         // Degrees
    double m_altitude;          // Metres
};

// Cartesian coordinate
struct SDRBASE_API AzElCartesian
{
    double m_x;                 // Metres
    double m_y;                 // Metres
    double m_z;                 // Metres
};

struct SDRBASE_API AzElPoint
{
    AzElSpherical m_spherical;  // Spherical coordinates
    AzElCartesian m_cart;       // Cartesian coordinates
    AzElCartesian m_norm;       // Normal vector on surface of sphere
    double m_radius;            // Radius from centre of sphere to the point
};


// Class to calculate azimuth, elevation and distance between two points specified
// by latitude, longitude and altitude
// See: https://doncross.net/geocalc
class SDRBASE_API AzEl
{
public:
    // Location is the point we are looking from
    void setLocation(double latitude, double longitude, double altitude)
    {
        m_location.m_spherical.m_latitude = latitude;
        m_location.m_spherical.m_longitude = longitude;
        m_location.m_spherical.m_altitude = altitude;
        sphericalToCartesian(m_location);
    }

    // Target is the point we are looking at
    void setTarget(double latiude, double longitude, double altitude)
    {
        m_target.m_spherical.m_latitude = latiude;
        m_target.m_spherical.m_longitude = longitude;
        m_target.m_spherical.m_altitude = altitude;
        sphericalToCartesian(m_target);
    }

    AzElSpherical getLocationSpherical()
    {
        return m_location.m_spherical;
    }

    AzElCartesian getLocationCartesian()
    {
        return m_location.m_cart;
    }

    AzElSpherical getTargetSpherical()
    {
        return m_target.m_spherical;
    }

    AzElCartesian getTargetCartesian()
    {
        return m_target.m_cart;
    }

    void calculate()
    {
        m_distance = cartDistance(m_location, m_target);
        calcAzimuth();
        calcElevation();
    }

    double getDistance()
    {
        return m_distance;
    }

    double getAzimuth()
    {
        return m_azimuth;
    }

    double getElevation()
    {
        return m_elevation;
    }

private:

    double cartDistance(const AzElPoint& a, const AzElPoint& b);
    bool normVectorDiff(const AzElCartesian& a, const AzElCartesian& b, AzElCartesian& n);
    double geocentricLatitude(double latRad) const;
    double earthRadiusInMetres(double geodeticLatRad) const;
    void sphericalToCartesian(AzElPoint& point);
    void calcAzimuth();
    void calcElevation();

    AzElPoint m_location;
    AzElPoint m_target;

    double m_azimuth;
    double m_elevation;
    double m_distance;

};

#endif
