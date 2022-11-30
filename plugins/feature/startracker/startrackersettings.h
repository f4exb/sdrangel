///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_STARTRACKERSETTINGS_H_
#define INCLUDE_FEATURE_STARTRACKERSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"

class Serializable;

struct StarTrackerSettings
{
    QString m_ra;
    QString m_dec;
    double m_latitude;
    double m_longitude;
    QString m_target;           // Sun, Moon, Custom
    QString m_dateTime;         // Date/time for observation, or "" for now
    QString m_refraction;       // Refraction correction. "None", "Saemundsson" or "Positional Astronomy Library"
    double m_pressure;          // Air pressure in millibars
    double m_temperature;       // Air temperature in C
    double m_humidity;          // Humidity in %
    double m_heightAboveSeaLevel; // In metres
    double m_temperatureLapseRate; // In K/km
    double m_frequency;         // Observation frequency in Hz
    double m_beamwidth;         // Halfpower beamwidth in degrees
    uint16_t m_serverPort;
    bool m_enableServer;        // Enable Stellarium server
    enum AzElUnits {DMS, DM, D, Decimal} m_azElUnits; // This needs to match DMSSpinBox::DisplayUnits
    enum SolarFluxData {DRAO_2800, L_245, L_410, L_610, L_1415, L2695, L_4995, L_8800, L_15400, TARGET_FREQ} m_solarFluxData; // What Solar flux density data to display
    enum SolarFluxUnits {SFU, JANSKY, WATTS_M_HZ} m_solarFluxUnits;
    float m_updatePeriod;
    bool m_jnow;                // Use JNOW epoch rather than J2000
    bool m_drawSunOnMap;
    bool m_drawMoonOnMap;
    bool m_drawStarOnMap;
    bool m_chartsDarkTheme;     // Dark theme for charts
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    double m_az;                // Azimuth for Custom Az/El
    double m_el;                // Elevation for Custom Az/El
    double m_l;                 // Galactic longitude for Custom l/b
    double m_b;                 // Galactic lattiude for Custom l/b
    bool m_link;                // Link settings to Radio Astronomy plugin
    QString m_owmAPIKey;        // API key for openweathermap.org
    int m_weatherUpdatePeriod;  // Time in minutes between weather updates
    double m_azOffset;
    double m_elOffset;
    bool m_drawSunOnSkyTempChart;
    bool m_drawMoonOnSkyTempChart;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    StarTrackerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const StarTrackerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;
};

#endif // INCLUDE_FEATURE_STARTRACKERSETTINGS_H_
