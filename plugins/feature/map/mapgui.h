///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef INCLUDE_FEATURE_MAPGUI_H_
#define INCLUDE_FEATURE_MAPGUI_H_

#include <QTimer>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTextEdit>
#include <QJsonObject>
#include <QFileDialog>
#ifdef QT_WEBENGINE_FOUND
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QWebEngineLoadingInfo>
#endif
#endif

#include <math.h>
#include <limits>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/giro.h"
#include "util/azel.h"
#include "util/openaip.h"
#include "util/ourairportsdb.h"
#include "util/waypoints.h"
#include "util/rainviewer.h"
#include "util/nasaglobalimagery.h"
#include "util/kiwisdrlist.h"
#include "util/spyserverlist.h"
#include "util/sdrangelserverlist.h"
#include "settings/rollupstate.h"
#include "availablechannelorfeaturehandler.h"

#include "SWGMapItem.h"
#include "SWGDeviceSettings.h"

#include "mapsettings.h"
#include "mapbeacondialog.h"
#include "mapibpbeacondialog.h"
#include "mapradiotimedialog.h"
#include "cesiuminterface.h"
#include "osmtemplateserver.h"
#include "maptileserver.h"
#include "webserver.h"
#include "mapmodel.h"

#include "maincore.h"

class PluginAPI;
class FeatureUISet;
class Map;

namespace Ui {
    class MapGUI;
}

class MapGUI;
struct Beacon;
class QSvgWidget;

struct RadioTimeTransmitter {
    QString m_callsign;
    int m_frequency;        // In Hz
    float m_latitude;       // In degrees
    float m_longitude;      // In degrees
    int m_power;            // In kW
};

struct IonosondeStation {
    QString m_name;
    float m_latitude;       // In degrees
    float m_longitude;      // In degrees
    QString m_text;
    QString m_label;

    IonosondeStation(const GIRO::GIROStationData& data) :
        m_name(data.m_station)
    {
        update(data);
    }

    void update(const GIRO::GIROStationData& data)
    {
        m_latitude = data.m_latitude;
        m_longitude = data.m_longitude;
        QStringList text;
        QStringList label;
        text.append("Ionosonde Station");
        text.append(QString("Name: %1").arg(m_name.split(",")[0]));
        if (!isnan(data.m_mufd))
        {
            text.append(QString("MUF: %1 MHz").arg(data.m_mufd));
            label.append(QString("%1").arg((int)round(data.m_mufd)));
        }
        else
        {
            label.append("-");
        }
        if (!isnan(data.m_md)) {
            text.append(QString("M(D): %1").arg(data.m_md));
        }
        if (!isnan(data.m_foF2))
        {
            text.append(QString("foF2: %1 MHz").arg(data.m_foF2));
            label.append(QString("%1").arg((int)round(data.m_foF2)));
        }
        else
        {
            label.append("-");
        }
        if (!isnan(data.m_hmF2)) {
            text.append(QString("hmF2: %1 km").arg(data.m_hmF2));
        }
        if (!isnan(data.m_foE)) {
            text.append(QString("foE: %1 MHz").arg(data.m_foE));
        }
        if (!isnan(data.m_tec)) {
            text.append(QString("TEC: %1").arg(data.m_tec));
        }
        if (data.m_confidence >= 0) {
            text.append(QString("Confidence: %1").arg(data.m_confidence));
        }
        if (data.m_dateTime.isValid()) {
            text.append(data.m_dateTime.toString());
        }
        m_text = text.join("\n");
        m_label = label.join("/");
    }
};

