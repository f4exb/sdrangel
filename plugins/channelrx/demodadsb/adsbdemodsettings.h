///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_

#include <QtGlobal>
#include <QString>
#include <QRegularExpression>

#include <stdint.h>
#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the table
#define ADSBDEMOD_COLUMNS           78

// ADS-B table columns
#define ADSB_COL_ICAO               0
#define ADSB_COL_CALLSIGN           1
#define ADSB_COL_ATC_CALLSIGN       2
#define ADSB_COL_MODEL              3
#define ADSB_COL_TYPE               4
#define ADSB_COL_SIDEVIEW           5
#define ADSB_COL_AIRLINE            6
#define ADSB_COL_COUNTRY            7
#define ADSB_COL_GROUND_SPEED       8
#define ADSB_COL_TRUE_AIRSPEED      9
#define ADSB_COL_INDICATED_AIRSPEED 10
#define ADSB_COL_MACH               11
#define ADSB_COL_SEL_ALTITUDE       12
#define ADSB_COL_ALTITUDE           13
#define ADSB_COL_VERTICALRATE       14
#define ADSB_COL_SEL_HEADING        15
#define ADSB_COL_HEADING            16
#define ADSB_COL_TRACK              17
#define ADSB_COL_TURNRATE           18
#define ADSB_COL_ROLL               19
#define ADSB_COL_RANGE              20
#define ADSB_COL_AZEL               21
#define ADSB_COL_CATEGORY           22
#define ADSB_COL_STATUS             23
#define ADSB_COL_SQUAWK             24
#define ADSB_COL_IDENT              25
#define ADSB_COL_REGISTRATION       26
#define ADSB_COL_REGISTERED         27
#define ADSB_COL_MANUFACTURER       28
#define ADSB_COL_OWNER              29
#define ADSB_COL_OPERATOR_ICAO      30
#define ADSB_COL_AP                 31
#define ADSB_COL_V_MODE             32
#define ADSB_COL_L_MODE             33
#define ADSB_COL_TCAS               34
#define ADSB_COL_ACAS               35
#define ADSB_COL_RA                 36
#define ADSB_COL_MAX_SPEED          37
#define ADSB_COL_VERSION            38
#define ADSB_COL_LENGTH             39
#define ADSB_COL_WIDTH              40
#define ADSB_COL_BARO               41
#define ADSB_COL_HEADWIND           42
#define ADSB_COL_EST_AIR_TEMP       43
#define ADSB_COL_WIND_SPEED         44
#define ADSB_COL_WIND_DIR           45
#define ADSB_COL_STATIC_PRESSURE    46
#define ADSB_COL_STATIC_AIR_TEMP    47
#define ADSB_COL_HUMIDITY           48
#define ADSB_COL_LATITUDE           49
#define ADSB_COL_LONGITUDE          50
#define ADSB_COL_IC                 51
#define ADSB_COL_TIME               52
#define ADSB_COL_FRAMECOUNT         53
#define ADSB_COL_ADSB_FRAMECOUNT    54
#define ADSB_COL_MODES_FRAMECOUNT   55
#define ADSB_COL_NON_TRANSPONDER    56
#define ADSB_COL_TIS_B_FRAMECOUNT   57
#define ADSB_COL_ADSR_FRAMECOUNT    58
#define ADSB_COL_RADIUS             59
#define ADSB_COL_NACP               60
#define ADSB_COL_NACV               61
#define ADSB_COL_GVA                62
#define ADSB_COL_NIC                63
#define ADSB_COL_NIC_BARO           64
#define ADSB_COL_SIL                65
#define ADSB_COL_CORRELATION        66
#define ADSB_COL_RSSI               67
#define ADSB_COL_FLIGHT_STATUS      68
#define ADSB_COL_DEP                69
#define ADSB_COL_ARR                70
#define ADSB_COL_STOPS              71
#define ADSB_COL_STD                72
#define ADSB_COL_ETD                73
#define ADSB_COL_ATD                74
#define ADSB_COL_STA                75
#define ADSB_COL_ETA                76
#define ADSB_COL_ATA                77

#define ADSB_IC_MAX                 63

struct ADSBDemodSettings
{
    struct NotificationSettings {
        int m_matchColumn;
        QString m_regExp;
        QString m_speech;
        QString m_command;
        QRegularExpression m_regularExpression;
        bool m_autoTarget;

        NotificationSettings();
        void updateRegularExpression();
    };

