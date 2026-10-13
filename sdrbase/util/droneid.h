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

#ifndef INCLUDE_DRONEID_H
#define INCLUDE_DRONEID_H

#include <cstdint>

#include <QString>
#include <QByteArray>
#include <QDateTime>

#include "export.h"

// DJI DroneID message, as broadcast by OcuSync drones.
// Flight information message layout from "Drone Security and the Mysterious Case of DJI's DroneID", NDSS 2023, Fig. 13.
// Messages start with a length byte (excluding first 3 bytes) and type, and end with a CRC16.
class SDRBASE_API DroneIDMessage {
public:

    enum MessageType {
        FlightInformation = 0x10,       // Position, velocity, pilot and home position
        FlightPurpose = 0x11            // Serial number and (optional) flight purpose
    };

    static constexpr int m_flightInfoLength = 91;   // Bytes, including CRC

    // state_info flags (provisional, from the Wi-Fi beacon variant of DroneID)
    enum StateFlags {
        SerialValid = 0x0001,
        PrivacyEnabled = 0x0002,
        HomeValid = 0x0004,
        UUIDValid = 0x0008,
        MotorsOn = 0x0010,
        InAir = 0x0020,
        GPSValid = 0x0040,
        AltitudeValid = 0x0080,
        HeightValid = 0x0100,
        HorizontalVelocityValid = 0x0200,
        VerticalVelocityValid = 0x0400
    };

    uint8_t m_packetLength;
    uint8_t m_type;
    uint8_t m_version;
    uint16_t m_sequence;
    uint16_t m_stateInfo;
    QString m_serial;
    double m_latitude;                  // Degrees
    double m_longitude;                 // Degrees
    float m_altitude;                   // Metres above WGS84 ellipsoid (GPS height, not MSL)
    float m_height;                     // Metres above take-off point
    float m_vNorth;                     // m/s
    float m_vEast;                      // m/s
    float m_vUp;                        // m/s
    float m_yaw;                        // Degrees
    QDateTime m_gpsTime;                // From the pilot's phone
    double m_pilotLatitude;             // Degrees
    double m_pilotLongitude;            // Degrees
    double m_homeLatitude;              // Degrees
    double m_homeLongitude;             // Degrees
    uint8_t m_productType;
    QString m_uuid;
    QByteArray m_purpose;               // Remaining bytes of flight purpose message
    uint16_t m_crc;                     // CRC in message
    bool m_crcValid;                    // Whether CRC in message matches calculated

    DroneIDMessage() :
        m_packetLength(0),
        m_type(0),
        m_version(0),
        m_sequence(0),
        m_stateInfo(0),
        m_latitude(0.0),
        m_longitude(0.0),
        m_altitude(0.0f),
        m_height(0.0f),
        m_vNorth(0.0f),
        m_vEast(0.0f),
        m_vUp(0.0f),
        m_yaw(0.0f),
        m_pilotLatitude(0.0),
        m_pilotLongitude(0.0),
        m_homeLatitude(0.0),
        m_homeLongitude(0.0),
        m_productType(0),
        m_crc(0),
        m_crcValid(false)
    {}

    // Decode a DroneID message. Returns nullptr if bytes is too short or doesn't look like a DroneID message.
    // Message is still returned if CRC is invalid, as it is protected by the PHY layer CRC.
    static DroneIDMessage* decode(const QByteArray& bytes);

    // Length of message at start of bytes, including CRC, or -1 if invalid
    static int messageLength(const QByteArray& bytes);

    bool isFlightInformation() const { return m_type == FlightInformation; }

    // Calculate CRC for first length-2 bytes of message
    static uint16_t calcCRC(const uint8_t *bytes, int length);

    bool hasPosition() const;
    bool hasPilotPosition() const;
    bool hasHomePosition() const;
    float getSpeed() const;              // Horizontal ground speed in m/s
    float getCourse() const;             // Ground track in degrees from velocity
    QString getProductTypeName() const { return productTypeName(m_productType); }
    QString getStateInfoString() const;

    static QString productTypeName(int productType);

    QString toString() const;

private:
    static bool validPosition(double latitude, double longitude);

    // Little-endian field access
    static uint16_t getU16(const uint8_t *p);
    static int16_t getS16(const uint8_t *p);
    static int32_t getS32(const uint8_t *p);
    static uint64_t getU64(const uint8_t *p);
    static double toDegrees(int32_t v);
    static QString getString(const uint8_t *p, int maxLength);
};

#endif // INCLUDE_DRONEID_H
