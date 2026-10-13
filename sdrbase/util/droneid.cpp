///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <cmath>
#include <algorithm>

#include <QMap>
#include <QStringList>

#include "util/crc.h"

#include "droneid.h"

uint16_t DroneIDMessage::getU16(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

int16_t DroneIDMessage::getS16(const uint8_t *p)
{
    return (int16_t) getU16(p);
}

int32_t DroneIDMessage::getS32(const uint8_t *p)
{
    return (int32_t) ((uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24));
}

uint64_t DroneIDMessage::getU64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) {
        v = (v << 8) | p[i];
    }
    return v;
}

// Positions are transmitted in radians * 1e7
double DroneIDMessage::toDegrees(int32_t v)
{
    return (v / 1.0e7) * (180.0 / M_PI);
}

QString DroneIDMessage::getString(const uint8_t *p, int maxLength)
{
    int len = 0;
    while ((len < maxLength) && (p[len] != 0)) {
        len++;
    }
    return QString::fromLatin1((const char *) p, len).trimmed();
}

uint16_t DroneIDMessage::calcCRC(const uint8_t *bytes, int length)
{
    crc16droneid crc;
    crc.calculate(bytes, length);
    return crc.get();
}

int DroneIDMessage::messageLength(const QByteArray& bytes)
{
    if (bytes.size() < 3) {
        return -1;
    }
    int length = (uint8_t) bytes[0] + 3;
    if (length > bytes.size()) {
        return -1;
    }
    return length;
}

DroneIDMessage* DroneIDMessage::decode(const QByteArray& bytes)
{
    int length = messageLength(bytes);
    if (length < 0) {
        return nullptr;
    }

    const uint8_t *p = (const uint8_t *) bytes.data();
    uint8_t type = p[1];

    if (type == FlightInformation)
    {
        // Versions 1 to 3 seen
        if ((length < m_flightInfoLength) || (p[2] < 1) || (p[2] > 3)) {
            return nullptr;
        }
    }
    else if (type == FlightPurpose)
    {
        if (length < 2 + 16 + 2) {
            return nullptr;
        }
    }
    else
    {
        return nullptr;
    }

    DroneIDMessage *msg = new DroneIDMessage();

    msg->m_packetLength = p[0];
    msg->m_type = type;
    msg->m_crc = getU16(&p[length - 2]);
    msg->m_crcValid = calcCRC(p, length - 2) == msg->m_crc;

    if (type == FlightInformation)
    {
        msg->m_version = p[2];
        msg->m_sequence = getU16(&p[3]);
        msg->m_stateInfo = getU16(&p[5]);
        msg->m_serial = getString(&p[7], 16);
        msg->m_longitude = toDegrees(getS32(&p[23]));
        msg->m_latitude = toDegrees(getS32(&p[27]));
        // Units of altitude and height are not documented. Sources differ, so these need confirming with real data.
        msg->m_altitude = getS16(&p[31]);
        msg->m_height = getS16(&p[33]) / 10.0f;
        msg->m_vNorth = getS16(&p[35]) / 100.0f;
        msg->m_vEast = getS16(&p[37]) / 100.0f;
        msg->m_vUp = getS16(&p[39]) / 100.0f;
        msg->m_yaw = getS16(&p[41]) / 100.0f;
        uint64_t gpsTime = getU64(&p[43]);
        if (gpsTime != 0) {
            msg->m_gpsTime = QDateTime::fromMSecsSinceEpoch((qint64) gpsTime).toUTC();
        }
        msg->m_pilotLatitude = toDegrees(getS32(&p[51]));
        msg->m_pilotLongitude = toDegrees(getS32(&p[55]));
        msg->m_homeLongitude = toDegrees(getS32(&p[59]));
        msg->m_homeLatitude = toDegrees(getS32(&p[63]));
        msg->m_productType = p[67];
        int uuidLength = std::min((int) p[68], 20);
        msg->m_uuid = getString(&p[69], uuidLength);
    }
    else
    {
        msg->m_serial = getString(&p[2], 16);
        msg->m_purpose = bytes.mid(18, length - 18 - 2);
    }

    return msg;
}

bool DroneIDMessage::validPosition(double latitude, double longitude)
{
    return (latitude != 0.0) && (longitude != 0.0)
        && (std::abs(latitude) <= 90.0) && (std::abs(longitude) <= 180.0);
}

