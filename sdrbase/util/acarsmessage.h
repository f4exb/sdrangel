///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSMESSGAE_H
#define INCLUDE_ACARSMESSGAE_H

#include <QString>
#include <QDate>
#include <QTime>
#include <QSharedPointer>
#include <QHash>
#include <QMetaType>

#include "export.h"

#define ASCII_SOH 0x01
#define ASCII_STX 0x02
#define ASCII_ETX 0x03
#define ASCII_ACK 0x06
#define ASCII_NAK 0x15
#define ASCII_ETB 0x17
#define ASCII_DEL 0x7f

// Shortest legal ARINC 618 block: SOH mode addr(7) ack label(2) blockid ETX BCS(2) DEL.
// STX is present only when message text follows.
#define ACARS_MIN_BYTES 17

// Sanity bound on an altitude recovered from a fixed-width position report, used to
// reject a field that has been read at the wrong offset. Airliners sending these do
// not cruise above FL430
#define ACARS_MAX_REPORTED_ALTITUDE_FT 50000

// See ARINC 618
class AirportInformation;

// The airport database the parsers resolve ICAO codes against, shared process wide and
// filled in once - see AcarsDemodWorker::startWork(). Declared here now that the parser
// is in the library, because the plugin that fills it in has to import the same symbol.
extern SDRBASE_API QSharedPointer<const QHash<QString, AirportInformation *>> m_airportInfo;

// Fill in m_airportInfo, once per process. Every demodulator worker and the Aircraft
// feature's tracker call this from their own threads, and QSharedPointer assignment is
// not atomic, so it must never be assigned anywhere else.
SDRBASE_API void acarsInitAirportInfo();

class SDRBASE_API AcarsMessage {
public:
    QDateTime m_dateTime;   // Date & time message was received
    QString m_mode;
    QString m_address;
    QChar m_ack;
    QString m_label;
    QChar m_blockId;
    QString m_text;
    QString m_originator; // Message originator (1-9,A-Z) - for downlinks
    QString m_messageNumber; // Two digit message number (0-99) - for downlinks
    QString m_blockSequence; // Block sequence for multiblock messages (A-Z)
    QString m_flight; // Flight number - for downlinks
    QString m_subLabel;
    //!< The character after a three character sublabel - the B of #M1B. Kept apart from
    //!< m_subLabel, which everything else treats as two characters.
    QString m_subLabelSupplement;
    bool m_multiBlock; // Indicates if this message has additional following blocks
    bool m_uplink;  // Indicates if uplink or downlink
    AcarsMessage(const QByteArray &message);
    virtual bool decode(const QString &message) { return true; }
    virtual QString toString();
    // Aircraft position carried in the message, for the table and Map. altitudeFt is
    // -1 when the message has a position but no altitude.
    virtual bool getPosition(float& latitude, float& longitude, int& altitudeFt) const
    {
        (void) latitude;
        (void) longitude;
        (void) altitudeFt;
        return false;
    }
    static AcarsMessage *decode(const QByteArray &message);
    // As above, but with the text decoded in place of the block's own - the assembled
    // whole, for the last block of several. Decoding the block and then calling the
    // virtual decode() again with the whole decoded twice, which doubled every list a
    // decoder appends to (route waypoints, METAR observations, airports...).
    static AcarsMessage *decode(const QByteArray &message, const QString &text);
protected:
    static AcarsMessage *decodeBlock(const QByteArray &message, const QString *text);
    static AcarsMessage *decode(const QByteArray &message, const QString &label, const QString &text, bool uplink);
    static bool isUplink(QChar blockId);
};

// ARINC622 - Character-Oriented Air Traffic Service (ATS) Applications
// Application data embedded in ACARS text field
class SDRBASE_API ARINC622 {
public:
    QString m_atcCenter;
    QString m_imi;
    QString m_aircraft;
    QString m_app;
    int m_crc;
    QString m_do219; // IMI == "AT1"
    bool m_valid;
    ARINC622() :
        m_valid(false)
    {}
    bool decode(const QString &message, bool uplink);
    QString toString();
};

