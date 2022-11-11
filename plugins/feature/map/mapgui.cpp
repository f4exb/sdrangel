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

#include <cmath>
#include <QQmlContext>
#include <QQmlProperty>
#include <QGeoLocation>
#include <QGeoCoordinate>
#include <QGeoCodingManager>
#include <QGeoServiceProvider>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWebEngineWidgets/QWebEngineProfile>

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "util/units.h"
#include "util/maidenhead.h"
#include "maplocationdialog.h"
#include "mapmaidenheaddialog.h"
#include "mapsettingsdialog.h"
#include "ibpbeacon.h"

#include "ui_mapgui.h"
#include "map.h"
#include "mapgui.h"
#include "SWGMapItem.h"
#include "SWGTargetAzimuthElevation.h"

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
        m_settings = cfg.getSettings();
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
            if (m_availableChannelOrFeatures[i].m_source == msgMapItem.getPipeSource())
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
    applySettings();
}

MapGUI::MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent) :
    FeatureGUI(parent),
    ui(new Ui::MapGUI),
    m_pluginAPI(pluginAPI),
    m_featureUISet(featureUISet),
    m_doApplySettings(true),
    m_mapModel(this),
    m_beacons(nullptr),
    m_beaconDialog(this),
    m_ibpBeaconDialog(this),
    m_radioTimeDialog(this),
    m_cesium(nullptr)
{
    m_feature = feature;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/feature/map/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    m_osmPort = 0;
    m_templateServer = new OSMTemplateServer(thunderforestAPIKey(), maptilerAPIKey(), m_osmPort);

    // Web server to serve dynamic files from QResources
    m_webPort = 0;
    m_webServer = new WebServer(m_webPort);

    ui->map->rootContext()->setContextProperty("mapModel", &m_mapModel);
    // 5.12 doesn't display map items when fully zoomed out
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map_5_12.qml")));
#else
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map.qml")));
#endif

    m_settings.m_modelURL = QString("http://127.0.0.1:%1/3d/").arg(m_webPort);
    m_webServer->addPathSubstitution("3d", m_settings.m_modelDir);

    m_map = reinterpret_cast<Map*>(feature);
    m_map->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    QWebEngineSettings *settings = ui->web->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    connect(ui->web->page(), &QWebEnginePage::fullScreenRequested, this, &MapGUI::fullScreenRequested);

    // Get station position
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();
    float stationAltitude = MainCore::instance()->getSettings().getAltitude();
    m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

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
    m_antennaMapItem.setFixedPosition(true);
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

    addRadioTimeTransmitters();
    addRadar();
    addIonosonde();

    displaySettings();
    applySettings(true);

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    ui->map->installEventFilter(this);

    makeUIConnections();
}

