///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <cmath>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlContext>
#include <QQmlProperty>
#include <QGeoLocation>
#include <QGeoCoordinate>
#include <QGeoCodingManager>
#include <QGeoServiceProvider>
#include <QSettings>
#include <QMenu>
#include <QActionGroup>
#include <QToolButton>
#include <QIcon>
#include <QScreen>
#include <QSvgWidget>
#include <QTableWidgetItem>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#ifdef QT_WEBENGINE_FOUND
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#endif

#include "feature/featureuiset.h"
// For aircraftSelect(), which is how an aircraft link in a popup reaches the
// Aircraft feature - see linkClicked()
#include "feature/featurewebapiutils.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"
#include "dsp/devicesamplesource.h"
#include "device/deviceenumerator.h"
#include "util/units.h"
#include "util/profiler.h"
#include "util/maidenhead.h"
#include "util/morse.h"
#include "util/navtex.h"
#include "util/vlftransmitters.h"
#include "maplocationdialog.h"
#include "mapmaidenheaddialog.h"
#include "mapsettingsdialog.h"
#include "ibpbeacon.h"

#include "ui_mapgui.h"
#include "map.h"
#include "mapgui.h"
#include "SWGKiwiSDRSettings.h"
#include "SWGRemoteTCPInputSettings.h"

MapGUI* MapGUI::create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature)
{
    MapGUI* gui = new MapGUI(pluginAPI, featureUISet, feature);
    return gui;
}

void MapGUI::destroy()
{
    delete this;
}

void MapGUI::resetToDefaults()
{
    const QString oldMSFSNavigraphDir = m_settings.m_msfsNavigraphDir;
    m_settings.resetToDefaults();
    if (oldMSFSNavigraphDir != m_settings.m_msfsNavigraphDir) {
        addWaypoints();
    }
    displaySettings();
    applyAllSettings();
}

QByteArray MapGUI::serialize() const
{
    // Save where the map is now, so it can be restored when next opened.
    // We can't write to m_settings here, so serialize a copy - MapSettings can be
    // copied safely, as it doesn't own the item settings it points at
    MapSettings settings = m_settings;
    QGeoCoordinate coords;
    double zoom;

    if (currentMapPosition(coords, zoom))
    {
        settings.m_mapCenterLatitude = coords.latitude();
        settings.m_mapCenterLongitude = coords.longitude();
        settings.m_mapZoomLevel = zoom;
    }
    settings.m_splitterState = ui->splitter->saveState();

    return settings.serialize();
}

// Put the splitter back where it was. Kept rather than consumed: the 3D map and the
// NASA panel are added to the splitter later and on their own schedule, and a state
// saved with three widgets will not apply to a splitter that currently has one - so this
// is called again each time the composition changes, and the first call that matches
// wins. restoreState() reports whether it applied, so a state that never fits is simply
// never used rather than half applied.
void MapGUI::restoreSplitterState()
{
    if (m_splitterStateToRestore.isEmpty()) {
        return;
    }
    if (ui->splitter->restoreState(m_splitterStateToRestore)) {
        m_splitterStateToRestore.clear();
    }
}

// Get the position and zoom level of the 2D map, if it has been created
bool MapGUI::currentMapPosition(QGeoCoordinate& coords, double& zoom) const
{
    QQuickItem *item = ui->map->rootObject();

    if (!item) {
        return false;
    }

    QObject *object = item->findChild<QObject*>("map");

    if (!object) {
        return false;
    }

    coords = object->property("center").value<QGeoCoordinate>();
    zoom = object->property("zoomLevel").value<double>();

    return coords.isValid();
}

bool MapGUI::deserialize(const QByteArray& data)
{
    const QString oldMSFSNavigraphDir = m_settings.m_msfsNavigraphDir;
    if (m_settings.deserialize(data))
    {
        if (oldMSFSNavigraphDir != m_settings.m_msfsNavigraphDir) {
            addWaypoints();
        }
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        m_restoreMapPosition = true;
        m_splitterStateToRestore = m_settings.m_splitterState;
        restoreSplitterState();
        displaySettings();
        applyAllSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool MapGUI::handleMessage(const Message& message)
{
    if (Map::MsgConfigureMap::match(message))
    {
        qDebug("MapGUI::handleMessage: Map::MsgConfigureMap");
        const Map::MsgConfigureMap& cfg = (Map::MsgConfigureMap&) message;
        const QString oldMSFSNavigraphDir = m_settings.m_msfsNavigraphDir;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        if (oldMSFSNavigraphDir != m_settings.m_msfsNavigraphDir) {
            addWaypoints();
        }

        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else if (Map::MsgReportAvailableChannelOrFeatures::match(message))
    {
        Map::MsgReportAvailableChannelOrFeatures& report = (Map::MsgReportAvailableChannelOrFeatures&) message;
        m_availableChannelOrFeatures = report.getItems();

        // Work out which group each source belongs to now, rather than for every map
        // item it sends us
        m_pipeGroups.clear();
        for (const auto& channelOrFeature : m_availableChannelOrFeatures)
        {
            if (MapSettings::m_pipeTypes.contains(channelOrFeature.m_type)) {
                m_pipeGroups.insert(channelOrFeature.m_object, channelOrFeature.m_type);
            }
        }

        return true;
    }
    else if (Map::MsgFind::match(message))
    {
        Map::MsgFind& msgFind = (Map::MsgFind&) message;
        find(msgFind.getTarget());
        return true;
    }
    else if (Map::MsgSetDateTime::match(message))
    {
        Map::MsgSetDateTime& msgSetDateTime = (Map::MsgSetDateTime&) message;
        if (m_cesium) {
            m_cesium->setDateTime(msgSetDateTime.getDateTime());
        }
        return true;
    }
    else if (MainCore::MsgMapItem::match(message))
    {
        PROFILER_START()
        MainCore::MsgMapItem& msgMapItem = (MainCore::MsgMapItem&) message;
        SWGSDRangel::SWGMapItem *swgMapItem = msgMapItem.getSWGMapItem();

        // TODO: Could have this in SWGMapItem so plugins can create additional groups
        const QString group = m_pipeGroups.value(msgMapItem.getPipeSource());

        update(msgMapItem.getPipeSource(), swgMapItem, group);
        // The sender creates one of these per pipe and hands it over to us
        delete swgMapItem;
        PROFILER_STOP("Map message")
        return true;
    }

    return false;
}

void MapGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

void MapGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
}

MapGUI::MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::MapGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_objectMapModel(this),
    m_imageMapModel(this),
    m_polygonMapModel(this),
    m_polylineMapModel(this),
    m_beacons(nullptr),
    m_beaconDialog(this),
    m_ibpBeaconDialog(this),
    m_radioTimeDialog(this),
    m_cesium(nullptr),
    m_legend(nullptr),
    m_nasaWidget(nullptr),
    m_legendWidget(nullptr),
    m_overviewWidget(nullptr),
    m_descriptionWidget(nullptr),
    m_restoreMapPosition(false),
    m_viewCoordinator(this)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/map/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    // Enable MSAA antialiasing on 2D map
    // This can be much faster than using layer.smooth in the QML, when there are many items
    // However, only seems to work when set to 16, and doesn't seem to be supported on all graphics cards
    int multisamples = MainCore::instance()->getSettings().getMapMultisampling();
    if (multisamples > 0)
    {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setSamples(multisamples);
        ui->map->setFormat(format);
    }

    createNASAGlobalImageryView();
    connect(&m_nasaGlobalImagery, &NASAGlobalImagery::dataUpdated, this, &MapGUI::nasaGlobalImageryDataUpdated);
    connect(&m_nasaGlobalImagery, &NASAGlobalImagery::metaDataUpdated, this, &MapGUI::nasaGlobalImageryMetaDataUpdated);
    connect(&m_nasaGlobalImagery, &NASAGlobalImagery::legendAvailable, this, &MapGUI::nasaGlobalImageryLegendAvailable);
    connect(&m_nasaGlobalImagery, &NASAGlobalImagery::htmlAvailable, this, &MapGUI::nasaGlobalImageryHTMLAvailable);
    m_nasaGlobalImagery.getData();
    m_nasaGlobalImagery.getMetaData();

    createLayersMenu();
    displayToolbar();
    connect(screen(), &QScreen::orientationChanged, this, &MapGUI::orientationChanged);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    screen()->setOrientationUpdateMask(Qt::PortraitOrientation
                                        | Qt::LandscapeOrientation
                                        | Qt::InvertedPortraitOrientation
                                        | Qt::InvertedLandscapeOrientation);
#endif

    clearWikiMediaOSMCache();

    m_rainViewer = new RainViewer();
    connect(m_rainViewer, &RainViewer::pathUpdated, this, &MapGUI::pathUpdated);
    m_rainViewer->getPathPeriodically();

    m_mapTileServerPort = 60602;
    m_mapTileServer = new MapTileServer(m_mapTileServerPort);
    m_mapTileServer->setThunderforestAPIKey(thunderforestAPIKey());
    m_mapTileServer->setMaptilerAPIKey(maptilerAPIKey());
    m_osmPort = 0;
    m_templateServer = new OSMTemplateServer(thunderforestAPIKey(), maptilerAPIKey(), m_mapTileServerPort, m_osmPort);

    // Web server to serve dynamic files from QResources
    m_webPort = 0;
    m_webServer = new WebServer(m_webPort);

    ui->map->setAttribute(Qt::WA_AcceptTouchEvents, true);

    m_objectMapFilter.setSourceModel(&m_objectMapModel);
    m_imageMapFilter.setSourceModel(&m_imageMapModel);
    m_polygonMapFilter.setSourceModel(&m_polygonMapModel);
    m_polylineMapFilter.setSourceModel(&m_polylineMapModel);

    ui->map->rootContext()->setContextProperty("viewCoordinator", &m_viewCoordinator);
    ui->map->rootContext()->setContextProperty("mapModelFiltered", &m_objectMapFilter);
    ui->map->rootContext()->setContextProperty("mapModel", &m_objectMapModel);
    ui->map->rootContext()->setContextProperty("imageModelFiltered", &m_imageMapFilter);
    ui->map->rootContext()->setContextProperty("polygonModelFiltered", &m_polygonMapFilter);
    ui->map->rootContext()->setContextProperty("polylineModelFiltered", &m_polylineMapFilter);
    connect(ui->map, &QQuickWidget::statusChanged, this, &MapGUI::statusChanged);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map.qml")));
#else
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map_6.qml")));
#endif

    m_settings.m_modelURL = QString("http://127.0.0.1:%1/3d/").arg(m_webPort);
    m_webServer->addPathSubstitution("3d", m_settings.m_modelDir);

    m_map = reinterpret_cast<Map*>(feature);
    m_map->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    connect(&m_kiwiSDRList, &KiwiSDRList::dataUpdated, this, &MapGUI::kiwiSDRUpdated);
    connect(&m_spyServerList, &SpyServerList::dataUpdated, this, &MapGUI::spyServerUpdated);
    connect(&m_sdrangelServerList, &SDRangelServerList::dataUpdated, this, &MapGUI::sdrangelServerUpdated);

#ifdef QT_WEBENGINE_FOUND
    // Before settings() and the page() connections below, which would otherwise apply to
    // the page this replaces
    ui->web->setPage(new MapWebPage(acquireWebEngineProfile(), ui->web));
    QWebEngineSettings *settings = ui->web->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    // The DCDU's ring is played by the page without any user gesture, which Chromium's
    // autoplay policy would otherwise block
    settings->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    connect(ui->web->page(), &QWebEnginePage::fullScreenRequested, this, &MapGUI::fullScreenRequested);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    connect(ui->web->page(), &QWebEnginePage::loadingChanged, this, &MapGUI::loadingChanged);
    connect(ui->web, &QWebEngineView::renderProcessTerminated, this, &MapGUI::renderProcessTerminated);
#endif

    connect(s_webEngineProfile, &QWebEngineProfile::downloadRequested, this, &MapGUI::downloadRequested);
#endif

    // Get station position
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);
    QGeoCoordinate stationPosition(stationLatitude, stationLongitude, stationAltitude);
    // The models work out how far each item is from the station, so the filters don't
    // have to do it for every item every time the map moves
    m_objectMapModel.setStationPosition(stationPosition);
    m_imageMapModel.setStationPosition(stationPosition);
    m_polygonMapModel.setStationPosition(stationPosition);
    m_polylineMapModel.setStationPosition(stationPosition);
    m_objectMapFilter.setPosition(stationPosition);
    m_imageMapFilter.setPosition(stationPosition);
    m_polygonMapFilter.setPosition(stationPosition);
    m_polylineMapFilter.setPosition(stationPosition);

    // Centre map at My Position
    QQuickItem *item = ui->map->rootObject();
    QObject *object = item->findChild<QObject*>("map");
    if (object != nullptr)
    {
        QGeoCoordinate coords = object->property("center").value<QGeoCoordinate>();
        coords.setLatitude(stationLatitude);
        coords.setLongitude(stationLongitude);
        object->setProperty("center", QVariant::fromValue(coords));
    }

    // Create antenna at My Position
    m_antennaMapItem.setName(new QString("Station"));
    m_antennaMapItem.setLatitude(stationLatitude);
    m_antennaMapItem.setLongitude(stationLongitude);
    m_antennaMapItem.setAltitude(stationAltitude);
    m_antennaMapItem.setImage(new QString("antenna.png"));
    m_antennaMapItem.setImageRotation(0);
    m_antennaMapItem.setText(new QString(MainCore::instance()->getSettings().getStationName()));
    m_antennaMapItem.setModel(new QString("antenna.glb"));
    m_antennaMapItem.setFixedPosition(false);
    m_antennaMapItem.setPositionDateTime(new QString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
    m_antennaMapItem.setAvailableUntil(new QString(QDateTime::currentDateTime().addDays(365).toString(Qt::ISODateWithMs)));
    m_antennaMapItem.setOrientation(0);
    m_antennaMapItem.setLabel(new QString(MainCore::instance()->getSettings().getStationName()));
    m_antennaMapItem.setLabelAltitudeOffset(4.5);
    m_antennaMapItem.setAltitudeReference(1);
    update(m_map, &m_antennaMapItem, "Station");

    // Get updated when position changes
    connect(&MainCore::instance()->getSettings(), &MainSettings::preferenceChanged, this, &MapGUI::preferenceChanged);

    // Read beacons, if they exist
    PROFILER_START()
    QList<Beacon *> *beacons = Beacon::readIARUCSV(MapGUI::getBeaconFilename());
    if (beacons != nullptr) {
        setBeacons(beacons);
    }
    addIBPBeacons();
    addNAT();
    addRadioTimeTransmitters();
    addWEFAXTransmitters();
    addRadar();
    addAurora();
    addIonosonde();
    addBroadcast();
    addNavAids();
    addAirspace();
    addAirports();
    addWaypoints();
    addNavtex();
    addVLF();
    addKiwiSDR();
    addSpyServer();
    addSDRangelServer();
    PROFILER_STOP("Map DB load")

    // Before displaySettings(), which sets the menu actions' check states
    createCockpitPanelsMenu();
    createCameraModeMenu();

    displaySettings();
    applyAllSettings();

    connect(&m_objectMapModel, &ObjectMapModel::linkClicked, this, &MapGUI::linkClicked);

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);

    // Items arrive far faster than the map can usefully be redrawn, so changes are
    // collected and handed over at this rate instead
    connect(&m_flushTimer, &QTimer::timeout, this, &MapGUI::flushModels);
    m_flushTimer.start(m_flushPeriodMS);
    // The AM Demods behind the RMP's VHF channels can be retuned from their own GUI or by
    // the ADS-B plugin, so the panel is kept in step by polling rather than notification
    connect(&m_rmpPollTimer, &QTimer::timeout, this, [this]() { sendRMPState(false); });
    m_rmpPollTimer.start(1000);
    m_redrawMapTimer.setSingleShot(true);
    ui->map->installEventFilter(this);

    makeUIConnections();
    new DialogPositioner(&m_beaconDialog, true);
    new DialogPositioner(&m_ibpBeaconDialog, true);
    new DialogPositioner(&m_radioTimeDialog, true);
    m_resizer.enableChildMouseTracking();
}

MapGUI::~MapGUI()
{
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);
    m_redrawMapTimer.stop();
    //m_cesium->deleteLater();
    delete m_rainViewer;
    delete m_cesium;
    if (m_templateServer)
    {
        m_templateServer->close();
        delete m_templateServer;
    }
    if (m_mapTileServer)
    {
        m_mapTileServer->close();
        delete m_mapTileServer;
    }
    if (m_webServer)
    {
        m_webServer->close();
        delete m_webServer;
    }
    delete m_giro;
    delete m_aurora;
    qDeleteAll(m_ionosondeStations);
#ifdef QT_WEBENGINE_FOUND
    // The page is a child of the view, and the view a child of this widget - which
    // QWidget's destructor deletes after this one, too late for the profile the page
    // uses. So the view goes now, and the profile after it
    delete ui->web;
    ui->web = nullptr;
#endif
    delete ui;
#ifdef QT_WEBENGINE_FOUND
    releaseWebEngineProfile();
#endif
}

#ifdef QT_WEBENGINE_FOUND

QWebEngineProfile *MapGUI::s_webEngineProfile = nullptr;
int MapGUI::s_webEngineProfileUsers = 0;

// Qt 6's default profile is off-the-record, so every run started with an empty cache
// and fetched all of Cesium's assets, imagery, terrain and Google's 3D tiles again.
// This one keeps them on disk, under the application's cache directory. It is shared
// by every map open at once - two profiles with the same storage name would fight over
// the directory - and has to outlive every page that uses it, so is counted rather
// than owned by one map or left to the application's shutdown, which tears down
// Qt WebEngine before it deletes the application's children.
QWebEngineProfile *MapGUI::acquireWebEngineProfile()
{
    if (!s_webEngineProfile)
    {
        s_webEngineProfile = new QWebEngineProfile("Map");
        s_webEngineProfile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
        // Chromium's own limit is a few hundred MB, and a city of photorealistic tiles
        // is that on its own
        s_webEngineProfile->setHttpCacheMaximumSize(1024 * 1024 * 1024);
    }
    s_webEngineProfileUsers++;
    return s_webEngineProfile;
}

void MapGUI::releaseWebEngineProfile()
{
    if (--s_webEngineProfileUsers == 0)
    {
        delete s_webEngineProfile;
        s_webEngineProfile = nullptr;
    }
}

#endif

void MapGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

// Update a map item
void MapGUI::update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &itemGroup)
{
    // The settings group is the sending plugin's, unless the item names one of its own -
    // one plugin can want different settings for different things it draws. An unknown
    // name falls back rather than failing: getItemSettings() returns nullptr for a group
    // that does not exist, and every item holds that pointer for its lifetime.
    QString group = itemGroup;
    const QString *named = swgMapItem->getGroup();
    if (named && !named->isEmpty() && m_settings.m_itemSettings.contains(*named)) {
        group = *named;
    }

    MapModel *models[] = {
        &m_objectMapModel,      // 0 - Object
        &m_imageMapModel,       // 1 - Image
        &m_polygonMapModel,     // 2 - Polygon
        &m_polylineMapModel,    // 3 - Polyline
        &m_polylineMapModel     // 4 - Track: a polyline coloured by the altitude of its points
    };
    const int count = (int) (sizeof(models) / sizeof(models[0]));

    // An empty image means take the item off the map, and a sender removing something by
    // name does not have to say what kind of item it was - an SWGMapItem built for the
    // purpose carries type 0 whatever was drawn. Looked up under type, a line or a polygon
    // would then be searched for among the objects, never found, and left on the map beside
    // whatever replaced it. Removals are rare, so one can afford to be looked for in every
    // model rather than only in the one the type names.
    const QString *image = swgMapItem->getImage();
    if (!image || !swgMapItem->getName()) {
        return;         // Neither drawable nor removable
    }
    if (image->isEmpty())
    {
        const QString& name = *swgMapItem->getName();
        for (int i = 0; i < count; i++)
        {
            // The same model appears twice, so only the first of them finds the item
            if (models[i]->findMapItem(source, name)) {
                models[i]->update(source, swgMapItem, group);
            }
        }
        return;
    }

    const int type = swgMapItem->getType();
    if ((type >= 0) && (type < count)) {
        models[type]->update(source, swgMapItem, group);
    }
}

