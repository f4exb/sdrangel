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

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/aviationweather.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "util/movingaverage.h"
#include "util/httpdownloadmanager.h"
#include "util/flightinformation.h"
#include "util/openaip.h"
#include "util/planespotters.h"
#include "settings/rollupstate.h"
#include "maincore.h"

#include "SWGMapItem.h"

#include "adsbdemodsettings.h"
#include "ourairportsdb.h"
#include "util/osndb.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ADSBDemod;
class WebAPIAdapterInterface;
class HttpDownloadManager;
class ADSBDemodGUI;
class ADSBOSMTemplateServer;

namespace Ui {
    class ADSBDemodGUI;
}

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
    Real m_latitude;            // Latitude in decimal degrees
    Real m_longitude;           // Longitude in decimal degrees
    int m_altitude;             // Altitude in feet
    bool m_onSurface;           // Indicates if on surface or airbourne
    bool m_altitudeGNSS;        // Altitude is GNSS HAE (Height above WGS-84 ellipsoid) rather than barometric alitute (relative to 29.92 Hg)
    float m_heading;            // Heading or track in degrees
    int m_verticalRate;         // Vertical climb rate in ft/min
    QString m_emitterCategory;  // Aircraft type
    QString m_status;           // Aircraft status
    int m_squawk;               // Mode-A code
    Real m_range;               // Distance from station to aircraft
    Real m_azimuth;             // Azimuth from station to aircraft
    Real m_elevation;           // Elevation from station to aicraft
    QDateTime m_time;           // When last updated

    int m_selAltitude;          // Selected altitude in MCP/FCU or FMS in feet
    int m_selHeading;           // Selected heading in MCP/FCU in degrees
    int m_baro;                 // Aircraft baro setting in mb (Mode-S)
    float m_roll;               // In degrees
    int m_groundspeed;          // In knots
    float m_turnRate;           // In degrees per second
    int m_trueAirspeed;         // In knots
    int m_indicatedAirspeed;    // In knots
    float m_mach;               // Mach number

    bool m_bdsCapabilities[16][16]; // BDS capabilities are indicaited by BDS 1.7

    bool m_positionValid;       // Indicates if we have valid data for the above fields
    bool m_altitudeValid;
    bool m_headingValid;
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
    bool m_bdsCapabilitiesValid;

    // State for calculating position using two CPR frames
    bool m_cprValid[2];
    Real m_cprLat[2];
    Real m_cprLong[2];
    QDateTime m_cprTime[2];

    int m_adsbFrameCount;       // Number of ADS-B frames for this aircraft
    int m_tisBFrameCount;
    float m_minCorrelation;
    float m_maxCorrelation;
    float m_correlation;
    MovingAverageUtil<float, double, 100> m_correlationAvg;

    bool m_isTarget;            // Are we targetting this aircraft (sending az/el to rotator)
    bool m_isHighlighted;       // Are we highlighting this aircraft in the table and map
    bool m_showAll;

    QVariantList m_coordinates; // Coordinates we've recorded the aircraft at

    AircraftInformation *m_aircraftInfo; // Info about the aircraft from the database
    QString m_aircraft3DModel;    // 3D model for map based on aircraft type
    QString m_aircraftCat3DModel; // 3D model based on aircraft category
    float m_modelAltitudeOffset;  // Altitude adjustment so aircraft model doesn't go underground
    float m_labelAltitudeOffset;  // How height to position label above aircraft
    ADSBDemodGUI *m_gui;
    QString m_flagIconURL;
    QString m_airlineIconURL;

    // For animation on 3D map
    float m_runwayAltitude;
    bool m_runwayAltitudeValid;
    bool m_gearDown;
    float m_flaps;              // 0 - no flaps, 1 - full flaps
    bool m_rotorStarted;        // Rotors started on 'Rotorcraft'
    bool m_engineStarted;       // Engines started (typically propellors)
    QDateTime m_positionDateTime;
    QDateTime m_orientationDateTime;
    QDateTime m_headingDateTime;
    QDateTime m_prevHeadingDateTime;
    int m_prevHeading;
    float m_pitchEst;           // Estimated pitch based on vertical rate
    float m_rollEst;            // Estimated roll based on rate of change in heading

    bool m_notified;            // Set when a notification has been made for this aircraft, so we don't repeat it

    // GUI table items for above data
    QTableWidgetItem *m_icaoItem;
    QTableWidgetItem *m_callsignItem;
    QTableWidgetItem *m_modelItem;
    QTableWidgetItem *m_airlineItem;
    QTableWidgetItem *m_latitudeItem;
    QTableWidgetItem *m_longitudeItem;
    QTableWidgetItem *m_altitudeItem;
    QTableWidgetItem *m_headingItem;
    QTableWidgetItem *m_verticalRateItem;
    CustomDoubleTableWidgetItem *m_rangeItem;
    QTableWidgetItem *m_azElItem;
    QTableWidgetItem *m_emitterCategoryItem;
    QTableWidgetItem *m_statusItem;
    QTableWidgetItem *m_squawkItem;
    QTableWidgetItem *m_registrationItem;
    QTableWidgetItem *m_countryItem;
    QTableWidgetItem *m_registeredItem;
    QTableWidgetItem *m_manufacturerNameItem;
    QTableWidgetItem *m_ownerItem;
    QTableWidgetItem *m_operatorICAOItem;
    QTableWidgetItem *m_timeItem;
    QTableWidgetItem *m_adsbFrameCountItem;
    QTableWidgetItem *m_correlationItem;
    QTableWidgetItem *m_rssiItem;
    QTableWidgetItem *m_flightStatusItem;
    QTableWidgetItem *m_depItem;
    QTableWidgetItem *m_arrItem;
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
    QTableWidgetItem *m_tisBItem;

    Aircraft(ADSBDemodGUI *gui) :
        m_icao(0),
        m_latitude(0),
        m_longitude(0),
        m_altitude(0),
        m_onSurface(false),
        m_altitudeGNSS(false),
        m_heading(0),
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
        m_positionValid(false),
        m_altitudeValid(false),
        m_headingValid(false),
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
        m_bdsCapabilitiesValid(false),
        m_adsbFrameCount(0),
        m_tisBFrameCount(0),
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
        m_modelItem = new QTableWidgetItem();
        m_airlineItem = new QTableWidgetItem();
        m_altitudeItem = new QTableWidgetItem();
        m_headingItem = new QTableWidgetItem();
        m_verticalRateItem = new QTableWidgetItem();
        m_rangeItem = new CustomDoubleTableWidgetItem();
        m_azElItem = new QTableWidgetItem();
        m_latitudeItem = new QTableWidgetItem();
        m_longitudeItem = new QTableWidgetItem();
        m_emitterCategoryItem = new QTableWidgetItem();
        m_statusItem = new QTableWidgetItem();
        m_squawkItem = new QTableWidgetItem();
        m_registrationItem = new QTableWidgetItem();
        m_countryItem = new QTableWidgetItem();
        m_registeredItem = new QTableWidgetItem();
        m_manufacturerNameItem = new QTableWidgetItem();
        m_ownerItem = new QTableWidgetItem();
        m_operatorICAOItem = new QTableWidgetItem();
        m_timeItem = new QTableWidgetItem();
        m_adsbFrameCountItem = new QTableWidgetItem();
        m_correlationItem = new QTableWidgetItem();
        m_rssiItem = new QTableWidgetItem();
        m_flightStatusItem = new QTableWidgetItem();
        m_depItem = new QTableWidgetItem();
        m_arrItem = new QTableWidgetItem();
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
        m_tisBItem = new QTableWidgetItem();
    }

    QString getImage() const;
    QString getText(bool all=false) const;

    // Name to use when selected as a target
    QString targetName()
    {
        if (!m_callsign.isEmpty())
            return QString("Callsign: %1").arg(m_callsign);
        else
            return QString("ICAO: %1").arg(m_icao, 0, 16);
    }

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
        targetRole = Qt::UserRole + 9
    };

    Q_INVOKABLE void addAircraft(Aircraft *aircraft) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_aircrafts.append(aircraft);
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

    void aircraftUpdated(Aircraft *aircraft) {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void allAircraftUpdated() {
        /*
        // Not sure why this doesn't work - it should be more efficient
        // than the following code
        emit dataChanged(index(0), index(rowCount()));
        */
        for (int i = 0; i < m_aircrafts.count(); i++)
        {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    }

    void removeAircraft(Aircraft *aircraft) {
        int row = m_aircrafts.indexOf(aircraft);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_aircrafts.removeAt(row);
            endRemoveRows();
        }
    }

    QHash<int, QByteArray> roleNames() const {
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
        return roles;
    }

    void setFlightPaths(bool flightPaths)
    {
        m_flightPaths = flightPaths;
        allAircraftUpdated();
    }

    void setAllFlightPaths(bool allFlightPaths)
    {
        m_allFlightPaths = allFlightPaths;
        allAircraftUpdated();
    }

   Q_INVOKABLE void findOnMap(int index);

private:
    QList<Aircraft *> m_aircrafts;
    bool m_flightPaths;
    bool m_allFlightPaths;
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

    Q_INVOKABLE void addAirport(AirportInformation *airport, float az, float el, float distance) {
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

    void airportFreq(AirportInformation *airport, float az, float el, float distance, QString& text, int& rows) {
        // Create the text to go in the bubble next to the airport
        // Display name and frequencies
        QStringList list;

        list.append(QString("%1: %2").arg(airport->m_ident).arg(airport->m_name));
        rows = 1;
        for (int i = 0; i < airport->m_frequencies.size(); i++)
        {
            AirportInformation::FrequencyInformation *frequencyInfo = airport->m_frequencies[i];
            list.append(QString("%1: %2 MHz").arg(frequencyInfo->m_type).arg(frequencyInfo->m_frequency));
            rows++;
        }
        list.append(QString("Az/El: %1/%2").arg((int)std::round(az)).arg((int)std::round(el)));
        list.append(QString("Distance: %1 km").arg(distance/1000.0f, 0, 'f', 1));
        rows += 2;
        text = list.join("\n");
    }

    void airportUpdated(AirportInformation *airport) {
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

private:
    ADSBDemodGUI *m_gui;
    QList<AirportInformation *> m_airports;
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

    Q_INVOKABLE void addAirspace(Airspace *airspace) {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_airspaces.append(airspace);
        // Convert QPointF to QVariantList of QGeoCoordinates
        QVariantList polygon;
        for (const auto p : airspace->m_polygon)
        {
            QGeoCoordinate coord(p.y(), p.x(), airspace->topHeightInMetres());
            polygon.push_back(QVariant::fromValue(coord));
        }
        m_polygons.append(polygon);
        endInsertRows();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent)
        return m_airspaces.count();
    }

    void removeAllAirspaces() {
        if (m_airspaces.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_airspaces.count() - 1);
            m_airspaces.clear();
            m_polygons.clear();
            endRemoveRows();
        }
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    QHash<int, QByteArray> roleNames() const {
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
    QList<Airspace *> m_airspaces;
    QList<QVariantList> m_polygons;
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
    bool setFrequency(float frequency);
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
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_basicSettingsShown;
    bool m_doApplySettings;

    ADSBDemod* m_adsbDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QHash<int, Aircraft *> m_aircraft;  // Hashed on ICAO
    QHash<int, AircraftInformation *> *m_aircraftInfo;
    QHash<int, AirportInformation *> *m_airportInfo; // Hashed on id
    AircraftModel m_aircraftModel;
    AirportModel m_airportModel;
    AirspaceModel m_airspaceModel;
    NavAidModel m_navAidModel;
    QList<Airspace *> m_airspaces;
    QList<NavAid *> m_navAids;

    AzEl m_azEl;                        // Position of station
    Aircraft *m_trackAircraft;          // Aircraft we want to track in Channel Report
    MovingAverageUtil<float, double, 10> m_correlationAvg;
    MovingAverageUtil<float, double, 10> m_correlationOnesAvg;
    Aircraft *m_highlightAircraft;      // Aircraft we want to highlight, when selected in table

    float m_currentAirportRange;        // Current settings, so we only update if changed
    ADSBDemodSettings::AirportType m_currentAirportMinimumSize;
    bool m_currentDisplayHeliports;

    QTextToSpeech *m_speech;
    QMenu *menu;                        // Column select context menu
    FlightInformation *m_flightInformation;
    PlaneSpotters m_planeSpotters;
    AviationWeather *m_aviationWeather;
    QString m_photoLink;
    WebAPIAdapterInterface *m_webAPIAdapterInterface;
    HttpDownloadManager m_dlm;
    QProgressDialog *m_progressDialog;
    quint16 m_osmPort;
    OpenAIP m_openAIP;
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

    static const char m_idMap[];
    static const QString m_categorySetA[];
    static const QString m_categorySetB[];
    static const QString m_categorySetC[];
    static const QString m_emergencyStatus[];
    static const QString m_flightStatuses[];
    static const QString m_hazardSeverity[];
    static const QString m_fomSources[];

    explicit ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ADSBDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void updatePosition(Aircraft *aircraft);
    bool updateLocalPosition(Aircraft *aircraft, double latitude, double longitude, bool surfacePosition);
    void sendToMap(Aircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations);
    Aircraft *getAircraft(int icao, bool &newAircraft);
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
    void decodeModeS(const QByteArray data, int df, Aircraft *aircraft);
    void decodeCommB(const QByteArray data, const QDateTime dateTime, int df, Aircraft *aircraft, bool &updatedCallsign);
    QList<SWGSDRangel::SWGMapAnimation *> *animate(QDateTime dateTime, Aircraft *aircraft);
    SWGSDRangel::SWGMapAnimation *gearAnimation(QDateTime startDateTime, bool up);
    SWGSDRangel::SWGMapAnimation *flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps);
    SWGSDRangel::SWGMapAnimation *slatsAnimation(QDateTime startDateTime, bool retract);
    SWGSDRangel::SWGMapAnimation *rotorAnimation(QDateTime startDateTime, bool stop);
    SWGSDRangel::SWGMapAnimation *engineAnimation(QDateTime startDateTime, int engine, bool stop);
    void checkStaticNotification(Aircraft *aircraft);
    void checkDynamicNotification(Aircraft *aircraft);
    void speechNotification(Aircraft *aircraft, const QString &speech);
    void commandNotification(Aircraft *aircraft, const QString &command);
    QString subAircraftString(Aircraft *aircraft, const QString &string);
    void resizeTable();
    QString getDataDir();
    QString getAirportDBFilename();
    QString getAirportFrequenciesDBFilename();
    QString getOSNDBZipFilename();
    QString getOSNDBFilename();
    QString getFastDBFilename();
    qint64 fileAgeInDays(QString filename);
    bool confirmDownload(QString filename);
    void readAirportDB(const QString& filename);
    void readAirportFrequenciesDB(const QString& filename);
    bool readOSNDB(const QString& filename);
    bool readFastDB(const QString& filename);
    void update3DModels();
    void updateAirports();
    void updateAirspaces();
    void updateNavAids();
    void updateDeviceSetList();
    QAction *createCheckableItem(QString& text, int idx, bool checked);
    Aircraft* findAircraftByFlight(const QString& flight);
    QString dataTimeToShortString(QDateTime dt);
    void initFlightInformation();
    void initAviationWeather();
    void applyMapSettings();
    void updatePhotoText(Aircraft *aircraft);
    void updatePhotoFlightInformation(Aircraft *aircraft);
    void findOnChannelMap(Aircraft *aircraft);
    int squawkDecode(int code) const;
    int gillhamToFeet(int n) const;
    int grayToBinary(int gray, int bits) const;
    void redrawMap();
    void applyImportSettings();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_phaseSteps_valueChanged(int value);
    void on_tapsPerPhase_valueChanged(int value);
    void adsbData_customContextMenuRequested(QPoint point);
    void on_adsbData_cellClicked(int row, int column);
    void on_adsbData_cellDoubleClicked(int row, int column);
    void adsbData_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void adsbData_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void on_spb_currentIndexChanged(int value);
    void on_correlateFullPreamble_clicked(bool checked);
    void on_demodModeS_clicked(bool checked);
    void on_feed_clicked(bool checked);
    void on_notifications_clicked();
    void on_flightInfo_clicked();
    void on_findOnMapFeature_clicked();
    void on_getOSNDB_clicked();
    void on_getAirportDB_clicked();
    void on_getAirspacesDB_clicked();
    void on_flightPaths_clicked(bool checked);
    void on_allFlightPaths_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void updateDownloadProgress(qint64 bytesRead, qint64 totalBytes);
    void downloadFinished(const QString& filename, bool success);
    void on_device_currentIndexChanged(int index);
    void feedSelect();
    void on_displaySettings_clicked();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadAirspaceFinished();
    void downloadNavAidsFinished();
    void photoClicked();
    virtual void showEvent(QShowEvent *event);
    virtual bool eventFilter(QObject *obj, QEvent *event);
    void import();
    void handleImportReply(QNetworkReply* reply);
    void preferenceChanged(int elementType);
    void requestMetar(const QString& icao);
    void weatherUpdated(const AviationWeather::METAR &metar);

signals:
    void homePositionChanged();
};

#endif // INCLUDE_ADSBDEMODGUI_H
