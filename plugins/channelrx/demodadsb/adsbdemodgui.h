///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_ADSBDEMODGUI_H
#define INCLUDE_ADSBDEMODGUI_H

#include <QTableWidgetItem>
#include <QMenu>
#include <QGeoCoordinate>
#include <QDateTime>
#include <QAbstractListModel>
#include <QProgressDialog>
#include <QTextToSpeech>
#include <QRandomGenerator>
#include <QNetworkAccessManager>
#include <QtCharts>

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "util/aviationweather.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "util/movingaverage.h"
#include "util/flightinformation.h"
#include "util/openaip.h"
#include "util/planespotters.h"
#include "settings/rollupstate.h"
#include "maincore.h"

#include "SWGMapItem.h"

#include "adsbdemodsettings.h"
#include "util/ourairportsdb.h"
#include "util/osndb.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ADSBDemod;
class WebAPIAdapterInterface;
class HttpDownloadManager;
class ADSBDemodGUI;
class ADSBOSMTemplateServer;
class CheckList;
class AircraftModel;

namespace Ui {
    class ADSBDemodGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

// Custom widget to allow formatted decimal numbers to be sorted numerically
class CustomDoubleTableWidgetItem : public QTableWidgetItem
{
public:
    CustomDoubleTableWidgetItem(const QString text = QString("")) :
        QTableWidgetItem(text)
    {
    }

    bool operator <(const QTableWidgetItem& other) const
    {
        // Treat "" as less than 0
        QString thisText = text();
        QString otherText = other.text();
        if (thisText == "")
            return true;
        if (otherText == "")
            return false;
        return thisText.toDouble() < otherText.toDouble();
    }
};

// Data about an aircraft extracted from ADS-B and Mode-S frames
struct Aircraft {
    int m_icao;                 // 24-bit ICAO aircraft address
    QString m_icaoHex;
    QString m_callsign;         // Flight callsign
    QString m_flight;           // Guess at flight number
    bool m_globalPosition;      // Position has been determined from global decode
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

    bool m_isTarget;            // Are we targeting this aircraft (sending az/el to rotator)
    bool m_isHighlighted;       // Are we highlighting this aircraft in the table and map
    bool m_showAll;

    QList<QVariantList> m_coordinates; // Coordinates we've recorded the aircraft at, split up in to altitude ranges
    QList<QVariantList> m_recentCoordinates; // Last 20 seconds of coordinates for ATC mode
    QList<QDateTime> m_coordinateDateTimes;
    QList<int> m_coordinateColors; // 0-7 index to 8 color palette according to altitude
    QList<int> m_recentCoordinateColors;
    int m_lastColor;

    AircraftInformation *m_aircraftInfo; // Info about the aircraft from the database
    QString m_aircraft3DModel;    // 3D model for map based on aircraft type
    QString m_aircraftCat3DModel; // 3D model based on aircraft category
    float m_modelAltitudeOffset;  // Altitude adjustment so aircraft model doesn't go underground
    float m_labelAltitudeOffset;  // How height to position label above aircraft
    ADSBDemodGUI *m_gui;
    QString m_flagIconURL;
    QString m_airlineIconURL;
    QString m_sideviewIconURL;

    // For animation on 3D map
    float m_runwayAltitude;
    bool m_runwayAltitudeValid;
    bool m_gearDown;
    float m_flaps;              // 0 - no flaps, 1 - full flaps
    bool m_rotorStarted;        // Rotors started on 'Rotorcraft'
    bool m_engineStarted;       // Engines started (typically propellors)
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

