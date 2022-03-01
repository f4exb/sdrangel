///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011-2020 Cesium Contributors                                   //
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

#include "coordinates.h"
#include "units.h"

// Scale cartesian position on to surface of ellipsoid
QVector3D Coordinates::scaleToGeodeticSurface(QVector3D cartesian, QVector3D oneOverRadii, QVector3D oneOverRadiiSquared)
{
    float centerToleranceSquared = 0.1;

    double x2 = cartesian.x() * cartesian.x() * oneOverRadii.x() * oneOverRadii.x();
    double y2 = cartesian.y() * cartesian.y() * oneOverRadii.y() * oneOverRadii.y();
    double z2 = cartesian.z() * cartesian.z() * oneOverRadii.z() * oneOverRadii.z();

    double squaredNorm = x2 + y2 + z2;
    double ratio = sqrt(1.0 / squaredNorm);

    QVector3D intersection = cartesian * ratio;

    if (squaredNorm < centerToleranceSquared) {
        return intersection;
    }

    QVector3D gradient(
        intersection.x() * oneOverRadiiSquared.x() * 2.0,
        intersection.y() * oneOverRadiiSquared.y() * 2.0,
        intersection.z() * oneOverRadiiSquared.z() * 2.0
        );

    double lambda = ((1.0 - ratio) * cartesian.length()) / (0.5 * gradient.length());

    double correction = 0.0;
    double func;
    double denominator;
    double xMultiplier;
    double yMultiplier;
    double zMultiplier;
    double xMultiplier2;
    double yMultiplier2;
    double zMultiplier2;
    double xMultiplier3;
    double yMultiplier3;
    double zMultiplier3;

    do
    {
        lambda -= correction;

        xMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquared.x());
        yMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquared.y());
        zMultiplier = 1.0 / (1.0 + lambda * oneOverRadiiSquared.z());

        xMultiplier2 = xMultiplier * xMultiplier;
        yMultiplier2 = yMultiplier * yMultiplier;
        zMultiplier2 = zMultiplier * zMultiplier;

        xMultiplier3 = xMultiplier2 * xMultiplier;
        yMultiplier3 = yMultiplier2 * yMultiplier;
        zMultiplier3 = zMultiplier2 * zMultiplier;

        func = x2 * xMultiplier2 + y2 * yMultiplier2 + z2 * zMultiplier2 - 1.0;

        denominator =
          x2 * xMultiplier3 * oneOverRadiiSquared.x() +
          y2 * yMultiplier3 * oneOverRadiiSquared.y() +
          z2 * zMultiplier3 * oneOverRadiiSquared.z();

        double derivative = -2.0 * denominator;

        correction = func / derivative;
    }
    while (abs(func) > 0.000000000001);

    QVector3D result(
        cartesian.x() * xMultiplier,
        cartesian.y() * yMultiplier,
        cartesian.z() * zMultiplier
        );
    return result;
}

// QVector3D.normalized doesn't work with small numbers
QVector3D Coordinates::normalized(QVector3D vec)
{
    QVector3D result;
    float magnitude = vec.length();
    result.setX(vec.x() / magnitude);
    result.setY(vec.y() / magnitude);
    result.setZ(vec.z() / magnitude);
    return result;
}

// Convert ECEF position to geodetic coordinates
void Coordinates::ecefToGeodetic(double x, double y, double z, double &latitude, double &longitude, double &height)
{
    QVector3D wgs84OneOverRadix(1.0 / 6378137.0,
                                1.0 / 6378137.0,
                                1.0 / 6356752.3142451793);
    QVector3D wgs84OneOverRadiiSquared(1.0 / (6378137.0 * 6378137.0),
                                        1.0 / (6378137.0 * 6378137.0),
                                        1.0 / (6356752.3142451793 * 6356752.3142451793));

    QVector3D cartesian(x, y, z);

    QVector3D p = scaleToGeodeticSurface(cartesian, wgs84OneOverRadix, wgs84OneOverRadiiSquared);

    QVector3D n = p * wgs84OneOverRadiiSquared;
    n = normalized(n);

    QVector3D h = cartesian - p;

    longitude = atan2(n.y(), n.x());
    latitude = asin(n.z());

    longitude = Units::radiansToDegrees(longitude);
    latitude = Units::radiansToDegrees(latitude);

    double t = QVector3D::dotProduct(h, cartesian);
    double sign = t >= 0.0 ? 1.0 : 0.0;
    height = sign * h.length();
}

