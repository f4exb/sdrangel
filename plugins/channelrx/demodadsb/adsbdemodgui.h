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
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QIcon>
#include <QMenu>
#include <QGeoCoordinate>
#include <QDateTime>
#include <QAbstractListModel>
#include <QProgressDialog>
#include <QTextToSpeech>
#include <QRandomGenerator>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QtCharts>

#include "adsbaircraft.h"
#include "adsbaircrafttablemodel.h"
#include "adsbdemodtracker.h"
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

// The aircraft as the GUI needs it: the decoded state, plus the flight path shown on
// the map and the icons shown in the table
struct Aircraft : ADSBAircraft {
    // Selection state, which belongs to the user rather than to the decoder
    bool m_isTarget;            // Are we targeting this aircraft (sending az/el to rotator)
    bool m_isHighlighted;       // Are we highlighting this aircraft in the table and map
    bool m_showAll;

    QList<QVariantList> m_coordinates; // Coordinates we've recorded the aircraft at, split up in to altitude ranges
    QList<QVariantList> m_recentCoordinates; // Last 20 seconds of coordinates for ATC mode
    QList<QDateTime> m_coordinateDateTimes;
    QList<int> m_coordinateColors; // 0-7 index to 8 color palette according to altitude
    QList<int> m_recentCoordinateColors;
    int m_lastColor;

    ADSBDemodGUI *m_gui;

    Aircraft(ADSBDemodGUI *gui) :
        m_isTarget(false),
        m_isHighlighted(false),
        m_showAll(false),
        m_lastColor(-1),
        m_gui(gui)
    {
    }
    Aircraft(const Aircraft&) = delete;
    Aircraft& operator=(const Aircraft&) = delete;

    void addCoordinate(const ADSBAircraftPosition& position, AircraftModel *model);
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

    AircraftPathModel(AircraftModel *aircraftModel, Aircraft *aircraft, QObject *parent) :
        QAbstractListModel(parent),
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
        m_pathModels.append(new AircraftPathModel(this, aircraft, this));
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

    const ADSBDemodSettings *m_settings = nullptr;

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
            m_minLatitude(std::numeric_limits<Real>::max()),
            m_maxLatitude(std::numeric_limits<Real>::lowest()),
            m_minLongitude(std::numeric_limits<Real>::max()),
            m_maxLongitude(std::numeric_limits<Real>::lowest()),
            m_valid(false),
            m_airspace{}
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

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();
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
public:
    QString mapItemName(const Aircraft *aircraft) const;
private:
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QSharedPointer<const QHash<int, AirportInformation *>> m_airportInfo; // Hashed on id
    AircraftModel m_aircraftModel;          // Aircraft on the map
    ADSBAircraftTableModel m_aircraftTableModel;
    ADSBAircraftSortProxy *m_aircraftProxy;

    // The GUI's copy of the aircraft, kept up to date by the tracker
    QHash<int, Aircraft *> m_aircraft;      // Hashed on ICAO
    Aircraft *m_trackAircraft;              // Aircraft being sent to a rotator
    AzEl m_azEl;                            // Position of station, for the map overlays
    QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInfo;
    QSharedPointer<const QHash<QString, AircraftRouteInformation *>> m_routeInfo;
    Airspace m_coverageAirspace[2];
    Interogator m_interogators[ADSB_IC_MAX];
    bool m_loadingData;
    // Aircraft whose map marker needs redrawing. Telling the QML model on every frame
    // makes it re-evaluate bindings and repaint far more often than the eye can use
    QSet<Aircraft *> m_dirtyMapAircraft;
    AirportModel m_airportModel;
    AirspaceModel m_airspaceModel;
    NavAidModel m_navAidModel;
    QSharedPointer<const QList<Airspace *>> m_airspaces;
    QSharedPointer<const QList<NavAid *>> m_navAids;
    QGeoCoordinate m_lastFullUpdatePosition;


    Aircraft *m_highlightAircraft;      // Aircraft we want to highlight, when selected in table

    float m_currentAirportRange;        // Current settings, so we only update if changed
    ADSBDemodSettings::AirportType m_currentAirportMinimumSize;
    bool m_currentDisplayHeliports;




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

    QTimer m_redrawMapTimer;
    QNetworkAccessManager *m_networkManager;


    explicit ADSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ADSBDemodGUI();

    QString maptilerAPIKey() const;
    QString cartoDBAPIKey() const;

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings(const QStringList& settingsKeys, bool force);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();


    void enableSpeechIfNeeded();
    void speechNotification(Aircraft *aircraft, const QString &speech);
    void resizeTable();
    Aircraft *aircraftForViewRow(int viewRow) const;
    void removeAircraft(QHash<int, Aircraft *>::iterator& i, Aircraft *aircraft);
    void initCoverageDisplay();
    Aircraft *findAircraft(quint32 icao) const { return m_aircraft.value((int) icao, nullptr); }
    void flushMapUpdates();
    QString getDataDir();
    void updateAirports();
    void updateAirspaces();
    void updateNavAids();
    void updateChannelList();
    QAction *createCheckableItem(QString& text, int idx, bool checked);
    void initAviationWeather();
    void setShowContainmentRadius(bool show);
    void applyMapSettings();
    void clearStaleTileCache(const QString& cachePath);
    void updatePhotoText(Aircraft *aircraft);
    void updatePhotoFlightInformation(Aircraft *aircraft);
    void findOnChannelMap(Aircraft *aircraft);
    void redrawMap();
    void updatePosition(float latitude, float longitude, float altitude);


    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void plotChart();
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
    // Fed by the tracker, which does the decoding on the channel's worker thread
    void aircraftUpdated(const QList<ADSBAircraftUpdate>& updates);
    void aircraftRemoved(const QList<quint32>& icaos);
    void statsUpdated(const QHash<int, QVariant>& stats);
    void coverageUpdated(int band, const QVector<QPointF>& polygon);
    void qnhEstimate(int qnh);
    void speechRequest(const QString& speech);
    void highlightRequest(quint32 icao);
    void targetRequest(quint32 icao);
    void photoRefresh(quint32 icao);
    void rates(double adsbRate, double modesRate, int activeAircraft);

    void adsbData_clicked(const QModelIndex& index);
    void adsbData_doubleClicked(const QModelIndex& index);
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
    void preferenceChanged(int elementType);
    void devicePositionChanged(float latitude, float longitude, float altitude);
    void requestMetar(const QString& icao);
    void weatherUpdated(const AviationWeather::METAR &metar);
    void on_manualQNH_clicked(bool checked);
    void on_qnh_valueChanged(int value);
    void clearStats(const QPoint& p);
    void clearChart(const QPoint& p);
    void resetChartAxes();

signals:
    void homePositionChanged();
};

#endif // INCLUDE_ADSBDEMODGUI_H
