///////////////////////////////////////////////////////////////////////////////////
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

#include <QDebug>

#include "ais.h"

AISMessage::AISMessage(const QByteArray ba)
{
    // All AIS messages have these 3 fields in common
    m_id = (ba[0] & 0xff) >> 2 & 0x3f;
    m_repeatIndicator = ba[0] & 0x3;
    m_mmsi = ((ba[1] & 0xff) << 22) | ((ba[2] & 0xff) << 14) | ((ba[3] & 0xff) << 6) | ((ba[4] & 0xff) >> 2);
    m_bytes = ba;
}

QString AISMessage::toHex()
{
    return m_bytes.toHex();
}

// See: https://gpsd.gitlab.io/gpsd/AIVDM.html
QString AISMessage::toNMEA(const QByteArray bytes)
{
    QStringList nmeaSentences;

    // Max payload is ~61 chars  -> 366 bits
    int sentences = bytes.size() / 45 + 1;
    int sentence = 1;

    int bits = 8;
    int i = 0;
    while (i < bytes.size())
    {
        QStringList nmeaSentence;
        QStringList nmea;
        QStringList payload;

        nmea.append(QString("AIVDM,%1,%2,%3,,").arg(sentences).arg(sentence).arg(sentences > 1 ? "1" : ""));

        int maxPayload = 80 - 1 - nmea[0].length() - 5;

        // Encode message data in 6-bit ASCII
        while ((payload.size() < maxPayload) && (i < bytes.size()))
        {
            int c = 0;
            for (int j = 0; j < 6; j++)
            {
                if (i < bytes.size()) {
                    c = (c << 1) | ((bytes[i] >> (bits - 1)) & 0x1);
                } else {
                    c = (c << 1);
                }
                bits--;
                if (bits == 0)
                {
                    i++;
                    bits = 8;
                }
            }
            if (c >= 40) {
                c += 56;
            } else {
                c += 48;
            }
            payload.append(QChar(c));
        }

        nmea.append(payload);
        nmea.append(QString(",%1").arg((i == bytes.size()) ? (8 - bits) : 0)); // Number of filler bits to ignore

        // Calculate checksum
        QString nmeaProtected = nmea.join("");
        int checksum = AISMessage::nmeaChecksum(nmeaProtected);

        // Construct complete sentence with leading ! and trailing checksum
        nmeaSentence.append("!");
        nmeaSentence.append(nmeaProtected);
        nmeaSentence.append(QString("*%1").arg(checksum, 2, 16, QChar('0')));

        nmeaSentences.append(nmeaSentence.join(""));
        sentence++;
    }

    return nmeaSentences.join("\r\n").append("\r\n");   // NMEA-0183 requires CR and LF
}

QString AISMessage::toNMEA()
{
    return AISMessage::toNMEA(m_bytes);
}

qint8 AISMessage::nmeaChecksum(QString string)
{
    qint8 checksum = 0;

    for (int i = 0; i < string.length(); i++)
    {
        qint8 c = (qint8)string[i].toLatin1();
        checksum ^= c;
    }

    return checksum;
}

// Type as in message 5 and 19
QString AISMessage::typeToString(quint8 type)
{
    if (type == 0) {
        return "N/A";
    } else if ((type >= 100) && (type < 199)) {
        return "Preserved for regional use";
    } else if (type >= 200) {
        return "Preserved for future use";
    } else if ((type >= 50) && (type <= 59)) {
        const QStringList specialCrafts = {
            "Pilot vessel",
            "Search and rescue vessel",
            "Tug",
            "Port tender",
            "Anti-pollution vessel",
            "Law enforcement vessel",
            "Spare (56)",
            "Spare (57)",
            "Medical transport",
            "Ships and aircraft of States not parties to an armed conflict"
        };
        return specialCrafts[type-50];
    } else {
        int firstDigit = type / 10;
        int secondDigit = type % 10;

        const QStringList shipType = {
            "0",
            "Reserved (1)",
            "WIG",
            "Vessel",
            "HSC",
            "5",
            "Passenger",
            "Cargo",
            "Tanker",
            "Other"
        };
        const QStringList activity = {
            "Fishing",
            "Towing",
            "Towing",
            "Dredging or underwater operations",
            "Diving operations",
            "Military operations",
            "Sailing",
            "Pleasure craft",
            "Reserved (8)",
            "Reserved (9)"
        };

        if (firstDigit == 3) {
            return shipType[firstDigit] + " - " + activity[secondDigit];
        } else {
            return shipType[firstDigit];
        }
    }
}