// Convert ECEF velocity to speed and heading
void Coordinates::ecefVelToSpeedHeading(double latitude, double longitude,
                                        double velX, double velY, double velZ,
                                        double &speed, double &verticalRate, double &heading)
{
    if ((velX == 0.0) && (velY == 0.0) && (velZ == 0.0))
    {
        speed = 0.0;
        heading = 0.0;
        verticalRate = 0.0;
        return;
    }

    double latRad = Units::degreesToRadians(latitude);
    double lonRad = Units::degreesToRadians(longitude);

    double sinLat = sin(latRad);
    double cosLat = cos(latRad);
    double sinLon = sin(lonRad);
    double cosLon = cos(lonRad);

    double velEast = -velX * sinLon + velY * cosLon;
    double velNorth = -velX * sinLat * cosLon - velY * sinLat * sinLon + velZ * cosLat;
    double velUp = velX * cosLat * cosLon + velY * cosLat * sinLon + velZ * sinLat;

    speed = sqrt(velNorth * velNorth + velEast * velEast);
    verticalRate = velUp;

    double headingRad = atan2(velEast, velNorth);
    heading = Units::radiansToDegrees(headingRad);
    if (heading < 0.0) {
        heading += 360.0;
    } else if (heading >= 360.0) {
        heading -= 360.0;
    }
}

// Convert a position specified in longitude, latitude in degrees and height in metres above WGS84 ellipsoid in to
// Earth Centered Earth Fixed frame cartesian coordinates
// See Cesium.Cartesian3.fromDegrees
QVector3D Coordinates::geodeticToECEF(double longitude, double latitude, double height)
{
    return geodeticRadiansToECEF(Units::degreesToRadians(longitude), Units::degreesToRadians(latitude), height);
}

// FIXME: QVector3D is only float!
// See Cesium.Cartesian3.fromRadians
QVector3D Coordinates::geodeticRadiansToECEF(double longitude, double latitude, double height)
{
    QVector3D wgs84RadiiSquared(6378137.0 * 6378137.0, 6378137.0 * 6378137.0, 6356752.3142451793 * 6356752.3142451793);

    double cosLatitude = cos(latitude);
    QVector3D n;
    n.setX(cosLatitude * cos(longitude));
    n.setY(cosLatitude * sin(longitude));
    n.setZ(sin(latitude));
    n.normalize();
    QVector3D k;
    k = wgs84RadiiSquared * n;
    double gamma = sqrt(QVector3D::dotProduct(n, k));
    k = k / gamma;
    n = n * height;
    return k + n;
}

// Convert heading, pitch and roll in degrees to a quaternoin
// See: Cesium.Quaternion.fromHeadingPitchRoll
QQuaternion Coordinates::fromHeadingPitchRoll(double heading, double pitch, double roll)
{
    QVector3D xAxis(1, 0, 0);
    QVector3D yAxis(0, 1, 0);
    QVector3D zAxis(0, 0, 1);

    QQuaternion rollQ = QQuaternion::fromAxisAndAngle(xAxis, roll);

    QQuaternion pitchQ = QQuaternion::fromAxisAndAngle(yAxis, -pitch);

    QQuaternion headingQ = QQuaternion::fromAxisAndAngle(zAxis, -heading);

    QQuaternion temp = rollQ * pitchQ;

    return headingQ * temp;
}

