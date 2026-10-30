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
#include <QSet>
#include <QQuickItem>
#include <QQuickWidget>
#include <QTextEdit>
#include <QJsonObject>
#include <QFileDialog>
#ifdef QT_WEBENGINE_FOUND
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
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
#include "util/aurora.h"
#include "util/openaip.h"
#include "util/ourairportsdb.h"
#include "util/waypoints.h"
#include "util/rainviewer.h"
#include "util/nasaglobalimagery.h"
#include "util/kiwisdrlist.h"
#include "util/spyserverlist.h"
#include "util/asset3dmanifest.h"
#include "util/sdrangelserverlist.h"
#include "settings/rollupstate.h"
#include "availablechannelorfeaturehandler.h"

#include "SWGMapItem.h"
#include "SWGDeviceSettings.h"

#include "mapsettings.h"
#include "mapviewcoordinator.h"
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

#ifdef QT_WEBENGINE_FOUND
// A page of our own, so the 3D map's JavaScript console reaches sdrangel.log. The page
// reports there what each frame and each message from SDRangel costs it (its "perf" line),
// and Cesium reports its own errors there, none of which was visible without attaching a
// browser debugger to it.
class MapWebPage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;   // Inherit the constructors

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID) override;
};
#endif

class QSvgWidget;

struct RadioTimeTransmitter {
    QString m_callsign;
    int m_frequency;        // In Hz
    float m_latitude;       // In degrees
    float m_longitude;      // In degrees
    int m_power;            // In kW
};

struct WEFAXTransmitter {
    QString m_callsign;         // Empty if not known
    QString m_location;
    float m_latitude;           // In degrees
    float m_longitude;          // In degrees
    QList<int> m_frequencies;   // In Hz
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
    void addWEFAXTransmitters();
    void addNAT();
    void addRadar();
    void addAurora();
    void addIonosonde();
    void addBroadcast();
    void addDAB();
    void addNavAids();
    void addAirspace(const Airspace *airspace, const QString& group, int cnt);
    void addAirspace();
    void loadAirportDatabase();
    void addAirports();
    void addWaypoints();
    //!< Build the map items for the datasets that are big enough to be worth not
    //!< building until something displays them. Called at startup and again whenever the
    //!< item settings change, so enabling a layer fills it in
    void addDeferredDatasets();
    //!< Whether anything displays a group, and so whether its items are worth building
    bool itemGroupEnabled(const QString& group) const;
    //!< Whether anything displays any group whose name starts with this, for the
    //!< datasets split over a group per category, such as the airspace classes
    bool itemGroupsEnabled(const QString& groupPrefix) const;
    void addNavtex();
    void addVLF();
    void addKiwiSDR();
    void addSpyServer();
    void addSDRangelServer();
    void find(const QString& target);
    void track3D(const QString& target);
    Q_INVOKABLE void supportedMapsChanged();
    MapSettings::MapItemSettings *getItemSettings(const QString &group);
    //!< Datasets whose map items have been built, so enabling a second group of the same
    //!< dataset, or any other settings change, does not build them again
    QSet<QString> m_builtDatasets;
    bool m_kiwiSDRListEnabled = false;  //!< KiwiSDR list is being downloaded periodically
    CesiumInterface *cesium() { return m_cesium; }
    bool useLiveryIcons() const { return m_settings.m_useLiveryIcons; }

private:
    Ui::MapGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    MapSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;
    AvailableChannelOrFeatureList m_availableChannelOrFeatures;
    QHash<const QObject *, QString> m_pipeGroups;   // Group each source belongs to, so we don't search for it per map item
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
    MapViewCoordinator m_viewCoordinator;
    QTimer m_redrawMapTimer;
    QTimer m_flushTimer;        // Hands batched item changes to the map
    QTimer m_rmpPollTimer;      // Re-reads the RMP's AM Demod frequencies, which can be changed from elsewhere
    QJsonObject m_rmpState;     // What the RMP panel was last told, so the poll only sends changes
    // Model updates are coalesced on to this period - a straight trade of CPU against
    // how far behind real time the map is. Running it at 100 ms was measured at 7.10 s of
    // flush time against 2.81 s, for 150 ms of latency, so it is only worth spending if
    // something is actually waiting on it.
    static const int m_flushPeriodMS = 250;
    bool m_restoreMapPosition;
    //!< The splitter's widgets are added as the 3D map and the NASA panel appear, and
    //!< QSplitter::restoreState() only applies when the widget count matches what was
    //!< saved - so the state is kept and re-applied as the composition changes, rather
    //!< than restored once and lost.
    QByteArray m_splitterStateToRestore;
    void restoreSplitterState();  // Use the saved position next time the map is created, rather than where it is now
    GIRO *m_giro;
    QDateTime m_giroDateTime;
    QString m_giroRunId;
    QHash<QString, IonosondeStation *> m_ionosondeStations;
    Aurora *m_aurora;
    QSharedPointer<const QList<NavAid *>> m_navAids;
    QSharedPointer<const QList<Airspace *>> m_airspaces;
    QSharedPointer<const QHash<int, AirportInformation *>> m_airportInfo;
    QSharedPointer<const QMultiHash<QString, Waypoint *>> m_waypoints;
    QGeoCoordinate m_lastFullUpdatePosition;
    KiwiSDRList m_kiwiSDRList;
    SpyServerList m_spyServerList;
    SDRangelServerList m_sdrangelServerList;
    // The versions of the downloadable 3D models, textures and sounds, fetched once a
    // session so that a new release can be offered to users who have an old one
    Asset3DManifest m_asset3DManifest;