void MapGUI::setBeacons(QList<Beacon *> *beacons)
{
    delete m_beacons;
    m_beacons = beacons;
    m_beaconDialog.updateTable();
    // Add to Map
    QListIterator<Beacon *> i(*m_beacons);
    while (i.hasNext())
    {
        Beacon *beacon = i.next();
        SWGSDRangel::SWGMapItem beaconMapItem;
        // Need to suffix frequency, as there are multiple becaons with same callsign at different locations
        QString name = QString("%1-%2").arg(beacon->m_callsign).arg(beacon->getFrequencyShortText());
        beaconMapItem.setName(new QString(name));
        beaconMapItem.setLatitude(beacon->m_latitude);
        beaconMapItem.setLongitude(beacon->m_longitude);
        beaconMapItem.setAltitude(beacon->m_altitude);
        beaconMapItem.setImage(new QString("antenna.png"));
        beaconMapItem.setImageRotation(0);
        beaconMapItem.setText(new QString(beacon->getText()));
        beaconMapItem.setModel(new QString("antenna.glb"));
        beaconMapItem.setFixedPosition(true);
        beaconMapItem.setOrientation(0);
        // Just use callsign for label, so we don't have multiple labels on top of each other on 3D map
        // as it makes them unreadable
        beaconMapItem.setLabel(new QString(beacon->m_callsign));
        beaconMapItem.setLabelAltitudeOffset(4.5);
        beaconMapItem.setAltitudeReference(1);
        update(m_map, &beaconMapItem, "Beacons");
    }
}

void MapGUI::addIBPBeacons()
{
    // Add to Map
    for (const auto& beacon : IBPBeacon::m_beacons)
    {
        SWGSDRangel::SWGMapItem beaconMapItem;
        beaconMapItem.setName(new QString(beacon.m_callsign));
        beaconMapItem.setLatitude(beacon.m_latitude);
        beaconMapItem.setLongitude(beacon.m_longitude);
        beaconMapItem.setAltitude(0);
        beaconMapItem.setImage(new QString("antenna.png"));
        beaconMapItem.setImageRotation(0);
        beaconMapItem.setText(new QString(beacon.getText()));
        beaconMapItem.setModel(new QString("antenna.glb"));
        beaconMapItem.setFixedPosition(true);
        beaconMapItem.setOrientation(0);
        beaconMapItem.setLabel(new QString(beacon.m_callsign));
        beaconMapItem.setLabelAltitudeOffset(4.5);
        beaconMapItem.setAltitudeReference(1);
        update(m_map, &beaconMapItem, "Beacons");
    }
}

// https://www.icao.int/EURNAT/EUR%20and%20NAT%20Documents/NAT%20Documents/NAT%20Documents/NAT%20Doc%20003/NAT%20Doc003%20-%20HF%20Guidance%20v3.0.0_2015.pdf
// Coords aren't precise
const QList<RadioTimeTransmitter> MapGUI::m_natTransmitters = {
    {"Bodo", 0, 67.26742f, 14.34990, 0},
    {"Gander", 0, 48.993056f, -54.674444f, 0},
    {"Iceland", 0, 64.08516f, -21.84531f, 0},
    {"New York", 0, 40.881111f, -72.647778f, 0},
    {"Santa Maria", 0, 36.995556f, -25.170556, 0},
    {"Shanwick", 0, 52.75f, -8.933333f, 0},
};

// North Atlantic HF ATC ground stations
void MapGUI::addNAT()
{
    for (int i = 0; i < m_natTransmitters.size(); i++)
    {
        SWGSDRangel::SWGMapItem natMapItem;
        // Need to suffix frequency, as there are multiple becaons with same callsign at different locations
        QString name = QString("%1").arg(m_natTransmitters[i].m_callsign);
        natMapItem.setName(new QString(name));
        natMapItem.setLatitude(m_natTransmitters[i].m_latitude);
        natMapItem.setLongitude(m_natTransmitters[i].m_longitude);
        natMapItem.setAltitude(0.0);
        natMapItem.setImage(new QString("antenna.png"));
        natMapItem.setImageRotation(0);
        QString text = QString("NAT ATC Transmitter\nCallsign: %1")
                                .arg(m_natTransmitters[i].m_callsign);
        natMapItem.setText(new QString(text));
        natMapItem.setModel(new QString("antenna.glb"));
        natMapItem.setFixedPosition(true);
        natMapItem.setOrientation(0);
        natMapItem.setLabel(new QString(name));
        natMapItem.setLabelAltitudeOffset(4.5);
        natMapItem.setAltitudeReference(1);
        update(m_map, &natMapItem, "NAT ATC Transmitters");
    }
}

void MapGUI::addVLF()
{
    for (int i = 0; i < VLFTransmitters::m_transmitters.size(); i++)
    {
        SWGSDRangel::SWGMapItem vlfMapItem;
        QString name = QString("%1").arg(VLFTransmitters::m_transmitters[i].m_callsign);
        vlfMapItem.setName(new QString(name));
        vlfMapItem.setLatitude(VLFTransmitters::m_transmitters[i].m_latitude);
        vlfMapItem.setLongitude(VLFTransmitters::m_transmitters[i].m_longitude);
        vlfMapItem.setAltitude(0.0);
        vlfMapItem.setImage(new QString("antenna.png"));
        vlfMapItem.setImageRotation(0);
        QString text = QString("VLF Transmitter\nCallsign: %1\nFrequency: %2 kHz")
                                .arg(VLFTransmitters::m_transmitters[i].m_callsign)
                                .arg(VLFTransmitters::m_transmitters[i].m_frequency/1000.0);
        if (VLFTransmitters::m_transmitters[i].m_power > 0) {
            text.append(QString("\nPower: %1 kW").arg(VLFTransmitters::m_transmitters[i].m_power));
        }
        vlfMapItem.setText(new QString(text));
        vlfMapItem.setModel(new QString("antenna.glb"));
        vlfMapItem.setFixedPosition(true);
        vlfMapItem.setOrientation(0);
        vlfMapItem.setLabel(new QString(name));
        vlfMapItem.setLabelAltitudeOffset(4.5);
        vlfMapItem.setAltitudeReference(1);
        update(m_map, &vlfMapItem, "VLF");
    }
}

const QList<RadioTimeTransmitter> MapGUI::m_radioTimeTransmitters = {
    {"MSF", 60000, 54.9075f, -3.27333f, 17},            // UK
    {"DCF77", 77500, 50.01611111f, 9.00805556f, 50},    // Germany
    {"TDF", 162000, 47.1694f, 2.2044f, 800},            // France
    {"WWVB", 60000, 40.67805556f, -105.04666667f, 70},  // USA
    {"JJY-40", 40000, 37.3725f, 140.848889f, 50},       // Japan
    {"JJY-60", 60000, 33.465556f, 130.175556f, 50},     // Japan
    {"RTZ", 50000, 52.436111f, 103.685833f, 10},        // Russia - On 22:00 to 21:00 UTC
    {"RBU", 66666, 56.733333f, 37.663333f, 10},         // Russia
    {"BPC", 68500, 34.457f, 115.837f, 90},              // China - On 1:00 to 21:00 UTC
};

void MapGUI::addRadioTimeTransmitters()
{
    for (int i = 0; i < m_radioTimeTransmitters.size(); i++)
    {
        SWGSDRangel::SWGMapItem timeMapItem;
        // Need to suffix frequency, as there are multiple becaons with same callsign at different locations
        QString name = QString("%1").arg(m_radioTimeTransmitters[i].m_callsign);
        timeMapItem.setName(new QString(name));
        timeMapItem.setLatitude(m_radioTimeTransmitters[i].m_latitude);
        timeMapItem.setLongitude(m_radioTimeTransmitters[i].m_longitude);
        timeMapItem.setAltitude(0.0);
        timeMapItem.setImage(new QString("antennatime.png"));
        timeMapItem.setImageRotation(0);
        QString text = QString("Radio Time Transmitter\nCallsign: %1\nFrequency: %2 kHz\nPower: %3 kW")
                                .arg(m_radioTimeTransmitters[i].m_callsign)
                                .arg(m_radioTimeTransmitters[i].m_frequency/1000.0)
                                .arg(m_radioTimeTransmitters[i].m_power);
        timeMapItem.setText(new QString(text));
        timeMapItem.setModel(new QString("antenna.glb"));
        timeMapItem.setFixedPosition(true);
        timeMapItem.setOrientation(0);
        timeMapItem.setLabel(new QString(name));
        timeMapItem.setLabelAltitudeOffset(4.5);
        timeMapItem.setAltitudeReference(1);
        update(m_map, &timeMapItem, "Radio Time Transmitters");
    }
}

// Stations and frequencies from https://weatherfax.com/stations/
// Coordinates from the station map at https://weatherfax.com/ - these are often
// of the city named, rather than the transmitter site, so aren't precise
const QList<WEFAXTransmitter> MapGUI::m_wefaxTransmitters = {
    {"VMC", "Charleville, Australia", -26.402062f, 146.24536f, {2628000, 5100000, 11030000, 13920000, 20469000}},
    {"VMW", "Wiluna, Australia", -26.633726f, 120.217289f, {5755000, 7535000, 10555000, 15615000, 18060000}},
    {"VCO", "Sydney, NS, Canada", 46.138927f, -60.193233f, {4416000, 6915100}},
    {"VFA", "Inuvik, Canada", 68.360741f, -133.723022f, {4292000, 8456000}},
    {"VFF", "Iqaluit, Canada", 63.748611f, -68.519722f, {3253000, 7710000}},
    {"VFR", "Resolute, Canada", 74.697299f, -94.829729f, {3253000, 7710000}},
    {"CBM", "Punta Arenas, Chile", -53.163833f, -70.917068f, {4322000, 8696000}},
    {"CBV", "Valparaiso, Chile", -33.047237f, -71.612686f, {4228000, 8677000, 17146400}},
    {"XSG", "Shanghai, China", 31.224361f, 121.46917f, {4170000, 8302000, 12382000, 16559000}},
    {"XSQ", "Guangzhou, China", 23.128994f, 113.25325f, {4199800, 8412500, 12629300, 16826300}},
    {"DDH3/DDK6", "Hamburg, Germany", 53.551086f, 9.993682f, {3855000, 7880000, 13882500}},
    {"SVJ4", "Athens, Greece", 37.98381f, 23.727539f, {4482900, 8106900}},
    {"JMH", "Tokyo, Japan", 35.652832f, 139.839478f, {3622500, 7795000, 13988500}},
    {"JFX", "Kagoshima, Japan", 31.5969f, 130.5571f, {4274000, 8658000, 13074000, 16907500, 22559600}},
    {"JJC", "Tokyo, Japan (Kyodo News)", 35.652832f, 139.839478f, {16971000}}, // Not on weatherfax.com map, so uses JMH's coordinates
    {"HLL2", "Seoul, Korea", 37.5326f, 127.024612f, {3585000, 5857500, 7433500, 9165000, 13570000}},
    {"UDK2", "Murmansk, Russia", 68.958524f, 33.08266f, {6328500, 8444000}},
    {"", "St. Petersburg, Russia", 59.9375f, 30.308611f, {2640000, 4212000}},
    {"", "Vanino, Russia", 49.084445f, 140.253438f, {6456900}},
    {"HSW64", "Bangkok, Thailand", 13.736717f, 100.523186f, {7396900}},
    {"GYA", "Northwood, UK", 51.61944f, -0.40944f, {2618500, 4610000, 8040000, 11086500}},
    {"NMC", "Point Reyes, USA", 38.00453f, -122.79319f, {4346000, 8682000, 12786000, 17151200, 22527000}},
    {"NMF", "Boston, USA", 42.360081f, -71.058884f, {4235000, 6340500, 9110000, 12750000}},
    {"NMG", "New Orleans, USA", 29.951759f, -90.074623f, {4317900, 8503900, 12789900, 17146400}},
    {"NOJ", "Kodiak, USA", 57.790001f, -152.407227f, {4298000, 8459000, 12412500}},
    {"KVM70", "Honolulu, USA", 21.315603f, -157.858093f, {9982500, 11090000, 16135000}},
};

void MapGUI::addWEFAXTransmitters()
{
    for (const auto& transmitter : m_wefaxTransmitters)
    {
        SWGSDRangel::SWGMapItem wefaxMapItem;
        // Some stations have no published callsign, so use location as the name for those
        QString name = transmitter.m_callsign.isEmpty() ? transmitter.m_location.section(',', 0, 0) : transmitter.m_callsign;
        wefaxMapItem.setName(new QString(name));
        wefaxMapItem.setLatitude(transmitter.m_latitude);
        wefaxMapItem.setLongitude(transmitter.m_longitude);
        wefaxMapItem.setAltitude(0.0);
        wefaxMapItem.setImage(new QString("antenna.png"));
        wefaxMapItem.setImageRotation(0);
        QStringList frequencies;
        for (const auto frequency : transmitter.m_frequencies) {
            frequencies.append(QString::number(frequency / 1000.0, 'f', (frequency % 1000) == 0 ? 0 : 1));
        }
        QString text = QString("WEFAX Transmitter");
        if (!transmitter.m_callsign.isEmpty()) {
            text.append(QString("\nCallsign: %1").arg(transmitter.m_callsign));
        }
        text.append(QString("\nLocation: %1\nFrequencies: %2 kHz")
                        .arg(transmitter.m_location)
                        .arg(frequencies.join(", ")));
        wefaxMapItem.setText(new QString(text));
        wefaxMapItem.setModel(new QString("antenna.glb"));
        wefaxMapItem.setFixedPosition(true);
        wefaxMapItem.setOrientation(0);
        wefaxMapItem.setLabel(new QString(name));
        wefaxMapItem.setLabelAltitudeOffset(4.5);
        wefaxMapItem.setAltitudeReference(1);
        update(m_map, &wefaxMapItem, "WEFAX Transmitters");
    }
}

void MapGUI::addRadar()
{
    SWGSDRangel::SWGMapItem radarMapItem;
    radarMapItem.setName(new QString("GRAVES"));
    radarMapItem.setLatitude(47.3480);
    radarMapItem.setLongitude(5.5151);
    radarMapItem.setAltitude(0.0);
    radarMapItem.setImage(new QString("antenna.png"));
    radarMapItem.setImageRotation(0);
    QString text = QString("Radar\nCallsign: %1\nFrequency: %2 MHz")
                            .arg("GRAVES")
                            .arg("143.050");
    radarMapItem.setText(new QString(text));
    radarMapItem.setModel(new QString("antenna.glb"));
    radarMapItem.setFixedPosition(true);
    radarMapItem.setOrientation(0);
    radarMapItem.setLabel(new QString("GRAVES"));
    radarMapItem.setLabelAltitudeOffset(4.5);
    radarMapItem.setAltitudeReference(1);
    update(m_map, &radarMapItem, "Radar");
}

QString MapGUI::formatFrequency(qint64 frequency) const
{
    QString s = QString::number(frequency);
    if (s.endsWith("000000000")) {
        return s.left(s.size() - 9) + " GHz";
    } else if (s.endsWith("000000")) {
        return s.left(s.size() - 6) + " MHz";
    } else if (s.endsWith("000")) {
        return s.left(s.size() - 3) + " kHz";
    }
    return s + " Hz";
}

void MapGUI::addKiwiSDR()
{
    m_kiwiSDRList.getDataPeriodically();
}

void MapGUI::kiwiSDRUpdated(const QList<KiwiSDRList::KiwiSDR>& sdrs)
{
    for (const auto& sdr : sdrs)
    {
        SWGSDRangel::SWGMapItem kiwiMapItem;
        kiwiMapItem.setName(new QString(sdr.m_name));
        kiwiMapItem.setLatitude(sdr.m_latitude);
        kiwiMapItem.setLongitude(sdr.m_longitude);
        kiwiMapItem.setAltitude(sdr.m_altitude);
        kiwiMapItem.setImage(new QString("antennakiwi.png"));
        kiwiMapItem.setImageRotation(0);
        QString url = QString("sdrangel-kiwisdr://%1").arg(sdr.m_url);
        QString antenna = sdr.m_antenna;
        if (!sdr.m_antennaConnected) {
            antenna.append(" (Not connected)");
        }
        QString text = QString("KiwiSDR\nName: %1\nHW: %2\nUsers: %3/%4\nFrequency: %5 - %6\nAntenna: %7\nSNR: %8 dB\nURL: %9")
                                .arg(sdr.m_name)
                                .arg(sdr.m_sdrHW)
                                .arg(sdr.m_users)
                                .arg(sdr.m_usersMax)
                                .arg(formatFrequency(sdr.m_lowFrequency))
                                .arg(formatFrequency(sdr.m_highFrequency))
                                .arg(antenna)
                                .arg(sdr.m_snr)
                                .arg(QString("<a href=%1 onclick=\"return parent.infoboxLink('%1')\">%2</a>").arg(url).arg(sdr.m_url))
                                ;
        kiwiMapItem.setText(new QString(text));
        kiwiMapItem.setModel(new QString("antenna.glb"));
        kiwiMapItem.setFixedPosition(true);
        kiwiMapItem.setOrientation(0);
        QString band = "HF";
        if (sdr.m_highFrequency > 300000000) {
            band = "UHF";
        } else if (sdr.m_highFrequency > 32000000) { // Technically 30MHz, but many HF Kiwis list up to 32MHz
            band = "VHF";
        }
        QString label = QString("Kiwi %1").arg(band);
        kiwiMapItem.setLabel(new QString(label));
        kiwiMapItem.setLabelAltitudeOffset(4.5);
        kiwiMapItem.setAltitudeReference(1);
        update(m_map, &kiwiMapItem, "KiwiSDR");
    }
}

void MapGUI::addSpyServer()
{
    m_spyServerList.getDataPeriodically();
}

void MapGUI::spyServerUpdated(const QList<SpyServerList::SpyServer>& sdrs)
{
    for (const auto& sdr : sdrs)
    {
        SWGSDRangel::SWGMapItem spyServerMapItem;

        QString address = QString("%1:%2").arg(sdr.m_streamingHost).arg(sdr.m_streamingPort);
        spyServerMapItem.setName(new QString(address));
        spyServerMapItem.setLatitude(sdr.m_latitude);
        spyServerMapItem.setLongitude(sdr.m_longitude);
        spyServerMapItem.setAltitude(0);
        spyServerMapItem.setImage(new QString("antennaspyserver.png"));
        spyServerMapItem.setImageRotation(0);
        QString url = QString("sdrangel-spyserver://%1").arg(address);
        QString text = QString("SpyServer\nDescription: %1\nHW: %2\nUsers: %3/%4\nFrequency: %5 - %6\nAntenna: %7\nOnline: %8\nURL: %9")
                                .arg(sdr.m_generalDescription)
                                .arg(sdr.m_deviceType)
                                .arg(sdr.m_currentClientCount)
                                .arg(sdr.m_maxClients)
                                .arg(formatFrequency(sdr.m_minimumFrequency))
                                .arg(formatFrequency(sdr.m_maximumFrequency))
                                .arg(sdr.m_antennaType)
                                .arg(sdr.m_online ? "Yes" : "No")
                                .arg(QString("<a href=%1 onclick=\"return parent.infoboxLink('%1')\">%2</a>").arg(url).arg(address))
                                ;
        spyServerMapItem.setText(new QString(text));
        spyServerMapItem.setModel(new QString("antenna.glb"));
        spyServerMapItem.setFixedPosition(true);
        spyServerMapItem.setOrientation(0);
        QStringList bands;
        if (sdr.m_minimumFrequency < 30000000) {
            bands.append("HF");
        }
        if ((sdr.m_minimumFrequency < 300000000) && (sdr.m_maximumFrequency > 30000000)) {
            bands.append("VHF");
        }
        if ((sdr.m_minimumFrequency < 3000000000) && (sdr.m_maximumFrequency > 300000000)) {
            bands.append("UHF");
        }
        if (sdr.m_maximumFrequency > 3000000000) {
            bands.append("SHF");
        }
        QString label = QString("SpyServer %1").arg(bands.join(" "));
        spyServerMapItem.setLabel(new QString(label));
        spyServerMapItem.setLabelAltitudeOffset(4.5);
        spyServerMapItem.setAltitudeReference(1);
        update(m_map, &spyServerMapItem, "SpyServer");
    }
}