    // GUI table items for above data
    QTableWidgetItem *m_icaoItem;
    QTableWidgetItem *m_callsignItem;
    QTableWidgetItem* m_atcCallsignItem;
    QTableWidgetItem *m_modelItem;
    QTableWidgetItem *m_typeItem;
    QTableWidgetItem *m_sideviewItem;
    QTableWidgetItem *m_airlineItem;
    QTableWidgetItem *m_latitudeItem;
    QTableWidgetItem *m_longitudeItem;
    QTableWidgetItem *m_altitudeItem;
    QTableWidgetItem *m_headingItem;
    QTableWidgetItem *m_trackItem;
    QTableWidgetItem *m_verticalRateItem;
    CustomDoubleTableWidgetItem *m_rangeItem;
    QTableWidgetItem *m_azElItem;
    QTableWidgetItem *m_emitterCategoryItem;
    QTableWidgetItem *m_statusItem;
    QTableWidgetItem *m_squawkItem;
    QTableWidgetItem *m_identItem;
    QTableWidgetItem *m_registrationItem;
    QTableWidgetItem *m_countryItem;
    QTableWidgetItem *m_registeredItem;
    QTableWidgetItem *m_manufacturerNameItem;
    QTableWidgetItem *m_ownerItem;
    QTableWidgetItem *m_operatorICAOItem;
    QTableWidgetItem *m_interogatorCodeItem;
    QTableWidgetItem *m_timeItem;
    QTableWidgetItem *m_totalFrameCountItem;
    QTableWidgetItem *m_adsbFrameCountItem;
    QTableWidgetItem *m_modesFrameCountItem;
    QTableWidgetItem *m_nonTransponderItem;
    QTableWidgetItem *m_tisBFrameCountItem;
    QTableWidgetItem *m_adsrFrameCountItem;
    QTableWidgetItem *m_radiusItem;
    QTableWidgetItem *m_nacpItem;
    QTableWidgetItem *m_nacvItem;
    QTableWidgetItem *m_gvaItem;
    QTableWidgetItem *m_nicItem;
    QTableWidgetItem *m_nicBaroItem;
    QTableWidgetItem *m_silItem;
    QTableWidgetItem *m_correlationItem;
    QTableWidgetItem *m_rssiItem;
    QTableWidgetItem *m_flightStatusItem;
    QTableWidgetItem *m_depItem;
    QTableWidgetItem *m_arrItem;
    QTableWidgetItem *m_stopsItem;
    QTableWidgetItem *m_stdItem;
    QTableWidgetItem *m_etdItem;
    QTableWidgetItem *m_atdItem;
    QTableWidgetItem *m_staItem;
    QTableWidgetItem *m_etaItem;
    QTableWidgetItem *m_ataItem;
    QTableWidgetItem *m_selAltitudeItem;
    QTableWidgetItem *m_selHeadingItem;
    QTableWidgetItem *m_baroItem;
    QTableWidgetItem *m_apItem;
    QTableWidgetItem *m_vModeItem;
    QTableWidgetItem *m_lModeItem;
    QTableWidgetItem *m_tcasItem;
    QTableWidgetItem *m_acasItem;
    QTableWidgetItem *m_raItem;
    QTableWidgetItem *m_maxSpeedItem;
    QTableWidgetItem *m_versionItem;
    QTableWidgetItem *m_lengthItem;
    QTableWidgetItem *m_widthItem;
    QTableWidgetItem *m_rollItem;
    QTableWidgetItem *m_groundspeedItem;
    QTableWidgetItem *m_turnRateItem;
    QTableWidgetItem *m_trueAirspeedItem;
    QTableWidgetItem *m_indicatedAirspeedItem;
    QTableWidgetItem *m_machItem;
    QTableWidgetItem *m_headwindItem;
    QTableWidgetItem *m_estAirTempItem;
    QTableWidgetItem *m_windSpeedItem;
    QTableWidgetItem *m_windDirItem;
    QTableWidgetItem *m_staticPressureItem;
    QTableWidgetItem *m_staticAirTempItem;
    QTableWidgetItem *m_humidityItem;

