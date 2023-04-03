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

#ifndef INCLUDE_FEATURE_SATELLITETRACKERSETTINGS_H_
#define INCLUDE_FEATURE_SATELLITETRACKERSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QHash>
#include <QList>
#include <QTime>

class Serializable;

#define SAT_COL_COLUMNS 18

struct SatelliteTrackerSettings
{
    struct SatelliteDeviceSettings
    {
        int m_deviceSetIndex;           //!< Device set index in the list of device sets of the SDRangel instance
        QString m_presetGroup;          //!< Preset to load to device set
        quint64 m_presetFrequency;
        QString m_presetDescription;
        QList<int> m_doppler;           //!< Which channels to apply Doppler correction to, if any
        bool m_startOnAOS;              //!< Start acquistion on AOS
        bool m_stopOnLOS;               //!< Stop acquistion on LOS
        bool m_startStopFileSink;       //!< Start&stop file sinks recording on AOS/LOS
        quint64 m_frequency;            //!< Optional center frequency to set (in Hz), to override preset value
        QString m_aosCommand;           //!< Command/script to execute on AOS
        QString m_losCommand;           //!< Command/script to execute on LOS
        SatelliteDeviceSettings();

        void getDebugString(std::ostringstream& ostr);
    };

    double m_latitude;                  //!< Antenna location, degrees
    double m_longitude;
    double m_heightAboveSeaLevel;       //!< In metres
    QString m_target;                   //!< Target satellite
    QList<QString> m_satellites;        //!< Selected satellites
    QList<QString> m_tles;              //!< TLE URLs
    QString m_dateTime;                 //!< Date/time for observation, or "" for now (UTC or local as per m_utc)
    int m_minAOSElevation;              //!< Minimum elevation for AOS
    int m_minPassElevation;             //!< Minimum elevation for a pass
    int m_rotatorMaxAzimuth;            //!< Maximum rotator azimuth 360/450
    int m_rotatorMaxElevation;          //!< Maximum rotator elevation 90/180
    enum AzElUnits {DMS, DM, D, Decimal} m_azElUnits;
    int m_groundTrackPoints;            //!< Number of points in ground tracks
    QString m_dateFormat;               //!< Format used for displaying dates in the GUI
    bool m_utc;                         //!< Set/display times as UTC rather than local
    float m_updatePeriod;               //!< How long in seconds between updates of satellite's position
    float m_dopplerPeriod;              //!< How long in seconds between Doppler corrections
    int m_predictionPeriod;             //!< How many days ahead to predict passes in
    QTime m_passStartTime;              //!< Time after which pass must start
    QTime m_passFinishTime;             //!< Time before which pass must finish
    float m_defaultFrequency;           //!< Frequency used for Doppler & path loss calculation in satellite table
    bool m_drawOnMap;
    bool m_autoTarget;                  //!< Automatically select target on AOS
    QString m_aosSpeech;                //!< Text to say on AOS
    QString m_losSpeech;                //!< Text to say on LOS
    QString m_aosCommand;               //!< Command/script to execute on AOS
    QString m_losCommand;               //!< Command/script to execute on LOS
    bool m_chartsDarkTheme;             //!< Set dark theme for charts (effective for GUI only)
    QHash<QString, QList<SatelliteDeviceSettings *> *> m_deviceSettings; //!< Settings for each device set for each satellite
    bool m_replayEnabled;               //!< Replay a pass in the past, by setting date and time to m_replayStartDateTime
    QDateTime m_replayStartDateTime;    //!< Time to start the replay at
    bool m_sendTimeToMap;               //!< Send time to map when start pressed
    enum DateTimeSelect {NOW, CUSTOM, FROM_MAP, FROM_FILE} m_dateTimeSelect;
    QString m_mapFeature;               //!< Which feature when FROM_MAP
    QString m_fileInputDevice;          //!< Which device when FROM_FILE
    enum Rotators {ALL_ROTATORS, NO_ROTATORS, MATCHING_TARGET} m_drawRotators; //!< Which rotators to draw on polar chart

    int m_columnSort;                    //!< Which column is used for sorting (-1 for none)
    Qt::SortOrder m_columnSortOrder;
    int m_columnIndexes[SAT_COL_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[SAT_COL_COLUMNS];  //!< Size of the coumns in the table

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

    SatelliteTrackerSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serializeStringList(const QList<QString>& strings) const;
    void deserializeStringList(const QByteArray& data, QList<QString>& strings);
    QByteArray serializeDeviceSettings(QHash<QString, QList<SatelliteDeviceSettings *> *> deviceSettings) const;
    void deserializeDeviceSettings(const QByteArray& data, QHash<QString, QList<SatelliteDeviceSettings *> *>& deviceSettings);
    void applySettings(const QStringList& settingsKeys, const SatelliteTrackerSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif // INCLUDE_FEATURE_SATELLITETRACKERSETTINGS_H_