void MapGUI::addSDRangelServer()
{
    m_sdrangelServerList.getDataPeriodically();
}

void MapGUI::sdrangelServerUpdated(const QList<SDRangelServerList::SDRangelServer>& sdrs)
{
    for (const auto& sdr : sdrs)
    {
        SWGSDRangel::SWGMapItem sdrangelServerMapItem;

        QString address = QString("%1:%2").arg(sdr.m_address).arg(sdr.m_port);
        sdrangelServerMapItem.setName(new QString(address));
        sdrangelServerMapItem.setLatitude(sdr.m_latitude);
        sdrangelServerMapItem.setLongitude(sdr.m_longitude);
        sdrangelServerMapItem.setAltitude(sdr.m_altitude);
        sdrangelServerMapItem.setImage(new QString("antennaangel.png"));
        sdrangelServerMapItem.setImageRotation(0);
        QStringList antenna;
        if (!sdr.m_antenna.isEmpty()) {
            antenna.append(sdr.m_antenna);
        }
        if (sdr.m_isotropic) {
            antenna.append("Isotropic");
        } else {
            antenna.append(QString("Az: %1%3 El: %2%3").arg(sdr.m_azimuth).arg(sdr.m_elevation).arg(QChar(0x00b0)));
        }

        QString text = QString("%9\n\nStation: %1\nDevice: %2\nAntenna: %3\nFrequency: %4 - %5\nRemote control: %6\nUsers: %7/%8")
                                .arg(sdr.m_stationName)
                                .arg(sdr.m_device)
                                .arg(antenna.join(" - "))
                                .arg(formatFrequency(sdr.m_minFrequency))
                                .arg(formatFrequency(sdr.m_maxFrequency))
                                .arg(sdr.m_remoteControl ? "Yes" : "No")
                                .arg(sdr.m_clients)
                                .arg(sdr.m_maxClients)
                                .arg(sdr.m_protocol)
                                ;
        if (sdr.m_timeLimit > 0) {
            text.append(QString("\nTime limit: %1 mins").arg(sdr.m_timeLimit));
        }
        QString url;
        if (sdr.m_protocol == "SDRangel wss") {
            url = QString("sdrangel-wss-server://%1").arg(address);
        } else {
            url = QString("sdrangel-server://%1").arg(address);
        }
        QString link = QString("<a href=%1 onclick=\"return parent.infoboxLink('%1')\">%2</a>").arg(url).arg(address);
        text.append(QString("\nURL: %1").arg(link));
        sdrangelServerMapItem.setText(new QString(text));
        sdrangelServerMapItem.setModel(new QString("antenna.glb"));
        sdrangelServerMapItem.setFixedPosition(true);
        sdrangelServerMapItem.setOrientation(0);
        QStringList bands;
        if (sdr.m_minFrequency < 30000000) {
            bands.append("HF");
        }
        if ((sdr.m_minFrequency < 300000000) && (sdr.m_maxFrequency > 30000000)) {
            bands.append("VHF");
        }
        if ((sdr.m_minFrequency < 3000000000) && (sdr.m_maxFrequency > 300000000)) {
            bands.append("UHF");
        }
        if (sdr.m_maxFrequency > 3000000000) {
            bands.append("SHF");
        }
        QString label = QString("%1 %2").arg(sdr.m_protocol).arg(bands.join(" "));
        sdrangelServerMapItem.setLabel(new QString(label));
        sdrangelServerMapItem.setLabelAltitudeOffset(4.5);
        sdrangelServerMapItem.setAltitudeReference(1);
        update(m_map, &sdrangelServerMapItem, "SDRangel");
    }
}

void MapGUI::addAurora()
{
    m_aurora = Aurora::create();
    if (m_aurora) {
        connect(m_aurora, &Aurora::dataUpdated, this, &MapGUI::auroraUpdated);
    }
}

// Ionosonde stations
void MapGUI::addIonosonde()
{
    m_giro = GIRO::create();
    if (m_giro)
    {
        connect(m_giro, &GIRO::indexUpdated, this, &MapGUI::giroIndexUpdated);
        connect(m_giro, &GIRO::dataUpdated, this, &MapGUI::giroDataUpdated);
        connect(m_giro, &GIRO::mufUpdated, this, &MapGUI::mufUpdated);
        connect(m_giro, &GIRO::foF2Updated, this, &MapGUI::foF2Updated);
    }
}

void MapGUI::giroIndexUpdated(const QList<GIRO::DataSet>& data)
{
    (void) data;
}

void MapGUI::giroDataUpdated(const GIRO::GIROStationData& data)
{
    if (!data.m_station.isEmpty())
    {
        IonosondeStation *station = nullptr;
        // See if we already have the station in our hash
        if (!m_ionosondeStations.contains(data.m_station))
        {
            // Create new station
            station = new IonosondeStation(data);
            m_ionosondeStations.insert(data.m_station, station);
        }
        else
        {
            station = m_ionosondeStations.value(data.m_station);
        }
        station->update(data);

        // Add/update map
        SWGSDRangel::SWGMapItem ionosondeStationMapItem;
        ionosondeStationMapItem.setName(new QString(station->m_name));
        ionosondeStationMapItem.setLatitude(station->m_latitude);
        ionosondeStationMapItem.setLongitude(station->m_longitude);
        ionosondeStationMapItem.setAltitude(0.0);
        ionosondeStationMapItem.setImage(new QString("ionosonde.png"));
        ionosondeStationMapItem.setImageRotation(0);
        ionosondeStationMapItem.setText(new QString(station->m_text));
        ionosondeStationMapItem.setModel(new QString("antenna.glb"));
        ionosondeStationMapItem.setFixedPosition(true);
        ionosondeStationMapItem.setOrientation(0);
        ionosondeStationMapItem.setLabel(new QString(station->m_label));
        ionosondeStationMapItem.setLabelAltitudeOffset(4.5);
        ionosondeStationMapItem.setAltitudeReference(1);
        ionosondeStationMapItem.setAvailableFrom(new QString(data.m_dateTime.toString(Qt::ISODateWithMs)));
        ionosondeStationMapItem.setAvailableUntil(new QString(data.m_dateTime.addDays(3).toString(Qt::ISODateWithMs))); // Remove after data is too old
        update(m_map, &ionosondeStationMapItem, "Ionosonde Stations");
    }
}

void MapGUI::mufUpdated(const QJsonDocument& document)
{
    // Could possibly try render on 2D map, but contours
    // that cross anti-meridian are not drawn properly
    //${Qt5Location_PRIVATE_INCLUDE_DIRS}
    //#include <QtLocation/private/qgeojson_p.h>
    //QVariantList list = QGeoJson::importGeoJson(document);
    m_webServer->addFile("/map/map/muf.geojson", document.toJson());
    if (m_cesium) {
        m_cesium->showMUF(m_settings.m_displayMUF);
    }
}

void MapGUI::foF2Updated(const QJsonDocument& document)
{
    m_webServer->addFile("/map/map/fof2.geojson", document.toJson());
    if (m_cesium) {
        m_cesium->showfoF2(m_settings.m_displayfoF2);
    }
}

void MapGUI::updateGIRO(const QDateTime& mapDateTime)
{
    if (m_giro)
    {
        if (m_settings.m_displayMUF || m_settings.m_displayfoF2)
        {
            QString giroRunId = m_giro->getRunId(mapDateTime);
            if (m_giroRunId.isEmpty() || (!giroRunId.isEmpty() && (giroRunId != m_giroRunId)))
            {
                m_giro->getMUF(giroRunId);
                m_giro->getfoF2(giroRunId);
                m_giroRunId = giroRunId;
                m_giroDateTime = mapDateTime;
            }
        }
    }
}

void MapGUI::auroraUpdated(const QImage& image)
{
    //QString filename = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)[0] + "/aurora.png";
    //image.save(filename);
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    m_webServer->addFile("/map/map/aurora.png", ba);
    if (m_cesium) {
        m_cesium->setLayerSettings("aurora", {"show"}, {m_settings.m_displayAurora});
    }
}

void MapGUI::pathUpdated(const QString& radarPath, const QString& satellitePath)
{
    m_radarPath = radarPath;
    m_satellitePath = satellitePath;
    m_mapTileServer->setRadarPath(radarPath);
    m_mapTileServer->setSatellitePath(satellitePath);
    if (m_settings.m_displayRain || m_settings.m_displayClouds)
    {
        clearOSMCache();
        applyMap2DSettings(true);
    }
    if (m_cesium)
    {
        m_cesium->setLayerSettings("rain", {"path", "show"}, {radarPath, m_settings.m_displayRain});
        m_cesium->setLayerSettings("clouds", {"path", "show"}, {satellitePath, m_settings.m_displayClouds});
    }
}

void MapGUI::addBroadcast()
{
    QFile file(":/map/data/transmitters.csv");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);

        QString error;
        QHash<QString, int> colIndexes = CSV::readHeader(in, {"Type", "Id", "Name", "Frequency (Hz)", "Latitude", "Longitude", "Altitude (m)", "Power", "TII Main", "TII Sub"}, error);
        if (error.isEmpty())
        {
            int typeCol = colIndexes.value("Type");
            int idCol = colIndexes.value("Id");
            int nameCol = colIndexes.value("Name");
            int frequencyCol = colIndexes.value("Frequency (Hz)");
            int latCol = colIndexes.value("Latitude");
            int longCol = colIndexes.value("Longitude");
            int altCol = colIndexes.value("Altitude (m)");
            int powerCol = colIndexes.value("Power");
            int tiiMainCol = colIndexes.value("TII Main");
            int tiiSubCol = colIndexes.value("TII Sub");

            QStringList cols;
            while (CSV::readRow(in, &cols))
            {
                QString type = cols[typeCol];
                QString id = cols[idCol];
                QString name = cols[nameCol];
                QString frequency = cols[frequencyCol];
                double latitude = cols[latCol].toDouble();
                double longitude = cols[longCol].toDouble();
                double altitude = cols[altCol].toDouble();
                QString power = cols[powerCol];

                SWGSDRangel::SWGMapItem mapItem;
                mapItem.setLatitude(latitude);
                mapItem.setLongitude(longitude);
                mapItem.setAltitude(altitude);
                mapItem.setImageRotation(0);
                mapItem.setModel(new QString("antenna.glb"));
                mapItem.setFixedPosition(true);
                mapItem.setOrientation(0);
                mapItem.setLabelAltitudeOffset(4.5);
                mapItem.setAltitudeReference(1);

                if (type == "AM")
                {
                    // Name should be unique
                    mapItem.setName(new QString(id));
                    mapItem.setImage(new QString("antennaam.png"));
                    mapItem.setLabel(new QString(name));
                    QString text = QString("%1 Transmitter\nStation: %2\nFrequency: %3 kHz\nPower (ERMP): %4 kW")
                                            .arg(type)
                                            .arg(name)
                                            .arg(frequency.toDouble() / 1e3)
                                            .arg(power)
                                            ;
                    mapItem.setText(new QString(text));
                    update(m_map, &mapItem, "AM");
                }
                else if (type == "FM")
                {
                    mapItem.setName(new QString(id));
                    mapItem.setImage(new QString("antennafm.png"));
                    mapItem.setLabel(new QString(name));
                    QString text = QString("%1 Transmitter\nStation: %2\nFrequency: %3 MHz\nPower (ERP): %4 kW")
                                            .arg(type)
                                            .arg(name)
                                            .arg(frequency.toDouble() / 1e6)
                                            .arg(power)
                                            ;
                    mapItem.setText(new QString(text));
                    update(m_map, &mapItem, "FM");
                }
                else if (type == "DAB")
                {
                    mapItem.setName(new QString(id));
                    mapItem.setImage(new QString("antennadab.png"));
                    mapItem.setLabel(new QString(name));
                    QString text = QString("%1 Transmitter\nEnsemble: %2\nFrequency: %3 MHz\nPower (ERP): %4 kW\nTII: %5 %6")
                                            .arg(type)
                                            .arg(name)
                                            .arg(frequency.toDouble() / 1e6)
                                            .arg(power)
                                            .arg(cols[tiiMainCol])
                                            .arg(cols[tiiSubCol])
                                            ;
                    mapItem.setText(new QString(text));
                    update(m_map, &mapItem, "DAB");
                }
           }
        }
        else
        {
        qCritical() << "MapGUI::addBroadcast: Failed reading transmitters.csv - " << error;
        }
    }
    else
    {
        qCritical() << "MapGUI::addBroadcast: Failed to open transmitters.csv";
    }
}

/*

static QString arrayToString(QJsonArray array)
{
    QString s;
    for (int i = 0; i < array.size(); i++)
    {
        s = s.append(array[i].toString());
        s = s.append(" ");
    }
    return s;
}

// Code for FM list / DAB list, should they allow access
void MapGUI::addDAB()
{
    QFile file("stationlist_SI.json");
    if (file.open(QIODevice::ReadOnly))
    {
        QByteArray bytes = file.readAll();
        QJsonParseError error;
        QJsonDocument json = QJsonDocument::fromJson(bytes, &error);
        if (!json.isNull())
        {
            if (json.isObject())
            {
                QJsonObject obj = json.object();
                QJsonValue stations = obj.value("stations");
                QJsonArray stationsArray = stations.toArray();
                for (int i = 0; i < stationsArray.size(); i++)
                {
                    QJsonObject station = stationsArray[i].toObject();
                    // "txs" contains array of transmitters
                    QString stationName = station.value("stationName").toString();
                    QJsonArray txs = station.value("txs").toArray();
                    QString languages = arrayToString(station.value("language").toArray());
                    QString format = arrayToString(station.value("format").toArray());
                    for (int j = 0; j < txs.size(); j++)
                    {
                        QJsonObject tx = txs[j].toObject();
                        QString band = tx.value("band").toString();
                        double lat = tx.value("latitude").toString().toDouble();
                        double lon = tx.value("longitude").toString().toDouble();
                        double alt = tx.value("haat").toString().toDouble(); // This is height above terrain - not actual height - Check "haatUnits" is m
                        double frequency = tx.value("frequency").toString().toDouble();  // Can be MHz or kHz for AM
                        double erp = tx.value("erp").toString().toDouble();
                        SWGSDRangel::SWGMapItem mapItem;
                        mapItem.setLatitude(lat);
                        mapItem.setLongitude(lon);
                        mapItem.setAltitude(alt);
                        mapItem.setImageRotation(0);
                        mapItem.setModel(new QString("antenna.glb"));
                        mapItem.setFixedPosition(true);
                        mapItem.setOrientation(0);
                        mapItem.setLabelAltitudeOffset(4.5);
                        mapItem.setAltitudeReference(1);
                        if (band == "DAB")
                        {
                            // Name should be unique - can we use TII code for this? can it repeat across countries?
                            QString name = QString("%1").arg(tx.value("tsId").toString());
                            mapItem.setName(new QString(name));
                            mapItem.setImage(new QString("antennadab.png"));
                            mapItem.setLabel(new QString(name));
                            // Need tiicode?
                            QString text = QString("%1 Transmitter\nStation: %2\nFrequency: %3 %4\nPower: %5 %6\nLanguage(s): %7\nType: %8\nService: %9\nEnsemble: %10")
                                                    .arg(band)
                                                    .arg(stationName)
                                                    .arg(frequency)
                                                    .arg(tx.value("frequencyUnits").toString())
                                                    .arg(erp)
                                                    .arg(tx.value("erpUnits").toString())
                                                    .arg(languages)
                                                    .arg(format)
                                                    .arg(tx.value("serviceLabel").toString())
                                                    .arg(tx.value("ensembleLabel").toString())
                                                    ;
                            mapItem.setText(new QString(text));
                            update(m_map, &mapItem, "DAB");
                        }
                        else if (band == "FM")
                        {
                            // Name should be unique
                            QString name = QString("%1").arg(tx.value("tsId").toString());
                            mapItem.setName(new QString(name));
                            mapItem.setImage(new QString("antennafm.png"));
                            mapItem.setLabel(new QString(name));
                            QString text = QString("%1 Transmitter\nStation: %2\nFrequency: %3 %4\nPower: %5 %6\nLanguage(s): %7\nType: %8")
                                                    .arg(band)
                                                    .arg(stationName)
                                                    .arg(frequency)
                                                    .arg(tx.value("frequencyUnits").toString())
                                                    .arg(erp)
                                                    .arg(tx.value("erpUnits").toString())
                                                    .arg(languages)
                                                    .arg(format)
                                                    ;
                            mapItem.setText(new QString(text));
                            update(m_map, &mapItem, "FM");
                        }
                        else if (band == "AM")
                        {
                            // Name should be unique
                            QString name = QString("%1").arg(tx.value("tsId").toString());
                            mapItem.setName(new QString(name));
                            mapItem.setImage(new QString("antennaam.png"));
                            mapItem.setLabel(new QString(name));
                            QString text = QString("%1 Transmitter\nStation: %2\nFrequency: %3 %4\nPower: %5 %6\nLanguage(s): %7\nType: %8")
                                                    .arg(band)
                                                    .arg(stationName)
                                                    .arg(frequency)
                                                    .arg(tx.value("frequencyUnits").toString())
                                                    .arg(erp)
                                                    .arg(tx.value("erpUnits").toString())
                                                    .arg(languages)
                                                    .arg(format)
                                                    ;
                            mapItem.setText(new QString(text));
                            update(m_map, &mapItem, "AM");
                        }
                    }
                }
            }
            else
            {
                qDebug() << "MapGUI::addDAB: Expecting an object in DAB json:";
            }
        }
        else
        {
            qDebug() << "MapGUI::addDAB: Failed to parse DAB json: " << error.errorString();
        }
    }
    else
    {
        qDebug() << "MapGUI::addDAB: Failed to open DAB json";
    }
}
*/

void MapGUI::addNavAids()
{
    m_navAids = OpenAIP::getNavAids();

    for (const auto navAid : *m_navAids)
    {
        SWGSDRangel::SWGMapItem navAidMapItem;
        navAidMapItem.setName(new QString(navAid->m_name + " " + navAid->m_ident)); // Neither name or ident are unique...
        navAidMapItem.setLatitude(navAid->m_latitude);
        navAidMapItem.setLongitude(navAid->m_longitude);
        navAidMapItem.setAltitude(Units::feetToMetres(navAid->m_elevation));
        QString image = QString("%1.png").arg(navAid->m_type);
        navAidMapItem.setImage(new QString(image));
        navAidMapItem.setImageRotation(0);
        QString text = QString("NAVAID\nName: %1").arg(navAid->m_name);
        if (navAid->m_type == "NDB") {
            text.append(QString("\nFrequency: %1 kHz").arg(navAid->m_frequencykHz, 0, 'f', 1));
        } else {
            text.append(QString("\nFrequency: %1 MHz").arg(navAid->m_frequencykHz / 1000.0f, 0, 'f', 2));
        }
        if (navAid->m_channel != "") {
            text.append(QString("\nChannel: %1").arg(navAid->m_channel));
        }
        text.append(QString("\nIdent: %1 %2").arg(navAid->m_ident).arg(Morse::toSpacedUnicodeMorse(navAid->m_ident)));
        text.append(QString("\nRange: %1 nm").arg(navAid->m_range));
        if (navAid->m_alignedTrueNorth) {
            text.append(QString("\nMagnetic declination: Aligned to true North"));
        } else if (navAid->m_magneticDeclination != 0.0f) {
            text.append(QString("\nMagnetic declination: %1%2").arg(std::round(navAid->m_magneticDeclination)).arg(QChar(0x00b0)));
        }
        navAidMapItem.setText(new QString(text));
        navAidMapItem.setModel(new QString("antenna.glb"));
        navAidMapItem.setFixedPosition(true);
        navAidMapItem.setOrientation(0);
        navAidMapItem.setLabel(new QString(navAid->m_name));
        navAidMapItem.setLabelAltitudeOffset(4.5);
        navAidMapItem.setAltitudeReference(1);
        update(m_map, &navAidMapItem, "NavAid");
    }

    sendNavAidsToND();
}