    Aircraft(ADSBDemodGUI *gui) :
        m_icao(0),
        m_globalPosition(false),
        m_latitude(0),
        m_longitude(0),
        m_radius(0.0f),
        m_altitude(0),
        m_onSurface(false),
        m_altitudeGNSS(false),
        m_heading(0),
        m_track(0),
        m_verticalRate(0),
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
        m_isTarget(false),
        m_isHighlighted(false),
        m_showAll(false),
        m_aircraftInfo(nullptr),
        m_modelAltitudeOffset(0.0f),
        m_labelAltitudeOffset(5.0f),
        m_gui(gui),
        m_runwayAltitude(0.0),
        m_runwayAltitudeValid(false),
        m_gearDown(false),
        m_flaps(0.0),
        m_rotorStarted(false),
        m_engineStarted(false),
        m_pitchEst(0.0),
        m_rollEst(0.0),
        m_notified(false)
    {
        for (int i = 0; i < 2; i++) {
            m_cprValid[i] = false;
        }
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < 16; j++) {
                m_bdsCapabilities[i][j] = false;
            }
        }
        // These are deleted by QTableWidget
        m_icaoItem = new QTableWidgetItem();
        m_callsignItem = new QTableWidgetItem();
        m_atcCallsignItem = new QTableWidgetItem();
        m_modelItem = new QTableWidgetItem();
        m_typeItem = new QTableWidgetItem();
        m_sideviewItem = new QTableWidgetItem();
        m_airlineItem = new QTableWidgetItem();
        m_altitudeItem = new QTableWidgetItem();
        m_headingItem = new QTableWidgetItem();
        m_trackItem = new QTableWidgetItem();
        m_verticalRateItem = new QTableWidgetItem();
        m_rangeItem = new CustomDoubleTableWidgetItem();
        m_azElItem = new QTableWidgetItem();
        m_latitudeItem = new QTableWidgetItem();
        m_longitudeItem = new QTableWidgetItem();
        m_emitterCategoryItem = new QTableWidgetItem();
        m_statusItem = new QTableWidgetItem();
        m_squawkItem = new QTableWidgetItem();
        m_identItem = new QTableWidgetItem();
        m_registrationItem = new QTableWidgetItem();
        m_countryItem = new QTableWidgetItem();
        m_registeredItem = new QTableWidgetItem();
        m_manufacturerNameItem = new QTableWidgetItem();
        m_ownerItem = new QTableWidgetItem();
        m_operatorICAOItem = new QTableWidgetItem();
        m_interogatorCodeItem = new QTableWidgetItem();
        m_timeItem = new QTableWidgetItem();
        m_totalFrameCountItem = new QTableWidgetItem();
        m_adsbFrameCountItem = new QTableWidgetItem();
        m_modesFrameCountItem = new QTableWidgetItem();
        m_nonTransponderItem = new QTableWidgetItem();
        m_tisBFrameCountItem = new QTableWidgetItem();
        m_adsrFrameCountItem = new QTableWidgetItem();
        m_radiusItem = new QTableWidgetItem();
        m_nacpItem = new QTableWidgetItem();
        m_nacvItem = new QTableWidgetItem();
        m_gvaItem = new QTableWidgetItem();
        m_nicItem = new QTableWidgetItem();
        m_nicBaroItem = new QTableWidgetItem();
        m_silItem = new QTableWidgetItem();
        m_correlationItem = new QTableWidgetItem();
        m_rssiItem = new QTableWidgetItem();
        m_flightStatusItem = new QTableWidgetItem();
        m_depItem = new QTableWidgetItem();
        m_arrItem = new QTableWidgetItem();
        m_stopsItem = new QTableWidgetItem();
        m_stdItem = new QTableWidgetItem();
        m_etdItem = new QTableWidgetItem();
        m_atdItem = new QTableWidgetItem();
        m_staItem = new QTableWidgetItem();
        m_etaItem = new QTableWidgetItem();
        m_ataItem = new QTableWidgetItem();
        m_selAltitudeItem = new QTableWidgetItem();
        m_selHeadingItem = new QTableWidgetItem();
        m_baroItem = new QTableWidgetItem();
        m_apItem = new QTableWidgetItem();
        m_vModeItem = new QTableWidgetItem();
        m_lModeItem = new QTableWidgetItem();
        m_tcasItem = new QTableWidgetItem();
        m_acasItem = new QTableWidgetItem();
        m_raItem = new QTableWidgetItem();
        m_maxSpeedItem = new QTableWidgetItem();
        m_versionItem = new QTableWidgetItem();
        m_lengthItem = new QTableWidgetItem();
        m_widthItem = new QTableWidgetItem();
        m_rollItem = new QTableWidgetItem();
        m_groundspeedItem = new QTableWidgetItem();
        m_turnRateItem = new QTableWidgetItem();
        m_trueAirspeedItem = new QTableWidgetItem();
        m_indicatedAirspeedItem = new QTableWidgetItem();
        m_machItem = new QTableWidgetItem();
        m_headwindItem = new QTableWidgetItem();
        m_estAirTempItem = new QTableWidgetItem();
        m_windSpeedItem = new QTableWidgetItem();
        m_windDirItem = new QTableWidgetItem();
        m_staticPressureItem = new QTableWidgetItem();
        m_staticAirTempItem = new QTableWidgetItem();
        m_humidityItem = new QTableWidgetItem();
    }

    QString getImage() const;
    QString getText(const ADSBDemodSettings *settings, bool all=false) const;
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
    void addCoordinate(const QDateTime& dateTime, AircraftModel *model);
    void clearCoordinates(AircraftModel *model);
};

class AircraftPathModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles{
        coordinatesRole = Qt::UserRole + 1,
        colorRole = Qt::UserRole + 2,
    };

    AircraftPathModel(AircraftModel *aircraftModel, Aircraft *aircraft) :
        m_aircraftModel(aircraftModel),
        m_aircraft(aircraft),
        m_paths(0),
        m_showFullPath(false),
        m_showATCPath(false)
    {
        settingsUpdated();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        (void) parent;
        return m_paths;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void add();
    void updateLast();
    void removed();
    void clear();
    void settingsUpdated();

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled;
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[coordinatesRole] = "coordinates";
        roles[colorRole] = "color";
        return roles;
    }

private:

    AircraftModel *m_aircraftModel;
    Aircraft *m_aircraft;
    int m_paths; // Should match m_aircraft->m_coordinates.count()
    bool m_showFullPath;
    bool m_showATCPath;
};

