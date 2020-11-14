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

#include "azel.h"

#include <cmath>

// Calculate cartesian distance between two points
double AzEl::cartDistance(const AzElPoint& a, const AzElPoint& b)
{
    double dx = b.m_cart.m_x - a.m_cart.m_x;
    double dy = b.m_cart.m_y - a.m_cart.m_y;
    double dz = b.m_cart.m_z - a.m_cart.m_z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

// Calculate vector difference then normalise the result
bool AzEl::normVectorDiff(const AzElCartesian& a, const AzElCartesian& b, AzElCartesian& n)
{
    n.m_x = b.m_x - a.m_x;
    n.m_y = b.m_y - a.m_y;
    n.m_z = b.m_z - a.m_z;
    double distance = std::sqrt(n.m_x*n.m_x + n.m_y*n.m_y + n.m_z*n.m_z);
    if (distance > 0.0f)
    {
        n.m_x = n.m_x / distance;
        n.m_y = n.m_y / distance;
        n.m_z = n.m_z / distance;
        return true;
    }
    else
    {
        return false;
    }
}

// Convert geodetic latitude (as given by GPS) to geocentric latitude (angle from centre of Earth between the point and equator)
// Both in radians.
// https://en.wikipedia.org/wiki/Latitude#Geocentric_latitude
double AzEl::geocentricLatitude(double latRad) const
{
    double e2 = 0.00669437999014;
    return std::atan((1.0 - e2) * std::tan(latRad));
}

// Earth radius for a given latitude, as it's not quite spherical
// http://en.wikipedia.org/wiki/Earth_radius
double AzEl::earthRadiusInMetres(double geodeticLatRad) const
{
   double equatorialRadius = 6378137.0;
   double polarRadius = 6356752.3;
   double cosLat = std::cos(geodeticLatRad);
   double sinLat = std::sin(geodeticLatRad);
   double t1 = equatorialRadius * equatorialRadius * cosLat;
   double t2 = polarRadius * polarRadius * sinLat;
   double t3 = equatorialRadius * cosLat;
   double t4 = polarRadius * sinLat;
   return std::sqrt((t1*t1 + t2*t2)/(t3*t3 + t4*t4));
}

// Convert spherical coordinate to cartesian. Also calculates radius and a normal vector
void AzEl::sphericalToCartesian(AzElPoint& point)
{
    // First calculate cartesian coords for point on Earth's surface
    double latRad = point.m_spherical.m_latitude * M_PI/180.0;
    double longRad = point.m_spherical.m_longitude * M_PI/180.0;
    point.m_radius = earthRadiusInMetres(latRad);
    double clat = geocentricLatitude(latRad);
    double cosLong = cos(longRad);
    double sinLong = sin(longRad);
    double cosLat = cos(clat);
    double sinLat = sin(clat);

    point.m_cart.m_x = point.m_radius * cosLong * cosLat;
    point.m_cart.m_y = point.m_radius * sinLong * cosLat;
    point.m_cart.m_z = point.m_radius * sinLat;

    // Calculate normal vector at surface
    double cosGLat = std::cos(latRad);
    double sinGLat = std::sin(latRad);

    point.m_norm.m_x = cosGLat * cosLong;
    point.m_norm.m_y = cosGLat * sinLong;
    point.m_norm.m_z = sinGLat;

    // Add altitude along normal vector
    point.m_cart.m_x += point.m_spherical.m_altitude * point.m_norm.m_x;
    point.m_cart.m_y += point.m_spherical.m_altitude * point.m_norm.m_y;
    point.m_cart.m_z += point.m_spherical.m_altitude * point.m_norm.m_z;
}

// Calculate azimuth of target from location
void AzEl::calcAzimuth()
{
    AzElPoint bRot;

    // Rotate so location is at lat=0, long=0
    bRot.m_spherical.m_latitude = m_target.m_spherical.m_latitude;
    bRot.m_spherical.m_longitude = m_target.m_spherical.m_longitude - m_location.m_spherical.m_longitude;
    bRot.m_spherical.m_altitude = m_target.m_spherical.m_altitude;
    sphericalToCartesian(bRot);

    double aLat = geocentricLatitude(-m_location.m_spherical.m_latitude * M_PI / 180.0);
    double aCos = std::cos(aLat);
    double aSin = std::sin(aLat);

    //double bx = (bRot.m_cart.m_x * aCos) - (bRot.m_cart.m_z * aSin);
    double by = bRot.m_cart.m_y;
    double bz = (bRot.m_cart.m_x * aSin) + (bRot.m_cart.m_z * aCos);

    if (bz*bz + by*by > 1e-6)
    {
        double theta = std::atan2(bz, by) * 180.0 / M_PI;
        m_azimuth = 90.0 - theta;
        if (m_azimuth < 0.0)
            m_azimuth += 360.0;
        else if (m_azimuth > 360.0)
            m_azimuth -= 360.0;
    }
    else
        m_azimuth = 0.0;
}

// Calculate elevation of target from location
void AzEl::calcElevation()
{
    AzElCartesian bma;
    if (normVectorDiff(m_location.m_cart, m_target.m_cart, bma))
    {
        m_elevation = 90.0 - (180.0/M_PI) * std::acos(bma.m_x * m_location.m_norm.m_x
                                                    + bma.m_y * m_location.m_norm.m_y
                                                    + bma.m_z * m_location.m_norm.m_z);
    }
    else
        m_elevation = 0.0;
}