// The VOR-family navaids for the ND's overlay, the same arrangement as the airports
// in sendAirportsToND() below
void MapGUI::sendNavAidsToND()
{
    if (!m_cesium || !m_navAids) {
        return;
    }

    QJsonArray navAids;
    for (const auto navAid : *m_navAids)
    {
        // VOR, VOR-DME and VORTAC; NDBs and bare DMEs would clutter more than help
        if (!navAid->m_type.startsWith("VOR")) {
            continue;
        }
        navAids.append(QJsonObject {
            {"n", navAid->m_ident},
            {"lat", navAid->m_latitude},
            {"lon", navAid->m_longitude}
        });
    }
    m_cesium->setNDNavAids(navAids);
}

void MapGUI::addAirspace(const Airspace *airspace, const QString& group, int cnt)
{
    //MapSettings::MapItemSettings *itemSettings = getItemSettings(group);

    QString details;
    details.append(airspace->m_name);
    details.append(QString("\n%1 - %2")
                .arg(airspace->getAlt(&airspace->m_bottom))
                .arg(airspace->getAlt(&airspace->m_top)));
    QString name = QString("Airspace %1 (%2)").arg(airspace->m_name).arg(cnt);

    SWGSDRangel::SWGMapItem airspaceMapItem;
    airspaceMapItem.setName(new QString(name));
    airspaceMapItem.setLatitude(airspace->m_position.y());
    airspaceMapItem.setLongitude(airspace->m_position.x());
    airspaceMapItem.setAltitude(airspace->bottomHeightInMetres());
    QString image = QString("none");
    airspaceMapItem.setImage(new QString(image));
    airspaceMapItem.setImageRotation(0);
    airspaceMapItem.setText(new QString(details));   // Not used - label is used instead for now
    airspaceMapItem.setFixedPosition(true);
    airspaceMapItem.setLabel(new QString(details));
    airspaceMapItem.setAltitudeReference(0);
    QList<SWGSDRangel::SWGMapCoordinate *> *coords = new QList<SWGSDRangel::SWGMapCoordinate *>();
    for (const auto p : airspace->m_polygon)
    {
        SWGSDRangel::SWGMapCoordinate* c = new SWGSDRangel::SWGMapCoordinate();
        c->setLatitude(p.y());
        c->setLongitude(p.x());
        c->setAltitude(airspace->bottomHeightInMetres());
        coords->append(c);
    }
    airspaceMapItem.setCoordinates(coords);
    airspaceMapItem.setExtrudedHeight(airspace->topHeightInMetres());
    airspaceMapItem.setType(2);
    update(m_map, &airspaceMapItem, group);
}


void MapGUI::addAirspace()
{
    m_airspaces = OpenAIP::getAirspaces();

    int cnt = 0;
    for (const auto airspace : *m_airspaces)
    {
         static const QMap<QString, QString> groupMap {
            {"A", "Airspace (A)"},
            {"B", "Airspace (B)"},
            {"C", "Airspace (C)"},
            {"D", "Airspace (D)"},
            {"E", "Airspace (E)"},
            {"F", "Airspace (F)"},
            {"G", "Airspace (G)"},
            {"FIR", "Airspace (FIR)"},
            {"CTR", "Airspace (CTR)"},
            {"RMZ", "Airspace (RMZ)"},
            {"TMA", "Airspace (TMA)"},
            {"TMZ", "Airspace (TMZ)"},
            {"OTH", "Airspace (OTH)"},
            {"RESTRICTED", "Airspace (Restricted)"},
            {"GLIDING", "Airspace (Gliding)"},
            {"DANGER", "Airspace (Danger)"},
            {"PROHIBITED", "Airspace (Prohibited)"},
            {"WAVE", "Airspace (Wave)"},
        };

        if (groupMap.contains(airspace->m_category))
        {
            QString group = groupMap[airspace->m_category];
            addAirspace(airspace, group, cnt);
            cnt++;

            if (   (airspace->bottomHeightInMetres() == 0)
                && ((airspace->m_category == "D") || (airspace->m_category == "G") || (airspace->m_category == "CTR"))
                && (!((airspace->m_country == "IT") && (airspace->m_name.startsWith("FIR"))))
                && (!((airspace->m_country == "PL") && (airspace->m_name.startsWith("FIS"))))
               )
            {
                group = "Airspace (Airports)";
                addAirspace(airspace, group, cnt);
                cnt++;
            }
        }
        else
        {
            qDebug() << "MapGUI::addAirspace: No group for airspace category " << airspace->m_category;
        }
    }
}

void MapGUI::addAirports()
{
    m_airportInfo = OurAirportsDB::getAirportsById();
    if (m_airportInfo)
    {
        // One item, reused for every airport - see addWaypoints() for why. The strings
        // have to be allocated before they can be assigned in place, as the default
        // SWGMapItem constructor leaves every pointer null
        SWGSDRangel::SWGMapItem airportMapItem;
        airportMapItem.setName(new QString());
        airportMapItem.setImage(new QString());
        airportMapItem.setText(new QString());
        airportMapItem.setLabel(new QString());
        airportMapItem.setModel(new QString("airport.glb")); // No such model currently, but we don't really want one
        airportMapItem.setImageRotation(0);
        airportMapItem.setFixedPosition(true);
        airportMapItem.setOrientation(0);
        airportMapItem.setLabelAltitudeOffset(4.5);
        airportMapItem.setAltitudeReference(1);

        QHashIterator<int, AirportInformation *> i(*m_airportInfo);
        while (i.hasNext())
        {
            i.next();
            AirportInformation *airport = i.value();

            *airportMapItem.getName() = airport->m_ident;
            airportMapItem.setLatitude(airport->m_latitude);
            airportMapItem.setLongitude(airport->m_longitude);
            airportMapItem.setAltitude(Units::feetToMetres(airport->m_elevation));
            *airportMapItem.getImage() = airport->getImageName();
            QStringList list;
            list.append(QString("%1: %2").arg(airport->m_ident).arg(airport->m_name));
            for (int i = 0; i < airport->m_frequencies.size(); i++)
            {
                const AirportInformation::FrequencyInformation *frequencyInfo = airport->m_frequencies[i];
                list.append(QString("%1: %2 MHz").arg(frequencyInfo->m_type).arg(frequencyInfo->m_frequency));
            }
            *airportMapItem.getText() = list.join("\n");
            *airportMapItem.getLabel() = airport->m_ident;
            QString group;
            if (airport->m_type == AirportInformation::Small) {
                group = "Airport (Small)";
            } else if (airport->m_type == AirportInformation::Medium) {
                group = "Airport (Medium)";
            } else if (airport->m_type == AirportInformation::Large) {
                group = "Airport (Large)";
            } else {
                group = "Heliport";
            }
            update(m_map, &airportMapItem, group);
        }

        sendAirportsToND();
    }
}

// The ND draws its own airport overlay - stars with idents - from a copy of the
// airport database sent to the page once, rather than scraping map entities, which
// come and go with the group settings. Re-sent when the database updates and when
// the 3D map (re)connects
void MapGUI::sendAirportsToND()
{
    if (!m_cesium || !m_airportInfo) {
        return;
    }

    QJsonArray airports;
    QHashIterator<int, AirportInformation *> i(*m_airportInfo);
    while (i.hasNext())
    {
        i.next();
        const AirportInformation *airport = i.value();
        // Large airports only - the same classification the Map's groups use. The
        // medium and small fields swamped the display; the type field is kept so the
        // page's declutter logic still works if this is ever widened again
        if (airport->m_type != AirportInformation::Large) {
            continue;
        }
        airports.append(QJsonObject {
            {"n", airport->m_ident},
            {"lat", airport->m_latitude},
            {"lon", airport->m_longitude},
            {"t", 2}
        });
    }
    m_cesium->setNDAirports(airports);
}

void MapGUI::addWaypoints()
{
    if (m_waypoints)
    {
        QSet<QString> oldWaypointNames;
        for (auto i = m_waypoints->constBegin(); i != m_waypoints->constEnd(); ++i) {
            oldWaypointNames.insert(i.key());
        }

        SWGSDRangel::SWGMapItem removeWaypoint;
        removeWaypoint.setName(new QString());
        removeWaypoint.setImage(new QString());
        for (const QString& name : oldWaypointNames)
        {
            *removeWaypoint.getName() = name;
            update(m_map, &removeWaypoint, "Waypoints");
        }
    }

    m_waypoints.reset();
    const QString bglDirectory = m_settings.m_msfsNavigraphDir.trimmed();
    if (!bglDirectory.isEmpty())
    {
        QStringList errors;
        QString bglSourceDirectory = QDir::fromNativeSeparators(bglDirectory);

        // Navigraph's standard nav-base package is a maintenance/exclusion
        // layer. The actual navigation records are installed in its sibling
        // nav-jepp package, so make the requested default package path useful
        // without asking the user to know Navigraph's internal split.
        if (QFileInfo(QDir::cleanPath(bglSourceDirectory)).fileName().compare(
                QStringLiteral("navigraph-nav-base"), Qt::CaseInsensitive) == 0)
        {
            const QString navJeppDirectory = QFileInfo(QDir::cleanPath(bglSourceDirectory))
                .dir().filePath(QStringLiteral("navigraph-nav-jepp"));
            if (QFileInfo(navJeppDirectory).isDir()) {
                bglSourceDirectory = navJeppDirectory;
            }
        }

        QMultiHash<QString, Waypoint *> *waypoints = Waypoint::readBGL(bglSourceDirectory, &errors);
        if (!waypoints->isEmpty())
        {
            m_waypoints = QSharedPointer<const QMultiHash<QString, Waypoint *>>(
                waypoints,
                [](const QMultiHash<QString, Waypoint *> *values) {
                    qDeleteAll(*values);
                    delete values;
                });
            qDebug() << "MapGUI::addWaypoints: Read" << waypoints->size()
                     << "waypoints from Navigraph BGL directory" << bglSourceDirectory;
        }
        else
        {
            delete waypoints;
            qWarning() << "MapGUI::addWaypoints: No waypoints read from Navigraph BGL directory"
                       << bglDirectory << "- falling back to waypoints.csv";
        }
        if (!errors.isEmpty()) {
            qWarning() << "MapGUI::addWaypoints:" << errors.size()
                       << "Navigraph BGL read errors; first error:" << errors.first();
        }
    }
    if (!m_waypoints) {
        m_waypoints = Waypoints::getWaypoints();
    }
    if (m_waypoints)
    {
        // One item, reused for every waypoint. Building a fresh one per waypoint means a
        // new QString for each field and a delete for each of them again straight after,
        // so the strings are allocated once here and assigned in place below. Note the
        // default SWGMapItem constructor leaves every pointer null - only the one that
        // takes JSON allocates - so they have to be given something to point at first
        SWGSDRangel::SWGMapItem waypointMapItem;
        waypointMapItem.setName(new QString());
        waypointMapItem.setImage(new QString("waypoint.png"));
        waypointMapItem.setText(new QString());
        waypointMapItem.setLabel(new QString());
        waypointMapItem.setImageRotation(0);
        waypointMapItem.setFixedPosition(true);
        waypointMapItem.setOrientation(0);
        waypointMapItem.setLabelAltitudeOffset(4.5);
        waypointMapItem.setAltitude(Units::feetToMetres(25000));
        waypointMapItem.setAltitudeReference(1);

        QMultiHash<QString, Waypoint *>::const_iterator i = m_waypoints->constBegin();
        while (i != m_waypoints->constEnd())
        {
            const Waypoint *waypoint = i.value();

            *waypointMapItem.getName() = waypoint->m_name;
            waypointMapItem.setLatitude(waypoint->m_latitude);
            waypointMapItem.setLongitude(waypoint->m_longitude);
            *waypointMapItem.getText() = QString("Waypoint: %1").arg(waypoint->m_name);
            *waypointMapItem.getLabel() = waypoint->m_name;
            update(m_map, &waypointMapItem, "Waypoints");
            ++i;
        }
    }
}

void MapGUI::navAidsUpdated()
{
    addNavAids();
}

void MapGUI::airspacesUpdated()
{
    addAirspace();
}

void MapGUI::airportsUpdated()
{
    addAirports();
}

void MapGUI::waypointsUpdated()
{
    addWaypoints();
}

void MapGUI::addNavtex()
{
    for (int i = 0; i < NavtexTransmitter::m_navtexTransmitters.size(); i++)
    {
        SWGSDRangel::SWGMapItem navtexMapItem;
        QString name = QString("%1").arg(NavtexTransmitter::m_navtexTransmitters[i].m_station);
        navtexMapItem.setName(new QString(name));
        navtexMapItem.setLatitude(NavtexTransmitter::m_navtexTransmitters[i].m_latitude);
        navtexMapItem.setLongitude(NavtexTransmitter::m_navtexTransmitters[i].m_longitude);
        navtexMapItem.setAltitude(0.0);
        navtexMapItem.setImage(new QString("antenna.png"));
        navtexMapItem.setImageRotation(0);
        QString text = QString("Navtex Transmitter\nStation: %1\nArea: %2")
                                .arg(NavtexTransmitter::m_navtexTransmitters[i].m_station)
                                .arg(NavtexTransmitter::m_navtexTransmitters[i].m_area);
        QStringList schedules;
        for (const auto& schedule : NavtexTransmitter::m_navtexTransmitters[i].m_schedules)
        {
            QString scheduleText = QString("\nFrequency: %1 kHz\nID: %2").arg(schedule.m_frequency / 1000).arg(schedule.m_id);
            if (schedule.m_times.size() > 0)
            {
                QStringList times;
                for (const auto& time : schedule.m_times) {
                    times.append(time.toString("hh:mm"));
                }
                scheduleText.append("\nTimes: ");
                scheduleText.append(times.join(" "));
                scheduleText.append(" UTC");
            }
            schedules.append(scheduleText);
        }
        text.append(schedules.join(""));
        navtexMapItem.setText(new QString(text));
        navtexMapItem.setModel(new QString("antenna.glb"));
        navtexMapItem.setFixedPosition(true);
        navtexMapItem.setOrientation(0);
        navtexMapItem.setLabel(new QString(name));
        navtexMapItem.setLabelAltitudeOffset(4.5);
        navtexMapItem.setAltitudeReference(1);
        update(m_map, &navtexMapItem, "Navtex");
    }
}

void MapGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MapGUI::nasaGlobalImageryDataUpdated(const QList<NASAGlobalImagery::DataSet>& dataSets)
{
    m_nasaDataSets = dataSets;
    m_nasaDataSetsHash.clear();
    ui->nasaGlobalImageryIdentifier->blockSignals(true);
    ui->nasaGlobalImageryIdentifier->clear();
    for (const auto& dataSet : m_nasaDataSets)
    {
        ui->nasaGlobalImageryIdentifier->addItem(dataSet.m_identifier);
        m_nasaDataSetsHash.insert(dataSet.m_identifier, dataSet);
    }
    ui->nasaGlobalImageryIdentifier->blockSignals(false);
    ui->nasaGlobalImageryIdentifier->setCurrentIndex(ui->nasaGlobalImageryIdentifier->findText(m_settings.m_nasaGlobalImageryIdentifier));
}

void MapGUI::createNASAGlobalImageryView()
{
    m_nasaWidget = new QWidget();
    m_nasaWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_legendWidget = new QSvgWidget();

    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, Qt::white);
    m_legendWidget->setAutoFillBackground(true);
    m_legendWidget->setPalette(pal);
    m_nasaWidget->setAutoFillBackground(true);
    m_nasaWidget->setPalette(pal);

    m_descriptionWidget = new QTextEdit();
    m_descriptionWidget->setReadOnly(true);
    m_descriptionWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_overviewWidget = new QTableWidget(NASA_ROWS, 2);
    m_overviewWidget->setItem(NASA_TITLE, 0, new QTableWidgetItem("Title"));
    m_overviewWidget->setItem(NASA_TITLE, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_SUBTITLE, 0, new QTableWidgetItem("Subtitle"));
    m_overviewWidget->setItem(NASA_SUBTITLE, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_DEFAULT_DATE, 0, new QTableWidgetItem("Default Date"));
    m_overviewWidget->setItem(NASA_DEFAULT_DATE, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_START_DATE, 0, new QTableWidgetItem("Start Date"));
    m_overviewWidget->setItem(NASA_START_DATE, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_END_DATE, 0, new QTableWidgetItem("End Date"));
    m_overviewWidget->setItem(NASA_END_DATE, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_PERIOD, 0, new QTableWidgetItem("Period"));
    m_overviewWidget->setItem(NASA_PERIOD, 1, new QTableWidgetItem(""));
    m_overviewWidget->setItem(NASA_LAYER_GROUP, 0, new QTableWidgetItem("Group"));
    m_overviewWidget->setItem(NASA_LAYER_GROUP, 1, new QTableWidgetItem(""));
    m_overviewWidget->horizontalHeader()->setStretchLastSection(true);
    m_overviewWidget->horizontalHeader()->hide();
    m_overviewWidget->verticalHeader()->hide();
    m_overviewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_overviewWidget->setSelectionMode(QAbstractItemView::NoSelection);

    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addWidget(m_overviewWidget);
    hbox->addWidget(m_legendWidget, 0, Qt::AlignHCenter | Qt::AlignTop);
    hbox->addWidget(m_descriptionWidget);
    hbox->setContentsMargins(0, 0, 0, 0);

    m_nasaWidget->setLayout(hbox);
    ui->splitter->addWidget(m_nasaWidget);
    restoreSplitterState();

    // Limit size of widget, otherwise the splitter makes it bigger than the maps for some unknown reason
    m_nasaWidget->setMaximumHeight(m_overviewWidget->sizeHint().height());

    m_nasaWidget->show();
}

void MapGUI::displayNASAMetaData()
{
    if (m_nasaMetaData.m_layers.contains(m_settings.m_nasaGlobalImageryIdentifier))
    {
        const NASAGlobalImagery::Layer& layer = m_nasaMetaData.m_layers.value(m_settings.m_nasaGlobalImageryIdentifier);
        const NASAGlobalImagery::DataSet& dataSet = m_nasaDataSetsHash.value(m_settings.m_nasaGlobalImageryIdentifier);

        m_overviewWidget->item(NASA_TITLE, 1)->setText(layer.m_title);
        m_overviewWidget->item(NASA_SUBTITLE, 1)->setText(layer.m_subtitle);
        m_overviewWidget->item(NASA_DEFAULT_DATE, 1)->setText(dataSet.m_defaultDateTime);
        m_overviewWidget->item(NASA_START_DATE, 1)->setText(layer.m_startDate.date().toString("yyyy-MM-dd"));
        if (layer.m_endDate.isValid()) {
            m_overviewWidget->item(NASA_END_DATE, 1)->setText(layer.m_endDate.date().toString("yyyy-MM-dd"));
        } else if (layer.m_ongoing) {
            m_overviewWidget->item(NASA_END_DATE, 1)->setText("Ongoing");
        } else {
            m_overviewWidget->item(NASA_END_DATE, 1)->setText("");
        }
        m_overviewWidget->item(NASA_PERIOD, 1)->setText(layer.m_layerPeriod);
        m_overviewWidget->item(NASA_LAYER_GROUP, 1)->setText(layer.m_layerGroup);
    }
    else
    {
        qDebug() << "MapGUI::displayNASAMetaData: No metadata for " << m_settings.m_nasaGlobalImageryIdentifier;
        m_overviewWidget->item(NASA_TITLE, 1)->setText("");
        m_overviewWidget->item(NASA_SUBTITLE, 1)->setText("");
        m_overviewWidget->item(NASA_DEFAULT_DATE, 1)->setText("");
        m_overviewWidget->item(NASA_START_DATE, 1)->setText("");
        m_overviewWidget->item(NASA_END_DATE, 1)->setText("");
        m_overviewWidget->item(NASA_PERIOD, 1)->setText("");
        m_overviewWidget->item(NASA_LAYER_GROUP, 1)->setText("");
    }
}

