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

#ifndef INCLUDE_UTIL_AIRCRAFTREPORT_H
#define INCLUDE_UTIL_AIRCRAFTREPORT_H

#include <QString>
#include <QList>
#include <QRegularExpression>
#include <QDateTime>
#include <QUrl>

namespace SWGSDRangel {
    class SWGMapItem;
}

// One sighting of an aircraft by a demodulator, passed from channels (ADS-B and
// ACARS family demodulators) to the Aircraft feature over the "aircraftreport"
// message pipe. Every field is optional except the source description - a report
// carries whichever identities, position and document the received frame had.
// A link in an aircraft's Map popup that selects it in the Aircraft feature when it is
// clicked. Both the href and the onclick are needed and neither is redundant: the 2D map
// is QML and follows the href through Text.onLinkActivated, while the 3D map is a Cesium
// infobox in a web view and has to post the URL back over the websocket, which is what
// parent.infoboxLink() does. MapGUI::linkClicked() ends up with it either way.
//
// Written here rather than in either producer because both the ADS-B demodulator and the
// Aircraft feature build popups, and a link that differs between them would send half the
// aircraft on the Map somewhere else.
//
// kind is "icao", "registration" or "flight".
// "Destination airport: EGCC" out of a VDL-2 XID logon's decoded text.
//
// Here rather than beside the VDL-2 decoder because both need it and they are separate
// plugins that cannot link one another: the ACARS demodulator reads it as logons arrive,
// and the Aircraft feature reads it again over stored logons, to fill in flights heard
// before it was read at all.
//
// Anchored to the end of the line so a longer value - the parameter is an octet string,
// so nothing guarantees four characters - is rejected rather than truncated to its first
// four. Only exactly four letters is an ICAO ident.
inline QString xidDestinationAirport(const QString& decoded)
{
    static const QRegularExpression re("Destination airport: ([A-Z]{4})\\s*$",
                                       QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = re.match(decoded);
    return match.hasMatch() ? match.captured(1) : QString();
}

// The name an aircraft is drawn under on the Map.
//
// Shared because the name is not always chosen by whoever decoded the aircraft. With an
// Aircraft feature present the ADS-B demodulator sends its map item to that feature
// rather than to the Map (ADSBDemodTracker::sendToMap), and the feature renames it -
// one airframe heard on ADS-B, ACARS and VDL-2 has to be one item, and the registration
// is the only identity all three protocols share. So the demodulator has to ask the Map
// for the name its aircraft ACTUALLY has, not the one it would have chosen, and the two
// have to agree on the rule to do that.
//
// Registration first for that reason. The flight is next because an airframe with no
// registration still has to be identifiable, and the address last because it is the one
// thing always available.
inline QString aircraftMapItemName(const QString& registration, const QString& flight,
                                   quint32 icao)
{
    if (!registration.isEmpty()) {
        return registration;
    }
    if (!flight.isEmpty()) {
        return flight;
    }
    if (icao) {
        return QString("%1").arg(icao, 6, 16, QChar('0')).toUpper();
    }
    return QString();
}

inline QString aircraftMapLink(const QString& kind, const QString& value)
{
    const QString url = QString("sdrangel-aircraft://%1/%2").arg(kind,
        QString::fromUtf8(QUrl::toPercentEncoding(value)));
    return QString("<a href=\"%1\" onclick=\"return parent.infoboxLink('%1')\">%2</a>")
        .arg(url, value.toHtmlEscaped());
}

struct AircraftReport
{
    // New protocols go on the END: the Aircraft feature keys a per-source map and a
    // message rate chart on this value
    enum Protocol {
        ADSB,
        ACARS,          // Plain VHF ACARS
        VDL2,
        HFDL,
        AERO,           // Inmarsat Classic Aero (aviation SATCOM)
        //!< A selective call on an HF or VHF voice channel. It identifies no aircraft on
        //!< its own - see m_selcalCode - and carries no position.
        SELCAL,
        //!< Iridium (satcom from low earth orbit). Its own protocol rather than plain
        //!< ACARS: the bearer is what the statistics, the range figures and the source
        //!< column are about, and an aircraft reporting through a satellite has nothing
        //!< in common with one heard directly on VHF. Added at the end so that every
        //!< value already stored in a database or a preset keeps the number it had
        IRIDIUM,
        ProtocolCount
    };

    //!< The order to list the protocols in for the user, which is not the order they are
    //!< stored in. A new protocol has to be appended to the enum so that every value
    //!< already in a database or a preset keeps its meaning, but that puts it after
    //!< SELCAL, which belongs at the end of a list: it is the one that names no aircraft,
    //!< carries no position and is not a datalink at all. Holds every value exactly once
    static const QList<int>& displayOrder()
    {
        static const QList<int> order = { ADSB, ACARS, VDL2, HFDL, AERO, IRIDIUM, SELCAL };

        return order;
    }

    //!< Whether reports on a protocol can carry a position. Statistics derived from one,
    //!< such as the furthest aircraft heard, are meaningless for a protocol that cannot,
    //!< and would show as a permanently empty entry
    static bool reportsPosition(int protocol)
    {
        return protocol != SELCAL;
    }

    enum DocumentKind {
        NoDocument,
        AcarsText,      // A plain ACARS message
        PositionReport,
        OooiEvent,      // Out/off/on/in event
        Loadsheet,
        FlightPlan,
        Clearance,      // Oceanic/departure clearance
        Cpdlc,
        Logon,          // Link management: logons, logoffs
        PerformanceReport,
        // Appended, not placed beside Clearance where it belongs: the kind is stored in
        // the database as an integer, so inserting one in the middle would reinterpret
        // every document already held.
        //
        // Distinct from Clearance because the two behave differently: an oceanic
        // clearance is drawn on the map as a clearance, while this one names the route
        // the aircraft is cleared to fly from departure and is drawn as its plan. Both
        // are clearances as far as the Flights table is concerned.
        DepartureClearance,
        // What the aircraft ASKED for, as against what it was given. Worth keeping for
        // the same reason the answers are: the request names the oceanic entry point,
        // level and Mach the crew wanted, and which airport they are planning to land
        // at. Appended, like the two above, because the kind is stored as an integer.
        ClearanceRequest,   // Label B1: request oceanic clearance
        AtisRequest,        // Label B9: request the ATIS for an airport
        // Categories worth keeping that have no decoder of their own - they are free
        // text, or airline specific, so the label is all that classifies them. Appended
        // like the rest because the kind is stored as an integer.
        WeatherRequest,     // Label 5U: which can name an alternate the crew is weighing
        AirlineDownlink,    // Label 5Z
        AircrewMessage,     // Labels 80 to 8F, addressed to or written by the crew
        EmergencyMessage    // Label 99
    };

    // Source
    Protocol m_protocol = ACARS;
    qint64 m_frequency = 0;         // Channel centre frequency in Hz, 0 if unknown
    int m_deviceSetIndex = -1;
    int m_channelIndex = -1;

    // Identity - whichever the frame carried
    quint32 m_icao = 0;             // 24 bit ICAO address, 0 if unknown
    QString m_registration;
    QString m_flight;               // Flight number / callsign as transmitted
    //!< Another name for the same flight, where a message gave one - the FMC's ICAO
    //!< callsign beside the block's IATA flight number, say. Recorded as an alias rather
    //!< than replacing m_flight, which would look like a different flight and start one.
    QString m_flightAlias;

    // Position and kinematics
    bool m_positionValid = false;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    bool m_altitudeValid = false;
    float m_altitudeFt = 0.0f;
    bool m_headingValid = false;
    float m_heading = 0.0f;         // Degrees
    bool m_speedValid = false;
    float m_speedKts = 0.0f;
    QDateTime m_positionDateTime;   // The report's own time when it differs from reception
    // When the altitude was observed. On ADS-B this is often NOT the position's time:
    // a Mode S altitude reply carries no position, so the two are measured separately.
    // Invalid on bearers that report a single fix, where the position's time serves.
    QDateTime m_altitudeDateTime;

    QDateTime m_received;

    // Set on reports emitted while an historical log is being replayed. The first
    // report for each aircraft also marks the boundary between the receiver's live
    // state (or an earlier replay) and this replay's independent track.
    bool m_replay = false;
    bool m_replayStart = false;

    // The OOOI event this report carried, if any. Out of gate, take off, landing and on
    // gate are the four times that describe a flight, and they arrive as separate ACARS
    // messages on labels 13 to 16.
    enum OooiEventKind { OooiNone = 0, OooiOut = 13, OooiOff, OooiOn, OooiIn };
    int m_oooiEvent = OooiNone;
    QDateTime m_oooiTime;       //!< When the event happened, not when it was reported

    // Weather this report carried. Aircraft ask for it and the ground sends it, so it
    // arrives through ACARS like everything else - but it describes an AIRPORT rather
    // than the aircraft that happened to request it, which is why it is kept separately.
    enum WeatherKind {
        WeatherNone = 0,
        Metar,          //!< Observation
        Taf,            //!< Forecast
        Atis,           //!< D-ATIS
        Twip,           //!< Terminal weather information for pilots
        Notam,
        Pirep,          //!< Pilot report
        Sigmet,         //!< Significant meteorological information, and AIRMET
        WeatherOther,   //!< Recognised as weather, not as any of the above
        //!< An automated observation made by the aircraft itself, in flight, rather than
        //!< a report about an airport - see AcarsMeteorologicalReport
        AirReport
    };
    int m_weatherKind = WeatherNone;
    QString m_weatherAirport;   //!< ICAO code the report is about, when it names one
    QString m_weatherText;      //!< The report itself, as sent

    // Decoded content
    DocumentKind m_documentKind = NoDocument;
    QString m_documentTitle;
    QString m_documentText;         // Full multi-line decode
    QString m_label;                // ACARS label or frame type
    bool m_uplink = false;          // true when ground to air
    QString m_atc;                  // Concise ATC message text (e.g. "WILCO") for CPDLC
    QString m_station;              // Ground station / ATC facility, when known
    //!< The SELCAL code called, as AB-CD, on a SELCAL report. A code is shared by
    //!< several aircraft, so the demodulator sends it without an identity and the
    //!< Aircraft feature decides which of them, if any, it was.
    QString m_selcalCode;

    // Route facts the message revealed (OOOI events, flight plans, clearances)
    QString m_departure;
    QString m_arrival;
    QString m_route;
    // One waypoint of m_route. A route insert gives the position of most of its fixes,
    // and using those beats looking the names up: it places fixes no database here
    // holds, and settles which of two identically named fixes was meant.
    struct RouteWaypoint {
        QString m_name;
        bool m_positionValid = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        //!< The departure or arrival procedure this fix belongs to, and which of the two
        //!< it is. Empty for a fix the plan named itself, which is most of them - these
        //!< are only set for fixes expanded out of a SID or STAR.
        QString m_procedure;
        QString m_procedureType;
        //!< What the procedure requires of the aircraft at this fix, in feet. A fix can
        //!< be given a floor, a ceiling, both - which is a window, or a crossing
        //!< altitude when they are equal - or neither.
        bool m_hasMinimumAltitude = false;
        int m_minimumAltitudeFt = 0;
        bool m_hasMaximumAltitude = false;
        int m_maximumAltitudeFt = 0;
        //!< What the aircraft's own FMS predicts for the fix, where the message carried a
        //!< predicted route. Not a constraint like the two above - this is where it
        //!< expects to be, and when.
        bool m_hasAltitude = false;
        int m_altitudeFt = 0;
        bool m_hasEta = false;
        QTime m_eta;
    };
    // Same order and length as the space separated names in m_route, and empty when the
    // route came from somewhere that names waypoints without placing them
    QList<RouteWaypoint> m_routeWaypoints;
    // The rest of the filed plan. A route insert carries one part of it - the departure
    // end, the path, or the arrival end - so these arrive in different messages from
    // each other and from m_route, and the Aircraft feature assembles one plan of them.
    QString m_routeRunway;
    QString m_routeSid;
    QString m_routeStar;
    QString m_routeApproach;
    QString m_routeAlternate;
    //!< What the document was rendered FROM, so that it can be rendered again by a later
    //!< decoder instead of being frozen as text - see AircraftTracker::renderDocument().
    //!< m_documentRaw is the block the message's header came from and m_documentSource the
    //!< text that was decoded, which for a multi block message is the assembled whole.
    //!< Only set when OUR parser produced the document: libacars and the ATN decoder live
    //!< in the demodulator plugin and cannot be re-run from the feature, so what they
    //!< produced is kept as text.
    QByteArray m_documentRaw;
    QString m_documentSource;
    bool m_documentDerivable = false;
    //!< What an oceanic clearance clears the aircraft to fly the route AT. Nothing else
    //!< fills these in - a filed plan gives its cruise level in the position report's
    //!< additional data instead.
    int m_routeFlightLevel = 0;     //!< Hundreds of feet, 0 when not given
    float m_routeMach = 0.0f;       //!< 0 when not given

    // Phase 2: a fully-built map item from the ADS-B demodulator, passed through
    // so its PFD state reaches the Map losslessly. Ownership transfers with the
    // report; nullptr for reports that do not carry one.
    SWGSDRangel::SWGMapItem *m_mapItem = nullptr;
};

#endif // INCLUDE_UTIL_AIRCRAFTREPORT_H