    CesiumInterface *m_cesium;
    //!< How many times the 3D map's renderer has died without lasting long enough in
    //!< between to count as recovered, and when the last one went. The view keeps the
    //!< dead page, so it is reloaded - but a page that died on sight would otherwise be
    //!< reloaded for ever.
    int m_renderProcessTerminations = 0;
    QDateTime m_lastRenderProcessTermination;
#ifdef QT_WEBENGINE_FOUND
    // One profile shared by every map, with a disk cache - see webEngineProfile()
    static QWebEngineProfile *s_webEngineProfile;
    static int s_webEngineProfileUsers;
    static QWebEngineProfile *acquireWebEngineProfile();
    static void releaseWebEngineProfile();
#endif
    WebServer *m_webServer;
    quint16 m_webPort;
    RainViewer *m_rainViewer;
    NASAGlobalImagery m_nasaGlobalImagery;
    QList<NASAGlobalImagery::DataSet> m_nasaDataSets;
    QHash<QString, NASAGlobalImagery::DataSet> m_nasaDataSetsHash;
    NASAGlobalImagery::MetaData m_nasaMetaData;
    // The cockpit-panels menu on the PFD toolbar button
    QAction *m_displayPFDAction;
    QAction *m_displayNDAction;
    QAction *m_displayEFISAction;
    QAction *m_displayRMPAction;
    QAction *m_displayDCDUAction;

    QAction *m_displaySeaMarks;
    QAction *m_displayRailways;
    QAction *m_displayRain;
    QAction *m_displayClouds;
    QAction *m_displayNASAGlobalImagery;
    QAction *m_displayMUF;
    QAction *m_displayfoF2;
    QAction *m_displayAurora;
    QAction *m_displayMagDec;
    QAction *m_displayMaidenheadGrid;

    QString m_radarPath;
    // The cloud layer's frame, as an ISO 8601 instant rather than a path: RainViewer
    // stopped publishing satellite infrared (its satellite.infrared array is empty),
    // so clouds come from EUMETSAT's global geostationary composite instead
    QString m_cloudTime;

    NASAGlobalImagery::Legend *m_legend;
    QWidget *m_nasaWidget;
    QSvgWidget *m_legendWidget;
    QString m_loadedLegendUrl;      //!< So an unchanged legend is not fetched again
    QTableWidget *m_overviewWidget;
    QTextEdit *m_descriptionWidget;

    // Settings for opening a device
    QString m_remoteDeviceAddress;
    quint16 m_remoteDevicePort;

    explicit MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~MapGUI();

    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group);
    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void applyMap2DSettings(bool reloadMap);

public:
    // Called by MapViewCoordinator once it decides the view has moved enough to matter
    void applyMapView(const QGeoRectangle& filterRect, const QGeoRectangle& visibleRect, double zoomLevel);

private:
    bool currentMapPosition(QGeoCoordinate& coords, double& zoom) const;
    void applyMap3DSettings(bool reloadMap);
    void applyAudioVolume();
    void sendAirportsToND();
    void sendNavAidsToND();
    void createCockpitPanelsMenu();
    void applyCockpitPanels();
    void createCameraModeMenu();
    void displayCameraMode();
    bool rmpReadActive(const QString& channelId, qint64& hz) const;
    bool rmpTune(const QString& channelId, qint64 hz);
    void sendRMPState(bool force);
    void sendPanelGeometry();
    void sendEFISState();
    QString osmCachePath();
    void clearOSMCache();
    void clearStaleTileCache();
    void clearWikiMediaOSMCache();
    void displaySettings();
    bool handleMessage(const Message& message);
    void geoReply();
    QString thunderforestAPIKey() const;
    QString maptilerAPIKey() const;
    QString cartoDBAPIKey() const;
    QString cesiumIonAPIKey() const;
    QString effectiveImagery() const;
    int effectiveFrameRate() const;
    QString effectiveTerrain() const;
    QString effectiveBuildings() const;
    bool effectiveGoogleTiles() const;
    void redrawMap();
    void flushModels();
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
    static const QList<WEFAXTransmitter> m_wefaxTransmitters;

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

    void on_muteAudio_clicked(bool checked=false);
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
    void on_displayAurora_clicked(bool checked=false);
    void on_displayMagDec_clicked(bool checked=false);
    void on_displayMaidenheadGrid_clicked(bool checked=false);
    void on_layersMenu_clicked();
    void on_find_returnPressed();
    void on_maidenhead_clicked();
    void on_save_clicked();
    void on_deleteAll_clicked();
    void on_displaySettings_clicked();
    void asset3DAssetsUpdated(const QList<Asset3D>& assets);
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
    void auroraUpdated(const QImage& image);
    void navAidsUpdated();
    void airspacesUpdated();
    void airportsUpdated();
    void waypointsUpdated();
    void kiwiSDRUpdated(const QList<KiwiSDRList::KiwiSDR>& sdrs);
    void spyServerUpdated(const QList<SpyServerList::SpyServer>& sdrs);
    void sdrangelServerUpdated(const QList<SDRangelServerList::SDRangelServer>& sdrs);
    void linkClicked(const QString& url);
    void showDisplaySettings(bool startAssetUpdate);
    void checkAsset3DUpdates();
};

#endif // INCLUDE_FEATURE_MAPGUI_H_
