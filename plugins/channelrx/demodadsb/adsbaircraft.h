///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_ADSBAIRCRAFT_H
#define INCLUDE_ADSBAIRCRAFT_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QVariant>
#include <QGeoCoordinate>

#include "util/movingaverage.h"
#include "util/osndb.h"

#include "adsbdemodsettings.h"

// A position decoded since the last tracker-to-GUI flush. Keeping these samples
// separate from the coalesced aircraft state lets fast CSV replay rebuild tracks.
struct ADSBAircraftPosition {
    Real m_latitude;
    Real m_longitude;
    int m_altitude;
    QDateTime m_dateTime;

    ADSBAircraftPosition(Real latitude, Real longitude, int altitude, const QDateTime& dateTime) :
        m_latitude(latitude),
        m_longitude(longitude),
        m_altitude(altitude),
        m_dateTime(dateTime)
    {}
};

// Data about an aircraft extracted from ADS-B and Mode-S frames.
//
// This is what the decoder produces and owns. It deliberately knows nothing about the
// table or the map, so that it can be decoded on the channel's worker thread and in a
// headless server build. The GUI derives Aircraft from it for the display-only state.
struct ADSBAircraft {
    int m_icao;                 // 24-bit ICAO aircraft address
    QString m_icaoHex;
    QString m_callsign;         // Flight callsign
    QString m_flight;           // Guess at flight number
    bool m_globalPosition;      // The current local-CPR reference chain was established by a global decode
    Real m_latitude;            // Latitude in decimal degrees
    Real m_longitude;           // Longitude in decimal degrees
    float m_radius;             // Horizontal containment radius limit (Rc) in metres
    int m_altitude;             // Altitude in feet (will be 0 if on surface)
    int m_pressureAltitude;     // Pressure altitude in feet for Map PFD altimeter (can be negative on surface)
    bool m_onSurface;           // Indicates if on surface or airborne
    bool m_altitudeGNSS;        // Altitude is GNSS HAE (Height above WGS-84 ellipsoid) rather than barometric alitute (relative to 29.92 Hg)
    float m_heading;            // Heading in degrees magnetic
    float m_track;              // Track in degrees true?
    int m_verticalRate;         // Vertical climb rate in ft/min
    QString m_emitterCategory;  // Aircraft type
    QString m_status;           // Aircraft status
    int m_squawk;               // Mode-A code
    bool m_spi;                 // Special position identification (IDENT) active
    Real m_range;               // Distance from station to aircraft
    Real m_azimuth;             // Azimuth from station to aircraft
    Real m_elevation;           // Elevation from station to aircraft
    QDateTime m_rxTime;         // When last frame received (can be long ago if reading from log file)
    QDateTime m_updateTime;     // Last time we updated data for this aircraft (used for determining when to remove an aircraft)

    int m_selAltitude;          // Selected altitude in MCP/FCU or FMS in feet
    int m_selHeading;           // Selected heading in MCP/FCU in degrees
    float m_baro;               // Aircraft baro setting in mb (Mode-S)
    float m_roll;               // In degrees
    int m_groundspeed;          // In knots
    float m_turnRate;           // In degrees per second
    int m_trueAirspeed;         // In knots
    int m_indicatedAirspeed;    // In knots
    float m_mach;               // Mach number
    bool m_autopilot;
    bool m_vnavMode;
    bool m_altHoldMode;
    bool m_approachMode;
    bool m_lnavMode;
    bool m_tcasOperational;     // Appears only to be true if TA/RA, false if TA ONLY

    bool m_bdsCapabilities[16][16]; // BDS capabilities are indicaited by BDS 1.7
    int m_adsbVersion;
    bool m_nicSupplementA;
    bool m_nicSupplementB;
    bool m_nicSupplementC;

