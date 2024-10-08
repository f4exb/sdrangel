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
#include <QQmlContext>
#include <QQmlProperty>
#include <QGeoLocation>
#include <QGeoCoordinate>
#include <QGeoCodingManager>
#include <QGeoServiceProvider>
#include <QSettings>
#include <QMenu>
#include <QScreen>
#include <QSvgWidget>
#include <QTableWidgetItem>

#ifdef QT_WEBENGINE_FOUND
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#endif

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "dsp/devicesamplesource.h"
#include "device/deviceenumerator.h"
#include "util/units.h"
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
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray MapGUI::serialize() const
{
    return m_settings.serialize();
}

bool MapGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        m_feature->setWorkspaceIndex(m_settings.m_workspaceIndex);
        displaySettings();
        applySettings(true);
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

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
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
        MainCore::MsgMapItem& msgMapItem = (MainCore::MsgMapItem&) message;
        SWGSDRangel::SWGMapItem *swgMapItem = msgMapItem.getSWGMapItem();

        // TODO: Could have this in SWGMapItem so plugins can create additional groups
        QString group;

        for (int i = 0; i < m_availableChannelOrFeatures.size(); i++)
        {
            if (m_availableChannelOrFeatures[i].m_object == msgMapItem.getPipeSource())
            {
                 for (int j = 0; j < MapSettings::m_pipeTypes.size(); j++)
                 {
                     if (m_availableChannelOrFeatures[i].m_type == MapSettings::m_pipeTypes[j]) {
                         group = m_availableChannelOrFeatures[i].m_type;
                     }
                 }
            }
        }

        update(msgMapItem.getPipeSource(), swgMapItem, group);
        return true;
    }

    return false;
}

void MapGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
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
    m_descriptionWidget(nullptr)
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
        QSurfaceFormat format;
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
    QWebEngineSettings *settings = ui->web->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    connect(ui->web->page(), &QWebEnginePage::fullScreenRequested, this, &MapGUI::fullScreenRequested);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    connect(ui->web->page(), &QWebEnginePage::loadingChanged, this, &MapGUI::loadingChanged);
    connect(ui->web, &QWebEngineView::renderProcessTerminated, this, &MapGUI::renderProcessTerminated);
#endif

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();
    connect(profile, &QWebEngineProfile::downloadRequested, this, &MapGUI::downloadRequested);
#endif

    // Get station position
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);
    QGeoCoordinate stationPosition(stationLatitude, stationLongitude, stationAltitude);
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
    QList<Beacon *> *beacons = Beacon::readIARUCSV(MapGUI::getBeaconFilename());
    if (beacons != nullptr) {
        setBeacons(beacons);
    }
    addIBPBeacons();
    addNAT();
    addRadioTimeTransmitters();
    addRadar();
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

    displaySettings();
    applySettings(true);

    connect(&m_objectMapModel, &ObjectMapModel::linkClicked, this, &MapGUI::linkClicked);

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);
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
    delete ui;
}

void MapGUI::setWorkspaceIndex(int index)
{
    m_settings.m_workspaceIndex = index;
    m_feature->setWorkspaceIndex(index);
}

// Update a map item
void MapGUI::update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group)
{
    int type = swgMapItem->getType();
    if (type == 0) {
        m_objectMapModel.update(source, swgMapItem, group);
    } else if (type == 1) {
        m_imageMapModel.update(source, swgMapItem, group);
    } else if (type == 2) {
        m_polygonMapModel.update(source, swgMapItem, group);
    } else if (type == 3) {
        m_polylineMapModel.update(source, swgMapItem, group);
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
        } else if (sdr.m_highFrequency > 320000000) { // Technically 30MHz, but many HF Kiwis list up to 32MHz
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
                m_giro->getMUF(giroRunId);
                m_giroRunId = giroRunId;
                m_giroDateTime = mapDateTime;
            }
        }
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
        QHashIterator<int, AirportInformation *> i(*m_airportInfo);
        while (i.hasNext())
        {
            i.next();
            AirportInformation *airport = i.value();

            SWGSDRangel::SWGMapItem airportMapItem;
            airportMapItem.setName(new QString(airport->m_ident));
            airportMapItem.setLatitude(airport->m_latitude);
            airportMapItem.setLongitude(airport->m_longitude);
            airportMapItem.setAltitude(Units::feetToMetres(airport->m_elevation));
            airportMapItem.setImage(new QString(airport->getImageName()));
            airportMapItem.setImageRotation(0);
            QStringList list;
            list.append(QString("%1: %2").arg(airport->m_ident).arg(airport->m_name));
            for (int i = 0; i < airport->m_frequencies.size(); i++)
            {
                const AirportInformation::FrequencyInformation *frequencyInfo = airport->m_frequencies[i];
                list.append(QString("%1: %2 MHz").arg(frequencyInfo->m_type).arg(frequencyInfo->m_frequency));
            }
            airportMapItem.setText(new QString(list.join("\n")));
            airportMapItem.setModel(new QString("airport.glb")); // No such model currently, but we don't really want one
            airportMapItem.setFixedPosition(true);
            airportMapItem.setOrientation(0);
            airportMapItem.setLabel(new QString(airport->m_ident));
            airportMapItem.setLabelAltitudeOffset(4.5);
            airportMapItem.setAltitudeReference(1);
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
    }
}