bool DroneIDMessage::hasPosition() const
{
    return validPosition(m_latitude, m_longitude);
}

bool DroneIDMessage::hasPilotPosition() const
{
    return validPosition(m_pilotLatitude, m_pilotLongitude);
}

bool DroneIDMessage::hasHomePosition() const
{
    return validPosition(m_homeLatitude, m_homeLongitude);
}

float DroneIDMessage::getSpeed() const
{
    return std::sqrt(m_vNorth * m_vNorth + m_vEast * m_vEast);
}

float DroneIDMessage::getCourse() const
{
    float course = std::atan2(m_vEast, m_vNorth) * (180.0f / (float) M_PI);
    if (course < 0.0f) {
        course += 360.0f;
    }
    return course;
}

QString DroneIDMessage::getStateInfoString() const
{
    QStringList flags;

    if (m_stateInfo & MotorsOn) {
        flags.append("Motors on");
    }
    if (m_stateInfo & InAir) {
        flags.append("In air");
    }
    if (m_stateInfo & GPSValid) {
        flags.append("GPS");
    }
    if (m_stateInfo & HomeValid) {
        flags.append("Home");
    }
    if (m_stateInfo & PrivacyEnabled) {
        flags.append("Privacy");
    }
    return flags.join(", ");
}

QString DroneIDMessage::productTypeName(int productType)
{
    // Codes from DroneSecurity, Kismet and others. Codes above 70 are less well verified.
    static const QMap<int, QString> names = {
        {1, "Inspire 1"},
        {2, "Phantom 3 Series"},
        {3, "Phantom 3 Series Pro"},
        {4, "Phantom 3 Std"},
        {5, "M100"},
        {6, "ACEONE"},
        {7, "WKM"},
        {8, "NAZA"},
        {9, "A2"},
        {10, "A3"},
        {11, "Phantom 4"},
        {12, "MG1"},
        {14, "M600"},
        {15, "Phantom 3 4K"},
        {16, "Mavic Pro"},
        {17, "Inspire 2"},
        {18, "Phantom 4 Pro"},
        {20, "N2"},
        {21, "Spark"},
        {23, "M600 Pro"},
        {24, "Mavic Air"},
        {25, "M200"},
        {26, "Phantom 4 Series"},
        {27, "Phantom 4 Adv"},
        {28, "M210"},
        {30, "M210 RTK"},
        {31, "A3 AG"},
        {32, "MG2"},
        {34, "MG1A"},
        {35, "Phantom 4 RTK"},
        {36, "Phantom 4 Pro V2.0"},
        {38, "MG1P"},
        {40, "MG1P-RTK"},
        {41, "Mavic 2"},
        {44, "M200 V2 Series"},
        {51, "Mavic 2 Enterprise"},
        {53, "Mavic Mini"},
        {58, "Mavic Air 2"},
        {59, "P4M"},
        {60, "M300 RTK"},
        {61, "DJI FPV"},
        {63, "Mini 2"},
        {64, "AGRAS T10"},
        {65, "AGRAS T30"},
        {66, "Air 2S"},
        {67, "M30"},
        {68, "Mavic 3"},
        {69, "Mavic 2 Enterprise Advanced"},
        {70, "Mini SE"},
        {73, "Mini 3 Pro"},
        {75, "Avata"},
        {77, "Mavic 3 Enterprise"},
        {82, "AGRAS T25"},
        {83, "AGRAS T50"},
        {86, "Mavic 3 Classic"},
        {87, "Mini 3"},
        {88, "Mini 2 SE"},
        {90, "Air 3"}
    };

    return names.value(productType, QString("Unknown (%1)").arg(productType));
}

QString DroneIDMessage::toString() const
{
    if (!isFlightInformation()) {
        return QString("Flight purpose %1").arg(m_serial);
    }
    return QString("%1 %2 seq %3 lat %4 lon %5 alt %6 height %7 pilot %8,%9 home %10,%11")
        .arg(getProductTypeName())
        .arg(m_serial)
        .arg(m_sequence)
        .arg(m_latitude, 0, 'f', 6)
        .arg(m_longitude, 0, 'f', 6)
        .arg(m_altitude)
        .arg(m_height)
        .arg(m_pilotLatitude, 0, 'f', 6)
        .arg(m_pilotLongitude, 0, 'f', 6)
        .arg(m_homeLatitude, 0, 'f', 6)
        .arg(m_homeLongitude, 0, 'f', 6);
}