class MapGUI : public FeatureGUI {
    Q_OBJECT
public:
    static MapGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }
    AzEl *getAzEl() { return &m_azEl; }
    Map *getMap() { return m_map; }
    QQuickItem *getMapItem();
    static QString getBeaconFilename();
    QList<Beacon *> *getBeacons() { return m_beacons; }
    void setBeacons(QList<Beacon *> *beacons);
    void addIBPBeacons();
    QList<RadioTimeTransmitter> getRadioTimeTransmitters() { return m_radioTimeTransmitters; }
    void addRadioTimeTransmitters();
    void addNAT();
    void addRadar();
    void addIonosonde();
    void addBroadcast();
    void addDAB();
    void addNavAids();
    void addAirspace(const Airspace *airspace, const QString& group, int cnt);
    void addAirspace();
    void addAirports();
    void addWaypoints();
    void addNavtex();
    void addVLF();
    void addKiwiSDR();
    void addSpyServer();
    void addSDRangelServer();
    void find(const QString& target);
    void track3D(const QString& target);
    Q_INVOKABLE void supportedMapsChanged();
    MapSettings::MapItemSettings *getItemSettings(const QString &group);
    CesiumInterface *cesium() { return m_cesium; }

private:
    Ui::MapGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    MapSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;
    AvailableChannelOrFeatureList m_availableChannelOrFeatures;
    QFileDialog m_fileDialog;

    Map* m_map;
    MessageQueue m_inputMessageQueue;
    ObjectMapModel m_objectMapModel;
    ObjectMapFilter m_objectMapFilter;
    ImageMapModel m_imageMapModel;
    ImageFilter m_imageMapFilter;
    PolygonMapModel m_polygonMapModel;
    PolygonFilter m_polygonMapFilter;
    PolylineMapModel m_polylineMapModel;
    PolylineFilter m_polylineMapFilter;
    AzEl m_azEl;                        // Position of station
    SWGSDRangel::SWGMapItem m_antennaMapItem;
    QList<Beacon *> *m_beacons;
    MapBeaconDialog m_beaconDialog;
    MapIBPBeaconDialog m_ibpBeaconDialog;
    MapRadioTimeDialog m_radioTimeDialog;
    quint16 m_osmPort;
    OSMTemplateServer *m_templateServer;
    quint16 m_mapTileServerPort;
    MapTileServer *m_mapTileServer;
    QTimer m_redrawMapTimer;
    GIRO *m_giro;
    QDateTime m_giroDateTime;
    QString m_giroRunId;
    QHash<QString, IonosondeStation *> m_ionosondeStations;
    QSharedPointer<const QList<NavAid *>> m_navAids;
    QSharedPointer<const QList<Airspace *>> m_airspaces;
    QSharedPointer<const QHash<int, AirportInformation *>> m_airportInfo;
    QSharedPointer<const QHash<QString, Waypoint *>> m_waypoints;
    QGeoCoordinate m_lastFullUpdatePosition;
    KiwiSDRList m_kiwiSDRList;
    SpyServerList m_spyServerList;
    SDRangelServerList m_sdrangelServerList;

    CesiumInterface *m_cesium;
    WebServer *m_webServer;
    quint16 m_webPort;
    RainViewer *m_rainViewer;
    NASAGlobalImagery m_nasaGlobalImagery;
    QList<NASAGlobalImagery::DataSet> m_nasaDataSets;
    QHash<QString, NASAGlobalImagery::DataSet> m_nasaDataSetsHash;
    NASAGlobalImagery::MetaData m_nasaMetaData;
    QAction *m_displaySeaMarks;
    QAction *m_displayRailways;
    QAction *m_displayRain;
    QAction *m_displayClouds;
    QAction *m_displayNASAGlobalImagery;
    QAction *m_displayMUF;
    QAction *m_displayfoF2;

    QString m_radarPath;
    QString m_satellitePath;

    NASAGlobalImagery::Legend *m_legend;
    QWidget *m_nasaWidget;
    QSvgWidget *m_legendWidget;
    QTableWidget *m_overviewWidget;
    QTextEdit *m_descriptionWidget;

    // Settings for opening a device
    QString m_remoteDeviceAddress;
    quint16 m_remoteDevicePort;

    explicit MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~MapGUI();

    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group);
    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void applyMap2DSettings(bool reloadMap);
    void applyMap3DSettings(bool reloadMap);
    QString osmCachePath();
    void clearOSMCache();
    void clearWikiMediaOSMCache();
    void displaySettings();
    bool handleMessage(const Message& message);
    void geoReply();
    QString thunderforestAPIKey() const;
    QString maptilerAPIKey() const;
    QString cesiumIonAPIKey() const;
    void redrawMap();
    void makeUIConnections();
    void createLayersMenu();
    void displayToolbar();
    void setEnableOverlay();
    void applyNASAGlobalImagerySettings();
    void createNASAGlobalImageryView();
    void displayNASAMetaData();
    void openKiwiSDR(const QString& url);
    void openSpyServer(const QString& url);
    void openSDRangelServer(const QString& url, bool wss);
    QString formatFrequency(qint64 frequency) const;
    void updateGIRO(const QDateTime& mapDateTime);

    static QString getDataDir();
    static const QList<RadioTimeTransmitter> m_radioTimeTransmitters;
    static const QList<RadioTimeTransmitter> m_natTransmitters;
    static const QList<RadioTimeTransmitter> m_vlfTransmitters;

    enum NASARow {
        NASA_TITLE,
        NASA_SUBTITLE,
        NASA_DEFAULT_DATE,
        NASA_START_DATE,
        NASA_END_DATE,
        NASA_PERIOD,
        NASA_LAYER_GROUP,
        NASA_ROWS
    };