void MapGUI::nasaGlobalImageryMetaDataUpdated(const NASAGlobalImagery::MetaData& metaData)
{
    m_nasaMetaData = metaData;
    displayNASAMetaData();
}

// NASA's colormap legends are drawn by matplotlib, which writes every character of
// every label as a <use> of a glyph defined in the file - including the spaces. A space
// has no outline, so no path is ever defined for one, and Qt's SVG renderer warns about
// the dangling reference every single time it meets it: 404 lines of log from two
// legends, all of them about a glyph that would draw nothing.
//
// Defining the missing ids as empty paths satisfies the references and changes nothing
// that is drawn. Done by finding what is referenced and not defined, rather than by
// naming the space glyph, so any other character the generator leaves out is covered too.
static QByteArray defineMissingGlyphs(const QByteArray& svg)
{
    const QString text = QString::fromUtf8(svg);

    // Custom delimiter: the pattern ends in )" , which would close a plain R"( ... )"
    static const QRegularExpression idRe(R"RX(\bid\s*=\s*"([^"]+)")RX");
    QSet<QString> defined;
    QRegularExpressionMatchIterator ids = idRe.globalMatch(text);
    while (ids.hasNext()) {
        defined.insert(ids.next().captured(1));
    }

    // Both spellings: SVG 1.1 writes xlink:href, SVG 2 writes href
    static const QRegularExpression refRe(R"RX((?:xlink:)?href\s*=\s*"#([^"]+)")RX");
    QStringList missing;
    QRegularExpressionMatchIterator refs = refRe.globalMatch(text);
    while (refs.hasNext())
    {
        const QString id = refs.next().captured(1);
        if (!defined.contains(id) && !missing.contains(id)) {
            missing.append(id);
        }
    }
    if (missing.isEmpty()) {
        return svg;
    }

    // After the opening <svg ...> tag, so the definitions precede every use of them
    static const QRegularExpression svgTagRe(R"(<svg\b[^>]*>)");
    const QRegularExpressionMatch svgTag = svgTagRe.match(text);
    if (!svgTag.hasMatch()) {
        return svg;
    }

    // With a "d", degenerate though it is. An empty <path id=...> registers nothing:
    // Qt's SVG parser wants the d attribute and discards the element without one, so the
    // id never enters its resolve map and every <use> of it still dangles - which is why
    // the first attempt at this changed the warning count not at all. "M0,0" is a moveto
    // and nothing else, so it draws exactly what a space should.
    QString defs = "<defs>";
    for (const QString& id : missing) {
        defs += QString("<path id=\"%1\" d=\"M0,0\"/>").arg(id);
    }
    defs += "</defs>";

    QString repaired = text;
    repaired.insert(svgTag.capturedEnd(), defs);
    return repaired.toUtf8();
}

void MapGUI::nasaGlobalImageryLegendAvailable(const QString& url, const QByteArray& data)
{
    if (m_legendWidget)
    {
        m_legendWidget->load(defineMissingGlyphs(data));
        m_loadedLegendUrl = url;
        if (m_legend && (m_legend->m_height > 0))
        {
            m_legendWidget->setFixedSize(m_legend->m_width, m_legend->m_height);
            m_nasaWidget->updateGeometry();
        }
    }
}

void MapGUI::nasaGlobalImageryHTMLAvailable(const QString& url, const QByteArray& data)
{
    (void) url;

    if (m_descriptionWidget) {
        m_descriptionWidget->setHtml(data);
    }
}

QString MapGUI::osmCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation/5.8/tiles/osm/sdrangel_map";
}

void MapGUI::clearOSMCache()
{
    // Delete all cached custom tiles when user changes the URL. Is there a better way to do this?
    // Now deletes all cached tiles, to remove overlays
    QDir dir(osmCachePath());
    if (dir.exists())
    {
        // FIXME: Restore this - and use custom URL for overlays!!!
        //QStringList filenames = dir.entryList({"osm_100-l-8-*.png"}); // 8=custom URL
        QStringList filenames = dir.entryList({"osm_100-l-*"});
        for (const auto& filename : filenames)
        {
            QFile file(dir.filePath(filename));
            if (!file.remove()) {
                qDebug() << "MapGUI::clearOSMCache: Failed to remove " << file.fileName();
            }
        }
    }
}

// Delete old cache if it might contain wikimedia OSM images before switch to using OSM directly
// as the images are different
void MapGUI::clearWikiMediaOSMCache()
{
    QSettings settings;
    QString cacheCleared = "sdrangel.feature.map/cacheCleared";
    if (!settings.value(cacheCleared).toBool())
    {
        qDebug() << "MapGUI::clearWikiMediaOSMCache: Clearing cache";
        QDir dir(osmCachePath());
        if (dir.exists())
        {
            QStringList filenames = dir.entryList({"osm_100-l-1-*.png"});
            for (const auto& filename : filenames)
            {
                QFile file(dir.filePath(filename));
                if (!file.remove()) {
                    qDebug() << "MapGUI::clearWikiMediaOSMCache: Failed to remove " << file.fileName();
                }
            }
        }
        settings.setValue(cacheCleared, true);
    }
}

void MapGUI::applyMap2DSettings(bool reloadMap)
{
    ui->map->setVisible(m_settings.m_map2DEnabled);

    if (m_settings.m_map2DEnabled && reloadMap)
    {
        float stationLatitude = MainCore::instance()->getSettings().getLatitude();
        float stationLongitude = MainCore::instance()->getSettings().getLongitude();
        float stationAltitude = MainCore::instance()->getSettings().getAltitude();

        QQuickItem *item = ui->map->rootObject();

        QGeoCoordinate coords;
        double zoom;
        if (m_restoreMapPosition && (m_settings.m_mapZoomLevel >= 0.0f))
        {
            // Restore the position the map was left in when it was last saved
            m_restoreMapPosition = false;
            coords.setLatitude(m_settings.m_mapCenterLatitude);
            coords.setLongitude(m_settings.m_mapCenterLongitude);
            zoom = m_settings.m_mapZoomLevel;
        }
        else if (currentMapPosition(coords, zoom))
        {
            // Keep the map where it is, as it's only being recreated
            m_restoreMapPosition = false;
        }
        else
        {
            // Center on my location when map is first opened
            coords.setLatitude(stationLatitude);
            coords.setLongitude(stationLongitude);
            coords.setAltitude(stationAltitude);
            zoom = 10.0;
        }

        // Update API keys in servers
        m_templateServer->setThunderforestAPIKey(thunderforestAPIKey());
        m_templateServer->setMaptilerAPIKey(maptilerAPIKey());
        m_mapTileServer->setThunderforestAPIKey(thunderforestAPIKey());
        m_mapTileServer->setMaptilerAPIKey(maptilerAPIKey());

        // Create the map using the specified provider
        QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
        QQmlProperty::write(item, "mapProvider", m_settings.m_mapProvider);
        QVariantMap parameters;
        if (!m_settings.m_mapBoxAPIKey.isEmpty() && m_settings.m_mapProvider == "mapbox")
        {
            parameters["mapbox.map_id"] = "mapbox.satellite"; // The only one that works
            parameters["mapbox.access_token"] = m_settings.m_mapBoxAPIKey;
        }
        if (!m_settings.m_mapBoxAPIKey.isEmpty() && m_settings.m_mapProvider == "mapboxgl")
        {
            parameters["mapboxgl.access_token"] = m_settings.m_mapBoxAPIKey;
            if (!m_settings.m_mapBoxStyles.isEmpty())
                parameters["mapboxgl.mapping.additional_style_urls"] = m_settings.m_mapBoxStyles;
        }
        if (m_settings.m_mapProvider == "maplibregl")
        {
            parameters["maplibregl.settings_template"] = "maptiler"; // Or "mapbox"
            parameters["maplibregl.access_token"] = m_settings.m_maptilerAPIKey;
            if (!m_settings.m_mapBoxStyles.isEmpty())
                parameters["maplibregl.mapping.additional_style_urls"] = m_settings.m_mapBoxStyles;
        }
        if (m_settings.m_mapProvider == "osm")
        {
            // Allow user to specify URL
            if (!m_settings.m_osmURL.isEmpty()) {
                parameters["osm.mapping.custom.host"] = m_settings.m_osmURL;  // E.g: "http://a.tile.openstreetmap.fr/hot/"
            }
#ifdef __EMSCRIPTEN__
            // Default is http://maps-redirect.qt.io/osm/5.8/ and Emscripten needs https
            parameters["osm.mapping.providersrepository.address"] = QString("https://sdrangel.beniston.com/sdrangel/maps/");
#else
            // Use our repo, so we can append API key
            parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
#endif
            // Use application specific cache, as other apps may not use API key so will have different images
            QString cachePath = osmCachePath();
            parameters["osm.mapping.cache.directory"] = cachePath;
            // On Linux, we need to create the directory
            QDir dir(cachePath);
            if (!dir.exists()) {
                dir.mkpath(cachePath);
            }
        }

        QVariant retVal;
        if (!QMetaObject::invokeMethod(item, "createMap", Qt::DirectConnection,
                                    Q_RETURN_ARG(QVariant, retVal),
                                    Q_ARG(QVariant, QVariant::fromValue(parameters)),
                                    //Q_ARG(QVariant, mapType),
                                    Q_ARG(QVariant, QVariant::fromValue(this))))
        {
            qCritical() << "MapGUI::applyMap2DSettings - Failed to invoke createMap";
        }

        supportedMapsChanged();

        QObject *newMap = retVal.value<QObject *>();
        if (newMap != nullptr)
        {
            if (coords.isValid())
            {
                // Restore position of map
                // With Qt5, we could set the properties immediately, but with Qt6, it seems we need a delay
                // otherwise they are overwritten. Is there a signal we should use instead?
                //newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
                //newMap->setProperty("center", QVariant::fromValue(coords));
                QTimer::singleShot(200, [this, zoom, coords]() {
                    QQuickItem *root = ui->map->rootObject();
                    if (root)
                    {
                        QObject *mapObject = root->findChild<QObject*>("map");
                        if (mapObject) {
                            mapObject->setProperty("zoomLevel", QVariant::fromValue(zoom));
                            mapObject->setProperty("center", QVariant::fromValue(coords));
                        }
                    }
                });
            }
        }
        else
        {
            qCritical() << "MapGUI::applyMap2DSettings - createMap returned a nullptr";
        }
    }
}

// The filters get a rectangle bigger than the screen, so that small pans don't need them
// re-run, while the tracks are split against what is actually visible
void MapGUI::applyMapView(const QGeoRectangle& filterRect, const QGeoRectangle& visibleRect, double zoomLevel)
{
    const double topLeftLongitude = filterRect.topLeft().longitude();
    const double topLeftLatitude = filterRect.topLeft().latitude();
    const double bottomRightLongitude = filterRect.bottomRight().longitude();
    const double bottomRightLatitude = filterRect.bottomRight().latitude();

    m_objectMapFilter.viewChanged(topLeftLongitude, topLeftLatitude, bottomRightLongitude, bottomRightLatitude, zoomLevel);
    m_imageMapFilter.viewChanged(topLeftLongitude, topLeftLatitude, bottomRightLongitude, bottomRightLatitude, zoomLevel);
    m_polygonMapFilter.viewChanged(topLeftLongitude, topLeftLatitude, bottomRightLongitude, bottomRightLatitude, zoomLevel);
    m_polylineMapFilter.viewChanged(topLeftLongitude, topLeftLatitude, bottomRightLongitude, bottomRightLatitude, zoomLevel);

    m_objectMapModel.viewChanged(visibleRect.bottomLeft().longitude(), visibleRect.bottomRight().longitude());
}

void MapGUI::flushModels()
{
    PROFILER_START()
    m_objectMapModel.flush();
    m_imageMapModel.flush();
    m_polygonMapModel.flush();
    m_polylineMapModel.flush();
    PROFILER_STOP("Map flush")
}

void MapGUI::redrawMap()
{
    // An awful workaround for https://bugreports.qt.io/browse/QTBUG-100333
    // Also used in ADS-B demod
    QQuickItem *item = ui->map->rootObject();
    if (item)
    {
        QObject *object = item->findChild<QObject*>("map");
        if (object)
        {
            double zoom = object->property("zoomLevel").value<double>();
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1.0));
            object->setProperty("zoomLevel", QVariant::fromValue(zoom));
        }
    }
}

void MapGUI::showEvent(QShowEvent *event)
{
    FeatureGUI::showEvent(event);

    if (!event->spontaneous())
    {
        // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
        // MapQuickItems can be in wrong position when window is first displayed
        m_redrawMapTimer.start(500);
    }
}

bool MapGUI::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->map)
    {
        if (event->type() == QEvent::Resize)
        {
            // Workaround for https://bugreports.qt.io/browse/QTBUG-100333
            // MapQuickItems can be in wrong position after vertical resize
            QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
            QSize oldSize = resizeEvent->oldSize();
            QSize size = resizeEvent->size();
            if (oldSize.height() != size.height()) {
                redrawMap();
            }
        }
    }
    return FeatureGUI::eventFilter(obj, event);
}

void MapGUI::orientationChanged(Qt::ScreenOrientation orientation)
{
    (void) orientation;

    // Need a delay before geometry() reflects new orientation
    // https://bugreports.qt.io/browse/QTBUG-109127
    QTimer::singleShot(200, [this]() {
        displayToolbar();
    });
}

void MapGUI::displayToolbar()
{
    ui->mapTypes->setVisible(m_settings.m_map2DEnabled);

    // Replace buttons with menu when window gets narrow
    bool narrow = this->screen()->availableGeometry().width() < 400;
    ui->layersMenu->setVisible(narrow);
    bool overlayButtons = !narrow && ((m_settings.m_mapProvider == "osm") || m_settings.m_map3DEnabled);
#ifdef __EMSCRIPTEN__
    overlayButtons = false;
#endif
    ui->displayRain->setVisible(overlayButtons);
    ui->displayClouds->setVisible(overlayButtons);
    ui->displaySeaMarks->setVisible(overlayButtons);
    ui->displayRailways->setVisible(overlayButtons);
    ui->displayNASAGlobalImagery->setVisible(overlayButtons);
    ui->displayMUF->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->displayfoF2->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->displayAurora->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->displayMagDec->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->displayMaidenheadGrid->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->displayPFD->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->muteAudio->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->cameraMode->setVisible(!narrow && m_settings.m_map3DEnabled);
    ui->save->setVisible(m_settings.m_map3DEnabled);
}

void MapGUI::setEnableOverlay()
{
    bool enable = m_settings.m_displayClouds || m_settings.m_displayRain || m_settings.m_displaySeaMarks || m_settings.m_displayRailways || m_settings.m_displayNASAGlobalImagery;
    m_templateServer->setEnableOverlay(enable);
}

MapSettings::MapItemSettings *MapGUI::getItemSettings(const QString &group)
{
    if (m_settings.m_itemSettings.contains(group)) {
        return m_settings.m_itemSettings[group];
    } else {
        return nullptr;
    }
}

void MapGUI::supportedMapsChanged()
{
    QQuickItem *item = ui->map->rootObject();
    QObject *object = item->findChild<QObject*>("map");

    // Get list of map types
    ui->mapTypes->blockSignals(true);
    ui->mapTypes->clear();
    if (object != nullptr)
    {
        // Mapbox plugin only works for Satellite imagary, despite what is indicated
        if (m_settings.m_mapProvider == "mapbox")
        {
            ui->mapTypes->addItem("Satellite");
        }
        else
        {
            QVariant mapTypesVariant;
            QMetaObject::invokeMethod(item, "getMapTypes", Q_RETURN_ARG(QVariant, mapTypesVariant));
            QStringList mapTypes = mapTypesVariant.value<QStringList>();
            for (int i = 0; i < mapTypes.size(); i++) {
                ui->mapTypes->addItem(mapTypes[i]);
            }
        }
    }
    ui->mapTypes->blockSignals(false);

    // Try to select desired map, if available
    if (!m_settings.m_mapType.isEmpty())
    {
        int index = ui->mapTypes->findText(m_settings.m_mapType);
        if (index != -1) {
            ui->mapTypes->setCurrentIndex(index);
        }
    }
}

void MapGUI::on_mapTypes_currentIndexChanged(int index)
{
    if (index >= 0)
    {
        QVariant mapType = index;
        QMetaObject::invokeMethod(ui->map->rootObject(), "setMapType", Q_ARG(QVariant, mapType));
        QString currentMap = ui->mapTypes->currentText();

        if (!currentMap.isEmpty())
        {
            m_settings.m_mapType = currentMap;
            applySetting("mapType");
        }
    }
}

void MapGUI::applyMap3DSettings(bool reloadMap)
{
#ifdef QT_WEBENGINE_FOUND
    if (m_settings.m_map3DEnabled && ((m_cesium == nullptr) || reloadMap))
    {
        if (m_cesium == nullptr)
        {
            m_cesium = new CesiumInterface(&m_settings);
            connect(m_cesium, &CesiumInterface::connected, this, &MapGUI::init3DMap);
            connect(m_cesium, &CesiumInterface::received, this, &MapGUI::receivedCesiumEvent);
        }
        m_webServer->addSubstitution("/map/map/map3d.html", "$WS_PORT$", QString::number(m_cesium->serverPort()));
        m_webServer->addSubstitution("/map/map/map3d.html", "$CESIUM_ION_API_KEY$", cesiumIonAPIKey());
        m_webServer->addSubstitution("/map/map/map3d.html", "$ARCGIS_API_KEY$", m_settings.m_arcGISAPIKey);
        // So the base layer picker doesn't offer imagery that would use our Cesium Ion quota
        m_webServer->addSubstitution("/map/map/map3d.html", "$HAS_CESIUM_ION_USER_KEY$", m_settings.hasCesiumIonUserKey() ? "true" : "");
        m_webServer->addSubstitution("/map/map/map3d.html", "$DEFAULT_IMAGERY$", effectiveImagery());
        //ui->web->page()->profile()->clearHttpCache();
        ui->web->load(QUrl(QString("http://127.0.0.1:%1/map/map/map3d.html").arg(m_webPort)));
        //ui->web->load(QUrl(QString("http://webglreport.com/")));
        //ui->web->load(QUrl(QString("https://sandcastle.cesium.com/")));
        ui->web->show();
    }
    else if (!m_settings.m_map3DEnabled && (m_cesium != nullptr))
    {
        ui->web->setHtml("<html></html>");
        m_cesium->deleteLater();
        m_cesium = nullptr;
    }
    ui->web->setVisible(m_settings.m_map3DEnabled);
    if (m_cesium && m_cesium->isConnected())
    {
        m_cesium->setDefaultImagery(effectiveImagery());
        m_cesium->setTerrain(effectiveTerrain(), maptilerAPIKey(), m_settings.m_terrainLighting, m_settings.m_water);
        m_cesium->setBuildings(effectiveBuildings());
        m_cesium->setGooglePhotorealistic3DTiles(effectiveGoogleTiles());
        m_cesium->setLighting(m_settings.m_sunLightEnabled, m_settings.m_lightIntensity);
        m_cesium->setCameraReferenceFrame(m_settings.m_eciCamera);
        m_cesium->setAntiAliasing(m_settings.m_fxaa, m_settings.m_msaa);
        m_cesium->getDateTime();
        m_cesium->setCameraMode(m_settings.m_cameraMode);
        m_cesium->setHDR(m_settings.m_hdr);
        m_cesium->setFog(m_settings.m_fog);
        m_cesium->showFPS(m_settings.m_fps);
        applyCockpitPanels();
        applyAudioVolume();
        m_cesium->showMUF(m_settings.m_displayMUF);
        m_cesium->showfoF2(m_settings.m_displayfoF2);
        m_cesium->showMagDec(m_settings.m_displayMagDec);
        m_cesium->showMaidenheadGrid(m_settings.m_displayMaidenheadGrid);
        m_cesium->showLayer("rain", m_settings.m_displayRain);
        m_cesium->showLayer("clouds", m_settings.m_displayClouds);
        m_cesium->showLayer("seaMarks", m_settings.m_displaySeaMarks);
        m_cesium->showLayer("railways", m_settings.m_displayRailways);
        m_cesium->showLayer("nasaGlobalImagery", m_settings.m_displayNASAGlobalImagery);
        m_cesium->showLayer("aurora", m_settings.m_displayAurora);
        applyNASAGlobalImagerySettings();
        m_objectMapModel.allUpdated();
        m_imageMapModel.allUpdated();
        m_polygonMapModel.allUpdated();
        m_polylineMapModel.allUpdated();
    }
    MapSettings::MapItemSettings *ionosondeItemSettings = getItemSettings("Ionosonde Stations");
    if (m_giro)
    {
        m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
        if (ionosondeItemSettings) {
            m_giro->getDataPeriodically(ionosondeItemSettings->m_enabled ? 2 : 0);
        }
    }
    if (m_aurora) {
        m_aurora->getDataPeriodically(m_settings.m_displayAurora ? 30 : 0);
    }
#else
    ui->displayMUF->setVisible(false);
    ui->displayfoF2->setVisible(false);
    ui->displayAurora->setVisible(false);
    ui->displayMagDec->setVisible(false);
    ui->displayMaidenheadGrid->setVisible(false);
    m_objectMapModel.allUpdated();
    m_imageMapModel.allUpdated();
    m_polygonMapModel.allUpdated();
    m_polylineMapModel.allUpdated();
    (void)reloadMap;
#endif
}

