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

#ifndef INCLUDE_AIS_H
#define INCLUDE_AIS_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include "export.h"

class SDRBASE_API AISMessage {
public:

    int m_id;
    int m_repeatIndicator;
    int m_mmsi;

    AISMessage(const QByteArray ba);
    virtual ~AISMessage() {}
    virtual QString getType() = 0;
    virtual bool hasPosition() { return false; }
    virtual float getLatitude() { return 0.0f; }
    virtual float getLongitude() { return 0.0f; }
    virtual bool hasCourse() { return false; }
    virtual float getCourse() { return 0.0f; }
    virtual bool hasSpeed() { return false; }
    virtual float getSpeed() { return 0.0f; }
    virtual bool hasHeading() { return false; }
    virtual float getHeading() { return 0.0f; }
    virtual QString toString() { return ""; }
    QString toHex();
    QString toNMEA();

    static AISMessage* decode(const QByteArray ba);
    static QString toNMEA(const QByteArray ba);
    static QString typeToString(quint8 type);

protected:
    static QString getString(QByteArray ba, int byteIdx, int bitsLeft, int chars);
    static qint8 nmeaChecksum(QString string);
    QByteArray m_bytes;

};

class SDRBASE_API AISPositionReport : public AISMessage {
public:
    int m_status;
    bool m_rateOfTurnAvailable;
    float m_rateOfTurn;             // Degrees per minute
    bool m_speedOverGroundAvailable;
    float m_speedOverGround;        // Knots
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    bool m_courseAvailable;
    float m_course;                 // Degrees
    bool m_headingAvailable;
    int m_heading;                  // Degrees
    int m_timeStamp;
    int m_specialManoeuvre;

    AISPositionReport(const QByteArray ba);
    virtual QString getType() override;
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual bool hasCourse() { return m_courseAvailable; }
    virtual float getCourse() { return m_course; }
    virtual bool hasSpeed() { return m_speedOverGroundAvailable; }
    virtual float getSpeed() { return m_speedOverGround; }
    virtual bool hasHeading() { return m_headingAvailable; }
    virtual float getHeading() { return m_heading; }
    virtual QString toString() override;

    static QString getStatusString(int status);
};

class SDRBASE_API AISBaseStationReport : public AISMessage {
public:
    QDateTime m_utc;
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;  // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;   // Degrees, East positive

    AISBaseStationReport(const QByteArray ba);
    virtual QString getType() override {
        if (m_id == 4)
            return "Base station report";
        else
            return "UTC and data reponse";
    }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual QString toString() override;
};

class SDRBASE_API AISShipStaticAndVoyageData : public AISMessage {
public:
    int m_version;
    int m_imo;
    QString m_callsign;
    QString m_name;
    quint8 m_type;
    int m_dimension;
    int m_a;
    int m_b;
    int m_c;
    int m_d;
    int m_positionFixing;
    int m_eta;
    int m_draught;
    QString m_destination;
    AISShipStaticAndVoyageData(const QByteArray ba);
    virtual QString getType() override { return "Ship static and voyage related data"; }
    virtual QString toString() override;
};

class SDRBASE_API AISBinaryMessage : public AISMessage {
public:
    int m_sequenceNumber;
    int m_destinationId;
    int m_retransmitFlag;
    AISBinaryMessage(const QByteArray ba);
    virtual QString getType() override { return "Addressed binary message"; }
    virtual QString toString() override;
};

class SDRBASE_API AISBinaryAck : public AISMessage {
public:
    AISBinaryAck(const QByteArray ba);
    virtual QString getType() override { return "Binary acknowledge"; }
};

class SDRBASE_API AISBinaryBroadcast : public AISMessage {
public:
    AISBinaryBroadcast(const QByteArray ba);
    virtual QString getType() override { return "Binary broadcast message"; }
};

class SDRBASE_API AISSARAircraftPositionReport : public AISMessage {
public:
    bool m_altitudeAvailable;
    float m_altitude;               // Metres. 4094 = 4094+
    bool m_speedOverGroundAvailable;
    float m_speedOverGround;        // Knots
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    bool m_courseAvailable;
    float m_course;                 // Degrees
    bool m_headingAvailable;
    int m_heading;                  // Degrees
    int m_timeStamp;

    AISSARAircraftPositionReport(const QByteArray ba);
    virtual QString getType() override { return "Standard SAR aircraft position report"; }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual bool hasCourse() { return m_courseAvailable; }
    virtual float getCourse() { return m_course; }
    virtual bool hasSpeed() { return m_speedOverGroundAvailable; }
    virtual float getSpeed() { return m_speedOverGround; }
    virtual QString toString() override;
};

class SDRBASE_API AISUTCInquiry : public AISMessage {
public:
    AISUTCInquiry(const QByteArray ba);
    virtual QString getType() override { return "UTC and date inquiry"; }
};

class SDRBASE_API AISSafetyMessage : public AISMessage {
public:
    int m_sequenceNumber;
    int m_destinationId;
    int m_retransmitFlag;
    QString m_safetyRelatedText;

    AISSafetyMessage(const QByteArray ba);
    virtual QString getType() override { return "Addressed safety related message"; }
    virtual QString toString() override;
};

class SDRBASE_API AISSafetyAck : public AISMessage {
public:
    AISSafetyAck(const QByteArray ba);
    virtual QString getType() override { return "Safety related acknowledge"; }
};

class SDRBASE_API AISSafetyBroadcast : public AISMessage {
public:
    QString m_safetyRelatedText;

    AISSafetyBroadcast(const QByteArray ba);
    virtual QString getType() override { return "Safety related broadcast message"; }
    virtual QString toString() override;
};

