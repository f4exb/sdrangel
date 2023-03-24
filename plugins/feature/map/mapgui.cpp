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

#ifdef QT_WEBENGINE_FOUND
#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineSettings>
#include <QtWebEngineWidgets/QWebEngineProfile>
#endif

#include "feature/featureuiset.h"
#include "gui/basicfeaturesettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "mainwindow.h"
#include "device/deviceuiset.h"
#include "util/units.h"
#include "util/maidenhead.h"
#include "util/morse.h"
#include "util/navtex.h"
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

    m_osmPort = 0;
    m_templateServer = new OSMTemplateServer(thunderforestAPIKey(), maptilerAPIKey(), m_osmPort);

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
    ui->map->setSource(QUrl(QStringLiteral("qrc:/map/map/map.qml")));

    m_settings.m_modelURL = QString("http://127.0.0.1:%1/3d/").arg(m_webPort);
    m_webServer->addPathSubstitution("3d", m_settings.m_modelDir);

    m_map = reinterpret_cast<Map*>(feature);
    m_map->setMessageQueueToGUI(&m_inputMessageQueue);

    m_settings.setRollupState(&m_rollupState);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

#ifdef QT_WEBENGINE_FOUND
    QWebEngineSettings *settings = ui->web->settings();
    settings->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    connect(ui->web->page(), &QWebEnginePage::fullScreenRequested, this, &MapGUI::fullScreenRequested);
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

    addRadioTimeTransmitters();
    addRadar();
    addIonosonde();
    addBroadcast();
    addNavAids();
    addAirspace();
    addAirports();
    addNavtex();

    displaySettings();
    applySettings(true);

    connect(&m_redrawMapTimer, &QTimer::timeout, this, &MapGUI::redrawMap);
    m_redrawMapTimer.setSingleShot(true);
    ui->map->installEventFilter(this);

    makeUIConnections();
    new DialogPositioner(&m_beaconDialog, true);
    new DialogPositioner(&m_ibpBeaconDialog, true);
    new DialogPositioner(&m_radioTimeDialog, true);
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
        m_objectMapModel.allUpdated();
        m_imageMapModel.allUpdated();
        m_polygonMapModel.allUpdated();
        m_polylineMapModel.allUpdated();
    }
    MapSettings::MapItemSettings *ionosondeItemSettings = getItemSettings("Ionosonde Stations");
    if (ionosondeItemSettings) {
        m_giro->getDataPeriodically(ionosondeItemSettings->m_enabled ? 2 : 0);
    }
    m_giro->getMUFPeriodically(m_settings.m_displayMUF ? 15 : 0);
    m_giro->getfoF2Periodically(m_settings.m_displayfoF2 ? 15 : 0);
#else
    ui->displayMUF->setVisible(false);
    ui->displayfoF2->setVisible(false);
    m_objectMapModel.allUpdated();
    m_imageMapModel.allUpdated();
    m_polygonMapModel.allUpdated();
    m_polylineMapModel.allUpdated();
#endif
}

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
    ui->displayMUF->setChecked(m_settings.m_displayMUF);
    ui->displayfoF2->setChecked(m_settings.m_displayfoF2);
    m_objectMapModel.setDisplayNames(m_settings.m_displayNames);
    m_objectMapModel.setDisplaySelectedGroundTracks(m_settings.m_displaySelectedGroundTracks);
    m_objectMapModel.setDisplayAllGroundTracks(m_settings.m_displayAllGroundTracks);
    m_objectMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_imageMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_polygonMapModel.updateItemSettings(m_settings.m_itemSettings);
    m_polylineMapModel.updateItemSettings(m_settings.m_itemSettings);
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
                // FIXME: Support polygon/polyline
                ObjectMapItem *mapItem = m_objectMapModel.findMapItem(target);
                if (mapItem != nullptr)
                {
                    map->setProperty("center", QVariant::fromValue(mapItem->getCoordinates()));
                    if (m_cesium) {
                        m_cesium->track(target);
                    }
                    m_objectMapModel.moveToFront(m_objectMapModel.findMapItemIndex(target).row());
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
            }
        }
    }
    else
    {
        qDebug() << "MapGUI::receivedCesiumEvent - Unexpected event: " << obj;
    }
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