void MapGUI::addWaypoints()
{
    m_waypoints = Waypoints::getWaypoints();
    if (m_waypoints)
    {
        QHashIterator<QString, Waypoint *> i(*m_waypoints);
        while (i.hasNext())
        {
            i.next();
            const Waypoint *waypoint = i.value();

            SWGSDRangel::SWGMapItem waypointMapItem;
            waypointMapItem.setName(new QString(waypoint->m_name));
            waypointMapItem.setLatitude(waypoint->m_latitude);
            waypointMapItem.setLongitude(waypoint->m_longitude);
            waypointMapItem.setAltitude(0);
            waypointMapItem.setImage(new QString("waypoint.png"));
            waypointMapItem.setImageRotation(0);
            QStringList list;
            list.append(QString("Waypoint: %1").arg(waypoint->m_name));
            waypointMapItem.setText(new QString(list.join("\n")));
            //waypointMapItem.setModel(new QString("waypoint.glb")); // No such model currently
            waypointMapItem.setFixedPosition(true);
            waypointMapItem.setOrientation(0);
            waypointMapItem.setLabel(new QString(waypoint->m_name));
            waypointMapItem.setLabelAltitudeOffset(4.5);
            waypointMapItem.setAltitude(Units::feetToMetres(25000));
            waypointMapItem.setAltitudeReference(1);
            update(m_map, &waypointMapItem, "Waypoints");
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

void MapGUI::nasaGlobalImageryLegendAvailable(const QString& url, const QByteArray& data)
{
    (void) url;

    if (m_legendWidget)
    {
        m_legendWidget->load(data);
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

        QObject *object = item->findChild<QObject*>("map");
        QGeoCoordinate coords;
        double zoom;
        if (object != nullptr)
        {
            // Save existing position of map
            coords = object->property("center").value<QGeoCoordinate>();
            zoom = object->property("zoomLevel").value<double>();
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
        QObject *newMap = retVal.value<QObject *>();
        // Restore position of map
        if (newMap != nullptr)
        {
            if (coords.isValid())
            {
                newMap->setProperty("zoomLevel", QVariant::fromValue(zoom));
                newMap->setProperty("center", QVariant::fromValue(coords));
            }
        }
        else
        {
            qCritical() << "MapGUI::applyMap2DSettings - createMap returned a nullptr";
        }

        supportedMapsChanged();
    }
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
            m_settingsKeys.append("mapType");
            applySettings();
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
        //ui->web->page()->profile()->clearHttpCache();
        ui->web->load(QUrl(QString("http://127.0.0.1:%1/map/map/map3d.html").arg(m_webPort)));
        //ui->web->load(QUrl(QString("http://webglreport.com/")));
        //ui->web->load(QUrl(QString("https://sandcastle.cesium.com/")));
        //ui->web->load(QUrl("chrome://gpu/"));
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
        m_cesium->setTerrain(m_settings.m_terrain, maptilerAPIKey());
        m_cesium->setBuildings(m_settings.m_buildings);
        m_cesium->setSunLight(m_settings.m_sunLightEnabled);
        m_cesium->setCameraReferenceFrame(m_settings.m_eciCamera);
        m_cesium->setAntiAliasing(m_settings.m_antiAliasing);
        m_cesium->getDateTime();
        m_cesium->showMUF(m_settings.m_displayMUF);
        m_cesium->showfoF2(m_settings.m_displayfoF2);
        m_cesium->showLayer("rain", m_settings.m_displayRain);
        m_cesium->showLayer("clouds", m_settings.m_displayClouds);
        m_cesium->showLayer("seaMarks", m_settings.m_displaySeaMarks);
        m_cesium->showLayer("railways", m_settings.m_displayRailways);
        m_cesium->showLayer("nasaGlobalImagery", m_settings.m_displayNASAGlobalImagery);
        applyNASAGlobalImagerySettings();
        m_objectMapModel.allUpdated();
        m_imageMapModel.allUpdated();
        m_polygonMapModel.allUpdated();
        m_polylineMapModel.allUpdated();
    }
    MapSettings::MapItemSettings *ionosondeItemSettings = getItemSettings("Ionosonde Stations");
    m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
    if (ionosondeItemSettings) {
        m_giro->getDataPeriodically(ionosondeItemSettings->m_enabled ? 2 : 0);
    }
#else
    ui->displayMUF->setVisible(false);
    ui->displayfoF2->setVisible(false);
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
    m_cesium->setTerrain(m_settings.m_terrain, maptilerAPIKey());
    m_cesium->setBuildings(m_settings.m_buildings);
    m_cesium->setSunLight(m_settings.m_sunLightEnabled);
    m_cesium->setCameraReferenceFrame(m_settings.m_eciCamera);
    m_cesium->setAntiAliasing(m_settings.m_antiAliasing);
    m_cesium->getDateTime();

    m_objectMapModel.allUpdated();
    m_imageMapModel.allUpdated();
    m_polygonMapModel.allUpdated();
    m_polylineMapModel.allUpdated();

    // Set 3D view after loading initial objects
    m_cesium->setHomeView(stationLatitude, stationLongitude);

    m_cesium->showMUF(m_settings.m_displayMUF);
    m_cesium->showfoF2(m_settings.m_displayfoF2);

    m_cesium->showLayer("rain", m_settings.m_displayRain);
    m_cesium->showLayer("clouds", m_settings.m_displayClouds);
    m_cesium->showLayer("seaMarks", m_settings.m_displaySeaMarks);
    m_cesium->showLayer("railways", m_settings.m_displayRailways);
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

        m_settingsKeys.append("title");
        m_settingsKeys.append("rgbColor");
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIFeatureSetIndex");
        m_settingsKeys.append("reverseAPIFeatureIndex");

        applySettings();
    }

    resetContextMenuType();
}

void MapGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        Map::MsgConfigureMap* message = Map::MsgConfigureMap::create(m_settings, m_settingsKeys, force);
        m_map->getInputMessageQueue()->push(message);
    }

    m_settingsKeys.clear();
}