class SDRBASE_API AISInterrogation : public AISMessage {
public:
    AISInterrogation(const QByteArray ba);
    virtual QString getType() override { return "Interrogation"; }
};

class SDRBASE_API AISAssignedModeCommand : public AISMessage {
public:
    int m_destinationIdA;
    int m_offsetA;
    int m_incrementA;
    int m_destinationIdB;
    int m_offsetB;
    int m_incrementB;
    bool m_bAvailable;
    AISAssignedModeCommand(const QByteArray ba);
    virtual QString getType() override { return "Assigned mode command"; }
    virtual QString toString() override;
};

class SDRBASE_API AISGNSSBroadcast : public AISMessage {
public:
    AISGNSSBroadcast(const QByteArray ba);
    virtual QString getType() override { return "GNSS broadcast binary message"; }
};

class SDRBASE_API AISStandardClassBPositionReport : public AISMessage {
public:
    bool m_speedOverGroundAvailable;
    float m_speedOverGround;        // Knots
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    bool m_courseAvailable;
    float m_course;                 // Degrees
    bool m_headingAvailable;
    int m_heading;                  // Degrees
    int m_timeStamp;

    AISStandardClassBPositionReport(const QByteArray ba);
    virtual QString getType() override { return "Standard Class B equipment position report"; }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual bool hasCourse() { return m_courseAvailable; }
    virtual float getCourse() { return m_course; }
    virtual bool hasSpeed() { return m_speedOverGroundAvailable; }
    virtual float getSpeed() { return m_speedOverGround; }
    virtual QString toString() override;
};

class SDRBASE_API AISExtendedClassBPositionReport : public AISMessage {
public:
    bool m_speedOverGroundAvailable;
    float m_speedOverGround;        // Knots
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    bool m_courseAvailable;
    float m_course;                 // Degrees
    bool m_headingAvailable;
    int m_heading;                  // Degrees
    int m_timeStamp;
    QString m_name;
    quint8 m_type;

    AISExtendedClassBPositionReport(const QByteArray ba);
    virtual QString getType() override { return "Extended Class B equipment position report"; }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual bool hasCourse() { return m_courseAvailable; }
    virtual float getCourse() { return m_course; }
    virtual bool hasSpeed() { return m_speedOverGroundAvailable; }
    virtual float getSpeed() { return m_speedOverGround; }
    virtual QString toString() override;
};

class SDRBASE_API AISDatalinkManagement : public AISMessage {
public:
    AISDatalinkManagement(const QByteArray ba);
    virtual QString getType() override { return "Data link management message"; }
};

class SDRBASE_API AISAidsToNavigationReport : public AISMessage {
public:
    int m_type;
    QString m_name;
    int m_positionAccuracy;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    AISAidsToNavigationReport(const QByteArray ba);
    virtual QString getType() override { return "Aids-to-navigation report"; }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual QString toString() override;
};

class SDRBASE_API AISChannelManagement : public AISMessage {
public:
    AISChannelManagement(const QByteArray ba);
    virtual QString getType() override { return "Channel management"; }
};

class SDRBASE_API AISGroupAssignment : public AISMessage {
public:
    AISGroupAssignment(const QByteArray ba);
    virtual QString getType() override { return "Group assignment command"; }
};

class SDRBASE_API AISStaticDataReport : public AISMessage {
public:
    int m_partNumber;
    QString m_name;
    quint8 m_type;
    QString m_vendorId;
    QString m_callsign;

    AISStaticDataReport(const QByteArray ba);
    virtual QString getType() override { return "Static data report"; }
    virtual QString toString() override;
};

class SDRBASE_API AISSingleSlotBinaryMessage : public AISMessage {
public:
    bool m_destinationIndicator;
    bool m_binaryDataFlag;
    int m_destinationId;
    bool m_destinationIdAvailable;

    AISSingleSlotBinaryMessage(const QByteArray ba);
    virtual QString getType() override { return "Single slot binary message"; }
    virtual QString toString() override;
};

class SDRBASE_API AISMultipleSlotBinaryMessage : public AISMessage {
public:
    AISMultipleSlotBinaryMessage(const QByteArray ba);
    virtual QString getType() override { return "Multiple slot binary message"; }
};

class SDRBASE_API AISLongRangePositionReport : public AISMessage {
public:
    int m_positionAccuracy;
    int m_raim;
    int m_status;
    bool m_longitudeAvailable;
    float m_longitude;              // Degrees, North positive
    bool m_latitudeAvailable;
    float m_latitude;               // Degrees, East positive
    bool m_speedOverGroundAvailable;
    float m_speedOverGround;        // Knots
    bool m_courseAvailable;
    float m_course;                 // Degrees

    AISLongRangePositionReport(const QByteArray ba);
    virtual QString getType() override { return "Position report for long-range applications"; }
    virtual bool hasPosition() { return m_latitudeAvailable && m_longitudeAvailable; }
    virtual float getLatitude() { return m_latitude; }
    virtual float getLongitude() { return m_longitude; }
    virtual bool hasCourse() { return m_courseAvailable; }
    virtual float getCourse() { return m_course; }
    virtual bool hasSpeed() { return m_speedOverGroundAvailable; }
    virtual float getSpeed() { return m_speedOverGround; }
    QString getStatus();
    virtual QString toString() override;
};

class SDRBASE_API AISUnknownMessageID : public AISMessage {
public:
    AISUnknownMessageID(const QByteArray ba);
    virtual QString getType() override { return QString("Unknown message ID (%1)").arg(m_id); }
};

#endif // INCLUDE_AIS_H