class SDRBASE_API AcarsDataTransceiverAutoTune : public AcarsMessage {
public:
    int m_frequencyHz;
    int m_precision;
    AcarsDataTransceiverAutoTune(const QByteArray &message);
    bool decode(const QString &text) override;

    virtual QString toString() override;
};


class SDRBASE_API AcarsGroundUTCRequest : public AcarsMessage {
public:
    QDate m_date;
    int m_day;
    QTime m_time;
    AcarsGroundUTCRequest(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsOceanicClearanceResponse : public AcarsMessage {
public:
    QTime m_messageTime;
    QDate m_messageDate;
    QString m_atcCenter;
    int m_clearanceNumber = 0;
    QString m_aircraft;
    QString m_destination;
    QString m_via;
    QString m_routeId;
    QString m_route;
    QString m_entryPoint;
    QTime m_entryTime;
    float m_mach = 0.0f;
    int m_flightLevel = 0;
    QString m_freeText;
    AcarsOceanicClearanceResponse(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsDepartureClearanceResponse : public AcarsMessage {
public:
    QTime m_messageTime;
    QDate m_messageDate;
    QString m_departureAirport;
    int m_clearanceNumber = 0;
    QString m_flight;
    QString m_destination;
    QString m_runway;
    QString m_sid;
    QString m_squawk;
    QString m_departureTimeType;
    QString m_departureTime;
    QString m_atis;
    // The European form of the clearance carries these where the North American one
    // carries a departure time type - see decode()
    int m_initialAltitudeFt = -1;
    QString m_tsat;             // Target start up approval time
    QString m_qnh;
    QString m_frequency;        // Airborne or next frequency, MHz as written
    QString m_freeText;
    // The plain language forms carry a route the aircraft can be drawn on, and an
    // aircraft type, where the two coded dialects carry neither - see decode()
    QString m_route;
    QString m_aircraftType;
    // A clearance can also be a refusal - "NO DEPARTURE CLEARANCE MESSAGE ON FILE,
    // CONTACT CLEARANCE DELIVERY VIA VOICE". It is still a departure clearance response
    // and worth showing, but it names no route, so nothing may be drawn from it.
    bool m_clearanceAvailable = true;
    // Set by the plain language dialects, which carry no message time - toString() used
    // to key on that time and so printed nothing at all for them
    bool m_plainLanguage = false;
    AcarsDepartureClearanceResponse(const QByteArray &message);
    bool decode(const QString &text) override;
    // The dispatch chooses this class on the text as well as on the label, because the
    // plain language dialects arrive under labels that carry other traffic too
    static bool isPlainLanguage(const QString &text);
    virtual QString toString() override;
};

class SDRBASE_API AcarsFlightSystemsMessage : public AcarsMessage {
public:
    QTime m_messageTime;
    QDate m_messageDate;
    QString m_departureAirport;
    QString m_flight;
    QString m_freeText;
    AcarsFlightSystemsMessage(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsATISReport: public AcarsMessage {
public:
    QString m_airport;
    QString m_indicator;
    QString m_atisId;
    QTime m_time;
    QString m_atisInformation;
    AcarsATISReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsATSFacilitiesNotification : public AcarsMessage {
public:
    QString m_atcCenter;
    QString m_flight;
    QString m_aircraftRegistration;
    QString m_aircraftICAO;
    QTime m_time;
    QString m_mti;  // Message text identifier
    AcarsATSFacilitiesNotification(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsRequestOceanicClearance : public AcarsMessage {
public:
    QString m_avionicsIndicator;
    QString m_aircraft;
    QString m_entryPoint;
    QTime m_entryTime;
    float m_mach = 0.0f;
    int m_flightLevel = 0;
    QString m_freeText;
    AcarsRequestOceanicClearance(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsOceanicClearanceReadback : public AcarsMessage {
public:
    QTime m_messageTime;
    QDate m_messageDate;
    QString m_atcCenter;
    int m_clearanceNumber = 0;
    QString m_aircraft;
    QString m_destination;
    QString m_via;
    QString m_routeId;
    QString m_route;
    QString m_entryPoint;
    QTime m_entryTime;
    float m_mach = 0.0f;
    int m_flightLevel = 0;
    QString m_freeText;
    AcarsOceanicClearanceReadback(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsRequestATISReport : public AcarsMessage {
public:
    QString m_avionicsIndicator;
    QString m_airport;
    QString m_arrivalDepartureIndicator;
    AcarsRequestATISReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsWeatherRequest : public AcarsMessage {
public:
    QStringList m_airports;
    QString m_reportType;       // MET report code ("SA" = METAR) or the WXRQ /TYP value
    QString m_registration;     // WXRQ form only
    QString m_origin;           // WXRQ form only
    QString m_destination;      // WXRQ form only
    AcarsWeatherRequest(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
    //!< The MET report codes, by code - "SA" gives "METAR"
    static const QHash<QString, QString>& reportTypeNames();
};

// A loadsheet: what the aircraft weighs, what is on it and where its centre of gravity
// sits. Sent before departure and again as a final once loading is done.
//
// Recognised by its content rather than by its label, because it is not sent under one -
// seen off air under 10, 30 and others, and behind several different prefixes (WAB02-,
// LSOTIFA002*, :WAB11-). Every airline lays the rest of it out the same way, which is
// what makes the figures worth pulling out of it.
class SDRBASE_API AcarsLoadsheet : public AcarsMessage {
public:
    bool m_final = false;           //!< Final rather than preliminary
    QString m_from;                 //!< IATA, as loadsheets give them
    QString m_to;
    QString m_registration;         //!< Without its dash, as written here
    int m_zeroFuelWeight = -1;      //!< -1 where the loadsheet does not give it
    int m_zeroFuelWeightMax = -1;
    int m_takeoffFuel = -1;
    int m_takeoffWeight = -1;
    int m_takeoffWeightMax = -1;
    int m_landingWeight = -1;
    int m_landingWeightMax = -1;
    int m_tripFuel = -1;
    int m_underload = -1;
    int m_passengers = -1;
    float m_macZeroFuel = -1.0f;    //!< Centre of gravity, percent of mean aerodynamic chord
    float m_macTakeoff = -1.0f;

    AcarsLoadsheet(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsFlightStatus : public AcarsMessage {
public:
    QString m_departureAirport;
    QString m_arrivalAirport;
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    int m_altitudeFt = -1;          // -1 when not reported
    int m_fuelOnBoardKg = -1;       // Units differ by layout, so held in kg
    int m_fuelUsedKg = -1;
    bool m_temperatureValid = false;
    int m_temperatureC = 0;         // Static air temperature
    int m_windSpeedKts = -1;
    int m_windDirectionDeg = -1;
    int m_headingDeg = -1;          // Compact layout only
    int m_trackDeg = -1;
    int m_speedKts = -1;
    QTime m_eta;
    QTime m_time;
    QString m_additionalData;       // Undecoded middle fields, when present
    AcarsFlightStatus(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
    bool getPosition(float& latitude, float& longitude, int& altitudeFt) const override;
};

// Position reports in the related airline formats used on labels 10, 12, 16, 22, 24 and 44
class SDRBASE_API AcarsPositionReport : public AcarsMessage {
public:
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    int m_altitudeFt = -1;
    QTime m_time;
    QTime m_eta;
    QString m_reportFlight;         // Flight number within the report (label 24)
    QString m_reportType;           // POS/ETA/IN/OFF/ON (label 44)
    QString m_departureAirport;     // Label 44
    QString m_arrivalAirport;
    int m_month = -1;
    int m_day = -1;
    float m_fuelTons = -1.0f;
    // Carried by the airline formats on labels 11, 21, 36, 45 and 8H. Fuel is kept as
    // written because the units are the operator's choice - pounds, kilos and tens of
    // kilos all appear - and guessing would be worse than showing what was sent.
    QString m_tail;
    QString m_alternateAirport;
    QString m_lastWaypoint;
    QString m_nextWaypoint;
    QString m_departureRunway;
    QString m_arrivalRunway;
    QString m_fuelOnBoard;
    int m_trackDeg = -1;
    int m_speedKts = -1;            // True airspeed where the format distinguishes it
    int m_groundSpeedKts = -1;
    int m_windDirDeg = -1;
    int m_windSpeedKts = -1;
    bool m_temperatureValid = false;
    int m_temperatureC = 0;
    AcarsPositionReport(const QByteArray &message);
    bool decode(const QString &text) override;
    // The airline formats, tried before the ARINC ones. Each returns true only when the
    // text really is that format, because these labels carry other things as well.
    bool decodeAirlinePosition(const QString &text);
    bool decodeInRangePosition(const QString &text);
    bool decodeWaypointPosition(const QString &text);
    bool decodeRoutedPosition(const QString &text);
    bool decodeCsvPosition(const QString &text);
    bool decodeLabel83Position(const QString &text);
    bool decodeStationPosition(const QString &text);
    bool decodePoxPosition(const QString &text);
    bool decodeCompanyPosition(const QString &text);
    bool decodeCruiseReport(const QString &text);
    virtual QString toString() override;
    bool getPosition(float& latitude, float& longitude, int& altitudeFt) const override;
};

// Airline operational reports on label 80 (3X01 POSRPT / OPNORM / INRANG formats)
class SDRBASE_API AcarsOpsReport : public AcarsMessage {
public:
    QString m_reportType;           // POSRPT, OPNORM or INRANG
    QString m_reportFlight;
    int m_day = -1;
    QString m_departureAirport;
    QString m_arrivalAirport;
    QString m_tail;
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    int m_altitudeFt = -1;
    QString m_nextWaypoint;
    int m_headingDeg = -1;
    int m_machX1000 = -1;
    int m_speedKts = -1;            // TAS or SPD
    bool m_temperatureValid = false;
    int m_temperatureC = 0;
    QString m_fuelOnBoard;          // As reported; units vary by airline
    QTime m_time;                   // From /UTC
    QTime m_eta;
    AcarsOpsReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
    bool getPosition(float& latitude, float& longitude, int& altitudeFt) const override;
};

// OOOI event reports on labels 13 to 18 (out of gate, takeoff, landing, in gate and
// the post reports), in the /13 OUT EVENT / KSFO KEWR ... format
class SDRBASE_API AcarsEventReport : public AcarsMessage {
public:
    int m_event = 0;                // The label as an int, 13 to 18
    QString m_eventDescription;     // From the message, e.g. "OUT EVENT"
    QString m_departureAirport;
    QString m_arrivalAirport;
    int m_day = -1;
    QTime m_eventTime;
    int m_fuelOnBoard = -1;         // In 100kg
    //!< Some of these are not an event at all but a destination and an ETA - see decode()
    QTime m_eta;
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    AcarsEventReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
    bool getPosition(float& latitude, float& longitude, int& altitudeFt) const override;
};

class SDRBASE_API AcarsATCCommunications : public AcarsMessage {
public:
    ARINC622 m_622;
    AcarsATCCommunications(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsMessageFromSubsystem : public AcarsMessage {
public:
    ARINC622 m_622;
    // FMS reports (e.g. #M1BPOS position, #M1BFPN flight plan) when the text is not
    // an ARINC 622 ATS message
    QString m_fmsSublabel;
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    int m_altitudeFt = -1;
    QString m_lastWaypoint;
    QTime m_timeOver;               // Time over the last waypoint
    QString m_nextWaypoint;
    QTime m_etaNext;                // ETA at the next waypoint
    QString m_nextPlus1Waypoint;
    bool m_temperatureValid = false;
    int m_temperatureC = 0;
    QString m_posAdditionalData;
    //!< Decoded out of the additional data - see extractAdditionalBlocks()
    bool m_windValid = false;
    int m_windDirection = 0;        //!< Degrees
    int m_windSpeed = 0;            //!< Knots
    bool m_fuelValid = false;
    int m_fuel = 0;                 //!< Units not established, see the README
    bool m_cruiseLevelValid = false;
    int m_cruiseLevelFt = 0;        //!< The level the flight is planned at, not its current one
    QString m_destination;          //!< From the destination block, which agrees with the arrival
    QString m_destinationRunway;
    bool m_destinationFuelValid = false;
    int m_destinationFuel = 0;
    bool m_destinationEtaValid = false;
    QTime m_destinationEta;
    QString m_fpnDeparture;
    QString m_fpnArrival;
    //!< The callsign the FMC uses, from an /FN block. It is the ICAO form where the
    //!< ACARS block's own flight field is the IATA one - SAS87J against SK087J - so it
    //!< is the other half of the flight's name, and in 380 messages here the two were
    //!< never the same.
    QString m_icaoCallsign;
    QString m_fpnRoute;         //!< Waypoints and airways, as a flight plan is written
    QString m_fpnRunway;        //!< Departure runway, with the arrival's in brackets
    QString m_fpnSid;           //!< Departure procedure
    QString m_fpnStar;          //!< Arrival procedure
    QString m_fpnApproach;      //!< Approach and its transition
    QString m_fpnAlternate;     //!< Alternate destination, from the /RA: block
    //!< One waypoint of m_fpnRoute. Deliberately not a QGeoCoordinate: this header is
    //!< compiled into the test harness, which does not link Qt Positioning.
    struct FpnWaypoint {
        QString m_name;
        bool m_hasPosition = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        //!< What the FMS expects at the fix, from a predicted route block - see
        //!< extractPredictedRoute(). A fix named by a route insert has neither.
        bool m_hasAltitude = false;
        int m_altitudeFt = 0;
        bool m_hasEta = false;
        QTime m_eta;
    };
    //!< Same order and length as the names in m_fpnRoute. A route insert gives the
    //!< position of most of its fixes, which places fixes no database here holds and
    //!< settles which of two identically named fixes was meant.
    QList<FpnWaypoint> m_fpnWaypoints;
    AcarsMessageFromSubsystem(const QByteArray &message);
    bool decode(const QString &text) override;
    bool extractPredictedRoute(const QString& additional);
    void extractAdditionalBlocks(const QString& additional);
    virtual QString toString() override;
    bool getPosition(float& latitude, float& longitude, int& altitudeFt) const override;
};

class SDRBASE_API AcarsOutReport : public AcarsMessage {
public:
    QString m_departure;
    QString m_destination;
    int m_outTimeHours;
    int m_outTimeMinutes;
    int m_fuel;
    QString m_boardedFuel;
    QString m_freeText;
    AcarsOutReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsOffReport : public AcarsMessage {
public:
    QString m_departure;
    QString m_destination;
    int m_offTimeHours;
    int m_offTimeMinutes;
    QString m_freeText;
    AcarsOffReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsOnReport : public AcarsMessage {
public:
    QString m_departure;
    QString m_destination;
    int m_onTimeHours;
    int m_onTimeMinutes;
    QString m_freeText;
    AcarsOnReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsInReport : public AcarsMessage {
public:
    QString m_departure;
    QString m_destination;
    int m_inTimeHours;
    int m_inTimeMinutes;
    int m_fuel;
    QChar m_captainFirstOfficerId;
    QChar m_landingCategory;
    QString m_freeText;
    AcarsInReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsOutReturnInReport : public AcarsMessage {
public:
    QString m_departure;
    QString m_destination;
    int m_outTimeHours;
    int m_outTimeMinutes;
    int m_returnInTimeHours;
    int m_returnInTimeMinutes;
    int m_fuel;
    QString m_freeText;
    AcarsOutReturnInReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};


// The short OOOI reports, on labels QA, QB, QD, QE, QF, QG and Q1.
//
// These are NOT the ARINC 620 form that AcarsOutReport and its siblings parse on QP to
// QT, which names its airports with four character ICAO codes. These name a station with
// a three character IATA code and give the time as four digits with nothing between:
//
//     QB  BOS1553                       off Boston at 15:53
//     QF  SAT1458SFO                     off San Antonio at 14:58 for San Francisco
//     QG  PER20302036                    out of Perth 20:30, back in 20:36
//     QE  SYD0642       62MEL            out of Sydney 06:42, 62 of fuel, for Melbourne
//     QD  PHL132300441/CA 01/DO 1323     in at Philadelphia 13:23
//
// "---" stands in for a station the aircraft did not give. Trailing blocks written
// /XX nnnn - fuel on board, fuel used, times repeated - vary by operator and are kept
// as they were sent rather than guessed at.
class SDRBASE_API AcarsShortOooiReport : public AcarsMessage {
public:
    int m_event = 0;                //!< AircraftReport::OooiEventKind, from the label
    QString m_station;              //!< IATA code, empty where the aircraft sent ---
    QString m_destination;          //!< IATA code, where the report names one
    QTime m_time;
    QTime m_secondTime;             //!< Return-in time on QG, arrival ETA on Q1
    QString m_fuel;                 //!< As sent; the units are the operator's
    QStringList m_extras;           //!< The /XX blocks, as sent
    AcarsShortOooiReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsSquitter : public AcarsMessage {
public:
    QString m_version;
    QString m_serviceProvider;
    QString m_iataStationId;
    QString m_icaoStationId;
    QString m_stationNumber;
    QString m_latitude;
    QString m_longitude;
    QStringList m_alternateService;
    QStringList m_alternateFrequency;
    QString m_freeText;
    AcarsSquitter(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

// ARINC 618 media advisory, label SA: the aircraft saying which datalink it has just
// gained or lost, and which ones it can still use. Sent by the avionics rather than by
// the airline, so unlike most of the labels above the format is the same everywhere.
//
//   0EV181451V     established on VHF at 18:14:51, VHF available
//   0LS1918452H    lost SATCOM at 19:18:45, VDL Mode 2 and HF still available
//   0E20832042S/   established on VDL Mode 2 at 08:32:04, VDL Mode 2 and SATCOM available
//
// The automated meteorological report on label H2 - the aircraft as a weather station.
// One message carries a route and a run of observations, each a position, a time, an
// altitude, the outside air temperature and the wind:
//
//   02E30EGPFEGGWN52381W00139420262773M422199015G
//
// which is Glasgow to Luton, 52 38.1 N 1 39.4 W at 20:26, FL277.3, -42.2 C, wind 199 at
// 15 knots. A long flight sends several in one message.
class SDRBASE_API AcarsMeteorologicalReport : public AcarsMessage {
public:
    struct Observation {
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        QTime m_time;
        int m_altitudeFt = 0;
        float m_temperatureC = 0.0f;
        int m_windDirectionDeg = 0;
        int m_windSpeedKts = 0;
    };
    QString m_departureAirport;
    QString m_arrivalAirport;
    QList<Observation> m_observations;
    AcarsMeteorologicalReport(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
};

class SDRBASE_API AcarsMediaAdvisory : public AcarsMessage {
public:
    QString m_version;
    bool m_established = true;      // False when the link was lost rather than gained
    QString m_media;                // The link gained or lost, expanded
    QTime m_time;
    QStringList m_availableMedia;   // Every link still usable, expanded
    QString m_freeText;
    AcarsMediaAdvisory(const QByteArray &message);
    bool decode(const QString &text) override;
    virtual QString toString() override;
    static QString mediaName(QChar code);
};

Q_DECLARE_METATYPE(AcarsMessage *)

#endif // INCLUDE_ACARSMESSGAE_H