private slots:
    void init3DMap();
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_displayNames_clicked(bool checked=false);
    void on_displayAllGroundTracks_clicked(bool checked=false);
    void on_displaySelectedGroundTracks_clicked(bool checked=false);
    void on_displayRain_clicked(bool checked=false);
    void on_displayClouds_clicked(bool checked=false);
    void on_displaySeaMarks_clicked(bool checked=false);
    void on_displayRailways_clicked(bool checked=false);
    void on_displayMUF_clicked(bool checked=false);
    void on_displayfoF2_clicked(bool checked=false);
    void on_displayNASAGlobalImagery_clicked(bool checked=false);
    void on_nasaGlobalImageryIdentifier_currentIndexChanged(int index);
    void on_nasaGlobalImageryOpacity_valueChanged(int index);
    void on_layersMenu_clicked();
    void on_find_returnPressed();
    void on_maidenhead_clicked();
    void on_save_clicked();
    void on_deleteAll_clicked();
    void on_displaySettings_clicked();
    void on_mapTypes_currentIndexChanged(int index);
    void on_beacons_clicked();
    void on_ibpBeacons_clicked();
    void on_radiotime_clicked();
    void receivedCesiumEvent(const QJsonObject &obj);
    virtual void showEvent(QShowEvent *event) override;
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    void orientationChanged(Qt::ScreenOrientation orientation);
#ifdef QT_WEBENGINE_FOUND
    void fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest);
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    void loadingChanged(const QWebEngineLoadingInfo &loadingInfo);
    void downloadRequested(QWebEngineDownloadRequest *download);
#else
    void downloadRequested(QWebEngineDownloadItem *download);
#endif
#endif
    void statusChanged(QQuickWidget::Status status);
    void preferenceChanged(int elementType);
    void giroIndexUpdated(const QList<GIRO::DataSet>& data);
    void giroDataUpdated(const GIRO::GIROStationData& data);
    void mufUpdated(const QJsonDocument& document);
    void foF2Updated(const QJsonDocument& document);
    void pathUpdated(const QString& radarPath, const QString& satellitePath);
    void nasaGlobalImageryDataUpdated(const QList<NASAGlobalImagery::DataSet>& dataSets);
    void nasaGlobalImageryMetaDataUpdated(const NASAGlobalImagery::MetaData& metaData);
    void nasaGlobalImageryLegendAvailable(const QString& url, const QByteArray& data);
    void nasaGlobalImageryHTMLAvailable(const QString& url, const QByteArray& data);
    void navAidsUpdated();
    void airspacesUpdated();
    void airportsUpdated();
    void waypointsUpdated();
    void kiwiSDRUpdated(const QList<KiwiSDRList::KiwiSDR>& sdrs);
    void spyServerUpdated(const QList<SpyServerList::SpyServer>& sdrs);
    void sdrangelServerUpdated(const QList<SDRangelServerList::SDRangelServer>& sdrs);
    void linkClicked(const QString& url);
};

#endif // INCLUDE_FEATURE_MAPGUI_H_