void MapGUI::statusChanged(QQuickWidget::Status status)
{
    // In Qt6, it seems a page can be loaded multiple times, and this slot is too
    // This causes a problem in that the map created by the call to createMap can
    // be lost, so we recreate it here each time
    if (status == QQuickWidget::Ready) {
        applyMap2DSettings(true);
    }
}

#ifdef QT_WEBENGINE_FOUND

// The page's console. Its "perf" line, every five seconds, is where the 3D map's frame and
// message costs are reported - see the 3D map performance section of the plugin readme
void MapWebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                          int lineNumber, const QString &sourceID)
{
    // sourceID is a URL, and only the file in it is worth the width of the log line
    const QString source = QUrl(sourceID).fileName();

    switch (level)
    {
    case ErrorMessageLevel:
        qWarning() << "Map 3D:" << message << "-" << source << "line" << lineNumber;
        break;
    case WarningMessageLevel:
        qWarning() << "Map 3D:" << message;
        break;
    default:
        qInfo() << "Map 3D:" << message;
        break;
    }
}

void MapGUI::renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode)
{
    qDebug() << "MapGUI::renderProcessTerminated: " << terminationStatus << "exitCode" << exitCode;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))

void MapGUI::loadingChanged(const QWebEngineLoadingInfo &loadingInfo)
{
    if (loadingInfo.status() == QWebEngineLoadingInfo::LoadFailedStatus)
    {
        qDebug() << "MapGUI::loadingChanged: Failed to load " << loadingInfo.url().toString()
            << "errorString: " << loadingInfo.errorString() << " "
            << "errorDomain:" << loadingInfo.errorDomain()
            << "errorCode:" << loadingInfo.errorCode()
            ;
    }
}
#endif
#endif

void MapGUI::init3DMap()
{
#ifdef QT_WEBENGINE_FOUND
    qDebug() << "MapGUI::init3DMap";

    m_cesium->initCZML();

    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getLongitude();

    m_cesium->setPosition(QGeoCoordinate(stationLatitude, stationLongitude, stationAltitude));
    m_cesium->setDefaultImagery(effectiveImagery());
    m_cesium->setTerrain(effectiveTerrain(), maptilerAPIKey(), m_settings.m_terrainLighting, m_settings.m_water);
    m_cesium->setBuildings(effectiveBuildings());
    m_cesium->setGooglePhotorealistic3DTiles(effectiveGoogleTiles());
    m_cesium->setLighting(m_settings.m_sunLightEnabled, m_settings.m_lightIntensity);
    m_cesium->setCameraReferenceFrame(m_settings.m_eciCamera);
    m_cesium->setAntiAliasing(m_settings.m_fxaa, m_settings.m_msaa);
    m_cesium->getDateTime();
    m_cesium->setHDR(m_settings.m_hdr);
    m_cesium->setFog(m_settings.m_fog);
    m_cesium->showFPS(m_settings.m_fps);

    m_objectMapModel.allUpdated();
    m_imageMapModel.allUpdated();
    m_polygonMapModel.allUpdated();
    m_polylineMapModel.allUpdated();

    // Set 3D view after loading initial objects
    m_cesium->setHomeView(stationLatitude, stationLongitude);

    m_cesium->showMUF(m_settings.m_displayMUF);
    m_cesium->showfoF2(m_settings.m_displayfoF2);
    m_cesium->showMagDec(m_settings.m_displayMagDec);
    m_cesium->showMaidenheadGrid(m_settings.m_displayMaidenheadGrid);

    m_cesium->setCameraMode(m_settings.m_cameraMode);
    applyCockpitPanels();
    sendPanelGeometry();
    sendEFISState();
    sendAirportsToND();
    sendRMPState(true);
    sendNavAidsToND();
    applyAudioVolume();

    m_cesium->showLayer("rain", m_settings.m_displayRain);
    m_cesium->showLayer("clouds", m_settings.m_displayClouds);
    m_cesium->showLayer("seaMarks", m_settings.m_displaySeaMarks);
    m_cesium->showLayer("railways", m_settings.m_displayRailways);
    m_cesium->showLayer("aurora", m_settings.m_displayAurora);
    applyNASAGlobalImagerySettings();

    if (!m_radarPath.isEmpty()) {
        m_cesium->setLayerSettings("rain", {"path", "show"}, {m_radarPath, m_settings.m_displayRain});
    }
    if (!m_satellitePath.isEmpty()) {
        m_cesium->setLayerSettings("clouds", {"path", "show"}, {m_satellitePath, m_settings.m_displayClouds});
    }
#endif
}

void MapGUI::displaySettings()
{
    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_settings.m_title);
    setTitle(m_settings.m_title);
    blockApplySettings(true);
    ui->displayNames->setChecked(m_settings.m_displayNames);
    displayCameraMode();
    ui->displayPFD->setChecked(m_settings.m_displayCockpitPanels);
    m_displayPFDAction->setChecked(m_settings.m_displayPFD);
    m_displayNDAction->setChecked(m_settings.m_displayND);
    m_displayEFISAction->setChecked(m_settings.m_displayEFIS);
    m_displayRMPAction->setChecked(m_settings.m_displayRMP);
    m_displayDCDUAction->setChecked(m_settings.m_displayDCDU);
    ui->muteAudio->setChecked(m_settings.m_audioMuted);
    ui->displaySelectedGroundTracks->setChecked(m_settings.m_displaySelectedGroundTracks);
    ui->displayAllGroundTracks->setChecked(m_settings.m_displayAllGroundTracks);
    ui->displayRain->setChecked(m_settings.m_displayRain);
    m_displayRain->setChecked(m_settings.m_displayRain);
    m_mapTileServer->setDisplayRain(m_settings.m_displayRain);
    ui->displayClouds->setChecked(m_settings.m_displayClouds);
    m_displayClouds->setChecked(m_settings.m_displayClouds);
    m_mapTileServer->setDisplayClouds(m_settings.m_displayClouds);
    ui->displaySeaMarks->setChecked(m_settings.m_displaySeaMarks);
    m_displaySeaMarks->setChecked(m_settings.m_displaySeaMarks);
    m_mapTileServer->setDisplaySeaMarks(m_settings.m_displaySeaMarks);
    ui->displayRailways->setChecked(m_settings.m_displayRailways);
    m_displayRailways->setChecked(m_settings.m_displayRailways);
    m_mapTileServer->setDisplayRailways(m_settings.m_displayRailways);
    ui->displayNASAGlobalImagery->setChecked(m_settings.m_displayNASAGlobalImagery);
    m_displayNASAGlobalImagery->setChecked(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryIdentifier->setVisible(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryOpacity->setVisible(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryOpacityText->setVisible(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryOpacity->setValue(m_settings.m_nasaGlobalImageryOpacity);
    if (m_nasaWidget) {
        m_nasaWidget->setVisible(m_settings.m_displayNASAGlobalImagery);
    }
    m_mapTileServer->setDisplayNASAGlobalImagery(m_settings.m_displayNASAGlobalImagery);
    ui->displayMUF->setChecked(m_settings.m_displayMUF);
    m_displayMUF->setChecked(m_settings.m_displayMUF);
    ui->displayfoF2->setChecked(m_settings.m_displayfoF2);
    m_displayfoF2->setChecked(m_settings.m_displayfoF2);
    ui->displayAurora->setChecked(m_settings.m_displayAurora);
    ui->displayMagDec->setChecked(m_settings.m_displayMagDec);
    ui->displayMaidenheadGrid->setChecked(m_settings.m_displayMaidenheadGrid);
    m_objectMapModel.setDisplayNames(m_settings.m_displayNames);
    m_objectMapModel.setDisplaySelectedGroundTracks(m_settings.m_displaySelectedGroundTracks);
    m_objectMapModel.setDisplayAllGroundTracks(m_settings.m_displayAllGroundTracks);
    m_objectMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_imageMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_polygonMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_polylineMapModel.updateItemSettings(m_settings.m_itemSettings);
    setEnableOverlay();
    displayToolbar();
    applyMap2DSettings(true);
    applyMap3DSettings(true);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void MapGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicFeatureSettingsDialog dialog(this);
        dialog.setTitle(m_settings.m_title);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIFeatureSetIndex(m_settings.m_reverseAPIFeatureSetIndex);
        dialog.setReverseAPIFeatureIndex(m_settings.m_reverseAPIFeatureIndex);
        dialog.setDefaultTitle(m_displayedName);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        QStringList settingsKeys({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex"
            });

        applySettings(settingsKeys);
    }

    resetContextMenuType();
}

void MapGUI::applySetting(const QString& settingsKey)
{
    applySettings({settingsKey});
}

void MapGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    m_settingsKeys.append(settingsKeys);
    if (m_doApplySettings)
    {
        Map::MsgConfigureMap* message = Map::MsgConfigureMap::create(m_settings, m_settingsKeys, force);
        m_map->getInputMessageQueue()->push(message);

        m_settingsKeys.clear();
    }
}

void MapGUI::applyAllSettings()
{
    applySettings(QStringList(), true);
}

void MapGUI::on_maidenhead_clicked()
{
    MapMaidenheadDialog dialog;
    new DialogPositioner(&dialog, true);
    dialog.exec();
}

// The camera mode button is a menu of the three mutually exclusive ways the camera can
// follow the tracked object; the button's icon shows which one is in use
void MapGUI::createCameraModeMenu()
{
    QMenu *menu = new QMenu(ui->cameraMode);
    QActionGroup *group = new QActionGroup(menu);

    auto addMode = [this, menu, group](const QString& text, int mode) {
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        action->setData(mode);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode]() {
            m_settings.m_cameraMode = mode;
            displayCameraMode();
            if (m_cesium) {
                m_cesium->setCameraMode(mode);
            }
            applySetting("cameraMode");
        });
        return action;
    };

    addMode("Third person", MapSettings::ThirdPerson);
    addMode("First person", MapSettings::FirstPerson);
    addMode("Fly-by", MapSettings::FlyBy);

    ui->cameraMode->setMenu(menu);
    ui->cameraMode->setPopupMode(QToolButton::InstantPopup);
}

void MapGUI::displayCameraMode()
{
    QMenu *menu = ui->cameraMode->menu();
    if (!menu) {
        return;
    }
    for (QAction *action : menu->actions())
    {
        if (action->data().toInt() == m_settings.m_cameraMode) {
            action->setChecked(true);
        }
    }
    QString icon;
    if (m_settings.m_cameraMode == MapSettings::FirstPerson) {
        icon = ":/map/icons/firstperson.png";
    } else if (m_settings.m_cameraMode == MapSettings::FlyBy) {
        icon = ":/map/icons/flyby.png";
    } else {
        icon = ":/map/icons/thirdperson.png";
    }
    ui->cameraMode->setIcon(QIcon(icon));
}

// Show the cockpit panels selected in the toolbar button's menu, or hide them all
// when the button itself is off - it is the master switch for the selection
void MapGUI::applyCockpitPanels()
{
    if (!m_cesium) {
        return;
    }
    const bool on = m_settings.m_displayCockpitPanels;
    m_cesium->showPFD(on && m_settings.m_displayPFD);
    m_cesium->showND(on && m_settings.m_displayND);
    m_cesium->showEFIS(on && m_settings.m_displayEFIS);
    m_cesium->showRMP(on && m_settings.m_displayRMP);
    m_cesium->showDCDU(on && m_settings.m_displayDCDU);
}

// The PFD toolbar button is a switch with a menu: the menu's checkboxes choose which
// cockpit panels are wanted, and the button turns that whole selection on or off
void MapGUI::createCockpitPanelsMenu()
{
    QMenu *menu = new QMenu(ui->displayPFD);

    auto addPanel = [this, menu](const QString& text, bool MapSettings::*flag, const char *key) {
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, flag, key](bool checked) {
            m_settings.*flag = checked;
            applyCockpitPanels();
            applySetting(key);
        });
        return action;
    };

    m_displayPFDAction = addPanel("PFD", &MapSettings::m_displayPFD, "displayPFD");
    m_displayNDAction = addPanel("ND", &MapSettings::m_displayND, "displayND");
    m_displayEFISAction = addPanel("EFIS", &MapSettings::m_displayEFIS, "displayEFIS");
    m_displayRMPAction = addPanel("RMP", &MapSettings::m_displayRMP, "displayRMP");
    m_displayDCDUAction = addPanel("DCDU", &MapSettings::m_displayDCDU, "displayDCDU");

    ui->displayPFD->setMenu(menu);
    // Clicking the button toggles; the arrow section opens the menu
    ui->displayPFD->setPopupMode(QToolButton::MenuButtonPopup);
    connect(ui->displayPFD, &ButtonSwitch::toggled, this, [this](bool checked) {
        m_settings.m_displayCockpitPanels = checked;
        applyCockpitPanels();
        applySetting("displayCockpitPanels");
    });
}

// The configured volumes with the toolbar mute applied on top - muting sends zero
// volumes to the page rather than changing the stored settings, so unmuting restores
// exactly what the sliders say
void MapGUI::applyAudioVolume()
{
    if (m_cesium)
    {
        m_cesium->setAudioVolume(
            m_settings.m_audioMuted ? 0 : m_settings.m_mapItemSoundVolume,
            m_settings.m_audioMuted ? 0 : m_settings.m_cockpitSoundVolume);
    }
}

void MapGUI::on_muteAudio_clicked(bool checked)
{
    m_settings.m_audioMuted = checked;
    applyAudioVolume();
    applySetting("audioMuted");
}

void MapGUI::on_displayNames_clicked(bool checked)
{
    m_settings.m_displayNames = checked;
    m_objectMapModel.setDisplayNames(checked);
    applySetting("displayNames");
}

void MapGUI::on_displaySelectedGroundTracks_clicked(bool checked)
{
    m_settings.m_displaySelectedGroundTracks = checked;
    m_objectMapModel.setDisplaySelectedGroundTracks(checked);
    applySetting("displaySelectedGroundTracks");
}

void MapGUI::on_displayAllGroundTracks_clicked(bool checked)
{
    m_settings.m_displayAllGroundTracks = checked;
    m_objectMapModel.setDisplayAllGroundTracks(checked);
    applySetting("displayAllGroundTracks");
}

void MapGUI::on_displayRain_clicked(bool checked)
{
    if (this->sender() != ui->displayRain) {
        ui->displayRain->setChecked(checked);
    }
    if (this->sender() != m_displayRain) {
        m_displayRain->setChecked(checked);
    }
    m_settings.m_displayRain = checked;
    m_mapTileServer->setDisplayRain(m_settings.m_displayRain);
    setEnableOverlay();
    clearOSMCache();
    applyMap2DSettings(true);
    if (m_cesium) {
        m_cesium->showLayer("rain", m_settings.m_displayRain);
    }
    applySetting("displayRain");
}

void MapGUI::on_displayClouds_clicked(bool checked)
{
    if (this->sender() != ui->displayClouds) {
        ui->displayClouds->setChecked(checked);
    }
    if (this->sender() != m_displayClouds) {
        m_displayClouds->setChecked(checked);
    }
    m_settings.m_displayClouds = checked;
    m_mapTileServer->setDisplayClouds(m_settings.m_displayClouds);
    setEnableOverlay();
    clearOSMCache();
    applyMap2DSettings(true);
    if (m_cesium) {
        m_cesium->showLayer("clouds", m_settings.m_displayClouds);
    }
    applySetting("displayClouds");
}

void MapGUI::on_displaySeaMarks_clicked(bool checked)
{
    if (this->sender() != ui->displaySeaMarks) {
        ui->displaySeaMarks->setChecked(checked);
    }
    if (this->sender() != m_displaySeaMarks) {
        m_displaySeaMarks->setChecked(checked);
    }
    m_settings.m_displaySeaMarks = checked;
    m_mapTileServer->setDisplaySeaMarks(m_settings.m_displaySeaMarks);
    setEnableOverlay();
    clearOSMCache();
    applyMap2DSettings(true);
    if (m_cesium) {
        m_cesium->showLayer("seaMarks", m_settings.m_displaySeaMarks);
    }
    applySetting("displaySeaMarks");
}

void MapGUI::on_displayRailways_clicked(bool checked)
{
    if (this->sender() != ui->displayRailways) {
        ui->displayRailways->setChecked(checked);
    }
    if (this->sender() != m_displayRailways) {
        m_displayRailways->setChecked(checked);
    }
    m_settings.m_displayRailways = checked;
    m_mapTileServer->setDisplayRailways(m_settings.m_displayRailways);
    setEnableOverlay();
    clearOSMCache();
    applyMap2DSettings(true);
    if (m_cesium) {
        m_cesium->showLayer("railways", m_settings.m_displayRailways);
    }
    applySetting("displayRailways");
}