void MapGUI::on_maidenhead_clicked()
{
    MapMaidenheadDialog dialog;
    new DialogPositioner(&dialog, true);
    dialog.exec();
}

void MapGUI::on_displayNames_clicked(bool checked)
{
    m_settings.m_displayNames = checked;
    m_objectMapModel.setDisplayNames(checked);
}

void MapGUI::on_displaySelectedGroundTracks_clicked(bool checked)
{
    m_settings.m_displaySelectedGroundTracks = checked;
    m_objectMapModel.setDisplaySelectedGroundTracks(checked);
}

void MapGUI::on_displayAllGroundTracks_clicked(bool checked)
{
    m_settings.m_displayAllGroundTracks = checked;
    m_objectMapModel.setDisplayAllGroundTracks(checked);
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
        m_nasaGlobalImagery.downloadLegend(*m_legend);
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
    // Only call show if disabling, so we don't get two updates
    // (as getMUFPeriodically results in a call to showMUF when the data is available)
    m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
    if (m_cesium && !m_settings.m_displayMUF) {
        m_cesium->showMUF(m_settings.m_displayMUF);
    }
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
    m_giro->getIndexPeriodically((m_settings.m_displayMUF || m_settings.m_displayfoF2) ? 15 : 0);
    if (m_cesium && !m_settings.m_displayfoF2) {
        m_cesium->showfoF2(m_settings.m_displayfoF2);
    }
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

    if ((pQGeoCode != nullptr) && (pQGeoCode->error() == QGeoCodeReply::NoError))
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
    return m_settings.m_maptilerAPIKey.isEmpty() ? "q2RVNAe3eFKCH4XsrE3r" : m_settings.m_maptilerAPIKey;
}

QString MapGUI::cesiumIonAPIKey() const
{
    return m_settings.m_cesiumIonAPIKey.isEmpty()
        ? "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiIyNTcxMDA2OC0yNTIzLTQxMGYtYTNiMS1iM2I3MDFhNWVlMDYiLCJpZCI6ODEyMDUsImlhdCI6MTY0MzY2OTIzOX0.A7NchU4LzaNsuAUpsrA9ZwekOJfMoNcja-8XeRdRoIg"
        : m_settings.m_cesiumIonAPIKey;
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
    new DialogPositioner(&dialog, true);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (dialog.m_osmURLChanged) {
            clearOSMCache();
        }
        displayToolbar();
        applyMap2DSettings(dialog.m_map2DSettingsChanged);
        applyMap3DSettings(dialog.m_map3DSettingsChanged);
        m_settingsKeys.append(dialog.m_settingsKeysChanged);
        applySettings();
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
    }
    else
    {
        qDebug() << "MapGUI::receivedCesiumEvent - Unexpected event: " << obj;
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
