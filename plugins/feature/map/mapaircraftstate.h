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

struct MapAircraftState {
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
    float m_windSpeed;
    float m_windDirection;
    float m_staticAirTemperature;
};

#endif // INCLUDE_FEATURE_MAPAIRCRAFTSTATE_H_