    bool m_positionValid;       // Indicates if we have valid data for the above fields
    bool m_positionOnSurface;   // Air/surface state at the last accepted position
    bool m_altitudeValid;
    bool m_pressureAltitudeValid;
    bool m_onSurfaceValid;
    bool m_headingValid;
    bool m_trackValid;
    bool m_verticalRateValid;
    bool m_selAltitudeValid;
    bool m_selHeadingValid;
    bool m_baroValid;
    bool m_rollValid;
    bool m_groundspeedValid;
    bool m_turnRateValid;
    bool m_trueAirspeedValid;
    bool m_indicatedAirspeedValid;
    bool m_machValid;
    bool m_autopilotValid;
    bool m_vnavModeValid;
    bool m_altHoldModeValid;
    bool m_approachModeValid;
    bool m_lnavModeValid;
    bool m_tcasOperationalValid;

    bool m_bdsCapabilitiesValid;
    bool m_adsbVersionValid;
    bool m_nicSupplementAValid;
    bool m_nicSupplementBValid;
    bool m_nicSupplementCValid;

    // State for calculating position using two CPR frames
    bool m_cprValid[2];
    double m_cprLat[2];
    double m_cprLong[2];
    QDateTime m_cprTime[2];

    int m_adsbFrameCount;       // Number of ADS-B frames for this aircraft
    int m_modesFrameCount;      // Number of Mode-S frames for this aircraft
    int m_nonTransponderFrameCount;
    int m_tisBFrameCount;
    int m_adsrFrameCount;
    float m_minCorrelation;
    float m_maxCorrelation;
    float m_correlation;
    MovingAverageUtil<float, double, 100> m_correlationAvg;



    AircraftInformation *m_aircraftInfo; // Info about the aircraft from the database
    QString m_aircraft3DModel;    // 3D model for map based on aircraft type
    QString m_aircraftCat3DModel; // 3D model based on aircraft category
    // The same two choices made from the 2D livery icons, when there is no model pack to
    // choose from: icon names, such as A320_BAW (see Aircraft3DModels::liveryIcons())
    QString m_liveryIcon;
    QString m_catLiveryIcon;
    float m_modelAltitudeOffset;  // Altitude adjustment so aircraft model doesn't go underground
    float m_labelAltitudeOffset;  // How height to position label above aircraft
    QString m_flagName;         // Country flag, for the GUI to resolve an icon from
    // Resource paths of the icons shown in the table, resolved by the GUI
    QString m_airlineIconPath;
    QString m_sideviewIconPath;
    QString m_flagIconPath;
    QString m_flagIconURL;
    QString m_airlineIconURL;
    QString m_sideviewIconURL;

    // For animation on 3D map
    float m_runwayAltitude;
    bool m_runwayAltitudeValid;
    // Elevation of the airport the aircraft is nearest to, so heights above the ground
    // can be worked out for an arrival - m_runwayAltitude only ever covers a departure
    // we watched. Looked up when low, then kept until the aircraft moves away from it
    float m_fieldElevation;
    bool m_fieldElevationValid;
    QDateTime m_fieldElevationTime;     // When it was last looked up
    bool m_gearDown;
    bool m_tyresFlat;           // Tyre deflection animation is at its start, where a newly loaded model sits
    float m_flaps;              // 0 - no flaps, 1 - full flaps
    bool m_rotorStarted;        // Rotors started on 'Rotorcraft'
    bool m_engineStarted;       // Engines started (typically propellors)
    QDateTime m_engineAnimationTime; // When the engine/rotor animation was last sent, so it
                                     // can be re-asserted for a map opened after the start
    QDateTime m_positionDateTime;
    QDateTime m_orientationDateTime; // FIXME
    QDateTime m_headingDateTime;
    QDateTime m_trackDateTime;
    QDateTime m_altitudeDateTime;
    QDateTime m_indicatedAirspeedDateTime;
    QDateTime m_prevTrackDateTime;
    int m_prevTrack;
    float m_trackWhenHeadingSet;
    float m_pitchEst;           // Estimated pitch based on vertical rate
    float m_rollEst;            // Estimated roll based on rate of change in heading

    bool m_notified;            // Set when a notification has been made for this aircraft, so we don't repeat it