// Aircraft data model used by QML map item
class AircraftModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles{
        positionRole = Qt::UserRole + 1,
        headingRole = Qt::UserRole + 2,
        adsbDataRole = Qt::UserRole + 3,
        aircraftImageRole = Qt::UserRole + 4,
        bubbleColourRole = Qt::UserRole + 5,
        aircraftPathRole = Qt::UserRole + 6,
        showAllRole = Qt::UserRole + 7,
        highlightedRole = Qt::UserRole + 8,
        targetRole = Qt::UserRole + 9,
        radiusRole = Qt::UserRole + 10,
        showRadiusRole = Qt::UserRole + 11,
        aircraftPathModelRole = Qt::UserRole + 12,
    };

    Q_INVOKABLE void addAircraft(Aircraft *aircraft) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_aircrafts.append(aircraft);
        m_pathModels.append(new AircraftPathModel(this, aircraft));
        endInsertRows();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_aircrafts.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void aircraftUpdated(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void highlightChanged(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            m_pathModels[row]->settingsUpdated();
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void clearCoords(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0) {
            m_pathModels[row]->clear();
        }
    }

    void aircraftCoordsUpdated(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0) {
            m_pathModels[row]->updateLast();
        }
    }

    void aircraftCoordsAdded(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0) {
            m_pathModels[row]->add();
        }
    }

    void aircraftCoordsRemoved(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0) {
            m_pathModels[row]->removed();
        }
    }

    void allAircraftUpdated()
    {
        emit dataChanged(index(0), index(rowCount()-1));

        for (int i = 0; i < m_aircrafts.count(); i++) {
            m_pathModels[i]->settingsUpdated();
        }
    }

    void removeAircraft(Aircraft *aircraft)
    {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_aircrafts.removeAt(row);
            delete m_pathModels.takeAt(row);
            endRemoveRows();
        }
    }

    QHash<int, QByteArray> roleNames() const
    {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[headingRole] = "heading";
        roles[adsbDataRole] = "adsbData";
        roles[aircraftImageRole] = "aircraftImage";
        roles[bubbleColourRole] = "bubbleColour";
        roles[aircraftPathRole] = "aircraftPath";
        roles[showAllRole] = "showAll";
        roles[highlightedRole] = "highlighted";
        roles[targetRole] = "target";
        roles[radiusRole] = "containmentRadius";
        roles[aircraftPathModelRole] = "aircraftPathModel";
        return roles;
    }

    void setSettings(const ADSBDemodSettings *settings)
    {
        m_settings = settings;
        allAircraftUpdated();
    }

    Q_INVOKABLE void findOnMap(int index);

    void updateAircraftInformation(QSharedPointer<const QHash<int, AircraftInformation *>> aircraftInfo)
    {
        for (auto aircraft : m_aircrafts)
        {
            if (aircraftInfo->contains(aircraft->m_icao)) {
                aircraft->m_aircraftInfo = aircraftInfo->value(aircraft->m_icao);
            } else {
                aircraft->m_aircraftInfo = nullptr;
            }
        }
    }

    const ADSBDemodSettings *m_settings;

private:
    QList<Aircraft *> m_aircrafts;
    QList<AircraftPathModel *> m_pathModels;
};

// Airport data model used by QML map item
class AirportModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles {
        positionRole = Qt::UserRole + 1,
        airportDataRole = Qt::UserRole + 2,
        airportDataRowsRole = Qt::UserRole + 3,
        airportImageRole = Qt::UserRole + 4,
        bubbleColourRole = Qt::UserRole + 5,
        showFreqRole  = Qt::UserRole + 6,
        selectedFreqRole  = Qt::UserRole + 7
    };

    AirportModel(ADSBDemodGUI *gui) :
        m_gui(gui)
    {
    }

    Q_INVOKABLE void addAirport(const AirportInformation *airport, float az, float el, float distance) {
        QString text;
        int rows;

        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_airports.append(airport);
        airportFreq(airport, az, el, distance, text, rows);
        m_airportDataFreq.append(text);
        m_airportDataFreqRows.append(rows);
        m_showFreq.append(false);
        m_azimuth.append(az);
        m_elevation.append(el);
        m_range.append(distance);
        m_metar.append("");
        endInsertRows();
    }

    void removeAirport(AirportInformation *airport) {
        int row = m_airports.indexOf(airport);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_airports.removeAt(row);
            m_airportDataFreq.removeAt(row);
            m_airportDataFreqRows.removeAt(row);
            m_showFreq.removeAt(row);
            m_azimuth.removeAt(row);
            m_elevation.removeAt(row);
            m_range.removeAt(row);
            m_metar.removeAt(row);
            endRemoveRows();
        }
    }

    void removeAllAirports() {
        if (m_airports.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_airports.count() - 1);
            m_airports.clear();
            m_airportDataFreq.clear();
            m_airportDataFreqRows.clear();
            m_showFreq.clear();
            m_azimuth.clear();
            m_elevation.clear();
            m_range.clear();
            m_metar.clear();
            endRemoveRows();
        }
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_airports.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void airportFreq(const AirportInformation *airport, float az, float el, float distance, QString& text, int& rows) {
        // Create the text to go in the bubble next to the airport
        // Display name and frequencies
        QStringList list;

        list.append(QString("%1: %2").arg(airport->m_ident).arg(airport->m_name));
        rows = 1;
        for (int i = 0; i < airport->m_frequencies.size(); i++)
        {
            const AirportInformation::FrequencyInformation *frequencyInfo = airport->m_frequencies[i];
            list.append(QString("%1: %2 MHz").arg(frequencyInfo->m_type).arg(frequencyInfo->m_frequency));
            rows++;
        }
        list.append(QString("Az/El: %1/%2").arg((int)std::round(az)).arg((int)std::round(el)));
        list.append(QString("Distance: %1 km").arg(distance/1000.0f, 0, 'f', 1));
        rows += 2;
        text = list.join("\n");
    }

    void airportUpdated(const AirportInformation *airport) {
        int row = m_airports.indexOf(airport);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[airportDataRole] = "airportData";
        roles[airportDataRowsRole] = "airportDataRows";
        roles[airportImageRole] = "airportImage";
        roles[bubbleColourRole] = "bubbleColour";
        roles[showFreqRole] = "showFreq";
        roles[selectedFreqRole] = "selectedFreq";
        return roles;
    }

    void updateWeather(const QString &icao, const QString &text, const QString &decoded)
    {
        for (int i = 0; i < m_airports.size(); i++)
        {
            if (m_airports[i]->m_ident == icao)
            {
                m_metar[i] = "METAR: " + text + "\n" + decoded;
                QModelIndex idx = index(i);
                emit dataChanged(idx, idx);
                break;
            }
        }
    }

    Q_INVOKABLE QStringList getFreqScanners() const;
    Q_INVOKABLE void sendToFreqScanner(int index, const QString& id);