MapGUI::~MapGUI()
{
    disconnect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);
    m_redrawMapTimer.stop();
    //m_cesium->deleteLater();
    delete m_cesium;
    if (m_templateServer)
    {
        m_templateServer->close();
        delete m_templateServer;
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

// Update a map item or image
void MapGUI::update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group)
{
    if (swgMapItem->getType() == 0)
    {
        m_mapModel.update(source, swgMapItem, group);
    }
    else if (m_cesium)
    {
        QString name = *swgMapItem->getName();
        QString image = *swgMapItem->getImage();
        if (!image.isEmpty())
        {
            m_cesium->updateImage(name,
                                swgMapItem->getImageTileEast(),
                                swgMapItem->getImageTileWest(),
                                swgMapItem->getImageTileNorth(),
                                swgMapItem->getImageTileSouth(),
                                swgMapItem->getAltitude(),
                                image);
        }
        else
        {
            m_cesium->removeImage(name);
        }
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

const QList<RadioTimeTransmitter> MapGUI::m_radioTimeTransmitters = {
    {"MSF", 60000, 54.9075f, -3.27333f, 17},
    {"DCF77", 77500, 50.01611111f, 9.00805556f, 50},
    {"TDF", 162000, 47.1694f, 2.2044f, 800},
    {"WWVB", 60000, 40.67805556f, -105.04666667f, 70},
    {"JJY", 60000, 33.465f, 130.175555f, 50}
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

// Ionosonde stations
void MapGUI::addIonosonde()
{
    m_giro = GIRO::create();
    if (m_giro)
    {
        connect(m_giro, &GIRO::dataUpdated, this, &MapGUI::giroDataUpdated);
        connect(m_giro, &GIRO::mufUpdated, this, &MapGUI::mufUpdated);
        connect(m_giro, &GIRO::foF2Updated, this, &MapGUI::foF2Updated);
    }
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
    m_cesium->showMUF(m_settings.m_displayMUF);
}

void MapGUI::foF2Updated(const QJsonDocument& document)
{
    m_webServer->addFile("/map/map/fof2.geojson", document.toJson());
    m_cesium->showfoF2(m_settings.m_displayfoF2);
}

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

// Coming soon
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

void MapGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

QString MapGUI::osmCachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation/5.8/tiles/osm/sdrangel_map";
}

void MapGUI::clearOSMCache()
{
    // Delete all cached custom tiles when user changes the URL. Is there a better way to do this?
    QDir dir(osmCachePath());
    if (dir.exists())
    {
        QStringList filenames = dir.entryList({"osm_100-l-8-*.png"});
        for (const auto& filename : filenames)
        {
            QFile file(dir.filePath(filename));
            if (!file.remove()) {
                qDebug() << "MapGUI::clearOSMCache: Failed to remove " << file;
            }
        }
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

        // Create the map using the specified provider
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
        if (m_settings.m_mapProvider == "maplibre")
        {
            parameters["maplibre.access_token"] = m_settings.m_mapBoxAPIKey;
            if (!m_settings.m_mapBoxStyles.isEmpty())
                parameters["maplibre.mapping.additional_style_urls"] = m_settings.m_mapBoxStyles;
        }
        if (m_settings.m_mapProvider == "osm")
        {
            // Allow user to specify URL
            if (!m_settings.m_osmURL.isEmpty()) {
                parameters["osm.mapping.custom.host"] = m_settings.m_osmURL;  // E.g: "http://a.tile.openstreetmap.fr/hot/"
            }
            // Use our repo, so we can append API key
            parameters["osm.mapping.providersrepository.address"] = QString("http://127.0.0.1:%1/").arg(m_osmPort);
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
            object->setProperty("zoomLevel", QVariant::fromValue(zoom+1));
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
    return false;
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
            applySettings();
        }
    }
}

void MapGUI::applyMap3DSettings(bool reloadMap)
{
    if (m_settings.m_map3DEnabled && ((m_cesium == nullptr) || reloadMap))
    {
        if (m_cesium == nullptr) {
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
    }
    MapSettings::MapItemSettings *ionosondeItemSettings = getItemSettings("Ionosonde Stations");
    if (ionosondeItemSettings) {
        m_giro->getDataPeriodically(ionosondeItemSettings->m_enabled ? 2 : 0);
    }
    m_giro->getMUFPeriodically(m_settings.m_displayMUF ? 15 : 0);
    m_giro->getfoF2Periodically(m_settings.m_displayfoF2 ? 15 : 0);
}

void MapGUI::init3DMap()
{
    qDebug() << "MapGUI::init3DMap";

    m_cesium->initCZML();

    m_cesium->setTerrain(m_settings.m_terrain, maptilerAPIKey());
    m_cesium->setBuildings(m_settings.m_buildings);
    m_cesium->setSunLight(m_settings.m_sunLightEnabled);
    m_cesium->setCameraReferenceFrame(m_settings.m_eciCamera);
    m_cesium->setAntiAliasing(m_settings.m_antiAliasing);
    m_cesium->getDateTime();

    m_mapModel.allUpdated();
    float stationLatitude = MainCore::instance()->getSettings().getLatitude();
    float stationLongitude = MainCore::instance()->getSettings().getLongitude();

    // Set 3D view after loading initial objects
    m_cesium->setHomeView(stationLatitude, stationLongitude);

    m_cesium->showMUF(m_settings.m_displayMUF);
    m_cesium->showfoF2(m_settings.m_displayfoF2);
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
    ui->displayMUF->setChecked(m_settings.m_displayMUF);
    ui->displayfoF2->setChecked(m_settings.m_displayfoF2);
    m_mapModel.setDisplayNames(m_settings.m_displayNames);
    m_mapModel.setDisplaySelectedGroundTracks(m_settings.m_displaySelectedGroundTracks);
    m_mapModel.setDisplayAllGroundTracks(m_settings.m_displayAllGroundTracks);
    m_mapModel.updateItemSettings(m_settings.m_itemSettings);
    applyMap2DSettings(true);
    applyMap3DSettings(true);
    getRollupContents()->restoreState(m_rollupState);
    blockApplySettings(false);
}

void MapGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
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
        dialog.exec();

        m_settings.m_title = dialog.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIFeatureSetIndex = dialog.getReverseAPIFeatureSetIndex();
        m_settings.m_reverseAPIFeatureIndex = dialog.getReverseAPIFeatureIndex();

        setTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }

    resetContextMenuType();
}

void MapGUI::applySettings(bool force)
{
    if (m_doApplySettings)
    {
        Map::MsgConfigureMap* message = Map::MsgConfigureMap::create(m_settings, force);
        m_map->getInputMessageQueue()->push(message);
    }
}

void MapGUI::on_maidenhead_clicked()
{
    MapMaidenheadDialog dialog;
    dialog.exec();
}

void MapGUI::on_displayNames_clicked(bool checked)
{
    m_settings.m_displayNames = checked;
    m_mapModel.setDisplayNames(checked);
}

void MapGUI::on_displaySelectedGroundTracks_clicked(bool checked)
{
    m_settings.m_displaySelectedGroundTracks = checked;
    m_mapModel.setDisplaySelectedGroundTracks(checked);
}

void MapGUI::on_displayAllGroundTracks_clicked(bool checked)
{
    m_settings.m_displayAllGroundTracks = checked;
    m_mapModel.setDisplayAllGroundTracks(checked);
}

void MapGUI::on_displayMUF_clicked(bool checked)
{
    m_settings.m_displayMUF = checked;
    // Only call show if disabling, so we don't get two updates
    // (as getMUFPeriodically results in a call to showMUF when the data is available)
     m_giro->getMUFPeriodically(m_settings.m_displayMUF ? 15 : 0);
    if (m_cesium && !m_settings.m_displayMUF) {
        m_cesium->showMUF(m_settings.m_displayMUF);
    }
}

void MapGUI::on_displayfoF2_clicked(bool checked)
{
    m_settings.m_displayfoF2 = checked;
    m_giro->getfoF2Periodically(m_settings.m_displayfoF2 ? 15 : 0);
    if (m_cesium && !m_settings.m_displayfoF2) {
        m_cesium->showfoF2(m_settings.m_displayfoF2);
    }
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
                MapItem *mapItem = m_mapModel.findMapItem(target);
                if (mapItem != nullptr)
                {
                    map->setProperty("center", QVariant::fromValue(mapItem->getCoordinates()));
                    if (m_cesium) {
                        m_cesium->track(target);
                    }
                }
                else
                {
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
}

void MapGUI::track3D(const QString& target)
{
    if (m_cesium) {
        m_cesium->track(target);
    }
}

void MapGUI::on_deleteAll_clicked()
{
    m_mapModel.removeAll();
    if (m_cesium)
    {
        m_cesium->removeAllCZMLEntities();
        m_cesium->removeAllImages();
    }
}

void MapGUI::on_displaySettings_clicked()
{
    MapSettingsDialog dialog(&m_settings);
    if (dialog.exec() == QDialog::Accepted)
    {
        if (dialog.m_osmURLChanged) {
            clearOSMCache();
        }
        applyMap2DSettings(dialog.m_map2DSettingsChanged);
        applyMap3DSettings(dialog.m_map3DSettingsChanged);
        applySettings();
        m_mapModel.allUpdated();
    }
}

void MapGUI::on_beacons_clicked()
{
    m_beaconDialog.show();
}

void MapGUI::on_ibpBeacons_clicked()
{
    m_ibpBeaconDialog.show();
}

void MapGUI::on_radiotime_clicked()
{
    m_radioTimeDialog.updateTable();
    m_radioTimeDialog.show();
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
                m_mapModel.setSelected3D(obj.value("id").toString());
            } else {
                m_mapModel.setSelected3D("");
            }
        }
        else if (event == "tracking")
        {
            if (obj.contains("id")) {
                //m_mapModel.setTarget(obj.value("id").toString());
            } else {
                //m_mapModel.setTarget("");
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
            }
        }
    }
    else
    {
        qDebug() << "MapGUI::receivedCesiumEvent - Unexpected event: " << obj;
    }
}

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

