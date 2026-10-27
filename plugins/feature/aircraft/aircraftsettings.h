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

#ifndef INCLUDE_FEATURE_AIRCRAFTSETTINGS_H_
#define INCLUDE_FEATURE_AIRCRAFTSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <QList>
#include <QSharedPointer>
#include <QRegularExpression>

class Serializable;

// Number of columns in the tables. These size the saved width and order arrays, and
// the header's own sectionMoved/sectionResized hand back a logical index straight out of
// the model - so a column added to a model without changing the count here writes off
// the end of the array. aircrafttablemodels.cpp asserts the two agree at compile time.
#define AIRCRAFT_COLUMNS 18
// Flight, Reg, From, To, Out, Off, On, In, First seen, Last seen, LS, OC, FP, Docs,
// Protocols
#define FLIGHT_COLUMNS 17

struct AircraftSettings
{
    // What a notification rule matches against
    enum NotificationMatch {
        MATCH_ICAO,
        MATCH_REG,
        MATCH_FLIGHT,
        MATCH_TYPE
    };

    struct NotificationSettings {
        int m_matchColumn;      // One of enum NotificationMatch
        QString m_regExp;
        QString m_speech;
        QString m_command;
        QRegularExpression m_regularExpression;

        NotificationSettings();
        void updateRegularExpression();
    };
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

    int m_removalMins;      // Archive an aircraft after this many minutes without a report
    // How long a position stays believable, and so how long the aircraft stays on the
    // Map. Separate figures because the protocols report at wildly different rates: ADS-B
    // is about once a second, while an ACARS, VDL-2 or HFDL position can be the only one
    // for many minutes. Applies to the 2D and 3D maps alike.
    int m_adsbPositionMins;
    int m_acarsPositionMins;
    int m_retentionDays;    // Discard archived aircraft after this many days; 0 keeps them
    bool m_atcLabels;       // Display route, altitude and speed in Map labels
    bool m_atcCallsigns;    // Map labels use the airline's callsign (SPEEDBIRD) not its code (BAW)
    // Keep the furthest aircraft ever heard on the Map, at the position it was in when
    // it set the record. Serialised at key 44.
    bool m_displayMaxRangeOnMap;
    // The Statistics tab's records - furthest, fastest, highest - serialised by the
    // tracker. Kept opaque here so a new statistic does not need a new settings key.
    QByteArray m_statistics;
    //!< How the splitters divide the space. One blob for all of them, so that adding
    //!< another splitter later does not need another settings key.
    //!< Serialised at key 43: SimpleDeserializer REJECTS THE WHOLE BLOB if an id appears
    //!< twice, so a clashing key does not lose one setting, it loses every setting the
    //!< feature has. This was first written at 41, which m_adsbPositionMins already had.
    QByteArray m_splitterStates;
    bool m_displayStatistics;   // Statistics table shown in the splitter
    bool m_displayChart;        // Message rate chart shown in the splitter
    // Prefer a model in the right airline livery over a model of exactly the right type
    bool m_favourLivery;
    bool m_useLiveryIcons;  // Use aircraft icons with airline liveries on 2D map, instead of black icons

    // Feeding received ACARS on to aggregators, in acarsdec style JSON. Here rather than
    // in the ACARS demodulator because a receiver runs one of those per band - VHF,
    // VDL-2, HFDL - and the feed is a property of the station, not of any one of them.
    bool m_feedEnabled;
    QString m_feedStationId;        //!< Station ident sent with each message
    bool m_feedAirframes;
    QString m_feedAirframesHost;
    int m_feedAirframesPort;
    bool m_feedAirframesTcp;        //!< TCP (as the airframes.io wizard assigns) or UDP
    bool m_feedAvdelphi;
    QString m_feedAvdelphiHost;
    int m_feedAvdelphiPort;
    bool m_feedAvdelphiTcp;
    //!< Draw the polar coverage of each protocol on the Map - see
    //!< AircraftTracker::updateCoverage()
    bool m_displayCoverage;
    //!< Protocols whose coverage is NOT drawn, one bit per AircraftReport::Protocol. Held
    //!< as the ones hidden rather than the ones shown, so a protocol added later is drawn
    //!< without anyone having to find the option.
    quint32 m_coverageHiddenProtocols;
    bool coverageShown(int protocol) const { return !(m_coverageHiddenProtocols & (1u << protocol)); }
    QString m_databaseFilename;     // Where the session is kept; empty means the default location
    QStringList m_disabledSources;  // Channel URIs not to accept reports from; empty accepts all

    // The demodulators that can feed this feature, and what to call them
    static const QStringList& sourceURIs();
    static QString sourceName(const QString& uri);
    static QString defaultDatabaseFilename();
    QList<QSharedPointer<NotificationSettings>> m_notificationSettings;

    int m_columnIndexes[AIRCRAFT_COLUMNS];
    int m_columnSizes[AIRCRAFT_COLUMNS];
    int m_flightColumnIndexes[FLIGHT_COLUMNS];
    int m_flightColumnSizes[FLIGHT_COLUMNS];

    AircraftSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    void applySettings(const QStringList& settingsKeys, const AircraftSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;

    QByteArray serializeNotificationSettings(const QList<QSharedPointer<NotificationSettings>>& notificationSettings) const;
    static void deserializeNotificationSettings(const QByteArray& data, QList<QSharedPointer<NotificationSettings>>& notificationSettings);
};

QDataStream& operator<<(QDataStream& out, const QSharedPointer<AircraftSettings::NotificationSettings>& settings);
QDataStream& operator>>(QDataStream& in, QSharedPointer<AircraftSettings::NotificationSettings>& settings);

#endif // INCLUDE_FEATURE_AIRCRAFTSETTINGS_H_