private:
    ADSBDemodGUI *m_gui;
    QList<const AirportInformation *> m_airports;
    QList<QString> m_airportDataFreq;
    QList<int> m_airportDataFreqRows;
    QList<bool> m_showFreq;
    QList<float> m_azimuth;
    QList<float> m_elevation;
    QList<float> m_range;
    QList<QString> m_metar;

signals:
    void requestMetar(const QString& icao);
};

// Airspace data model used by QML map item
class AirspaceModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles {
        nameRole = Qt::UserRole + 1,
        detailsRole = Qt::UserRole + 2,
        positionRole = Qt::UserRole + 3,
        airspaceBorderColorRole = Qt::UserRole + 4,
        airspaceFillColorRole = Qt::UserRole + 5,
        airspacePolygonRole = Qt::UserRole + 6
    };

    Q_INVOKABLE void addAirspace(Airspace *airspace)
    {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_airspaces.append(airspace);
        updatePolygon(airspace, -1);
        endInsertRows();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return m_airspaces.count();
    }

    void removeAirspace(Airspace *airspace)
    {
        int idx = m_airspaces.indexOf(airspace);
        if (idx >= 0)
        {
            beginRemoveRows(QModelIndex(), idx, idx);
            m_airspaces.removeAt(idx);
            m_polygons.removeAt(idx);
            endRemoveRows();
        }
    }

    void removeAllAirspaces()
    {
        if (m_airspaces.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_airspaces.count() - 1);
            m_airspaces.clear();
            m_polygons.clear();
            endRemoveRows();
        }
    }

    void airspaceUpdated(const Airspace *airspace)
    {
        int row = m_airspaces.indexOf(airspace);
        if (row >= 0)
        {
            updatePolygon(airspace, row);

            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    bool contains(const Airspace *airspace)
    {
        return m_airspaces.contains(airspace);
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    QHash<int, QByteArray> roleNames() const
    {
        QHash<int, QByteArray> roles;
        roles[nameRole] = "name";
        roles[detailsRole] = "details";
        roles[positionRole] = "position";
        roles[airspaceBorderColorRole] = "airspaceBorderColor";
        roles[airspaceFillColorRole] = "airspaceFillColor";
        roles[airspacePolygonRole] = "airspacePolygon";
        return roles;
    }

private:
    QList<const Airspace *> m_airspaces;
    QList<QVariantList> m_polygons;

    void updatePolygon(const Airspace *airspace, int row)
    {
        // Convert QPointF to QVariantList of QGeoCoordinates
        QVariantList polygon;
        for (const auto p : airspace->m_polygon)
        {
            QGeoCoordinate coord(p.y(), p.x(), airspace->topHeightInMetres());
            polygon.push_back(QVariant::fromValue(coord));
        }
        if (row == -1) {
            m_polygons.append(polygon);
        } else {
            m_polygons.replace(row, polygon);
        }
    }

};

// NavAid model used for each NavAid on the map
class NavAidModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles{
        positionRole = Qt::UserRole + 1,
        navAidDataRole = Qt::UserRole + 2,
        navAidImageRole = Qt::UserRole + 3,
        bubbleColourRole = Qt::UserRole + 4,
        selectedRole = Qt::UserRole + 5
    };

    Q_INVOKABLE void addNavAid(NavAid *vor) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_navAids.append(vor);
        m_selected.append(false);
        endInsertRows();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_navAids.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void allNavAidsUpdated() {
        for (int i = 0; i < m_navAids.count(); i++)
        {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    }

    void removeNavAid(NavAid *vor) {
        int row = m_navAids.indexOf(vor);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_navAids.removeAt(row);
            m_selected.removeAt(row);
            endRemoveRows();
        }
    }

    void removeAllNavAids() {
        if (m_navAids.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_navAids.count() - 1);
            m_navAids.clear();
            m_selected.clear();
            endRemoveRows();
        }
    }

    QHash<int, QByteArray> roleNames() const {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[navAidDataRole] = "navAidData";
        roles[navAidImageRole] = "navAidImage";
        roles[bubbleColourRole] = "bubbleColour";
        roles[selectedRole] = "selected";
        return roles;
    }

private:
    QList<NavAid *> m_navAids;
    QList<bool> m_selected;
};