    int32_t m_inputFrequencyOffset;
    Real m_rfBandwidth;
    Real m_correlationThreshold; //!< Correlation power threshold in dB
    int m_chipsThreshold; //!< How many chips in preamble can be incorrect for Mode S
    int m_samplesPerBit;
    int m_removeTimeout;                //!< Time in seconds before removing an aircraft, unless a new frame is received

    bool m_feedEnabled;
    bool m_exportClientEnabled;
    QString m_exportClientHost;
    uint16_t m_exportClientPort;
    enum FeedFormat {
        BeastBinary,
        BeastHex
    } m_exportClientFormat;
    bool m_exportServerEnabled;
    uint16_t m_exportServerPort;
    bool m_importEnabled;
    QString m_importHost;
    QString m_importUsername;
    QString m_importPassword;
    QString m_importParameters;
    float m_importPeriod;
    QString m_importMinLatitude;
    QString m_importMaxLatitude;
    QString m_importMinLongitude;
    QString m_importMaxLongitude;

    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_columnIndexes[ADSBDEMOD_COLUMNS];//!< How the columns are ordered in the table
    int m_columnSizes[ADSBDEMOD_COLUMNS];  //!< Size of the coumns in the table

    Serializable *m_channelMarker;

    float m_airportRange;               //!< How far away should we display airports (km)
    enum AirportType {
        Small,
        Medium,
        Large,
        Heliport
    } m_airportMinimumSize;             //!< What's the minimum size airport that should be displayed
    bool m_displayHeliports;            //!< Whether to display heliports on the map
    bool m_flightPaths;                 //!< Whether to display flight paths
    bool m_allFlightPaths;              //!< Whether to display flight paths for all aircraft
    bool m_siUnits;                     //!< Uses m,kph rather than ft/knts
    QString m_tableFontName;            //!< Font to use for table
    int m_tableFontSize;
    bool m_displayDemodStats;
    bool m_demodModeS;                  //!< Demodulate all Mode-S frames, not just ADS-B
    QString m_amDemod;                  //!< AM Demod to tune to selected ATC frequency
    bool m_autoResizeTableColumns;
    int m_interpolatorPhaseSteps;
    float m_interpolatorTapsPerPhase;

    QList<NotificationSettings *> m_notificationSettings;
    QString m_aviationstackAPIKey;      //!< aviationstack.com API key
    QString m_checkWXAPIKey;            //!< checkwxapi.com API key
    QString m_maptilerAPIKey;           //!< maptiler.com API key (for osm/satellite map)

    QString m_logFilename;
    bool m_logEnabled;

    QStringList m_airspaces;            //!< Airspace names to display
    float m_airspaceRange;              //!< How far away we display airspace (mkm)
    QString m_mapProvider;
    enum MapType {
        AVIATION_LIGHT,                 //!< White map with no place names
        AVIATION_DARK,
        STREET,
        SATELLITE
    } m_mapType;
    bool m_displayNavAids;
    bool m_displayPhotos;
    Serializable *m_rollupState;
    bool m_verboseModelMatching;
    int m_aircraftMinZoom;

    bool m_atcLabels;
    bool m_atcCallsigns;
    int m_transitionAlt;

    float m_qnh;
    bool m_manualQNH;
    bool m_displayCoverage;
    bool m_displayChart;
    bool m_displayOrientation;
    bool m_displayRadius;
    bool m_displayIC[ADSB_IC_MAX];
    QString m_flightPathPaletteName;
    bool m_favourLivery;            //!< Favour airline livery over aircraft type when exact 3D model isn't available

    const QVariant *m_flightPathPalette;

    ADSBDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    QByteArray serializeNotificationSettings(QList<NotificationSettings *> notificationSettings) const;
    void deserializeNotificationSettings(const QByteArray& data, QList<NotificationSettings *>& notificationSettings);
    void applySettings(const QStringList& settingsKeys, const ADSBDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;
    void applyPalette();

    static const QVariant m_rainbowPalette[8];
    static const QVariant m_pastelPalette[8];
    static const QVariant m_spectralPalette[8];
    static const QVariant m_bluePalette[8];
    static const QVariant m_purplePalette[8];
    static const QVariant m_greyPalette[8];

    static const QHash<QString, const QVariant *> m_palettes;
};

#endif /* PLUGINS_CHANNELRX_DEMODADSB_ADSBDEMODSETTINGS_H_ */