    // Set while decoding, cleared when the state is sent to the GUI
    bool m_isNew;               // First frame from this aircraft
    bool m_positionChanged;     // A point should be added to the flight path
    bool m_trailCleared;        // The flight path should be thrown away and started again
    bool m_mapTrailResetPending; // The external Map still needs the same reset
    QList<ADSBAircraftPosition> m_positionUpdates; // Every position since the previous GUI flush

    // Values shown in the table. These are the store for a good deal of decoded data,
    // not merely a display copy - getText, getLabel and the notification substitution
    // all read values back out of them
    QVariant m_cells[ADSBDEMOD_COLUMNS];
    quint64 m_dirtyCells[2];        // Cells changed since the table was last updated

    bool m_tcasAlert;               // Draw the TCAS cell in red

    // Active TCAS Resolution Advisory, decoded from an RA broadcast (DF16 MV with
    // VDS=0x30, DF17 TC=28 ST=2, or DF20/21 BDS 3,0 - decodeRABroadcast()). Broadcasts
    // repeat while the RA is active, so the tracker's tick() expires the state when
    // they stop, and the terminated flag shortly after it is seen
    bool m_raActive = false;
    bool m_raCorrective = false;    // Corrective (fly the sense now) vs preventive
    bool m_raDownward = false;      // Downward sense (descend) vs upward (climb)
    bool m_raPositive = false;      // Positive (climb/descend) vs vertical speed limit
    bool m_raTerminated = false;    // RA terminated - clear of conflict
    bool m_raMultipleThreats = false;
    bool m_raOppositeSense = false; // Multiple threats require opposite-sense RAs
    unsigned m_raThreatIcao = 0;    // Threat's Mode S address, 0 if not identified
    QDateTime m_raDateTime;         // When the last RA broadcast was received