void MapGUI::on_displayNASAGlobalImagery_clicked(bool checked)
{
    if (this->sender() != ui->displayNASAGlobalImagery) {
        ui->displayNASAGlobalImagery->setChecked(checked);
    }
    if (this->sender() != m_displayNASAGlobalImagery) {
        m_displayNASAGlobalImagery->setChecked(checked);
    }
    m_settings.m_displayNASAGlobalImagery = checked;
    ui->nasaGlobalImageryIdentifier->setVisible(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryOpacity->setVisible(m_settings.m_displayNASAGlobalImagery);
    ui->nasaGlobalImageryOpacityText->setVisible(m_settings.m_displayNASAGlobalImagery);
    if (m_nasaWidget) {
        m_nasaWidget->setVisible(m_settings.m_displayNASAGlobalImagery);
    }
    m_mapTileServer->setDisplayNASAGlobalImagery(m_settings.m_displayNASAGlobalImagery);
    setEnableOverlay();
    clearOSMCache();
    applyMap2DSettings(true);
    if (m_cesium) {
        m_cesium->showLayer("NASAGlobalImagery", m_settings.m_displayNASAGlobalImagery);
    }
    applySetting("displayNASAGlobalImagery");
}

void MapGUI::on_nasaGlobalImageryIdentifier_currentIndexChanged(int index)
{
    if ((index >= 0) && (index < m_nasaDataSets.size()))
    {
        m_settings.m_nasaGlobalImageryIdentifier = m_nasaDataSets[index].m_identifier;
        // MODIS_Terra_Aerosol/default/2014-04-09/GoogleMapsCompatible_Level6
        QString date = "default"; // FIXME: Get from 3D map
        QString path = QString("%1/default/%2/%3").arg(m_settings.m_nasaGlobalImageryIdentifier).arg(date).arg(m_nasaDataSets[index].m_tileMatrixSet);
        m_mapTileServer->setNASAGlobalImageryPath(path);
        QString format = m_nasaDataSets[index].m_format;
        if (format == "image/jpeg") {
            m_mapTileServer->setNASAGlobalImageryFormat("jpeg");
        } else {
            m_mapTileServer->setNASAGlobalImageryFormat("png");
        }
        setEnableOverlay();
        clearOSMCache();
        applyMap2DSettings(true);
        applyNASAGlobalImagerySettings();
    }
}

void MapGUI::applyNASAGlobalImagerySettings()
{
    int index = ui->nasaGlobalImageryIdentifier->currentIndex();

    // Update 3D map
    if (m_cesium)
    {
        if ((index >= 0) && (index < m_nasaDataSets.size()))
        {
            QString format = m_nasaDataSets[index].m_format;
            QString extension = (format == "image/jpeg") ? "jpg" : "png";
            // Does cesium want epsg3857 or epsg4326
            //QString url = QString("https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/%1/default/{Time}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.%2").arg(m_settings.m_nasaGlobalImageryIdentifier).arg(extension);
            //QString url = QString("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/%1/default/default/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.%2").arg(m_settings.m_nasaGlobalImageryIdentifier).arg(extension);
            QString url = QString("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/%1/default/{Time}/{TileMatrixSet}/{TileMatrix}/{TileRow}/{TileCol}.%2").arg(m_settings.m_nasaGlobalImageryIdentifier).arg(extension);
            QString show = m_settings.m_displayNASAGlobalImagery ? "true" : "false";

            m_cesium->setLayerSettings("NASAGlobalImagery",
                {"url", "tileMatrixSet", "format", "show", "opacity", "dates"},
                {url, m_nasaDataSets[index].m_tileMatrixSet, format, m_settings.m_displayNASAGlobalImagery, m_settings.m_nasaGlobalImageryOpacity, m_nasaDataSets[index].m_dates});
        }
    }

    // Update NASA table / legend / description
    if ((index >= 0) && (m_nasaDataSets[index].m_legends.size() > 0))
    {
        m_legend = &m_nasaDataSets[index].m_legends[0];
        // Only when it is not the one already shown. This runs on every 3D settings
        // apply as well as on a change of dataset, and re-fetching an unchanged legend
        // costs a request to NASA and a re-parse to arrive at the same picture - which
        // is why the warnings above arrived in twos.
        if (m_legend->m_url != m_loadedLegendUrl) {
            m_nasaGlobalImagery.downloadLegend(*m_legend);
        }
        m_descriptionWidget->setHtml("");
        if (m_nasaMetaData.m_layers.contains(m_settings.m_nasaGlobalImageryIdentifier))
        {
            QString url = m_nasaMetaData.m_layers.value(m_settings.m_nasaGlobalImageryIdentifier).m_descriptionURL;
            m_nasaGlobalImagery.downloadHTML(url);
        }
        displayNASAMetaData();
        m_nasaWidget->setVisible(m_settings.m_displayNASAGlobalImagery);
    }
    else
    {
        if (m_nasaWidget) {
            m_nasaWidget->setVisible(false);
        }
    }
}

void MapGUI::on_nasaGlobalImageryOpacity_valueChanged(int value)
{
    m_settings.m_nasaGlobalImageryOpacity = value;
    ui->nasaGlobalImageryOpacityText->setText(QString("%1%").arg(m_settings.m_nasaGlobalImageryOpacity));

    if (m_cesium)
    {
        m_cesium->setLayerSettings("NASAGlobalImagery",
                {"opacity"},
                {m_settings.m_nasaGlobalImageryOpacity}
        );
    }
    applySetting("nasaGlobalImageryOpacity");
}

void MapGUI::on_displayMUF_clicked(bool checked)
{
    if (this->sender() != ui->displayMUF) {
        ui->displayMUF->setChecked(checked);
    }
    if (this->sender() != m_displayMUF) {
        m_displayMUF->setChecked(checked);
    }
    m_settings.m_displayMUF = checked;
    if (m_giro) {
        m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
    }
    if (m_cesium) {
        m_cesium->showMUF(m_settings.m_displayMUF);
    }
    applySetting("displayMUF");
}

void MapGUI::on_displayfoF2_clicked(bool checked)
{
    if (this->sender() != ui->displayfoF2) {
        ui->displayfoF2->setChecked(checked);
    }
    if (this->sender() != m_displayfoF2) {
        m_displayfoF2->setChecked(checked);
    }
    m_settings.m_displayfoF2 = checked;
    if (m_giro) {
        m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
    }
    if (m_cesium) {
        m_cesium->showfoF2(m_settings.m_displayfoF2);
    }
    applySetting("displayfoF2");
}

void MapGUI::on_displayAurora_clicked(bool checked)
{
    if (this->sender() != ui->displayAurora) {
        ui->displayAurora->setChecked(checked);
    }
    if (this->sender() != m_displayAurora) {
        m_displayAurora->setChecked(checked);
    }
    m_settings.m_displayAurora = checked;
    // Only call show if disabling, so we don't get two updates
    // (as getDataPeriodically results in a call to showLayer when the data is available)
    if (m_aurora) {
        m_aurora->getDataPeriodically(m_settings.m_displayAurora ? 30 : 0);
    }
    if (m_cesium && !m_settings.m_displayAurora) {
        m_cesium->showLayer("aurora", m_settings.m_displayAurora);
    }
    applySetting("displayAurora");
}

void MapGUI::on_displayMagDec_clicked(bool checked)
{
    if (this->sender() != ui->displayMagDec) {
        ui->displayMagDec->setChecked(checked);
    }
    if (this->sender() != m_displayMagDec) {
        m_displayMagDec->setChecked(checked);
    }
    m_settings.m_displayMagDec = checked;
    if (m_cesium) {
        m_cesium->showMagDec(m_settings.m_displayMagDec);
    }
    applySetting("displayMagDec");
}

void MapGUI::on_displayMaidenheadGrid_clicked(bool checked)
{
    if (this->sender() != ui->displayMaidenheadGrid) {
        ui->displayMaidenheadGrid->setChecked(checked);
    }
    if (this->sender() != m_displayMaidenheadGrid) {
        m_displayMaidenheadGrid->setChecked(checked);
    }
    m_settings.m_displayMaidenheadGrid = checked;
    if (m_cesium) {
        m_cesium->showMaidenheadGrid(m_settings.m_displayMaidenheadGrid);
    }
    applySetting("displayMaidenheadGrid");
}

void MapGUI::createLayersMenu()
{
    QMenu *menu = new QMenu();

    m_displayRain = menu->addAction("Weather radar");
    m_displayRain->setCheckable(true);
    m_displayRain->setToolTip("Display weather radar (rain/snow)");
    connect(m_displayRain, &QAction::triggered, this, &MapGUI::on_displayRain_clicked);

    m_displayClouds = menu->addAction("Satellite IR");
    m_displayClouds->setCheckable(true);
    m_displayClouds->setToolTip("Display satellite infra-red (clouds)");
    connect(m_displayClouds, &QAction::triggered, this, &MapGUI::on_displayClouds_clicked);

    m_displaySeaMarks = menu->addAction("Sea marks");
    m_displaySeaMarks->setCheckable(true);
    m_displaySeaMarks->setToolTip("Display sea marks");
    //QIcon seaMarksIcon;
    //seaMarksIcon.addPixmap(QPixmap("://map/icons/anchor.png"), QIcon::Normal, QIcon::On);
    //displaySeaMarks->setIcon(seaMarksIcon);
    connect(m_displaySeaMarks, &QAction::triggered, this, &MapGUI::on_displaySeaMarks_clicked);

    m_displayRailways = menu->addAction("Railways");
    m_displayRailways->setCheckable(true);
    m_displayRailways->setToolTip("Display railways");
    connect(m_displayRailways, &QAction::triggered, this, &MapGUI::on_displayRailways_clicked);

    m_displayNASAGlobalImagery = menu->addAction("NASA Global Imagery");
    m_displayNASAGlobalImagery->setCheckable(true);
    m_displayNASAGlobalImagery->setToolTip("Display NASA Global Imagery");
    connect(m_displayNASAGlobalImagery, &QAction::triggered, this, &MapGUI::on_displayNASAGlobalImagery_clicked);

    m_displayMUF = menu->addAction("MUF");
    m_displayMUF->setCheckable(true);
    m_displayMUF->setToolTip("Display Maximum Usable Frequency contours");
    connect(m_displayMUF, &QAction::triggered, this, &MapGUI::on_displayMUF_clicked);

    m_displayfoF2 = menu->addAction("foF2");
    m_displayfoF2->setCheckable(true);
    m_displayfoF2->setToolTip("Display F2 layer critical frequency contours");
    connect(m_displayfoF2, &QAction::triggered, this, &MapGUI::on_displayfoF2_clicked);

    m_displayAurora = menu->addAction("Aurora");
    m_displayAurora->setCheckable(true);
    m_displayAurora->setToolTip("Display aurora prediction");
    connect(m_displayAurora, &QAction::triggered, this, &MapGUI::on_displayAurora_clicked);

    m_displayMagDec = menu->addAction("Mag Dec");
    m_displayMagDec->setCheckable(true);
    m_displayMagDec->setToolTip("Display magnetic declination");
    connect(m_displayMagDec, &QAction::triggered, this, &MapGUI::on_displayMagDec_clicked);

    m_displayMaidenheadGrid = menu->addAction("Maidenhead grid");
    m_displayMaidenheadGrid->setCheckable(true);
    m_displayMaidenheadGrid->setToolTip("Display magnetic declination");
    connect(m_displayMaidenheadGrid, &QAction::triggered, this, &MapGUI::on_displayMaidenheadGrid_clicked);

    ui->layersMenu->setMenu(menu);
}

void MapGUI::on_layersMenu_clicked()
{
}

void MapGUI::on_find_returnPressed()
{
    find(ui->find->text().trimmed());
}

void MapGUI::geoReply()
{
    QGeoCodeReply *pQGeoCode = dynamic_cast<QGeoCodeReply*>(sender());

    if (!pQGeoCode)
    {
        qWarning() << "MapGUI::geoReply: Invalid sender";
        return;
    }

    if (pQGeoCode->error() == QGeoCodeReply::NoError)
    {
        QList<QGeoLocation> qGeoLocs = pQGeoCode->locations();
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");
        if (qGeoLocs.size() == 1)
        {
            // Only one result, so centre map on that
            QGeoCoordinate coord = qGeoLocs.at(0).coordinate();
            map->setProperty("center", QVariant::fromValue(coord));
            if (m_cesium) {
                m_cesium->setView(coord.latitude(), coord.longitude());
            }
        }
        else if (qGeoLocs.size() == 0)
        {
            qDebug() << "MapGUI::geoReply: No location found for address";
            QApplication::beep();
        }
        else
        {
            // Show dialog allowing user to select from the results
            MapLocationDialog dialog(qGeoLocs);
            new DialogPositioner(&dialog, true);
            if (dialog.exec() == QDialog::Accepted)
            {
                QGeoCoordinate coord = dialog.m_selectedLocation.coordinate();
                map->setProperty("center", QVariant::fromValue(coord));
                if (m_cesium) {
                    m_cesium->setView(coord.latitude(), coord.longitude());
                }
            }
        }
    }
    else
    {
        qWarning() << "MapGUI::geoReply: GeoCode error: " << pQGeoCode->error();
    }
    pQGeoCode->deleteLater();
}

// Free keys, so no point in stealing them :)

QString MapGUI::thunderforestAPIKey() const
{
    return m_settings.m_thunderforestAPIKey.isEmpty() ? "3e1f614f78a345459931ba3c898e975e" : m_settings.m_thunderforestAPIKey;
}

QString MapGUI::maptilerAPIKey() const
{
    return m_settings.m_maptilerAPIKey.isEmpty() ? "Nzl2cSyOnewxUc9VWg4n" : m_settings.m_maptilerAPIKey;
}

QString MapGUI::cesiumIonAPIKey() const
{
    return m_settings.m_cesiumIonAPIKey.isEmpty()
        ? "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJjN2FlYzcxMy1iNDJjLTRmYjgtOWQ1MC04M2Y4N2JlOGRiN2QiLCJpZCI6ODEyMDUsImlhdCI6MTc0ODg3Mzc2NH0.-zNQm-tDoPzvobxbVfd0G7Ju1oQkojCT5BCKAc1X4HU"
        : m_settings.m_cesiumIonAPIKey;
}

// The effective* methods coerce settings that would consume Cesium Ion quota to
// free alternatives when the user hasn't entered their own Cesium Ion API key.
// The settings themselves are left untouched, so a preference saved in a preset
// comes back as soon as a key is entered.

QString MapGUI::effectiveImagery() const
{
    if (!m_settings.hasCesiumIonUserKey() && MapSettings::m_cesiumIonImagery.contains(m_settings.m_defaultImagery)) {
        return "ArcGIS world imagery";
    }
    return m_settings.m_defaultImagery;
}

QString MapGUI::effectiveTerrain() const
{
    if (effectiveGoogleTiles()) { // Google 3D tiles include terrain
        return "Ellipsoid";
    }
    if (!m_settings.hasCesiumIonUserKey() && MapSettings::m_cesiumIonTerrain.contains(m_settings.m_terrain)) {
        return "Ellipsoid";
    }
    return m_settings.m_terrain;
}

QString MapGUI::effectiveBuildings() const
{
    if (effectiveGoogleTiles()) { // Google 3D tiles include buildings
        return "None";
    }
    if (!m_settings.hasCesiumIonUserKey() && MapSettings::m_cesiumIonBuildings.contains(m_settings.m_buildings)) {
        return "None";
    }
    return m_settings.m_buildings;
}

bool MapGUI::effectiveGoogleTiles() const
{
    return m_settings.m_googlePhotorealistic3DTiles && m_settings.hasCesiumIonUserKey();
}

void MapGUI::find(const QString& target)
{
    if (!target.isEmpty())
    {
        QQuickItem *item = ui->map->rootObject();
        QObject *map = item->findChild<QObject*>("map");
        if (map != nullptr)
        {
            // Search as:
            //  latitude and longitude
            //  Maidenhead locator
            //  object name
            //  address
            float latitude, longitude;
            if (Units::stringToLatitudeAndLongitude(target, latitude, longitude))
            {
                map->setProperty("center", QVariant::fromValue(QGeoCoordinate(latitude, longitude)));
                if (m_cesium) {
                    m_cesium->setView(latitude, longitude);
                }
            }
            else if (Maidenhead::fromMaidenhead(target, latitude, longitude))
            {
                map->setProperty("center", QVariant::fromValue(QGeoCoordinate(latitude, longitude)));
                if (m_cesium) {
                    m_cesium->setView(latitude, longitude);
                }
            }
            else
            {
                ObjectMapItem *mapItem = (ObjectMapItem *)m_objectMapModel.findMapItem(target);
                if (mapItem != nullptr)
                {
                    map->setProperty("center", QVariant::fromValue(mapItem->getCoordinates()));
                    if (m_cesium) {
                        m_cesium->track(target);
                    }
                    m_objectMapModel.moveToFront(m_objectMapModel.findMapItemIndex(target).row());
                    return;
                }

                PolylineMapItem *polylineMapItem = (PolylineMapItem *)m_polylineMapModel.findMapItem(target);
                if (polylineMapItem != nullptr)
                {
                    map->setProperty("center", QVariant::fromValue(polylineMapItem->getCoordinates()));
                    if (m_cesium) {
                        m_cesium->track(target);
                    }
                    //m_polylineMapModel.moveToFront(m_polylineMapModel.findMapItemIndex(target).row());
                    return;
                }

                PolygonMapItem *polygonMapItem = (PolygonMapItem *)m_polylineMapModel.findMapItem(target);
                if (polygonMapItem != nullptr)
                {
                    map->setProperty("center", QVariant::fromValue(polygonMapItem->getCoordinates()));
                    if (m_cesium) {
                        m_cesium->track(target);
                    }
                    //m_polylineMapModel.moveToFront(m_polylineMapModel.findMapItemIndex(target).row());
                    return;
                }

                // Search as an address
                QGeoServiceProvider* geoSrv = new QGeoServiceProvider("osm");
                if (geoSrv != nullptr)
                {
                    QLocale qLocaleC(QLocale::C, QLocale::AnyCountry);
                    geoSrv->setLocale(qLocaleC);
                    QGeoCodeReply *pQGeoCode = geoSrv->geocodingManager()->geocode(target);
                    if (pQGeoCode) {
                        QObject::connect(pQGeoCode, &QGeoCodeReply::finished, this, &MapGUI::geoReply);
                    } else {
                        qDebug() << "MapGUI::find: GeoCoding failed";
                    }
                }
                else
                {
                    qDebug() << "MapGUI::find: osm not available";
                }
            }
        }
    }
}

void MapGUI::track3D(const QString& target)
{
    if (m_cesium) {
        m_cesium->track(target);
    }
}

void MapGUI::on_save_clicked()
{
    if (m_cesium)
    {
        m_fileDialog.setAcceptMode(QFileDialog::AcceptSave);
        m_fileDialog.setNameFilter("*.kml *.kmz");
        if (m_fileDialog.exec())
        {
            QStringList fileNames = m_fileDialog.selectedFiles();
            if (fileNames.size() > 0) {
                m_cesium->save(fileNames[0], getDataDir());
            }
        }
    }
}

void MapGUI::on_deleteAll_clicked()
{
    m_objectMapModel.removeAll();
    m_imageMapModel.removeAll();
    m_polygonMapModel.removeAll();
    m_polylineMapModel.removeAll();
    if (m_cesium)
    {
        m_cesium->removeAllCZMLEntities();
        m_cesium->removeAllImages();
    }
}

void MapGUI::on_displaySettings_clicked()
{
    MapSettingsDialog dialog(&m_settings);
    connect(&dialog, &MapSettingsDialog::navAidsUpdated, this, &MapGUI::navAidsUpdated);
    connect(&dialog, &MapSettingsDialog::airspacesUpdated, this, &MapGUI::airspacesUpdated);
    connect(&dialog, &MapSettingsDialog::airportsUpdated, this, &MapGUI::airportsUpdated);
    connect(&dialog, &MapSettingsDialog::waypointsUpdated, this, &MapGUI::waypointsUpdated);
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (dialog.m_osmURLChanged) {
            clearOSMCache();
        }
        displayToolbar();
        applyMap2DSettings(dialog.m_map2DSettingsChanged);
        applyMap3DSettings(dialog.m_map3DSettingsChanged);
        sendRMPState(true); // The VHF channel assignments may have changed
        applySettings(dialog.m_settingsKeysChanged);
        if (dialog.m_settingsKeysChanged.contains("msfsNavigraphDir")) {
            addWaypoints();
        }
        m_objectMapModel.allUpdated();
        m_imageMapModel.allUpdated();
        m_polygonMapModel.allUpdated();
        m_polylineMapModel.allUpdated();
    }
}

void MapGUI::on_beacons_clicked()
{
    m_beaconDialog.show();
    m_beaconDialog.raise();
}

void MapGUI::on_ibpBeacons_clicked()
{
    m_ibpBeaconDialog.show();
    m_ibpBeaconDialog.raise();
}

void MapGUI::on_radiotime_clicked()
{
    m_radioTimeDialog.updateTable();
    m_radioTimeDialog.show();
    m_radioTimeDialog.raise();
}