// Match 3D models to Opensky-Network Aircraft database
// The database doesn't use consistent names for aircraft, so we use regexps
class ModelMatch {
public:
    ModelMatch(const QString &aircraftRegExp, const QString &model) :
        m_aircraftRegExp(aircraftRegExp),
        m_model(model)
    {
        m_aircraftRegExp.optimize();
    }

    virtual ~ModelMatch() = default;

    virtual bool match(const QString &aircraft, const QString &manufacturer, QString &model)
    {
        (void) manufacturer;

        QRegularExpressionMatch match = m_aircraftRegExp.match(aircraft);
        if (match.hasMatch())
        {
            model = m_model;
            return true;
        }
        else
        {
            return false;
        }
    }

protected:
    QRegularExpression m_aircraftRegExp;
    QString m_model;
};

// For very generic aircraft names, also match against manufacturer name
class ManufacturerModelMatch : public ModelMatch {
public:
    ManufacturerModelMatch(const QString &modelRegExp, const QString &manufacturerRegExp, const QString &model) :
        ModelMatch(modelRegExp, model),
        m_manufacturerRegExp(manufacturerRegExp)
    {
        m_manufacturerRegExp.optimize();
    }

    virtual bool match(const QString &aircraft, const QString &manufacturer, QString &model) override
    {
        QRegularExpressionMatch matchManufacturer = m_manufacturerRegExp.match(manufacturer);
        if (matchManufacturer.hasMatch())
        {
            QRegularExpressionMatch matchAircraft = m_aircraftRegExp.match(aircraft);
            if (matchAircraft.hasMatch())
            {
                model = m_model;
                return true;
            }
        }
        return false;
    }

protected:
    QRegularExpression m_manufacturerRegExp;
};

class ADSBDemodGUI : public ChannelGUI {
    Q_OBJECT

    struct Interogator {
        Real m_minLatitude;
        Real m_maxLatitude;
        Real m_minLongitude;
        Real m_maxLongitude;
        bool m_valid;
        Airspace m_airspace;

        Interogator() :
            m_valid(false)
        {
        }

        void update(int ic, Aircraft *aircraft, AirspaceModel *airspaceModel, CheckList *checkList, bool display);
        void calcPoly();
    };

public:
    static ADSBDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

    void highlightAircraft(Aircraft *aircraft);
    void targetAircraft(Aircraft *aircraft);
    void target(const QString& name, float az, float el, float range);
    bool setFrequency(qint64 frequency);
    bool useSIUints() const { return m_settings.m_siUnits; }
    Q_INVOKABLE void clearHighlighted();
    QString get3DModel(const QString &aircraft, const QString &operatorICAO) const;
    QString get3DModel(const QString &aircraft);
    void get3DModel(Aircraft *aircraft);
    void get3DModelBasedOnCategory(Aircraft *aircraft);

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();
    void flightInformationUpdated(const FlightInformation::Flight& flight);
    void aircraftPhoto(const PlaneSpottersPhoto *photo);

private:
    Ui::ADSBDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    ADSBDemodSettings m_settings;
    QStringList m_settingsKeys;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    ADSBDemod* m_adsbDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QHash<int, Aircraft *> m_aircraft;  // Hashed on ICAO
    QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInfo;
    QSharedPointer<const QHash<QString, AircraftRouteInformation *>> m_routeInfo; // Hashed on callsign
    QSharedPointer<const QHash<int, AirportInformation *>> m_airportInfo; // Hashed on id
    AircraftModel m_aircraftModel;
    AirportModel m_airportModel;
    AirspaceModel m_airspaceModel;
    NavAidModel m_navAidModel;
    QSharedPointer<const QList<Airspace *>> m_airspaces;
    QSharedPointer<const QList<NavAid *>> m_navAids;
    QGeoCoordinate m_lastFullUpdatePosition;

    AzEl m_azEl;                        // Position of station
    Aircraft *m_trackAircraft;          // Aircraft we want to track in Channel Report
    MovingAverageUtil<float, double, 10> m_correlationAvg;
    MovingAverageUtil<float, double, 10> m_correlationOnesAvg;
    MovingAverageUtil<float, double, 100> m_qnhAvg;

    Aircraft *m_highlightAircraft;      // Aircraft we want to highlight, when selected in table

    float m_currentAirportRange;        // Current settings, so we only update if changed
    ADSBDemodSettings::AirportType m_currentAirportMinimumSize;
    bool m_currentDisplayHeliports;

    static const int m_maxRangeDeg = 5;
    QList<float> m_maxRange[2];
    Airspace m_coverageAirspace[2];

    Interogator m_interogators[ADSB_IC_MAX];