AISMessage* AISMessage::decode(const QByteArray ba)
{
    int id = (ba[0] >> 2) & 0x3f;

    if ((id == 1) || (id == 2) || (id == 3)) {
        return new AISPositionReport(ba);
    } else if ((id == 4) || (id == 11)) {
        return new AISBaseStationReport(ba);
    } else if (id == 5) {
        return new AISShipStaticAndVoyageData(ba);
    } else if (id == 6) {
        return new AISBinaryMessage(ba);
    } else if (id == 7) {
        return new AISBinaryAck(ba);
    } else if (id == 8) {
        return new AISBinaryBroadcast(ba);
    } else if (id == 9) {
        return new AISSARAircraftPositionReport(ba);
    } else if (id == 10) {
        return new AISUTCInquiry(ba);
    } else if (id == 12) {
        return new AISSafetyMessage(ba);
    } else if (id == 13) {
        return new AISSafetyAck(ba);
    } else if (id == 14) {
        return new AISSafetyBroadcast(ba);
    } else if (id == 15) {
        return new AISInterrogation(ba);
    } else if (id == 16) {
        return new AISAssignedModeCommand(ba);
    } else if (id == 17) {
        return new AISGNSSBroadcast(ba);
    } else if (id == 18) {
        return new AISStandardClassBPositionReport(ba);
    } else if (id == 19) {
        return new AISExtendedClassBPositionReport(ba);
    } else if (id == 20) {
        return new AISDatalinkManagement(ba);
    } else if (id == 21) {
        return new AISAidsToNavigationReport(ba);
    } else if (id == 22) {
        return new AISChannelManagement(ba);
    } else if (id == 23) {
        return new AISGroupAssignment(ba);
    } else if (id == 24) {
        return new AISStaticDataReport(ba);
    } else if (id == 25) {
        return new AISSingleSlotBinaryMessage(ba);
    } else if (id == 26) {
        return new AISMultipleSlotBinaryMessage(ba);
    } else if (id == 27) {
        return new AISLongRangePositionReport(ba);
    } else {
        return new AISUnknownMessageID(ba);
    }
}

// Extract 6-bit ASCII string
QString AISMessage::getString(const QByteArray ba, int byteIdx, int bitsLeft, int chars)
{
    QString s;
    for (int i = 0; i < chars; i++)
    {
        // Extract 6-bits
        int c = 0;
        for (int j = 0; j < 6; j++)
        {
            c = (c << 1) | ((ba[byteIdx] >> (bitsLeft - 1)) & 0x1);
            bitsLeft--;
            if (bitsLeft == 0)
            {
                byteIdx++;
                bitsLeft = 8;
            }
        }
        // Map from 6-bit to 8-bit ASCII
        if (c < 32) {
            c |= 0x40;
        }
        s.append(QChar(c));
    }
    // Remove leading/trailing spaces
    s = s.trimmed();
    // Remave @s, which indiciate no character
    while (s.endsWith("@")) {
        s = s.left(s.length() - 1);
    }
    while (s.startsWith("@")) {
        s = s.mid(1);
    }
    return s;
}

AISPositionReport::AISPositionReport(QByteArray ba) :
    AISMessage(ba)
{
    m_status = ((ba[4] & 0x3) << 2) | ((ba[5] >> 6) & 0x3);

    int rateOfTurn = ((ba[5] << 2) & 0xfc) | ((ba[6] >> 6) & 0x3);
    if (rateOfTurn == 127) {
        m_rateOfTurn = 720.0f;
    } else if (rateOfTurn == -127) {
        m_rateOfTurn = -720.0f;
    } else {
        m_rateOfTurn = (rateOfTurn / 4.733f) * (rateOfTurn / 4.733f);
    }
    m_rateOfTurnAvailable = rateOfTurn != 0x80;

    int sog = ((ba[6] & 0x3f) << 4) | ((ba[7] >> 4) & 0xf);
    m_speedOverGroundAvailable = sog != 1023;
    m_speedOverGround = sog * 0.1f;

    m_positionAccuracy = (ba[7] >> 3) & 0x1;

    int32_t longitude = ((ba[7] & 0x7) << 25) | ((ba[8] & 0xff) << 17) | ((ba[9] & 0xff) << 9) | ((ba[10] & 0xff) << 1) | ((ba[11] >> 7) & 1);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[11] & 0x7f) << 20) | ((ba[12] & 0xff) << 12) | ((ba[13] & 0xff) << 4) | ((ba[14] >> 4) & 0x4);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;

    int cog = ((ba[14] & 0xf) << 8) | (ba[15] & 0xff);
    m_courseAvailable = cog != 3600;
    m_course = cog * 0.1f;

    m_heading = ((ba[16] & 0xff) << 1) | ((ba[17] >> 7) & 0x1);
    m_headingAvailable = m_heading != 511;

    m_timeStamp = (ba[17] >> 1) & 0x3f;

    m_specialManoeuvre = ((ba[17] & 0x1) << 1) | ((ba[18] >> 7) & 0x1);
}