// Calculate a transformation matrix from a East, North, Up frame at the given position to Earth Centered Earth Fixed frame
// See: Cesium.Transforms.eastNorthUpToFixedFrame
QMatrix4x4 Coordinates::eastNorthUpToECEF(QVector3D origin)
{
    // TODO: Handle special case at centre of earth and poles
    QVector3D up = origin.normalized();
    QVector3D east(-origin.y(), origin.x(), 0.0);
    east.normalize();
    QVector3D north = QVector3D::crossProduct(up, east);
    QMatrix4x4 result(
        east.x(), north.x(), up.x(), origin.x(),
        east.y(), north.y(), up.y(), origin.y(),
        east.z(), north.z(), up.z(), origin.z(),
        0.0, 0.0, 0.0, 1.0
    );
    return result;
}

// Convert 3x3 rotation matrix to a quaternoin
// Although there is a method for this in Qt: QQuaternion::fromRotationMatrix, it seems to
// result in different signs, so the following is based on Cesium code
QQuaternion Coordinates::fromRotation(QMatrix3x3 mat)
{
    QQuaternion q;

    double trace = mat(0, 0) + mat(1, 1) + mat(2, 2);

    if (trace > 0.0)
    {
        double root = sqrt(trace + 1.0);
        q.setScalar(0.5 * root);
        root = 0.5 / root;

        q.setX((mat(2,1) - mat(1,2)) * root);
        q.setY((mat(0,2) - mat(2,0)) * root);
        q.setZ((mat(1,0) - mat(0,1)) * root);
    }
    else
    {
        double next[] = {1, 2, 0};
        int i = 0;
        if (mat(1,1) > mat(0,0)) {
            i = 1;
        }
        if (mat(2,2) > mat(0,0) && mat(2,2) > mat(1,1)) {
            i = 2;
        }
        int j = next[i];
        int k = next[j];

        double root = sqrt(mat(i,i) - mat(j,j) - mat(k,k) + 1);
        double quat[] = {0.0, 0.0, 0.0};
        quat[i] = 0.5 * root;
        root = 0.5 / root;

        q.setScalar((mat(j,k) - mat(k,j)) * root);
        quat[j] = (mat(i,j) + mat(j,i)) * root;
        quat[k] = (mat(i,k) + mat(k,i)) * root;
        q.setX(-quat[0]);
        q.setY(-quat[1]);
        q.setZ(-quat[2]);
    }
    return q;
}

// Calculate orientation quaternion for a model (such as an aircraft) based on position and (HPR) heading, pitch and roll (in degrees)
// While Cesium supports specifying orientation as HPR, CZML doesn't currently. See https://github.com/CesiumGS/cesium/issues/5184
// CZML requires the orientation to be in the Earth Centered Earth Fixed (geocentric) reference frame (https://en.wikipedia.org/wiki/Local_tangent_plane_coordinates)
// The orientation therefore depends not only on HPR but also on position
//
// glTF uses a right-handed axis convention; that is, the cross product of right and forward yields up. glTF defines +Y as up, +Z as forward, and -X as right.
// Cesium.Quaternion.fromHeadingPitchRoll  Heading is the rotation about the negative z axis. Pitch is the rotation about the negative y axis. Roll is the rotation about the positive x axis.
QQuaternion Coordinates::orientation(double longitude, double latitude, double altitude, double heading, double pitch, double roll)
{
    // Forward direction for gltf models in Cesium seems to be Eastward, rather than Northward, so we adjust heading by -90 degrees
    heading = -90 + heading;

    // Convert position to Earth Centered Earth Fixed (ECEF) frame
    QVector3D positionECEF = geodeticToECEF(longitude, latitude, altitude);

    // Calculate matrix to transform from East, North, Up (ENU) frame to ECEF frame
    QMatrix4x4 enuToECEFTransform = eastNorthUpToECEF(positionECEF);

    // Calculate rotation based on HPR in ENU frame
    QQuaternion hprENU = fromHeadingPitchRoll(heading, pitch, roll);

    // Transform rotation from ENU to ECEF
    QMatrix3x3 hprENU3 = hprENU.toRotationMatrix();
    QMatrix4x4 hprENU4(hprENU3);
    QMatrix4x4 transform = enuToECEFTransform * hprENU4;

    // Convert from 4x4 matrix to 3x3 matrix then to a quaternion
    QQuaternion oq = fromRotation(transform.toGenericMatrix<3,3>());

    return oq;
}