    enum StatsRow {
        ADSB_FRAMES,
        MODE_S_FRAMES,
        TOTAL_FRAMES,
        ADSB_RATE,
        MODE_S_RATE,
        TOTAL_RATE,
        DATA_RATE,
        CORRELATOR_MATCHES,
        PERCENT_VALID,
        PREAMBLE_FAILS,
        CRC_FAILS,
        TYPE_FAILS,
        INVALID_FAILS,
        ICAO_FAILS,
        RANGE_FAILS,
        ALT_FAILS,
        AVERAGE_CORRELATION,
        TC_0,
        TC_1_4,
        TC_5_8,
        TC_9_18,
        TC_19,
        TC_20_22,
        TC_24,
        TC_28,
        TC_29,
        TC_31,
        TC_UNUSED,
        DF0,
        DF4,
        DF5,
        DF11,
        DF16,
        DF17,
        DF18,
        DF19,
        DF20_21,
        DF22,
        DF24,
        MAX_RANGE,
        MAX_ALTITUDE,
        MAX_RATE
    };

    qint64 m_rangeFails;
    qint64 m_altFails;
    QDateTime m_frameRateTime;
    qint64 m_adsbFrameRateCount;
    qint64 m_modesFrameRateCount;
    qint64 m_totalBytes;
    float m_maxRangeStat;
    float m_maxAltitudeStat;
    float m_maxRateState;
    qint64 m_dfStats[32];
    qint64 m_tcStats[32];
    QChart *m_chart;
    QLineSeries *m_adsbFrameRateSeries;
    QLineSeries *m_modesFrameRateSeries;
    QLineSeries *m_aircraftSeries;
    QDateTimeAxis *m_xAxis;
    QValueAxis *m_fpsYAxis;
    QValueAxis *m_aircraftYAxis;
    QDateTime m_averageTime;

#ifdef QT_TEXTTOSPEECH_FOUND
    QTextToSpeech *m_speech;
#endif
    QMenu *menu;                        // Column select context menu
    FlightInformation *m_flightInformation;
    PlaneSpotters m_planeSpotters;
    AviationWeather *m_aviationWeather;
    QString m_photoLink;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    QProgressDialog *m_progressDialog;
    quint16 m_osmPort;
    OpenAIP m_openAIP;
    OsnDB m_osnDB;
    OurAirportsDB m_ourAirportsDB;
    ADSBOSMTemplateServer *m_templateServer;
    QRandomGenerator m_random;
    QHash<QString, QString> m_3DModels; // Hashed aircraft_icao or just aircraft
    QHash<QString, QStringList> m_3DModelsByType; // Hashed aircraft to list of all of that type
    QList<ModelMatch *> m_3DModelMatch; // Map of database aircraft names to 3D model names
    QHash<QString, float> m_modelAltitudeOffset;
    QHash<QString, float> m_labelAltitudeOffset;

    QTimer m_importTimer;
    QTimer m_redrawMapTimer;
    QNetworkAccessManager *m_networkManager;
    bool m_loadingData;

    static const char m_idMap[];
    static const QString m_categorySetA[];
    static const QString m_categorySetB[];
    static const QString m_categorySetC[];
    static const QString m_emergencyStatus[];
    static const QString m_flightStatuses[];
    static const QString m_hazardSeverity[];
    static const QString m_fomSources[];
    static const QString m_nacvStrings[];

    explicit ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ADSBDemodGUI();

    QString maptilerAPIKey() const;

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings(const QStringList& settingsKeys, bool force);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void clearFromMap(const QString& name);
    void sendToMap(Aircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations);
    Aircraft *getAircraft(int icao, bool &newAircraft);
    void atcCallsign(Aircraft *aircraft);
    void callsignToFlight(Aircraft *aircraft);
    int roundTo50Feet(int alt);
    bool calcAirTemp(Aircraft *aircraft);
    void handleADSB(
        const QByteArray data,
        const QDateTime dateTime,
        float correlation,
        float correlationOnes,
        unsigned crc,
        bool updateModel);