void MapGUI::preferenceChanged(int elementType)
{
    Preferences::ElementType pref = (Preferences::ElementType)elementType;
    if ((pref == Preferences::Latitude) || (pref == Preferences::Longitude) || (pref == Preferences::Altitude))
    {
        float stationLatitude = MainCore::instance()->getSettings().getLatitude();
        float stationLongitude = MainCore::instance()->getSettings().getLongitude();
        float stationAltitude = MainCore::instance()->getSettings().getAltitude();

        if (   (stationLatitude != m_azEl.getLocationSpherical().m_latitude)
            || (stationLongitude != m_azEl.getLocationSpherical().m_longitude)
            || (stationAltitude != m_azEl.getLocationSpherical().m_altitude))
        {
            // Update position of station
            m_azEl.setLocation(stationLatitude, stationLongitude, stationAltitude);

            m_antennaMapItem.setLatitude(stationLatitude);
            m_antennaMapItem.setLongitude(stationLongitude);
            m_antennaMapItem.setAltitude(stationAltitude);
            update(m_map, &m_antennaMapItem, "Station");
        }
    }
    if (pref == Preferences::StationName)
    {
        // Update station name
        m_antennaMapItem.setLabel(new QString(MainCore::instance()->getSettings().getStationName()));
        m_antennaMapItem.setText(new QString(MainCore::instance()->getSettings().getStationName()));
        update(m_map, &m_antennaMapItem, "Station");
    }
}