QString AISPositionReport::getStatusString(int status)
{
    const QStringList statuses = {
        "Under way using engine",
        "At anchor",
        "Not under command",
        "Restricted manoeuvrability",
        "Constrained by her draught",
        "Moored",
        "Aground",
        "Engaged in fishing",
        "Under way sailing",
        "Reserved for future amendment of navigational status for ships carrying DG, HS, or MP, or IMO hazard or pollutant category C (HSC)",
        "Reserved for future amendment of navigational status for carrying DG, HS or MP, or IMO hazard or pollutant category A (WIG)",
        "Reserved for future use",
        "Reserved for future use",
        "Reserved for future use",
        "Reserved for future use",
        "Not defined"
        };
   return statuses[status];
}

QString AISPositionReport::toString()
{
    return QString("Lat: %1%6 Lon: %2%6 Speed: %3 knts Course: %4%6 Status: %5")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_speedOverGround)
                .arg(m_course)
                .arg(AISPositionReport::getStatusString(m_status))
                .arg(QChar(0xb0));
}

AISBaseStationReport::AISBaseStationReport(QByteArray ba) :
    AISMessage(ba)
{
    int year = ((ba[4] & 0x3) << 12) | ((ba[5] & 0xff) << 4) | ((ba[6] >> 4) & 0xf);
    int month = ba[6] & 0xf;
    int day = ((ba[7] >> 3) & 0x1f);
    int hour = ((ba[7] & 0x7) << 2) | ((ba[8] >> 6) & 0x3);
    int minute = ba[8] & 0x3f;
    int second = (ba[9] >> 2) & 0x3f;
    m_utc = QDateTime(QDate(year, month, day), QTime(hour, minute, second), Qt::UTC);

    m_positionAccuracy = (ba[9] >> 1) & 0x1;

    int32_t longitude = ((ba[9] & 0x1) << 27) | ((ba[10] & 0xff) << 19) | ((ba[11] & 0xff) << 11) | ((ba[12] & 0xff) << 3) | ((ba[13] >> 5) & 0x7);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[13] & 0x1f) << 22) | ((ba[14] & 0xff) << 14) | ((ba[15] & 0xff) << 6) | ((ba[16] >> 2) & 0x3f);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;
}

QString AISBaseStationReport::toString()
{
    return QString("Lat: %1%3 Lon: %2%3 %4")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(QChar(0xb0))
                .arg(m_utc.toString());
}

AISShipStaticAndVoyageData::AISShipStaticAndVoyageData(QByteArray ba) :
    AISMessage(ba)
{
    m_version = ba[4] & 0x3;
    m_imo = ((ba[5] & 0xff) << 22) | ((ba[6] & 0xff) << 14) | ((ba[7] & 0xff) << 6) | ((ba[8] >> 2) & 0x3f);
    m_callsign = AISMessage::getString(ba, 8, 2, 7);
    m_name = AISMessage::getString(ba, 14, 8, 20);
    m_type = ba[29] & 0xff;
    m_dimension = ((ba[30] & 0xff) << 22) | ((ba[31] & 0xff) << 14) | ((ba[32] & 0xff) << 6) | ((ba[33] >> 2) & 0x3f);
    m_a = (m_dimension >> 21) & 0x1ff;
    m_b = (m_dimension >> 12) & 0x1ff;
    m_c = (m_dimension >> 6) & 0x3f;
    m_d = m_dimension & 0x3f;
    m_positionFixing = ((ba[33] & 0x3) << 2) | ((ba[34] >> 6) & 0x3);
    m_eta = ((ba[34] & 0x3f) << 14) | ((ba[35] & 0xff) << 6) | ((ba[36] >> 2) & 0x3f);
    m_draught = ((ba[36] & 0x3) << 6) | ((ba[37] >> 2) & 0x3f);
    m_destination = AISMessage::getString(ba, 37, 2, 20);
}