    void decodeID(const QByteArray& data, QString& emitterCategory, QString& callsign);
    void decodeGroundspeed(const QByteArray& data, float& v, float& h);
    void decodeAirspeed(const QByteArray& data, bool& tas, int& as, bool& hdgValid, float& hdg);
    void decodeVerticalRate(const QByteArray& data, int& verticalRate);
    void updateAircraftPosition(Aircraft *aircraft, double latitude, double longitude, const QDateTime& dateTime);
    bool validateGlobalPosition(double latitude, double longitude, bool countFailure);
    bool validateLocalPosition(double latitude, double longitude, bool surfacePosition, bool countFailure);
    bool decodeGlobalPosition(int f, const double cprLat[2], const double cprLong[2], const QDateTime cprTime[2], double& latitude, double& longitude, bool countFailure);
    bool decodeLocalPosition(int f, double cprLat, double cprLong, bool onSurface, const Aircraft *aircraft, double& latitude, double& longitude, bool countFailure);
    void decodeCpr(const QByteArray& data, int& f, double& latCpr, double& lonCpr) const;
    bool decodeAltitude(const QByteArray& data, int& altFt) const;
    void decodeModeSAltitude(const QByteArray& data, const QDateTime dateTime, Aircraft *aircraft);
    void decodeModeS(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft);
    void decodeCommB(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft, bool &updatedCallsign);
    QList<SWGSDRangel::SWGMapAnimation *> *animate(QDateTime dateTime, Aircraft *aircraft);
    SWGSDRangel::SWGMapAnimation *gearAnimation(QDateTime startDateTime, bool up);
    SWGSDRangel::SWGMapAnimation *gearAngle(QDateTime startDateTime, bool flat);
    SWGSDRangel::SWGMapAnimation *flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps);
    SWGSDRangel::SWGMapAnimation *slatsAnimation(QDateTime startDateTime, bool retract);
    SWGSDRangel::SWGMapAnimation *rotorAnimation(QDateTime startDateTime, bool stop);
    SWGSDRangel::SWGMapAnimation *engineAnimation(QDateTime startDateTime, int engine, bool stop);
    void checkStaticNotification(Aircraft *aircraft);
    void checkDynamicNotification(Aircraft *aircraft);
    void enableSpeechIfNeeded();
    void speechNotification(Aircraft *aircraft, const QString &speech);
    void commandNotification(Aircraft *aircraft, const QString &command);
    QString subAircraftString(Aircraft *aircraft, const QString &string);
    void resizeTable();
    QString getDataDir();
    void update3DModels();
    void updateAirports();
    void updateAirspaces();
    void updateNavAids();
    void updateChannelList();
    void removeAircraft(QHash<int, Aircraft *>::iterator& i, Aircraft *aircraft);
    QAction *createCheckableItem(QString& text, int idx, bool checked);
    Aircraft* findAircraftByFlight(const QString& flight);
    QString dataTimeToShortString(QDateTime dt);
    void initFlightInformation();
    void initAviationWeather();
    void setShowContainmentRadius(bool show);
    void applyMapSettings();
    void updatePhotoText(Aircraft *aircraft);
    void updatePhotoFlightInformation(Aircraft *aircraft);
    void findOnChannelMap(Aircraft *aircraft);
    int squawkDecode(int code) const;
    int gillhamToFeet(int n) const;
    int grayToBinary(int gray, int bits) const;
    void redrawMap();
    void applyImportSettings();
    void sendAircraftReport();
    void updatePosition(float latitude, float longitude, float altitude);
    void clearOldHeading(Aircraft *aircraft, const QDateTime& dateTime, float newTrack);
    void updateQNH(const Aircraft *aircraft, float qnh);
    void setCallsign(Aircraft *aircraft, const QString& callsign);

    void initCoverageMap();
    void clearCoverageMap();
    void updateCoverageMap(float azimuth, float elevation, float distance, float altitude);

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void updateDFStats(int df);
    bool updateTCStats(int tc, int row, int low, int high);
    void resetStats();
    void plotChart();
    int countActiveAircraft();
    void averageSeries(QLineSeries *series, const QDateTime& startTime, const QDateTime& endTime);
    void legendMarkerClicked();

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_chipsThreshold_valueChanged(int value);
    void on_phaseSteps_valueChanged(int value);
    void on_tapsPerPhase_valueChanged(int value);
    void statsTable_customContextMenuRequested(QPoint pos);
    void adsbData_customContextMenuRequested(QPoint point);
    void on_adsbData_cellClicked(int row, int column);
    void on_adsbData_cellDoubleClicked(int row, int column);
    void adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void adsbData_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void on_spb_currentIndexChanged(int value);
    void on_demodModeS_clicked(bool checked);
    void on_feed_clicked(bool checked);
    void on_notifications_clicked();
    void on_flightInfo_clicked();
    void on_findOnMapFeature_clicked();
    void on_deleteAircraft_clicked();
    void on_getAircraftDB_clicked();
    void on_getAirportDB_clicked();
    void on_getAirspacesDB_clicked();
    void on_coverage_clicked(bool checked);
    void on_displayChart_clicked(bool checked);
    void on_stats_clicked(bool checked);
    void on_flightPaths_clicked(bool checked);
    void on_allFlightPaths_clicked(bool checked);
    void on_atcLabels_clicked(bool checked);
    void on_displayOrientation_clicked(bool checked);
    void on_displayRadius_clicked(bool checked);
    void on_ic_globalCheckStateChanged(int state);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void on_amDemod_currentIndexChanged(int index);
    void feedSelect(const QPoint& p);
    void on_displaySettings_clicked();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadAirspaceFinished();
    void downloadNavAidsFinished();
    void downloadAircraftInformationFinished();
    void downloadAirportInformationFinished();
    void photoClicked();
    virtual void showEvent(QShowEvent *event) override;
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    void import();
    void handleImportReply(QNetworkReply* reply);
    void preferenceChanged(int elementType);
    void devicePositionChanged(float latitude, float longitude, float altitude);
    void requestMetar(const QString& icao);
    void weatherUpdated(const AviationWeather::METAR &metar);
    void on_manualQNH_clicked(bool checked);
    void on_qnh_valueChanged(int value);
    void clearCoverage(const QPoint& p);
    void clearStats(const QPoint& p);
    void clearChart(const QPoint& p);
    void resetChartAxes();

signals:
    void homePositionChanged();
};

#endif // INCLUDE_ADSBDEMODGUI_H

