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

#ifndef INCLUDE_FEATURE_APRSSETTINGS_H_
#define INCLUDE_FEATURE_APRSSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"

class Serializable;

// Number of columns in the tables
#define APRS_PACKETS_TABLE_COLUMNS      6
#define APRS_WEATHER_TABLE_COLUMNS      15
#define APRS_STATUS_TABLE_COLUMNS       7
#define APRS_MESSAGES_TABLE_COLUMNS     5
#define APRS_TELEMETRY_TABLE_COLUMNS    17
#define APRS_MOTION_TABLE_COLUMNS       7

struct APRSSettings
{
    struct AvailableChannel
    {
        int m_deviceSetIndex;
        int m_channelIndex;
        QString m_type;

        AvailableChannel() = default;
        AvailableChannel(const AvailableChannel&) = default;
        AvailableChannel& operator=(const AvailableChannel&) = default;
    };

    QString m_igateServer;
    int m_igatePort;
    QString m_igateCallsign;
    QString m_igatePasscode;
    QString m_igateFilter;
    bool m_igateEnabled;
    enum StationFilter {
        ALL, STATIONS, OBJECTS, WEATHER, TELEMETRY, COURSE_AND_SPEED
    } m_stationFilter;
    QString m_filterAddressee;
    enum AltitudeUnits {
        FEET, METRES
    } m_altitudeUnits;
    enum SpeedUnits {
        KNOTS, MPH, KPH
    } m_speedUnits;
    enum TemperatureUnits {
        FAHRENHEIT, CELSIUS
    } m_temperatureUnits;
    enum RainfallUnits {
        HUNDREDTHS_OF_AN_INCH, MILLIMETRE
    } m_rainfallUnits;
    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    int m_packetsTableColumnIndexes[APRS_PACKETS_TABLE_COLUMNS];//!< How the columns are ordered in the table
    int m_packetsTableColumnSizes[APRS_PACKETS_TABLE_COLUMNS];  //!< Size of the columns in the table

    int m_weatherTableColumnIndexes[APRS_WEATHER_TABLE_COLUMNS];
    int m_weatherTableColumnSizes[APRS_WEATHER_TABLE_COLUMNS];

    int m_statusTableColumnIndexes[APRS_STATUS_TABLE_COLUMNS];
    int m_statusTableColumnSizes[APRS_STATUS_TABLE_COLUMNS];

    int m_messagesTableColumnIndexes[APRS_MESSAGES_TABLE_COLUMNS];
    int m_messagesTableColumnSizes[APRS_MESSAGES_TABLE_COLUMNS];

    int m_telemetryTableColumnIndexes[APRS_TELEMETRY_TABLE_COLUMNS];
    int m_telemetryTableColumnSizes[APRS_TELEMETRY_TABLE_COLUMNS];

    int m_motionTableColumnIndexes[APRS_MOTION_TABLE_COLUMNS];
    int m_motionTableColumnSizes[APRS_MOTION_TABLE_COLUMNS];

    APRSSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const APRSSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;

    static const QStringList m_altitudeUnitNames;
    static const QStringList m_speedUnitNames;
    static const QStringList m_temperatureUnitNames;
};

#endif // INCLUDE_FEATURE_APRSSETTINGS_H_