QString AISShipStaticAndVoyageData::toString()
{
    return QString("IMO: %1 Callsign: %2 Name: %3 Type: %4 Destination: %5")
                .arg(m_imo == 0 ? "N/A" : QString::number(m_imo))
                .arg(m_callsign)
                .arg(m_name)
                .arg(AISMessage::typeToString(m_type))
                .arg(m_destination);
}

AISBinaryMessage::AISBinaryMessage(QByteArray ba) :
    AISMessage(ba)
{
    m_sequenceNumber = ba[4] & 0x3;
    m_destinationId = ((ba[5] & 0xff) << 22) | ((ba[6] & 0xff) << 14) | ((ba[7] & 0xff) << 6) | ((ba[8] >> 2) & 0x3f);
    m_retransmitFlag = (ba[8] >> 1) & 0x1;
}

QString AISBinaryMessage::toString()
{
    return QString("Seq No: %1 Destination: %2 Retransmit: %3")
                .arg(m_sequenceNumber)
                .arg(m_destinationId)
                .arg(m_retransmitFlag);
}


AISBinaryAck::AISBinaryAck(QByteArray ba) :
    AISMessage(ba)
{
}

AISBinaryBroadcast::AISBinaryBroadcast(QByteArray ba) :
    AISMessage(ba)
{
}

AISSARAircraftPositionReport::AISSARAircraftPositionReport(QByteArray ba) :
    AISMessage(ba)
{
    m_altitude = ((ba[4] & 0x3) << 10) | ((ba[5] & 0xff) << 2) | ((ba[6] >> 6) & 0x3);
    m_altitudeAvailable = m_altitude != 4095;

    int sog = ((ba[6] & 0x3f) << 4) | ((ba[7] >> 4) & 0xf);
    m_speedOverGroundAvailable = sog != 1023;
    m_speedOverGround = sog * 0.1f;

    m_positionAccuracy = (ba[7] >> 3) & 0x1;

    int32_t longitude = ((ba[7] & 0x7) << 25) | ((ba[8] & 0xff) << 17) | ((ba[9] & 0xff) << 9) | ((ba[10] & 0xff) << 1) | ((ba[11] >> 7) & 1);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[11] & 0x7f) << 20) | ((ba[12] & 0xff) << 12) | ((ba[13] & 0xff) << 4) | ((ba[14] >> 4) & 0x4);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;

    int cog = ((ba[14] & 0xf) << 8) | (ba[15] & 0xff);
    m_courseAvailable = cog != 3600;
    m_course = cog * 0.1f;

    m_timeStamp = (ba[16] >> 2) & 0x3f;
}

QString AISSARAircraftPositionReport::toString()
{
    return QString("Lat: %1%6 Lon: %2%6 Speed: %3 knts Course: %4%6 Alt: %5 m")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_speedOverGround)
                .arg(m_course)
                .arg(m_altitude)
                .arg(QChar(0xb0));
}


AISUTCInquiry::AISUTCInquiry(QByteArray ba) :
    AISMessage(ba)
{
}

AISSafetyMessage::AISSafetyMessage(QByteArray ba) :
    AISMessage(ba)
{
    m_sequenceNumber = ba[4] & 0x3;
    m_destinationId = ((ba[5] & 0xff) << 22) | ((ba[6] & 0xff) << 14) | ((ba[7] & 0xff) << 6) | ((ba[8] >> 2) & 0x3f);
    m_retransmitFlag = (ba[8] >> 1) & 0x1;
    m_safetyRelatedText = AISMessage::getString(ba, 9, 0, (ba.size() - 9) * 8 / 6);
}

QString AISSafetyMessage::toString()
{
    return QString("To %1: Safety message: %2").arg(m_destinationId).arg(m_safetyRelatedText);
}

AISSafetyAck::AISSafetyAck(QByteArray ba) :
    AISMessage(ba)
{
}

AISSafetyBroadcast::AISSafetyBroadcast(QByteArray ba) :
    AISMessage(ba)
{
    m_safetyRelatedText = AISMessage::getString(ba, 5, 0, (ba.size() - 6) * 8 / 6);
}

QString AISSafetyBroadcast::toString()
{
    return QString("Safety message: %1").arg(m_safetyRelatedText);
}

AISInterrogation::AISInterrogation(QByteArray ba) :
    AISMessage(ba)
{
}

AISAssignedModeCommand::AISAssignedModeCommand(QByteArray ba) :
    AISMessage(ba)
{
}

AISGNSSBroadcast::AISGNSSBroadcast(QByteArray ba) :
    AISMessage(ba)
{
}