void MapGUI::makeUIConnections()
{
    QObject::connect(ui->displayNames, &ButtonSwitch::clicked, this, &MapGUI::on_displayNames_clicked);
    QObject::connect(ui->displayAllGroundTracks, &ButtonSwitch::clicked, this, &MapGUI::on_displayAllGroundTracks_clicked);
    QObject::connect(ui->displaySelectedGroundTracks, &ButtonSwitch::clicked, this, &MapGUI::on_displaySelectedGroundTracks_clicked);
    QObject::connect(ui->displayMUF, &ButtonSwitch::clicked, this, &MapGUI::on_displayMUF_clicked);
    QObject::connect(ui->displayfoF2, &ButtonSwitch::clicked, this, &MapGUI::on_displayfoF2_clicked);
    QObject::connect(ui->find, &QLineEdit::returnPressed, this, &MapGUI::on_find_returnPressed);
    QObject::connect(ui->maidenhead, &QToolButton::clicked, this, &MapGUI::on_maidenhead_clicked);
    QObject::connect(ui->deleteAll, &QToolButton::clicked, this, &MapGUI::on_deleteAll_clicked);
    QObject::connect(ui->displaySettings, &QToolButton::clicked, this, &MapGUI::on_displaySettings_clicked);
    QObject::connect(ui->mapTypes, qOverload<int>(&QComboBox::currentIndexChanged), this, &MapGUI::on_mapTypes_currentIndexChanged);
    QObject::connect(ui->beacons, &QToolButton::clicked, this, &MapGUI::on_beacons_clicked);
    QObject::connect(ui->ibpBeacons, &QToolButton::clicked, this, &MapGUI::on_ibpBeacons_clicked);
    QObject::connect(ui->radiotime, &QToolButton::clicked, this, &MapGUI::on_radiotime_clicked);
}