QString MapGUI::getDataDir()
{
    // Get directory to store app data in (aircraft & airport databases and user-definable icons)
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString MapGUI::getBeaconFilename()
{
    return MapGUI::getDataDir() + "/iaru_beacons.csv";
}

QQuickItem *MapGUI::getMapItem() {
    return ui->map->rootObject();
}

void MapGUI::receivedCesiumEvent(const QJsonObject &obj)
{
    if (obj.contains("event"))
    {
        QString event = obj.value("event").toString();
        if (event == "selected")
        {
            if (obj.contains("id")) {
                m_objectMapModel.setSelected3D(obj.value("id").toString());
            } else {
                m_objectMapModel.setSelected3D("");
            }
        }
        else if (event == "tracking")
        {
            if (obj.contains("id")) {
                //m_objectMapModel.setTarget(obj.value("id").toString());
            } else {
                //m_objectMapModel.setTarget("");
            }
        }
        else if (event == "clock")
        {
            if (m_map)
            {
                QDateTime mapDateTime = QDateTime::fromString(obj.value("currentTime").toString(), Qt::ISODateWithMs);
                QDateTime systemDateTime = QDateTime::fromString(obj.value("systemTime").toString(), Qt::ISODateWithMs);
                double multiplier = obj.value("multiplier").toDouble();
                bool canAnimate = obj.value("canAnimate").toBool();
                bool shouldAnimate = obj.value("shouldAnimate").toBool();
                m_map->setMapDateTime(mapDateTime, systemDateTime, canAnimate && shouldAnimate ? multiplier : 0.0);
                updateGIRO(mapDateTime);
            }
        }
        else if (event == "link")
        {
            QString url = obj.value("url").toString();
            linkClicked(url);
        }
        else if (event == "rmpTune")
        {
            // The RMP made a standby frequency active: retune the linked AM Demod
            const int vhf = obj.value("vhf").toInt();
            const qint64 hz = (qint64) obj.value("frequency").toDouble();
            const QString& channelId = (vhf == 2) ? m_settings.m_vhf2Channel : m_settings.m_vhf1Channel;
            if (!rmpTune(channelId, hz)) {
                qDebug() << "MapGUI::receivedCesiumEvent - Failed to tune" << channelId << "to" << hz;
            }
            sendRMPState(true);
        }
        else if (event == "panelClosed")
        {
            // A cockpit panel's close button: remember it as closed, and untick it in
            // the toolbar's panel menu
            const QString panel = obj.value("panel").toString();
            bool MapSettings::*flag = nullptr;
            QAction *action = nullptr;
            QString key;
            if (panel == "pfd") {
                flag = &MapSettings::m_displayPFD; action = m_displayPFDAction; key = "displayPFD";
            } else if (panel == "nd") {
                flag = &MapSettings::m_displayND; action = m_displayNDAction; key = "displayND";
            } else if (panel == "efis") {
                flag = &MapSettings::m_displayEFIS; action = m_displayEFISAction; key = "displayEFIS";
            } else if (panel == "rmp") {
                flag = &MapSettings::m_displayRMP; action = m_displayRMPAction; key = "displayRMP";
            } else if (panel == "dcdu") {
                flag = &MapSettings::m_displayDCDU; action = m_displayDCDUAction; key = "displayDCDU";
            }
            if (flag)
            {
                m_settings.*flag = false;
                if (action) {
                    action->setChecked(false);
                }
                // The page has hidden it already, but the show command also clears the
                // page's own enable - without it a new datalink message reopens the DCDU
                applyCockpitPanels();
                applySetting(key);
            }
        }
        else if (event == "efisState")
        {
            // The EFIS knobs and pushbuttons were changed
            QJsonObject state = obj;
            state.remove("event");
            m_settings.m_efisState = QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
            applySetting("efisState");
        }
        else if (event == "panelGeometry")
        {
            // A cockpit panel was dragged or resized: remember where it was left
            const QString panel = obj.value("panel").toString();
            if (!panel.isEmpty())
            {
                QJsonObject panels = QJsonDocument::fromJson(m_settings.m_cockpitPanelGeometry.toUtf8()).object();
                panels.insert(panel, QJsonObject {
                    {"l", obj.value("left").toInt()},
                    {"t", obj.value("top").toInt()},
                    {"w", obj.value("width").toInt()},
                    {"h", obj.value("height").toInt()}
                });
                m_settings.m_cockpitPanelGeometry = QString::fromUtf8(QJsonDocument(panels).toJson(QJsonDocument::Compact));
                applySetting("cockpitPanelGeometry");
            }
        }
        else if (event == "rmpStandby")
        {
            // Standby frequencies are kept here so they survive a restart
            const int vhf = obj.value("vhf").toInt();
            const int hz = (int) obj.value("frequency").toDouble();
            if (vhf == 2) {
                m_settings.m_vhf2Standby = hz;
            } else {
                m_settings.m_vhf1Standby = hz;
            }
            applySetting((vhf == 2) ? "vhf2Standby" : "vhf1Standby");
        }
    }
    else
    {
        qDebug() << "MapGUI::receivedCesiumEvent - Unexpected event: " << obj;
    }
}

// The absolute frequency an AM Demod is tuned to: the device centre plus the channel
// offset. False when the channel id no longer resolves (the channel was removed, or was
// never assigned)
bool MapGUI::rmpReadActive(const QString& channelId, qint64& hz) const
{
    unsigned int deviceSet, channelIndex;
    if (channelId.isEmpty() || !MainCore::getDeviceAndChannelIndexFromId(channelId, deviceSet, channelIndex)) {
        return false;
    }
    double centerFrequency;
    int offset;
    if (!ChannelWebAPIUtils::getCenterFrequency(deviceSet, centerFrequency)
        || !ChannelWebAPIUtils::getFrequencyOffset(deviceSet, channelIndex, offset)) {
        return false;
    }
    hz = (qint64) centerFrequency + offset;
    return true;
}

// Tune an AM Demod to an absolute frequency. As ADSBDemodGUI::setFrequency: the device is
// retuned when the target is outside the usable 75% of its passband, or when the channel
// would straddle DC, in which case the channel is parked just above DC. Unlike ADS-B the
// channel's own "frequency" setting is also patched, as the AM Demod GUI's dial shows
// that rather than the offset in its Airband and Medium Wave modes
bool MapGUI::rmpTune(const QString& channelId, qint64 hz)
{
    unsigned int deviceSet, channelIndex;
    if (channelId.isEmpty() || !MainCore::getDeviceAndChannelIndexFromId(channelId, deviceSet, channelIndex)) {
        return false;
    }

    double rfBandwidth;
    if (!ChannelWebAPIUtils::getChannelSetting(deviceSet, channelIndex, "rfBandwidth", rfBandwidth) || (rfBandwidth <= 0)) {
        rfBandwidth = 20000.0;
    }
    const qint64 halfChannelBW = (qint64) (rfBandwidth / 2);
    const qint64 dcOffset = halfChannelBW;

    double centerFrequency;
    int sampleRate;
    if (!ChannelWebAPIUtils::getCenterFrequency(deviceSet, centerFrequency)
        || !ChannelWebAPIUtils::getDevSampleRate(deviceSet, sampleRate)) {
        return false;
    }
    const qint64 usable = (qint64) (sampleRate * 0.75); // Keep clear of the guard bands
    const qint64 centre = (qint64) centerFrequency;
    const qint64 offset = hz - centre;

    if (   ((hz - halfChannelBW) < (centre - usable / 2))
        || ((hz + halfChannelBW) >= (centre + usable / 2))
        || (std::abs(offset) < halfChannelBW))
    {
        ChannelWebAPIUtils::setCenterFrequency(deviceSet, (double) (hz - dcOffset));
        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, (int) dcOffset);
    }
    else
    {
        ChannelWebAPIUtils::setFrequencyOffset(deviceSet, channelIndex, (int) offset);
    }
    ChannelWebAPIUtils::patchChannelSetting(deviceSet, channelIndex, "frequency", (double) hz);
    return true;
}

// Set the EFIS controls to what they were left on. Always sent, even when nothing has
// been saved: the page only starts reporting changes once it has been told
void MapGUI::sendEFISState()
{
    if (!m_cesium) {
        return;
    }
    m_cesium->setEFISState(QJsonDocument::fromJson(m_settings.m_efisState.toUtf8()).object());
}

// Put the cockpit panels back where they were left when SDRangel last closed
void MapGUI::sendPanelGeometry()
{
    if (!m_cesium || m_settings.m_cockpitPanelGeometry.isEmpty()) {
        return;
    }
    m_cesium->setPanelGeometry(QJsonDocument::fromJson(m_settings.m_cockpitPanelGeometry.toUtf8()).object());
}

// Tell the RMP panel what its two VHF channels are tuned to. With force false, only when
// something has changed since the last send - this is called from a 1 s poll
void MapGUI::sendRMPState(bool force)
{
    if (!m_cesium) {
        return;
    }
    QJsonObject state;
    const QString* channels[2] = { &m_settings.m_vhf1Channel, &m_settings.m_vhf2Channel };
    const int standbys[2] = { m_settings.m_vhf1Standby, m_settings.m_vhf2Standby };
    for (int i = 0; i < 2; i++)
    {
        qint64 active;
        const bool available = rmpReadActive(*channels[i], active);
        QJsonObject vhf {
            {"available", available},
            {"standby", standbys[i]},
            {"active", available ? QJsonValue((double) active) : QJsonValue()}
        };
        state.insert(QString("vhf%1").arg(i + 1), vhf);
    }
    if (force || (state != m_rmpState))
    {
        m_rmpState = state;
        m_cesium->setRMPState(state);
    }
}

// Handle link clicked in 2D Map Text box or 3D map infobox.
void MapGUI::linkClicked(const QString& url)
{
    if (url.startsWith("sdrangel-kiwisdr://"))
    {
        QString kiwiURL = url.mid(19);
        openKiwiSDR(kiwiURL);
    }
    else if (url.startsWith("sdrangel-spyserver://"))
    {
        QString spyServerURL = url.mid(21);
        openSpyServer(spyServerURL);
    }
    else if (url.startsWith("sdrangel-wss-server://"))
    {
        QString sdrangelServerURL = url.mid(22);
        openSDRangelServer(sdrangelServerURL, true);
    }
    else if (url.startsWith("sdrangel-server://"))
    {
        QString sdrangelServerURL = url.mid(18);
        openSDRangelServer(sdrangelServerURL, false);
    }
    else if (url.startsWith("sdrangel-aircraft://"))
    {
        // sdrangel-aircraft://<icao|registration|flight>/<value>, written into an
        // aircraft's popup by whatever put the aircraft on the Map. The Map has no idea
        // what an aircraft is, so it hands the whole thing to the Aircraft feature.
        const QString rest = url.mid(20);
        const int slash = rest.indexOf('/');
        if (slash <= 0)
        {
            qWarning() << "MapGUI::linkClicked: malformed aircraft link" << url;
            return;
        }
        const QString kind = rest.left(slash);
        const QString value = QUrl::fromPercentEncoding(rest.mid(slash + 1).toUtf8());
        if (kind == "icao") {
            FeatureWebAPIUtils::aircraftSelect("selectIcao", value);
        } else if (kind == "registration") {
            FeatureWebAPIUtils::aircraftSelect("selectRegistration", value);
        } else if (kind == "flight") {
            FeatureWebAPIUtils::aircraftSelect("selectFlight", value);
        } else {
            qWarning() << "MapGUI::linkClicked: unknown aircraft link kind" << kind;
        }
    }
}

// Open a KiwiSDR RX device
void MapGUI::openKiwiSDR(const QString& url)
{
    m_remoteDeviceAddress = url;
    QStringList deviceSettingsKeys = {"serverAddress"};
    SWGSDRangel::SWGDeviceSettings *response = new SWGSDRangel::SWGDeviceSettings();
    response->init();
    SWGSDRangel::SWGKiwiSDRSettings *deviceSettings = response->getKiwiSdrSettings();
    deviceSettings->setServerAddress(new QString(m_remoteDeviceAddress));

    ChannelWebAPIUtils::addDevice("KiwiSDR", 0, deviceSettingsKeys, response);
}

// Open a RemoteTCPInput device to use for SpyServer
void MapGUI::openSpyServer(const QString& url)
{
    QStringList address = url.split(":");
    m_remoteDeviceAddress = address[0];
    m_remoteDevicePort = address[1].toInt();

    QStringList deviceSettingsKeys = {"dataAddress", "dataPort", "protocol", "overrideRemoteSettings"};
    SWGSDRangel::SWGDeviceSettings *response = new SWGSDRangel::SWGDeviceSettings();
    response->init();
    SWGSDRangel::SWGRemoteTCPInputSettings *deviceSettings = response->getRemoteTcpInputSettings();
    deviceSettings->setDataAddress(new QString(m_remoteDeviceAddress));
    deviceSettings->setDataPort(m_remoteDevicePort);
    deviceSettings->setProtocol(new QString("Spy Server"));
    deviceSettings->setOverrideRemoteSettings(false);

    ChannelWebAPIUtils::addDevice("RemoteTCPInput", 0, deviceSettingsKeys, response);
}

// Open a RemoteTCPInput device to use for SDRangel
void MapGUI::openSDRangelServer(const QString& url, bool wss)
{
    QStringList address = url.split(":");
    m_remoteDeviceAddress = address[0];
    m_remoteDevicePort = address[1].toInt();

    QStringList deviceSettingsKeys = {"dataAddress", "dataPort", "protocol", "overrideRemoteSettings"};
    SWGSDRangel::SWGDeviceSettings *response = new SWGSDRangel::SWGDeviceSettings();
    response->init();
    SWGSDRangel::SWGRemoteTCPInputSettings *deviceSettings = response->getRemoteTcpInputSettings();
    deviceSettings->setDataAddress(new QString(m_remoteDeviceAddress));
    deviceSettings->setDataPort(m_remoteDevicePort);
    deviceSettings->setProtocol(new QString(wss ? "SDRangel wss" : "SDRangel"));
    deviceSettings->setOverrideRemoteSettings(false);

    ChannelWebAPIUtils::addDevice("RemoteTCPInput", 0, deviceSettingsKeys, response);
}

#ifdef QT_WEBENGINE_FOUND
void MapGUI::fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest)
{
    fullScreenRequest.accept();
    if (fullScreenRequest.toggleOn())
    {
        ui->web->setParent(nullptr);
        ui->web->showFullScreen();
    }
    else
    {
        ui->splitter->addWidget(ui->web);
        restoreSplitterState();
    }
}
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
void MapGUI::downloadRequested(QWebEngineDownloadRequest *download)
{
    download->accept();
}
#else
void MapGUI::downloadRequested(QWebEngineDownloadItem *download)
{
    download->accept();
}
#endif
#endif

void MapGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        float stationLatitude = MainCore::instance()->getSettings().getLatitude();
        float stationLongitude = MainCore::instance()->getSettings().getLongitude();
        float stationAltitude = MainCore::instance()->getSettings().getAltitude();

        QGeoCoordinate stationPosition(stationLatitude, stationLongitude, stationAltitude);
        QGeoCoordinate previousPosition(m_azEl.getLocationSpherical().m_latitude, m_azEl.getLocationSpherical().m_longitude, m_azEl.getLocationSpherical().m_altitude);

        if (stationPosition != previousPosition)
        {
            // Update position of station
            m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

            m_antennaMapItem.setLatitude(stationLatitude);
            m_antennaMapItem.setLongitude(stationLongitude);
            m_antennaMapItem.setAltitude(stationAltitude);
            delete m_antennaMapItem.getPositionDateTime();
            m_antennaMapItem.setPositionDateTime(new QString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
            update(m_map, &m_antennaMapItem, "Station");

            m_objectMapModel.setStationPosition(stationPosition);
            m_imageMapModel.setStationPosition(stationPosition);
            m_polygonMapModel.setStationPosition(stationPosition);
            m_polylineMapModel.setStationPosition(stationPosition);
            m_objectMapFilter.setPosition(stationPosition);
            m_imageMapFilter.setPosition(stationPosition);
            m_polygonMapFilter.setPosition(stationPosition);
            m_polylineMapFilter.setPosition(stationPosition);
            if (m_cesium)
            {
                m_cesium->setPosition(stationPosition);
                if (!m_lastFullUpdatePosition.isValid() || (stationPosition.distanceTo(m_lastFullUpdatePosition) >= 1000))
                {
                    // Update all objects so distance filter is reapplied
                    m_objectMapModel.allUpdated();
                    m_lastFullUpdatePosition = stationPosition;
                }
            }
        }
    }
    else if (pref == Preferences::StationName)
    {
        // Update station name
        m_antennaMapItem.setLabel(new QString(MainCore::instance()->getSettings().getStationName()));
        m_antennaMapItem.setText(new QString(MainCore::instance()->getSettings().getStationName()));
        update(m_map, &m_antennaMapItem, "Station");
    }
    else if (pref == Preferences::MapSmoothing)
    {
        QQuickItem *item = ui->map->rootObject();
        QQmlProperty::write(item, "smoothing", MainCore::instance()->getSettings().getMapSmoothing());
    }
}

void MapGUI::makeUIConnections()
{
    QObject::connect(ui->displayNames, &ButtonSwitch::clicked, this, &MapGUI::on_displayNames_clicked);
    QObject::connect(ui->muteAudio, &ButtonSwitch::clicked, this, &MapGUI::on_muteAudio_clicked);
    QObject::connect(ui->displayAllGroundTracks, &ButtonSwitch::clicked, this, &MapGUI::on_displayAllGroundTracks_clicked);
    QObject::connect(ui->displaySelectedGroundTracks, &ButtonSwitch::clicked, this, &MapGUI::on_displaySelectedGroundTracks_clicked);
    QObject::connect(ui->displayRain, &ButtonSwitch::clicked, this, &MapGUI::on_displayRain_clicked);
    QObject::connect(ui->displayClouds, &ButtonSwitch::clicked, this, &MapGUI::on_displayClouds_clicked);
    QObject::connect(ui->displaySeaMarks, &ButtonSwitch::clicked, this, &MapGUI::on_displaySeaMarks_clicked);
    QObject::connect(ui->displayRailways, &ButtonSwitch::clicked, this, &MapGUI::on_displayRailways_clicked);
    QObject::connect(ui->displayNASAGlobalImagery, &ButtonSwitch::clicked, this, &MapGUI::on_displayNASAGlobalImagery_clicked);
    QObject::connect(ui->nasaGlobalImageryIdentifier, qOverload<int>(&QComboBox::currentIndexChanged), this, &MapGUI::on_nasaGlobalImageryIdentifier_currentIndexChanged);
    QObject::connect(ui->nasaGlobalImageryOpacity, qOverload<int>(&QDial::valueChanged), this, &MapGUI::on_nasaGlobalImageryOpacity_valueChanged);
    QObject::connect(ui->displayMUF, &ButtonSwitch::clicked, this, &MapGUI::on_displayMUF_clicked);
    QObject::connect(ui->displayfoF2, &ButtonSwitch::clicked, this, &MapGUI::on_displayfoF2_clicked);
    QObject::connect(ui->displayAurora, &ButtonSwitch::clicked, this, &MapGUI::on_displayAurora_clicked);
    QObject::connect(ui->displayMagDec, &ButtonSwitch::clicked, this, &MapGUI::on_displayMagDec_clicked);
    QObject::connect(ui->displayMaidenheadGrid, &ButtonSwitch::clicked, this, &MapGUI::on_displayMaidenheadGrid_clicked);
    QObject::connect(ui->find, &QLineEdit::returnPressed, this, &MapGUI::on_find_returnPressed);
    QObject::connect(ui->maidenhead, &QToolButton::clicked, this, &MapGUI::on_maidenhead_clicked);
    QObject::connect(ui->save, &QToolButton::clicked, this, &MapGUI::on_save_clicked);
    QObject::connect(ui->deleteAll, &QToolButton::clicked, this, &MapGUI::on_deleteAll_clicked);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &MapGUI::on_displaySettings_clicked);
    QObject::connect(ui->mapTypes, qOverload<int>(&QComboBox::currentIndexChanged), this, &MapGUI::on_mapTypes_currentIndexChanged);
    QObject::connect(ui->beacons, &QToolButton::clicked, this, &MapGUI::on_beacons_clicked);
    QObject::connect(ui->ibpBeacons, &QToolButton::clicked, this, &MapGUI::on_ibpBeacons_clicked);
    QObject::connect(ui->radiotime, &QToolButton::clicked, this, &MapGUI::on_radiotime_clicked);
}