AISStandardClassBPositionReport::AISStandardClassBPositionReport(QByteArray ba) :
    AISMessage(ba)
{
    int sog = ((ba[5] & 0x3) << 8) | (ba[6] & 0xff);
    m_speedOverGroundAvailable = sog != 1023;
    m_speedOverGround = sog * 0.1f;

    m_positionAccuracy = (ba[7] >> 7) & 0x1;

    int32_t longitude = ((ba[7] & 0x7f) << 21) | ((ba[8] & 0xff) << 13) | ((ba[9] & 0xff) << 5) | ((ba[10] >> 3) & 0x1f);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[10] & 0x7) << 24) | ((ba[11] & 0xff) << 16) | ((ba[12] & 0xff) << 8) | (ba[13] & 0xff);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;

    int cog = ((ba[14] & 0xff) << 4) | ((ba[15] >> 4) & 0xf);
    m_courseAvailable = cog != 3600;
    m_course = cog * 0.1f;

    m_heading = ((ba[15] & 0xf) << 5) | ((ba[16] >> 3) & 0x1f);
    m_headingAvailable = m_heading != 511;

    m_timeStamp = ((ba[16] & 0x7) << 3) | ((ba[17] >> 5) & 0x7);
}

QString AISStandardClassBPositionReport::toString()
{
    return QString("Lat: %1%5 Lon: %2%5 Speed: %3 knts Course: %4%5")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_speedOverGround)
                .arg(m_course)
                .arg(QChar(0xb0));
}


AISExtendedClassBPositionReport::AISExtendedClassBPositionReport(QByteArray ba) :
    AISMessage(ba)
{
    int sog = ((ba[5] & 0x3) << 8) | (ba[6] & 0xff);
    m_speedOverGroundAvailable = sog != 1023;
    m_speedOverGround = sog * 0.1f;

    m_positionAccuracy = (ba[7] >> 7) & 0x1;

    int32_t longitude = ((ba[7] & 0x7f) << 21) | ((ba[8] & 0xff) << 13) | ((ba[9] & 0xff) << 5) | ((ba[10] >> 3) & 0x1f);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[10] & 0x7) << 24) | ((ba[11] & 0xff) << 16) | ((ba[12] & 0xff) << 8) | (ba[13] & 0xff);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;

    int cog = ((ba[14] & 0xff) << 4) | ((ba[15] >> 4) & 0xf);
    m_courseAvailable = cog != 3600;
    m_course = cog * 0.1f;

    m_heading = ((ba[15] & 0xf) << 5) | ((ba[16] >> 3) & 0x1f);
    m_headingAvailable = m_heading != 511;

    m_timeStamp = ((ba[16] & 0x7) << 3) | ((ba[17] >> 5) & 0x7);

    m_name = AISMessage::getString(ba, 17, 1, 20);

    m_type = ((ba[32] & 1) << 7) | ((ba[33] >> 1) & 0x3f);
}

QString AISExtendedClassBPositionReport::toString()
{
    return QString("Lat: %1%5 Lon: %2%5 Speed: %3 knts Course: %4%5 Name: %6 Type: %7")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_speedOverGround)
                .arg(m_course)
                .arg(QChar(0xb0))
                .arg(m_name)
                .arg(typeToString(m_type));
}

AISDatalinkManagement::AISDatalinkManagement(QByteArray ba) :
    AISMessage(ba)
{
}

AISAidsToNavigationReport::AISAidsToNavigationReport(QByteArray ba) :
    AISMessage(ba)
{
    m_type = ((ba[4] & 0x3) << 3) | ((ba[5] >> 5) & 0x7);

    m_name = AISMessage::getString(ba, 5, 5, 20);

    m_positionAccuracy = (ba[20] >> 4) & 0x1;

    int32_t longitude = ((ba[20] & 0xf) << 24) | ((ba[21] & 0xff) << 16) | ((ba[22] & 0xff) << 8) | (ba[23] & 0xff);
    longitude = (longitude << 4) >> 4;
    m_longitudeAvailable = longitude != 0x6791ac0;
    m_longitude = longitude / 60.0f / 10000.0f;

    int32_t latitude = ((ba[24] & 0xff) << 19) | ((ba[25] & 0xff) << 11) | ((ba[26] & 0xff) << 3) | ((ba[27] >> 5) & 0x7);
    latitude = (latitude << 5) >> 5;
    m_latitudeAvailable = latitude != 0x3412140;
    m_latitude = latitude / 60.0f / 10000.0f;
}