    ADSBAircraft() :
        m_icao(0),
        m_globalPosition(false),
        m_latitude(0),
        m_longitude(0),
        m_radius(0.0f),
        m_altitude(0),
        m_pressureAltitude(0),
        m_onSurface(false),
        m_altitudeGNSS(false),
        m_heading(0),
        m_track(0),
        m_verticalRate(0),
        m_squawk(0),
        m_spi(false),
        m_range(0.0f),
        m_azimuth(0),
        m_elevation(0),
        m_selAltitude(0),
        m_selHeading(0),
        m_baro(0),
        m_roll(0.0f),
        m_groundspeed(0),
        m_turnRate(0.0f),
        m_trueAirspeed(0),
        m_indicatedAirspeed(0),
        m_mach(0.0f),
        m_autopilot(false),
        m_vnavMode(false),
        m_altHoldMode(false),
        m_approachMode(false),
        m_lnavMode(false),
        m_tcasOperational(false),
        m_adsbVersion(0),
        m_nicSupplementA(false),
        m_nicSupplementB(false),
        m_nicSupplementC(false),
        m_positionValid(false),
        m_positionOnSurface(false),
        m_altitudeValid(false),
        m_pressureAltitudeValid(false),
        m_onSurfaceValid(false),
        m_headingValid(false),
        m_trackValid(false),
        m_verticalRateValid(false),
        m_selAltitudeValid(false),
        m_selHeadingValid(false),
        m_baroValid(false),
        m_rollValid(false),
        m_groundspeedValid(false),
        m_turnRateValid(false),
        m_trueAirspeedValid(false),
        m_indicatedAirspeedValid(false),
        m_machValid(false),
        m_autopilotValid(false),
        m_vnavModeValid(false),
        m_altHoldModeValid(false),
        m_approachModeValid(false),
        m_lnavModeValid(false),
        m_tcasOperationalValid(false),
        m_bdsCapabilitiesValid(false),
        m_adsbVersionValid(false),
        m_nicSupplementAValid(false),
        m_nicSupplementBValid(false),
        m_nicSupplementCValid(false),
        m_adsbFrameCount(0),
        m_modesFrameCount(0),
        m_nonTransponderFrameCount(0),
        m_tisBFrameCount(0),
        m_adsrFrameCount(0),
        m_minCorrelation(INFINITY),
        m_maxCorrelation(-INFINITY),
        m_correlation(0.0f),
        m_tcasAlert(false),
        m_aircraftInfo(nullptr),
        m_modelAltitudeOffset(0.0f),
        m_labelAltitudeOffset(5.0f),
        m_runwayAltitude(0.0),
        m_runwayAltitudeValid(false),
        m_fieldElevation(0.0),
        m_fieldElevationValid(false),
        m_gearDown(false),
        m_tyresFlat(true),
        m_flaps(0.0),
        m_rotorStarted(false),
        m_engineStarted(false),
        m_prevTrack(0),
        m_trackWhenHeadingSet(0.0f),
        m_pitchEst(0.0),
        m_rollEst(0.0),
        m_notified(false),
        m_isNew(true),
        m_positionChanged(false),
        m_trailCleared(false),
        m_mapTrailResetPending(false)
    {
        for (int i = 0; i < 2; i++) {
            m_cprValid[i] = false;
            m_cprLat[i] = 0.0;
            m_cprLong[i] = 0.0;
        }
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                m_bdsCapabilities[i][j] = false;
            }
        }
        for (int i = 0; i < 2; i++) {
            m_dirtyCells[i] = 0;
        }
    }

    // The table model reads these; setCell records which cells changed so only
    // those are repainted
    void setCell(int col, const QVariant& value)
    {
        if (m_cells[col] != value)
        {
            m_cells[col] = value;
            m_dirtyCells[col >> 6] |= 1ULL << (col & 63);
        }
    }
    const QVariant& cell(int col) const { return m_cells[col]; }
    QString cellText(int col) const { return m_cells[col].toString(); }
    bool cellsDirty() const { return m_dirtyCells[0] || m_dirtyCells[1]; }
    bool cellDirty(int col) const { return (m_dirtyCells[col >> 6] >> (col & 63)) & 1; }
    void clearDirtyCells() { m_dirtyCells[0] = 0; m_dirtyCells[1] = 0; }
    void markAllCellsDirty() { m_dirtyCells[0] = ~0ULL; m_dirtyCells[1] = ~0ULL; }

    // The range cell holds a number so that the table sorts it numerically; this is
    // how it is shown, and how it reads in notifications
    static QString rangeText(const QVariant& range)
    {
        return range.isNull() ? QString() : QString::number(range.toDouble(), 'f', 1);
    }

    QString getImage() const;
    // Icon showing the airline's livery, derived from the matched 3D model.
    // Empty when there is no model match or no icon for the livery
    QString getLiveryImage() const;
    // links makes the ICAO, registration and callsign clickable, selecting the
    // aircraft in the Aircraft feature. Only passed where that can actually happen -
    // this same text feeds the demodulator's own map, whose delegate has no link
    // handler, and there may be no Aircraft feature to select anything in
    QString getText(const ADSBDemodSettings *settings, bool all=false, bool links=false) const;
    // Label to use for aircraft on map
    QString getLabel(const ADSBDemodSettings *settings, QDateTime& dateTime) const;

    // Name to use when selected as a target (E.g. for use as target name in Rotator Controller)
    QString targetName() const
    {
        if (!m_callsign.isEmpty())
            return QString("Callsign: %1").arg(m_callsign);
        else
            return QString("ICAO: %1").arg(m_icao, 0, 16);
    }

    void setOnSurface(const QDateTime& dateTime);
    void setAltitude(int altitudeFt, bool gnss, const QDateTime& dateTime, const ADSBDemodSettings& settings);
    void setVerticalRate(int verticalRate, const ADSBDemodSettings& settings);
    void setGroundspeed(float groundspeed, const ADSBDemodSettings& settings);
    void setTrueAirspeed(int airspeed, const ADSBDemodSettings& settings);
    void setIndicatedAirspeed(int airspeed, const QDateTime& dateTime, const ADSBDemodSettings& settings);
    void setTrack(float track, const QDateTime& dateTime);
    void setHeading(float heading, const QDateTime& dateTime);
};


#endif // INCLUDE_ADSBAIRCRAFT_H
