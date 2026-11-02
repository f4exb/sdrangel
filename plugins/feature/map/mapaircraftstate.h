///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_MAPAIRCRAFTSTATE_H_
#define INCLUDE_FEATURE_MAPAIRCRAFTSTATE_H_

#include <QDateTime>

struct MapRouteWaypoint {
    QString m_name;
    float m_latitude;   // NaN if the waypoint could not be placed
    float m_longitude;
    float m_altitudeFt; // NaN for unknown
    QString m_eta;      // Empty for unknown
};

struct MapFlightPlan {
    bool m_valid = false;
    QString m_departure;
    QString m_arrival;
    QString m_arrivalRunway;    // E.g. 27R. Empty for unknown
    QString m_approach;         // As filed, e.g. ILSY01R. Empty for unknown
    int m_activeWaypoint = -1;
    QList<MapRouteWaypoint> m_waypoints;
};

struct MapDatalinkMessage {
    bool m_valid = false;
    int m_id = 0;           // Sequence number, so a new message can be told from a re-send
    QString m_received;     // ISO 8601
    bool m_uplink = false;
    QString m_station;      // Empty for unknown
    QString m_text;
};

struct MapAircraftState {
    QDateTime m_dateTime;   // Time this complete state applies; invalid for a live/coalesced update
    QString m_icao;     // Mode S address as lower-case hex, empty for unknown
    QString m_callsign;
    QString m_aircraftType;
    int m_onSurface;    // -1 for unknown
    float m_indicatedAirspeed; // NaN for unknown
    QString m_indicatedAirspeedDateTime;
    float m_trueAirspeed;
    float m_groundspeed;
    float m_mach;
    float m_altitude;
    QString m_altitudeDateTime;
    float m_qnh;
    float m_verticalSpeed;
    float m_heading;
    float m_track;
    float m_selectedAltitude;
    float m_selectedHeading;
    int m_autopilot; // -1 for unknown
    enum VerticalMode {
        UNKNOWN_VERTICAL_MODE,
        VNAV,
        ALT_HOLD,
        GS
    } m_verticalMode;
    enum LateralMode {
        UNKNOWN_LATERAL_MODE,
        LNAV,
        LOC
    } m_lateralMode;
    enum TCASMode {
        UNKNOWN_TCAS_MODE,
        TCAS_OFF,
        TA,
        TA_RA
    } m_tcasMode;
    // Active TCAS resolution advisory bitfield. 0 none; bit 0 active, bit 1 corrective,
    // bit 2 downward sense, bit 3 positive (vs vertical speed limit), bit 4 clear of
    // conflict, bit 5 multiple threats
    int m_tcasRA = 0;
    QString m_tcasThreatIcao;   // Threat's Mode S address as hex, empty if unknown
    int m_squawk = -1;          // Mode A code as decimal digits (e.g. 7000), -1 unknown
    float m_windSpeed;
    float m_windDirection;
    float m_staticAirTemperature;
    MapFlightPlan m_flightPlan;
    MapDatalinkMessage m_datalinkMessage;
};

#endif // INCLUDE_FEATURE_MAPAIRCRAFTSTATE_H_