QString AISAidsToNavigationReport::toString()
{
    const QStringList types = {
        "N/A",
        "Reference point",
        "RACON",
        "Fixed structure off short",
        "Emergency wreck marking buoy",
        "Light, without sectors",
        "Light, with sectors",
        "Leading light front",
        "Leading light rear",
        "Beacon, Cardinal N",
        "Beacon, Cardinal E",
        "Beacon, Cardinal S",
        "Beacon, Cardianl W",
        "Beacon, Port hand",
        "Beacon, Starboard hand",
        "Beacon, Preferred channel port hand",
        "Beacon, Preferred channel starboard hand",
        "Beacon, Isolated danger",
        "Beacon, Safe water",
        "Beacon, Special mark"
        "Cardinal mark N",
        "Cardinal mark E",
        "Cardinal mark S",
        "Cardinal mark W",
        "Port hand mark",
        "Starboard hand mark",
        "Preferred channel port hand",
        "Preferred channel starboard hand",
        "Isolated danger",
        "Safe water",
        "Special mark",
        "Light vessel/LANBY/Rigs"
        };
    return QString("Lat: %1%5 Lon: %2%5 Name: %3 Type: %4")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_name)
                .arg(types[m_type])
                .arg(QChar(0xb0));
}

AISChannelManagement::AISChannelManagement(QByteArray ba) :
    AISMessage(ba)
{
}

AISGroupAssignment::AISGroupAssignment(QByteArray ba) :
    AISMessage(ba)
{
}

AISStaticDataReport::AISStaticDataReport(QByteArray ba) :
    AISMessage(ba)
{
    m_partNumber = ba[4] & 0x3;
    if (m_partNumber == 0)
    {
        m_name = AISMessage::getString(ba, 5, 8, 20);
    }
    else if (m_partNumber == 1)
    {
        m_type = ba[5] & 0xff;
        m_vendorId = AISMessage::getString(ba, 6, 8, 7);
        m_callsign = AISMessage::getString(ba, 11, 6, 7);
    }
}

QString AISStaticDataReport::toString()
{
    if (m_partNumber == 0) {
        return QString("Name: %1").arg(m_name);
    } else if (m_partNumber == 1) {
        return QString("Type: %1 Vendor ID: %2 Callsign: %3").arg(typeToString(m_type)).arg(m_vendorId).arg(m_callsign);
    } else {
        return "";
    }
}

AISSingleSlotBinaryMessage::AISSingleSlotBinaryMessage(QByteArray ba) :
    AISMessage(ba)
{
}

AISMultipleSlotBinaryMessage::AISMultipleSlotBinaryMessage(QByteArray ba) :
    AISMessage(ba)
{
}

AISLongRangePositionReport::AISLongRangePositionReport(QByteArray ba) :
    AISMessage(ba)
{
    m_positionAccuracy = (ba[4] >> 1) & 0x1;
    m_raim = ba[4] & 0x1;
    m_status = (ba[5] >> 4) & 0xf;

    int32_t longitude = ((ba[5] & 0xf) << 14) | ((ba[6] & 0xff) << 6) | ((ba[7] >> 2) & 0x3f);
    longitude = (longitude << 14) >> 14;
    m_longitudeAvailable = longitude != 0x1a838;
    m_longitude = longitude / 60.0f / 10.0f;

    int32_t latitude = ((ba[7] & 0x3) << 15) | ((ba[8] & 0xff) << 7) | ((ba[9] >> 1) & 0x7f);
    latitude = (latitude << 15) >> 15;
    m_latitudeAvailable = latitude != 0xd548;
    m_latitude = latitude / 60.0f / 10.0f;

    m_speedOverGround = ((ba[9] & 0x1) << 5) | ((ba[10] >> 3) & 0x1f);
    m_speedOverGroundAvailable = m_speedOverGround != 63;

    m_course = ((ba[10] & 0x7) << 6) | ((ba[11] >> 2) & 0x3f);
    m_courseAvailable = m_course != 512;
}

QString AISLongRangePositionReport::toString()
{
    return QString("Lat: %1%6 Lon: %2%6 Speed: %3 knts Course: %4%6 Status: %5")
                .arg(m_latitude)
                .arg(m_longitude)
                .arg(m_speedOverGround)
                .arg(m_course)
                .arg(AISPositionReport::getStatusString(m_status))
                .arg(QChar(0xb0));
}

AISUnknownMessageID::AISUnknownMessageID(QByteArray ba) :
    AISMessage(ba)
{
}

