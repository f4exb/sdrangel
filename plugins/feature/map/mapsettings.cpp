///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2017, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 CRD716 <crd716@gmail.com>                                  //
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

#include <QColor>
#include <QDebug>
#include <QDir>

#include "util/simpleserializer.h"
#include "util/httpdownloadmanager.h"
#include "settings/serializable.h"

#include "mapsettings.h"

static QString defaultMSFSAircraftDirectory()
{
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        localAppData = QDir::home().filePath("AppData/Local");
    }
    return QDir(localAppData).filePath(
        "Packages/Microsoft.Limitless_8wekyb3d8bbwe/LocalCache/Packages/"
        "Community/fsltl-traffic-base/SimObjects/Airplanes");
}

static QString defaultMSFSNavigraphDirectory()
{
#ifdef Q_OS_WIN
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) {
        localAppData = QDir::home().filePath("AppData/Local");
    }
    return QDir(localAppData).filePath(
        "Packages/Microsoft.Limitless_8wekyb3d8bbwe/LocalCache/Packages/"
        "Community/navigraph-nav-base");
#else
    return QString();
#endif
}

const QStringList MapSettings::m_pipeTypes = {
    QStringLiteral("ACARSDemod"),
    QStringLiteral("ADSBDemod"),
    QStringLiteral("Aircraft"),
    QStringLiteral("AIS"),
    QStringLiteral("APRS"),
    QStringLiteral("APTDemod"),
    QStringLiteral("DSCDemod"),
    QStringLiteral("DroneIDDemod"),
    QStringLiteral("FT8Demod"),
    QStringLiteral("HeatMap"),
    QStringLiteral("ILSDemod"),
    QStringLiteral("InmarsatDemod"),
    QStringLiteral("PagerDemod"),
    QStringLiteral("Radiosonde"),
    QStringLiteral("StarTracker"),
    QStringLiteral("SatelliteTracker"),
    QStringLiteral("SID"),
    QStringLiteral("VORLocalizer")
};

const QStringList MapSettings::m_pipeURIs = {
    QStringLiteral("sdrangel.channel.acarsdemod"),
    QStringLiteral("sdrangel.channel.adsbdemod"),
    QStringLiteral("sdrangel.feature.aircraft"),
    QStringLiteral("sdrangel.feature.ais"),
    QStringLiteral("sdrangel.feature.aprs"),
    QStringLiteral("sdrangel.channel.aptdemod"),
    QStringLiteral("sdrangel.channel.dscdemod"),
    QStringLiteral("sdrangel.channel.droneiddemod"),
    QStringLiteral("sdrangel.channel.ft8demod"),
    QStringLiteral("sdrangel.channel.heatmap"),
    QStringLiteral("sdrangel.channel.ilsdemod"),
    QStringLiteral("sdrangel.channel.inmarsatdemod"),
    QStringLiteral("sdrangel.channel.pagerdemod"),
    QStringLiteral("sdrangel.feature.radiosonde"),
    QStringLiteral("sdrangel.feature.startracker"),
    QStringLiteral("sdrangel.feature.satellitetracker"),
    QStringLiteral("sdrangel.feature.sid"),
    QStringLiteral("sdrangel.feature.vorlocalizer")
};

const QStringList MapSettings::m_mapProviders = {
    QStringLiteral("osm"),
    QStringLiteral("esri"),
    QStringLiteral("mapboxgl"),
    QStringLiteral("maplibregl")
};

// Names as used in combo box in settings dialog
const QStringList MapSettings::m_mapProviderNames = {
    QStringLiteral("OpenStreetMap"),
    QStringLiteral("ESRI"),
    QStringLiteral("MapboxGL"),
    QStringLiteral("MapLibreGL")
};

// 3D map options that consume Cesium Ion quota, so need a user-supplied Cesium Ion
// API key. Names as used in the combo boxes in the settings dialog.
const QStringList MapSettings::m_cesiumIonImagery = {
    QStringLiteral("Bing Maps Aerial"),
    QStringLiteral("Sentinel-2"),
    QStringLiteral("Earth at night"),
    QStringLiteral("Google Maps Satellite"),
    QStringLiteral("Google Maps Satellite with Labels"),
    QStringLiteral("Google Maps Roadmap"),
    QStringLiteral("Google Maps Contour")
};

const QStringList MapSettings::m_cesiumIonTerrain = {
    QStringLiteral("Cesium World Terrain")
};

const QStringList MapSettings::m_cesiumIonBuildings = {
    QStringLiteral("Cesium OSM Buildings")
};

MapSettings::MapSettings() :
    m_rgbColor(QColor(0, 0, 0).rgb()),
    m_rollupState(nullptr)
{
    // Source names should match m_pipeTypes
    // Colors currently match color of rollup widget for that plugin
    int modelMinPixelSize = 50;
    MapItemSettings *aptSettings = new MapItemSettings("APTDemod", true, QColor(216, 112, 169), true, false, 11);
    aptSettings->m_display2DIcon = false; // Disabled as 2D projection is wrong
    m_itemSettings.insert("APTDemod", aptSettings);
    // Built from the corrected defaults, so nothing to migrate
    m_itemSettingsVersion = 1;
    m_splitterState.clear();
    m_itemSettings.insert("ACARSDemod", new MapItemSettings("ACARSDemod", true, QColor(244, 151, 57), true, false, 11, modelMinPixelSize));
    m_itemSettings.insert("ADSBDemod", new MapItemSettings("ADSBDemod", true, QColor(244, 151, 57), true, false, 11, modelMinPixelSize));
    m_itemSettings.insert("Aircraft", new MapItemSettings("Aircraft", true, QColor(244, 151, 57), true, false, 11, modelMinPixelSize));
    // Filed flight plans and oceanic clearances, which the Aircraft feature asks for by
    // name - see MapItem.group in the API. They are here rather than sharing the group
    // above because a route wants the opposite of what an aircraft wants from a minimum
    // zoom: an icon has to be decluttered away when zoomed out, while a route drawn
    // across an ocean is exactly what you want to see at that zoom. Not a pipe type, so
    // no source is ever assigned to it - only an item that names it.
    m_itemSettings.insert("Aircraft Routes", new MapItemSettings("Aircraft Routes", true, QColor(244, 151, 57), false, false, 0, modelMinPixelSize));
    // The per protocol coverage polygons. Each carries its own colour, so the colour
    // here only matters if one ever arrives without one.
    //
    // display2DTrack must be TRUE, unlike the routes above: PolygonMapModel::data()
    // returns a transparent fill for a polygon whose group does not have it, and the
    // border of every polygon is transparent unconditionally - so with it false the
    // coverage was on the 2D map, filtered in and drawn, and completely invisible.
    // Polylines have no such gate, which is why the routes are visible without it.
    m_itemSettings.insert("Aircraft Coverage", new MapItemSettings("Aircraft Coverage", true, QColor(244, 151, 57), true, false, 0, modelMinPixelSize));
    m_itemSettings.insert("AIS", new MapItemSettings("AIS", true, QColor(102, 0, 0), true, false, 11, modelMinPixelSize));
    MapItemSettings *aprsSettings = new MapItemSettings("APRS", true, QColor(255, 255, 0), true, false, 11);
    aprsSettings->m_extrapolate = 0;
    m_itemSettings.insert("APRS", aprsSettings);
    m_itemSettings.insert("DSCDemod", new MapItemSettings("DSCDemod", true,  QColor(181, 230, 29), true, true, 3));
    m_itemSettings.insert("DroneIDDemod", new MapItemSettings("DroneIDDemod", true, QColor(255, 128, 0), true, false, 11, modelMinPixelSize));
    m_itemSettings.insert("StarTracker", new MapItemSettings("StarTracker", true,  QColor(230, 230, 230), true, true, 3));
    m_itemSettings.insert("SatelliteTracker", new MapItemSettings("SatelliteTracker", true, QColor(0, 0, 255), true, false, 0, modelMinPixelSize));
    m_itemSettings.insert("Beacons", new MapItemSettings("Beacons", true, QColor(255, 0, 0), false, true, 8));
    m_itemSettings.insert("PagerDemod", new MapItemSettings("PagerDemod", true, QColor(200, 191, 231), true, false, 11));
    m_itemSettings.insert("Radiosonde", new MapItemSettings("Radiosonde", true, QColor(102, 0, 102), true, false, 11, modelMinPixelSize));
    m_itemSettings.insert("Radio Time Transmitters", new MapItemSettings("Radio Time Transmitters", true, QColor(255, 0, 0), false, true, 8));
    m_itemSettings.insert("WEFAX Transmitters", new MapItemSettings("WEFAX Transmitters", true, QColor(255, 0, 0), false, true, 8));
    m_itemSettings.insert("Radar", new MapItemSettings("Radar", true, QColor(255, 0, 0), false, true, 8));
    m_itemSettings.insert("NAT ATC Transmitters", new MapItemSettings("NAT ATC Transmitters", false, QColor(255, 0, 0), false, true, 8));
    m_itemSettings.insert("FT8Demod", new MapItemSettings("FT8Demod", true, QColor(0, 192, 255), true, true, 8));
    m_itemSettings.insert("HeatMap", new MapItemSettings("HeatMap", true, QColor(102, 40, 220), true, true, 11));
    m_itemSettings.insert("VLF", new MapItemSettings("VLF", false, QColor(255, 0, 0), false, true, 8));

    m_itemSettings.insert("AM", new MapItemSettings("AM", false, QColor(255, 0, 0), false, true, 10));
    MapItemSettings *fmSettings = new MapItemSettings("FM", false, QColor(255, 0, 0), false, true, 12);
    fmSettings->m_filterDistance = 150000;
    m_itemSettings.insert("FM", fmSettings);
    MapItemSettings *dabSettings = new MapItemSettings("DAB", false, QColor(255, 0, 0), false, true, 12);
    dabSettings->m_filterDistance = 75000;
    m_itemSettings.insert("DAB", dabSettings);

    m_itemSettings.insert("Navtex", new MapItemSettings("Navtex", false, QColor(255, 0, 255), false, true, 8));
    m_itemSettings.insert("ILSDemod", new MapItemSettings("ILSDemod", true, QColor(0, 205, 200), true, true, 10));
    m_itemSettings.insert("InmarsatDemod", new MapItemSettings("InmarsatDemod", true, QColor(40, 180, 75), false, true, 5));

    MapItemSettings *navAidSettings = new MapItemSettings("NavAid", false, QColor(255, 0, 255), false, true, 11);
    navAidSettings->m_filterDistance = 500000;
    m_itemSettings.insert("NavAid", navAidSettings);

    m_itemSettings.insert("Airport (Large)", new MapItemSettings("Airport (Large)", false, QColor(255, 0, 255), false, true, 11));
    m_itemSettings.insert("Airport (Medium)", new MapItemSettings("Airport (Medium)", false, QColor(255, 0, 255), false, true, 11));
    m_itemSettings.insert("Airport (Small)", new MapItemSettings("Airport (Small)", false, QColor(255, 0, 255), false, true, 13));
    m_itemSettings.insert("Heliport", new MapItemSettings("Heliport", false, QColor(255, 0, 255), false, true, 12));
    MapItemSettings *stationSettings = new MapItemSettings("Station", true, QColor(255, 0, 0), false, true, 11);
    stationSettings->m_extrapolate = 0;
    stationSettings->m_display3DTrack = false;
    m_itemSettings.insert("Station", stationSettings);
    m_itemSettings.insert("VORLocalizer", new MapItemSettings("VORLocalizer", true, QColor(255, 255, 0), false, true, 11));
    m_itemSettings.insert("SID", new MapItemSettings("SID", true, QColor(255, 255, 0), false, true, 3));

    MapItemSettings *ionosondeItemSettings = new MapItemSettings("Ionosonde Stations", true, QColor(255, 255, 0), false, true, 4);
    ionosondeItemSettings->m_display2DIcon = false;
    m_itemSettings.insert("Ionosonde Stations", ionosondeItemSettings);

    m_itemSettings.insert("Airspace (A)", new MapItemSettings("Airspace (A)", false, QColor(255, 0, 0, 0x20), true, false, 7));
    m_itemSettings.insert("Airspace (B)", new MapItemSettings("Airspace (B)", false, QColor(255, 0, 0, 0x20), true, false, 7));
    m_itemSettings.insert("Airspace (C)", new MapItemSettings("Airspace (C)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (D)", new MapItemSettings("Airspace (D)", false, QColor(0, 0, 255, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (E)", new MapItemSettings("Airspace (E)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (F)", new MapItemSettings("Airspace (F)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (G)", new MapItemSettings("Airspace (G)", false, QColor(0, 0, 255, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (FIR)", new MapItemSettings("Airspace (FIR)", false, QColor(255, 0, 0, 0x20), true, false, 7));
    m_itemSettings.insert("Airspace (CTR)", new MapItemSettings("Airspace (CTR)", false, QColor(0, 0, 255, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (RMZ)", new MapItemSettings("Airspace (RMZ)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (TMA)", new MapItemSettings("Airspace (TMA)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (TMZ)", new MapItemSettings("Airspace (TMZ)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (OTH)", new MapItemSettings("Airspace (OTH)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Restricted)", new MapItemSettings("Airspace (Restricted)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Gliding)", new MapItemSettings("Airspace (Gliding)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Danger)", new MapItemSettings("Airspace (Danger)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Prohibited)", new MapItemSettings("Airspace (Prohibited)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Wave)", new MapItemSettings("Airspace (Wave)", false, QColor(255, 0, 0, 0x20), true, false, 11));
    m_itemSettings.insert("Airspace (Airports)", new MapItemSettings("Airspace (Airports)", false, QColor(0, 0, 255, 0x20), true, false, 11));

    MapItemSettings *waypointsSettings = new MapItemSettings("Waypoints", false, QColor(255, 0, 255), false, true, 11);
    waypointsSettings->m_filterDistance = 500000;
    m_itemSettings.insert("Waypoints", waypointsSettings);

    bool showOtherServers = true;
#ifdef __EMSCRIPTEN__
    showOtherServers = false; // Can't use without proxy
#endif
    m_itemSettings.insert("KiwiSDR", new MapItemSettings("KiwiSDR", showOtherServers, QColor(0, 255, 0), false, true, 8));
    m_itemSettings.insert("SpyServer", new MapItemSettings("SpyServer", showOtherServers, QColor(0, 0, 255), false, true, 8));
    m_itemSettings.insert("SDRangel", new MapItemSettings("SDRangel", true, QColor(255, 0, 255), false, true, 8));

    resetToDefaults();
}

MapSettings::MapSettings(const MapSettings& other) :
    MapSettings()
{
    deserialize(other.serialize());
    m_rollupState = other.m_rollupState;
}


MapSettings& MapSettings::operator=(const MapSettings& other)
{
    if (this != &other)
    {
        deserialize(other.serialize());
        m_rollupState = other.m_rollupState;
    }

    return *this;
}

MapSettings::~MapSettings()
{
    qDeleteAll(m_itemSettings);
}

void MapSettings::resetToDefaults()
{
    m_displayNames = true;
#if defined(LINUX) && (QT_VERSION < QT_VERSION_CHECK(5, 15, 13))
    m_mapProvider = "mapboxgl"; // osm maps do not work in some versions of Linux https://github.com/f4exb/sdrangel/issues/1169 & 1369
#else
    m_mapProvider = "osm";
#endif
    m_thunderforestAPIKey = "";
    m_maptilerAPIKey = "";
    m_cartoDBAPIKey = "";
    m_mapBoxAPIKey = "";
    m_osmURL = "";
    m_mapBoxStyles = "";
    m_displaySelectedGroundTracks = true;
    m_displayAllGroundTracks = true;
    m_title = "Map";
    m_rgbColor = QColor(225, 25, 99).rgba();
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIFeatureSetIndex = 0;
    m_reverseAPIFeatureIndex = 0;
    m_map2DEnabled = true;
    m_useLiveryIcons = true;
    m_map3DEnabled = true;
    m_terrain = "Ellipsoid";
    m_buildings = "None";
    m_sunLightEnabled = true;
    m_lightIntensity = 3.0f;
    m_distantLightIntensity = 0.5f;
    m_lowDetailModels = true;
    m_eciCamera = false;
    m_modelDir = HttpDownloadManager::downloadDir() + "/3d";
    m_msfsNavigraphDir = defaultMSFSNavigraphDirectory();
    m_msfsAircraftDir = defaultMSFSAircraftDirectory();
    m_msfsMaxTextureSize = 2048;
    m_msfsLowDetailMaxTextureSize = 512;
    m_fxaa = true;
    m_msaa = 1;
    m_mapCenterLatitude = 0.0f;
    m_mapCenterLongitude = 0.0f;
    m_mapZoomLevel = -1.0f;
    m_terrainLighting = true;
    m_water = false;
    m_googlePhotorealistic3DTiles = false;
    m_hdr = true;
    m_fog = true;
    m_fps = false;
    m_frameRate = 60;
    m_displayMUF = false;
    m_displayfoF2 = false;
    m_displayRain = false;
    m_displayClouds = false;
    m_displayRailways = false;
    m_displaySeaMarks = false;
    m_displayNASAGlobalImagery = false;
    m_nasaGlobalImageryIdentifier = "";
    m_nasaGlobalImageryOpacity = 50;
    m_displayAurora = false;
    m_displayMagDec = false;
    m_displayMaidenheadGrid = false;
    m_displayPFD = false;
    m_displayND = false;
    m_mapItemSoundVolume = 50;
    m_cockpitSoundVolume = 100;
    m_audioMuted = false;
    m_displayCockpitPanels = true;
    m_displayEFIS = false;
    m_displayRMP = false;
    m_vhf1Channel = "";
    m_vhf2Channel = "";
    m_vhf1Standby = 121500000;  // The guard frequency, until something is entered
    m_vhf2Standby = 121500000;
    m_cockpitPanelGeometry = "";
    m_efisState = "";
    m_displayDCDU = false;
    m_cameraMode = ThirdPerson;
    m_defaultImagery = "ArcGIS world imagery";
    m_arcGISAPIKey = "";
    m_workspaceIndex = 0;
    m_checkWXAPIKey = "";
}

QByteArray MapSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(1, m_displayNames);
    s.writeString(2, m_mapProvider);
    s.writeString(3, m_mapBoxAPIKey);
    s.writeString(4, m_mapBoxStyles);
    s.writeString(8, m_title);
    s.writeU32(9, m_rgbColor);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIFeatureSetIndex);
    s.writeU32(14, m_reverseAPIFeatureIndex);
    s.writeBool(15, m_displaySelectedGroundTracks);
    s.writeBool(16, m_displayAllGroundTracks);
    s.writeString(17, m_thunderforestAPIKey);
    s.writeString(18, m_maptilerAPIKey);
    s.writeString(86, m_cartoDBAPIKey);

    if (m_rollupState) {
        s.writeBlob(19, m_rollupState->serialize());
    }

    s.writeString(20, m_osmURL);
    s.writeString(21, m_mapType);
    s.writeBool(22, m_map2DEnabled);
    s.writeBool(23, m_map3DEnabled);
    s.writeString(24, m_terrain);
    s.writeString(25, m_buildings);
    s.writeBlob(27, serializeItemSettings(m_itemSettings));
    s.writeString(28, m_modelDir);
    s.writeBool(29, m_sunLightEnabled);
    s.writeBool(30, m_eciCamera);
    s.writeString(31, m_cesiumIonAPIKey);
    s.writeS32(33, m_workspaceIndex);
    s.writeBlob(34, m_geometryBytes);

    s.writeBool(35, m_displayMUF);
    s.writeBool(36, m_displayfoF2);
    s.writeBool(37, m_displayRain);
    s.writeBool(38, m_displayClouds);
    s.writeBool(39, m_displaySeaMarks);
    s.writeBool(40, m_displayRailways);
    s.writeBool(41, m_displayNASAGlobalImagery);
    s.writeString(42, m_nasaGlobalImageryIdentifier);
    s.writeS32(43, m_nasaGlobalImageryOpacity);
    s.writeBool(44, m_displayAurora);
    s.writeBool(45, m_displayMagDec);

    s.writeString(46, m_checkWXAPIKey);

    s.writeFloat(47, m_lightIntensity);
    s.writeFloat(87, m_distantLightIntensity);
    s.writeBool(88, m_lowDetailModels);

    s.writeBool(48, m_displayMaidenheadGrid);
    s.writeString(49, m_defaultImagery);
    s.writeString(50, m_arcGISAPIKey);
    s.writeBool(51, m_displayPFD);

    s.writeBool(53, m_terrainLighting);
    s.writeBool(54, m_water);
    s.writeBool(55, m_hdr);
    s.writeBool(56, m_fog);
    s.writeBool(57, m_fps);
    s.writeS32(85, m_frameRate);
    s.writeBool(58, m_fxaa);
    s.writeS32(59, m_msaa);
    s.writeFloat(60, m_mapCenterLatitude);
    s.writeFloat(61, m_mapCenterLongitude);
    s.writeFloat(62, m_mapZoomLevel);
    s.writeBool(63, m_useLiveryIcons);
    s.writeBool(64, m_displayND);
    s.writeS32(65, m_itemSettingsVersion);
    s.writeBlob(66, m_splitterState);
    // 67 was m_dcduSound, superseded by the cockpit volume slider
    s.writeS32(68, m_mapItemSoundVolume);
    s.writeS32(69, m_cockpitSoundVolume);
    s.writeBool(70, m_audioMuted);
    // 71 is m_googlePhotorealistic3DTiles (below); these start at 72
    s.writeBool(72, m_displayEFIS);
    s.writeBool(73, m_displayRMP);
    s.writeBool(74, m_displayDCDU);
    s.writeBool(75, m_displayCockpitPanels);
    s.writeString(76, m_vhf1Channel);
    s.writeString(77, m_vhf2Channel);
    s.writeS32(78, m_vhf1Standby);
    s.writeS32(79, m_vhf2Standby);
    s.writeString(80, m_cockpitPanelGeometry);
    s.writeString(81, m_efisState);
    s.writeBool(71, m_googlePhotorealistic3DTiles);
    s.writeString(82, m_msfsAircraftDir);
    s.writeS32(83, m_cameraMode);
    s.writeString(84, m_msfsNavigraphDir);
    s.writeS32(89, m_msfsMaxTextureSize);
    s.writeS32(90, m_msfsLowDetailMaxTextureSize);

    return s.final();
}

bool MapSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;
        QString strtmp;
        QByteArray blob;
        QString string;

        d.readBool(1, &m_displayNames, true);
        d.readString(2, &m_mapProvider, "osm");
        d.readString(3, &m_mapBoxAPIKey, "");
#if defined(LINUX) && (QT_VERSION < QT_VERSION_CHECK(5, 15, 13))
        if (m_mapProvider == "osm") {
            m_mapProvider = "mapboxgl";
        }
#else
        // mapboxgl doesn't work without a key
        if ((m_mapProvider == "mapboxgl") && m_mapBoxAPIKey.isEmpty()) {
            m_mapProvider = "osm";
        }
#endif
        d.readString(4, &m_mapBoxStyles, "");
        d.readString(8, &m_title, "Map");
        d.readU32(9, &m_rgbColor, QColor(225, 25, 99).rgba());
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &utmp, 0);
        m_reverseAPIFeatureSetIndex = utmp > 99 ? 99 : utmp;
        d.readU32(14, &utmp, 0);
        m_reverseAPIFeatureIndex = utmp > 99 ? 99 : utmp;
        d.readBool(15, &m_displaySelectedGroundTracks, true);
        d.readBool(16, &m_displayAllGroundTracks, true);
        d.readString(17, &m_thunderforestAPIKey, "");
        d.readString(18, &m_maptilerAPIKey, "");
        d.readString(86, &m_cartoDBAPIKey, "");

        if (m_rollupState)
        {
            d.readBlob(19, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readString(20, &m_osmURL, "");
        d.readString(21, &m_mapType, "");

        d.readBool(22, &m_map2DEnabled, true);
        d.readBool(23, &m_map3DEnabled, true);
        d.readString(24, &m_terrain, "Ellipsoid");
        d.readString(25, &m_buildings, "None");
        d.readBlob(27, &blob);
        deserializeItemSettings(blob, m_itemSettings);
        d.readString(28, &m_modelDir, HttpDownloadManager::downloadDir() + "/3d");
        d.readBool(29, &m_sunLightEnabled, true);
        d.readBool(30, &m_eciCamera, false);
        d.readString(31, &m_cesiumIonAPIKey, "");
        d.readS32(33, &m_workspaceIndex, 0);
        d.readBlob(34, &m_geometryBytes);

        d.readBool(35, &m_displayMUF, false);
        d.readBool(36, &m_displayfoF2, false);
        d.readBool(37, &m_displayRain, false);
        d.readBool(38, &m_displayClouds, false);
        d.readBool(39, &m_displaySeaMarks, false);
        d.readBool(40, &m_displayRailways, false);
        d.readBool(41, &m_displayNASAGlobalImagery, false);
        d.readString(42, &m_nasaGlobalImageryIdentifier, "");
        d.readS32(43, &m_nasaGlobalImageryOpacity, 50);
        d.readBool(44, &m_displayAurora, false);
        d.readBool(45, &m_displayMagDec, false);

        d.readString(46, &m_checkWXAPIKey, "");

        d.readFloat(47, &m_lightIntensity, 3.0f);
        d.readFloat(87, &m_distantLightIntensity, 0.5f);
        d.readBool(88, &m_lowDetailModels, true);
        d.readBool(48, &m_displayMaidenheadGrid, false);
        d.readString(49, &m_defaultImagery, "ArcGIS world imagery");
        d.readString(50, &m_arcGISAPIKey, "");
        d.readBool(51, &m_displayPFD, false);
        // Key 52 held the old first person/third person switch. It is no longer
        // written, but it still chooses the default camera mode for anyone upgrading
        bool viewFirstPerson;
        d.readBool(52, &viewFirstPerson, false);
        d.readS32(83, &m_cameraMode, viewFirstPerson ? FirstPerson : ThirdPerson);

        d.readBool(53, &m_terrainLighting, true);
        d.readBool(54, &m_water, false);
        d.readBool(55, &m_hdr, true);
        d.readBool(56, &m_fog, true);
        d.readBool(57, &m_fps, false);
        d.readS32(85, &m_frameRate, 60);
        d.readBool(58, &m_fxaa, false);
        d.readS32(59, &m_msaa, 1);
        d.readFloat(60, &m_mapCenterLatitude, 0.0f);
        d.readFloat(61, &m_mapCenterLongitude, 0.0f);
        d.readFloat(62, &m_mapZoomLevel, -1.0f);
        d.readBool(63, &m_useLiveryIcons, true);
        d.readBool(64, &m_displayND, false);
        d.readS32(65, &m_itemSettingsVersion, 0);
        d.readBlob(66, &m_splitterState);
        d.readS32(68, &m_mapItemSoundVolume, 50);
        d.readS32(69, &m_cockpitSoundVolume, 100);
        d.readBool(70, &m_audioMuted, false);
        d.readBool(72, &m_displayEFIS, false);
        d.readBool(73, &m_displayRMP, false);
        d.readBool(74, &m_displayDCDU, true);
        d.readBool(75, &m_displayCockpitPanels, true);
        d.readString(76, &m_vhf1Channel, "");
        d.readString(77, &m_vhf2Channel, "");
        d.readS32(78, &m_vhf1Standby, 121500000);
        d.readS32(79, &m_vhf2Standby, 121500000);
        d.readString(80, &m_cockpitPanelGeometry, "");
        d.readString(81, &m_efisState, "");
        d.readBool(71, &m_googlePhotorealistic3DTiles, false);
        d.readString(82, &m_msfsAircraftDir, defaultMSFSAircraftDirectory());
        d.readString(84, &m_msfsNavigraphDir, defaultMSFSNavigraphDirectory());
        d.readS32(89, &m_msfsMaxTextureSize, 2048);
        d.readS32(90, &m_msfsLowDetailMaxTextureSize, 512);

        // Airspaces never had a usable 2D colour: their groups were created without
        // display2DTrack, which makes PolygonMapModel::data() return a transparent fill,
        // so they were drawn and invisible. A preset written before that was fixed still
        // carries the false, and would go on overriding the corrected default - so it is
        // turned on once, here, and the fact recorded so a later decision to switch them
        // off again is left alone.
        if (m_itemSettingsVersion < 1)
        {
            for (MapItemSettings *itemSettings : m_itemSettings)
            {
                if (itemSettings && itemSettings->m_group.startsWith("Airspace (")) {
                    itemSettings->m_display2DTrack = true;
                }
            }
            m_itemSettingsVersion = 1;
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

MapSettings::MapItemSettings::MapItemSettings(const QString& group,
                                              bool enabled,
                                              const QColor color,
                                              bool display2DTrack,
                                              bool display3DPoint,
                                              int minZoom,
                                              int modelMinPixelSize)
{
    m_group = group;
    resetToDefaults();
    m_enabled = enabled,
    m_3DPointColor = color.rgba();
    m_2DTrackColor = color.darker().rgba();
    m_3DTrackColor = color.darker().rgba();
    m_display2DTrack = display2DTrack;
    m_display3DPoint = display3DPoint;
    m_2DMinZoom = minZoom;
    m_3DModelMinPixelSize = modelMinPixelSize;
}

MapSettings::MapItemSettings::MapItemSettings(const QByteArray& data)
{
    deserialize(data);
}

void MapSettings::MapItemSettings::resetToDefaults()
{
    m_enabled = true;
    m_display2DIcon = true;
    m_display2DLabel = true;
    m_display2DTrack = true;
    m_2DTrackColor = QColor(150, 0, 20).rgba();
    m_2DTrackPalette = defaultTrackPalette(m_group);
    m_2DMinZoom = 1;
    m_display3DModel = true;
    m_display3DPoint = false;
    m_3DPointColor = QColor(225, 0, 0).rgba();
    m_display3DLabel = true;
    m_display3DTrack = true;
    m_3DTrackColor = QColor(150, 0, 20).rgba();
    m_3DModelMinPixelSize = 0;
    m_3DLabelScale = 0.5f;
    m_filterName = "";
    m_filterDistance = 0;
    m_extrapolate = 60;
    m_smoothingWindow = 0;
    m_smoothingLambda = 100;
}

// Aircraft tracks are coloured by altitude by default, as they are on the ADS-B
// demodulator's own map. Everything else is drawn in a single colour.
QString MapSettings::MapItemSettings::defaultTrackPalette(const QString& group)
{
    if ((group == "ADSBDemod") || (group == "Aircraft")) {
        return "Spectral";
    } else {
        return "";
    }
}

QByteArray MapSettings::MapItemSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_group);
    s.writeBool(2, m_enabled);
    s.writeBool(3, m_display2DIcon);
    s.writeBool(4, m_display2DLabel);
    s.writeBool(5, m_display2DTrack);
    s.writeU32(6, m_2DTrackColor);
    s.writeS32(7, m_2DMinZoom);
    s.writeBool(8, m_display3DModel);
    s.writeBool(9, m_display3DLabel);
    s.writeBool(10, m_display3DPoint);
    s.writeU32(11, m_3DPointColor);
    s.writeBool(12, m_display3DTrack);
    s.writeU32(13, m_3DTrackColor);
    s.writeS32(14, m_3DModelMinPixelSize);
    s.writeFloat(15, m_3DLabelScale);
    s.writeString(16, m_filterName);
    s.writeS32(17, m_filterDistance);
    s.writeS32(18, m_extrapolate);
    s.writeS32(19, m_smoothingWindow);
    s.writeFloat(20, m_smoothingLambda);
    s.writeString(21, m_2DTrackPalette);

    return s.final();
}

bool MapSettings::MapItemSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_group, "");
        d.readBool(2, &m_enabled, true);
        d.readBool(3, &m_display2DIcon, true);
        d.readBool(4, &m_display2DLabel, true);
        d.readBool(5, &m_display2DTrack, true);
        d.readU32(6, &m_2DTrackColor, QColor(150, 0, 0).rgba());
        d.readS32(7, &m_2DMinZoom, 1);
        d.readBool(8, &m_display3DModel, true);
        d.readBool(9, &m_display3DLabel, true);
        d.readBool(10, &m_display3DPoint, true);
        d.readU32(11, &m_3DPointColor, QColor(255, 0, 0).rgba());
        d.readBool(12, &m_display3DTrack, true);
        d.readU32(13, &m_3DTrackColor, QColor(150, 0, 20).rgba());
        d.readS32(14, &m_3DModelMinPixelSize, 0);
        d.readFloat(15, &m_3DLabelScale, 0.5f);
        d.readString(16, &m_filterName, "");
        d.readS32(17, &m_filterDistance, 0);
        d.readS32(18, &m_extrapolate, 60);
        d.readString(21, &m_2DTrackPalette, defaultTrackPalette(m_group));
        m_filterNameRE.setPattern(m_filterName);
        m_filterNameRE.optimize();
        if ((m_group == "ADSBDemod") || (m_group == "Aircraft"))
        {
            d.readS32(19, &m_smoothingWindow, 10);
            d.readFloat(20, &m_smoothingLambda, 100);
        }
        else
        {
            d.readS32(19, &m_smoothingWindow, 0);
            d.readFloat(20, &m_smoothingLambda, 100);
        }
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QByteArray MapSettings::serializeItemSettings(QHash<QString, MapItemSettings *> itemSettings) const
{
    SimpleSerializer s(1);

    int idx = 1;
    QHashIterator<QString, MapItemSettings *> i(itemSettings);
    while (i.hasNext())
    {
        i.next();

        s.writeString(idx+1, i.key());
        s.writeBlob(idx+2, i.value()->serialize());

        idx += 2;
    }

    return s.final();
}

void MapSettings::deserializeItemSettings(const QByteArray& data, QHash<QString, MapItemSettings *>& itemSettings)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return;
    }

    int idx = 1;
    bool done = false;
    do
    {
        QString key;
        QByteArray blob;

        if (!d.readString(idx+1, &key))
        {
            done = true;
        }
        else
        {
            d.readBlob(idx+2, &blob);
            if (itemSettings.contains(key))
            {
                itemSettings[key]->deserialize(blob);
            }
            else
            {
                itemSettings.insert(key, new MapItemSettings(blob));
            }
        }

        idx += 2;
    } while(!done);
}

void MapSettings::applySettings(const QStringList& settingsKeys, const MapSettings& settings)
{
    if (settingsKeys.contains("displayNames")) {
        m_displayNames = settings.m_displayNames;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("mapProvider")) {
        m_mapProvider = settings.m_mapProvider;
    }
    if (settingsKeys.contains("thunderforestAPIKey")) {
        m_thunderforestAPIKey = settings.m_thunderforestAPIKey;
    }
    if (settingsKeys.contains("maptilerAPIKey")) {
        m_maptilerAPIKey = settings.m_maptilerAPIKey;
    }
    if (settingsKeys.contains("cartoDBAPIKey")) {
        m_cartoDBAPIKey = settings.m_cartoDBAPIKey;
    }
    if (settingsKeys.contains("osmURL")) {
        m_osmURL = settings.m_osmURL;
    }
    if (settingsKeys.contains("mapBoxStyles")) {
        m_mapBoxStyles = settings.m_mapBoxStyles;
    }
    if (settingsKeys.contains("mapType")) {
        m_mapType = settings.m_mapType;
    }
    if (settingsKeys.contains("displaySelectedGroundTracks")) {
        m_displaySelectedGroundTracks = settings.m_displaySelectedGroundTracks;
    }
    if (settingsKeys.contains("displayAllGroundTracks")) {
        m_displayAllGroundTracks = settings.m_displayAllGroundTracks;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("displayAllGroundTracks")) {
        m_displayAllGroundTracks = settings.m_displayAllGroundTracks;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex")) {
        m_reverseAPIFeatureSetIndex = settings.m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex")) {
        m_reverseAPIFeatureIndex = settings.m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("map2DEnabled")) {
        m_map2DEnabled = settings.m_map2DEnabled;
    }
    if (settingsKeys.contains("useLiveryIcons")) {
        m_useLiveryIcons = settings.m_useLiveryIcons;
    }
    if (settingsKeys.contains("map3DEnabled")) {
        m_map3DEnabled = settings.m_map3DEnabled;
    }
    if (settingsKeys.contains("terrain")) {
        m_terrain = settings.m_terrain;
    }
    if (settingsKeys.contains("buildings")) {
        m_buildings = settings.m_buildings;
    }
    if (settingsKeys.contains("sunLightEnabled")) {
        m_sunLightEnabled = settings.m_sunLightEnabled;
    }
    if (settingsKeys.contains("distantLightIntensity")) {
        m_distantLightIntensity = settings.m_distantLightIntensity;
    }
    if (settingsKeys.contains("lowDetailModels")) {
        m_lowDetailModels = settings.m_lowDetailModels;
    }
    if (settingsKeys.contains("lightIntensity")) {
        m_lightIntensity = settings.m_lightIntensity;
    }
    if (settingsKeys.contains("eciCamera")) {
        m_eciCamera = settings.m_eciCamera;
    }
    if (settingsKeys.contains("modelDir")) {
        m_modelDir = settings.m_modelDir;
    }
    if (settingsKeys.contains("msfsNavigraphDir")) {
        m_msfsNavigraphDir = settings.m_msfsNavigraphDir;
    }
    if (settingsKeys.contains("msfsAircraftDir")) {
        m_msfsAircraftDir = settings.m_msfsAircraftDir;
    }
    if (settingsKeys.contains("msfsMaxTextureSize")) {
        m_msfsMaxTextureSize = settings.m_msfsMaxTextureSize;
    }
    if (settingsKeys.contains("msfsLowDetailMaxTextureSize")) {
        m_msfsLowDetailMaxTextureSize = settings.m_msfsLowDetailMaxTextureSize;
    }
    if (settingsKeys.contains("fxaa")) {
        m_fxaa = settings.m_fxaa;
    }
    if (settingsKeys.contains("msaa")) {
        m_msaa = settings.m_msaa;
    }
    if (settingsKeys.contains("mapCenterLatitude")) {
        m_mapCenterLatitude = settings.m_mapCenterLatitude;
    }
    if (settingsKeys.contains("mapCenterLongitude")) {
        m_mapCenterLongitude = settings.m_mapCenterLongitude;
    }
    if (settingsKeys.contains("mapZoomLevel")) {
        m_mapZoomLevel = settings.m_mapZoomLevel;
    }
    if (settingsKeys.contains("terrainLighting")) {
        m_terrainLighting = settings.m_terrainLighting;
    }
    if (settingsKeys.contains("water")) {
        m_water = settings.m_water;
    }
    if (settingsKeys.contains("googlePhotorealistic3DTiles")) {
        m_googlePhotorealistic3DTiles = settings.m_googlePhotorealistic3DTiles;
    }
    if (settingsKeys.contains("hdr")) {
        m_hdr = settings.m_hdr;
    }
    if (settingsKeys.contains("fog")) {
        m_fog = settings.m_fog;
    }
    if (settingsKeys.contains("fps")) {
        m_fps = settings.m_fps;
    }
    if (settingsKeys.contains("frameRate")) {
        m_frameRate = settings.m_frameRate;
    }
    if (settingsKeys.contains("displayMUF")) {
        m_displayMUF = settings.m_displayMUF;
    }
    if (settingsKeys.contains("displayfoF2")) {
        m_displayfoF2 = settings.m_displayfoF2;
    }
    if (settingsKeys.contains("displayRain")) {
        m_displayRain = settings.m_displayRain;
    }
    if (settingsKeys.contains("displayClouds")) {
        m_displayClouds = settings.m_displayClouds;
    }
    if (settingsKeys.contains("displaySeaMarks")) {
        m_displaySeaMarks = settings.m_displaySeaMarks;
    }
    if (settingsKeys.contains("displayRailways")) {
        m_displayRailways = settings.m_displayRailways;
    }
    if (settingsKeys.contains("displayNASAGlobalImagery")) {
        m_displayNASAGlobalImagery = settings.m_displayNASAGlobalImagery;
    }
    if (settingsKeys.contains("nasaGlobalImageryIdentifier")) {
        m_nasaGlobalImageryIdentifier = settings.m_nasaGlobalImageryIdentifier;
    }
    if (settingsKeys.contains("nasaGlobalImageryOpacity")) {
        m_nasaGlobalImageryOpacity = settings.m_nasaGlobalImageryOpacity;
    }
    if (settingsKeys.contains("displayAurora")) {
        m_displayAurora = settings.m_displayAurora;
    }
    if (settingsKeys.contains("displayMagDec")) {
        m_displayMagDec = settings.m_displayMagDec;
    }
    if (settingsKeys.contains("displayMaidenheadGrid")) {
        m_displayMaidenheadGrid = settings.m_displayMaidenheadGrid;
    }
    if (settingsKeys.contains("defaultImagery")) {
        m_defaultImagery = settings.m_defaultImagery;
    }
    if (settingsKeys.contains("arcGISAPIKey")) {
        m_arcGISAPIKey = settings.m_arcGISAPIKey;
    }
    if (settingsKeys.contains("cesiumIonAPIKey")) {
        m_cesiumIonAPIKey = settings.m_cesiumIonAPIKey;
    }
    if (settingsKeys.contains("displayPFD")) {
        m_displayPFD = settings.m_displayPFD;
    }
    if (settingsKeys.contains("displayND")) {
        m_displayND = settings.m_displayND;
    }
    if (settingsKeys.contains("mapItemSoundVolume")) {
        m_mapItemSoundVolume = settings.m_mapItemSoundVolume;
    }
    if (settingsKeys.contains("cockpitSoundVolume")) {
        m_cockpitSoundVolume = settings.m_cockpitSoundVolume;
    }
    if (settingsKeys.contains("audioMuted")) {
        m_audioMuted = settings.m_audioMuted;
    }
    if (settingsKeys.contains("displayCockpitPanels")) {
        m_displayCockpitPanels = settings.m_displayCockpitPanels;
    }
    if (settingsKeys.contains("displayEFIS")) {
        m_displayEFIS = settings.m_displayEFIS;
    }
    if (settingsKeys.contains("displayRMP")) {
        m_displayRMP = settings.m_displayRMP;
    }
    if (settingsKeys.contains("vhf1Channel")) {
        m_vhf1Channel = settings.m_vhf1Channel;
    }
    if (settingsKeys.contains("vhf2Channel")) {
        m_vhf2Channel = settings.m_vhf2Channel;
    }
    if (settingsKeys.contains("vhf1Standby")) {
        m_vhf1Standby = settings.m_vhf1Standby;
    }
    if (settingsKeys.contains("vhf2Standby")) {
        m_vhf2Standby = settings.m_vhf2Standby;
    }
    if (settingsKeys.contains("cockpitPanelGeometry")) {
        m_cockpitPanelGeometry = settings.m_cockpitPanelGeometry;
    }
    if (settingsKeys.contains("efisState")) {
        m_efisState = settings.m_efisState;
    }
    if (settingsKeys.contains("displayDCDU")) {
        m_displayDCDU = settings.m_displayDCDU;
    }
    if (settingsKeys.contains("cameraMode")) {
        m_cameraMode = settings.m_cameraMode;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
}

QString MapSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("displayNames") || force) {
        ostr << " m_displayNames: " << m_displayNames;
    }
    if (settingsKeys.contains("mapProvider") || force) {
        ostr << " m_mapProvider: " << m_mapProvider.toStdString();
    }
    if (settingsKeys.contains("thunderforestAPIKey") || force) {
        ostr << " m_thunderforestAPIKey: " << m_thunderforestAPIKey.toStdString();
    }
    if (settingsKeys.contains("maptilerAPIKey") || force) {
        ostr << " m_maptilerAPIKey: " << m_maptilerAPIKey.toStdString();
    }
    if (settingsKeys.contains("cartoDBAPIKey") || force) {
        ostr << " m_cartoDBAPIKey: " << m_cartoDBAPIKey.toStdString();
    }
    if (settingsKeys.contains("mapBoxAPIKey") || force) {
        ostr << " m_mapBoxAPIKey: " << m_mapBoxAPIKey.toStdString();
    }
    if (settingsKeys.contains("osmURL") || force) {
        ostr << " m_osmURL: " << m_osmURL.toStdString();
    }
    if (settingsKeys.contains("mapBoxStyles") || force) {
        ostr << " m_mapBoxStyles: " << m_mapBoxStyles.toStdString();
    }
    if (settingsKeys.contains("displaySelectedGroundTracks") || force) {
        ostr << " m_displaySelectedGroundTracks: " << m_displaySelectedGroundTracks;
    }
    if (settingsKeys.contains("_displayAllGroundTracks") || force) {
        ostr << " m_displayAllGroundTracks: " << m_displayAllGroundTracks;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("displayAllGroundTracks") || force) {
        ostr << " m_displayAllGroundTracks: " << m_displayAllGroundTracks;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIFeatureSetIndex") || force) {
        ostr << " m_reverseAPIFeatureSetIndex: " << m_reverseAPIFeatureSetIndex;
    }
    if (settingsKeys.contains("reverseAPIFeatureIndex") || force) {
        ostr << " m_reverseAPIFeatureIndex: " << m_reverseAPIFeatureIndex;
    }
    if (settingsKeys.contains("map2DEnabled") || force) {
        ostr << " m_map2DEnabled: " << m_map2DEnabled;
    }
    if (settingsKeys.contains("useLiveryIcons") || force) {
        ostr << " m_useLiveryIcons: " << m_useLiveryIcons;
    }
    if (settingsKeys.contains("map3DEnabled") || force) {
        ostr << " m_map3DEnabled: " << m_map3DEnabled;
    }
    if (settingsKeys.contains("terrain") || force) {
        ostr << " m_terrain: " << m_terrain.toStdString();
    }
    if (settingsKeys.contains("buildings") || force) {
        ostr << " m_buildings: " << m_buildings.toStdString();
    }
    if (settingsKeys.contains("sunLightEnabled") || force) {
        ostr << " m_sunLightEnabled: " << m_sunLightEnabled;
    }
    if (settingsKeys.contains("distantLightIntensity") || force) {
        ostr << " m_distantLightIntensity: " << m_distantLightIntensity;
    }
    if (settingsKeys.contains("lowDetailModels") || force) {
        ostr << " m_lowDetailModels: " << m_lowDetailModels;
    }
    if (settingsKeys.contains("lightIntensity") || force) {
        ostr << " m_lightIntensity: " << m_lightIntensity;
    }
    if (settingsKeys.contains("eciCamera") || force) {
        ostr << " m_eciCamera: " << m_eciCamera;
    }
    if (settingsKeys.contains("modelDir") || force) {
        ostr << " m_modelDir: " << m_modelDir.toStdString();
    }
    if (settingsKeys.contains("msfsNavigraphDir") || force) {
        ostr << " m_msfsNavigraphDir: " << m_msfsNavigraphDir.toStdString();
    }
    if (settingsKeys.contains("msfsAircraftDir") || force) {
        ostr << " m_msfsAircraftDir: " << m_msfsAircraftDir.toStdString();
    }
    if (settingsKeys.contains("msfsMaxTextureSize") || force) {
        ostr << " m_msfsMaxTextureSize: " << m_msfsMaxTextureSize;
    }
    if (settingsKeys.contains("msfsLowDetailMaxTextureSize") || force) {
        ostr << " m_msfsLowDetailMaxTextureSize: " << m_msfsLowDetailMaxTextureSize;
    }
    if (settingsKeys.contains("fxaa") || force) {
        ostr << " m_fxaa: " << m_fxaa;
    }
    if (settingsKeys.contains("msaa") || force) {
        ostr << " m_msaa: " << m_msaa;
    }
    if (settingsKeys.contains("mapCenterLatitude") || force) {
        ostr << " m_mapCenterLatitude: " << m_mapCenterLatitude;
    }
    if (settingsKeys.contains("mapCenterLongitude") || force) {
        ostr << " m_mapCenterLongitude: " << m_mapCenterLongitude;
    }
    if (settingsKeys.contains("mapZoomLevel") || force) {
        ostr << " m_mapZoomLevel: " << m_mapZoomLevel;
    }
    if (settingsKeys.contains("terrainLighting") || force) {
        ostr << " m_terrainLighting: " << m_terrainLighting;
    }
    if (settingsKeys.contains("water") || force) {
        ostr << " m_water: " << m_water;
    }
    if (settingsKeys.contains("googlePhotorealistic3DTiles") || force) {
        ostr << " m_googlePhotorealistic3DTiles: " << m_googlePhotorealistic3DTiles;
    }
    if (settingsKeys.contains("hdr") || force) {
        ostr << " m_hdr: " << m_hdr;
    }
    if (settingsKeys.contains("fog") || force) {
        ostr << " m_fog: " << m_fog;
    }
    if (settingsKeys.contains("fps") || force) {
        ostr << " m_fps: " << m_fps;
    }
    if (settingsKeys.contains("frameRate") || force) {
        ostr << " m_frameRate: " << m_frameRate;
    }
    if (settingsKeys.contains("displayMUF") || force) {
        ostr << " m_displayMUF: " << m_displayMUF;
    }
    if (settingsKeys.contains("displayfoF2") || force) {
        ostr << " m_displayfoF2: " << m_displayfoF2;
    }
    if (settingsKeys.contains("displayRain") || force) {
        ostr << " m_displayRain: " << m_displayRain;
    }
    if (settingsKeys.contains("displayClouds") || force) {
        ostr << " m_displayClouds: " << m_displayClouds;
    }
    if (settingsKeys.contains("displaySeaMarks") || force) {
        ostr << " m_displaySeaMarks: " << m_displaySeaMarks;
    }
    if (settingsKeys.contains("displayRailways") || force) {
        ostr << " m_displayRailways: " << m_displayRailways;
    }
    if (settingsKeys.contains("displayNASAGlobalImagery") || force) {
        ostr << " m_displayNASAGlobalImagery: " << m_displayNASAGlobalImagery;
    }
    if (settingsKeys.contains("nasaGlobalImageryIdentifier") || force) {
        ostr << " m_nasaGlobalImageryIdentifier: " << m_nasaGlobalImageryIdentifier.toStdString();
    }
    if (settingsKeys.contains("nasaGlobalImageryOpacity") || force) {
        ostr << " m_nasaGlobalImageryOpacity: " << m_nasaGlobalImageryOpacity;
    }
    if (settingsKeys.contains("displayAurora") || force) {
        ostr << " m_displayAurora: " << m_displayAurora;
    }
    if (settingsKeys.contains("displayMagDec") || force) {
        ostr << " m_displayMagDec: " << m_displayMagDec;
    }
    if (settingsKeys.contains("displayMaidenheadGrid") || force) {
        ostr << " m_displayMaidenheadGrid: " << m_displayMaidenheadGrid;
    }
    if (settingsKeys.contains("defaultImagery") || force) {
        ostr << " m_defaultImagery: " << m_defaultImagery.toStdString();
    }
    if (settingsKeys.contains("cesiumIonAPIKey") || force) {
        ostr << " m_cesiumIonAPIKey: " << m_cesiumIonAPIKey.toStdString();
    }
    if (settingsKeys.contains("arcGISAPIKey") || force) {
        ostr << " m_arcGISAPIKey: " << m_arcGISAPIKey.toStdString();
    }
    if (settingsKeys.contains("displayPFD") || force) {
        ostr << " m_displayPFD: " << m_displayPFD;
    }
    if (settingsKeys.contains("displayND") || force) {
        ostr << " m_displayND: " << m_displayND;
    }
    if (settingsKeys.contains("mapItemSoundVolume") || force) {
        ostr << " m_mapItemSoundVolume: " << m_mapItemSoundVolume;
    }
    if (settingsKeys.contains("cockpitSoundVolume") || force) {
        ostr << " m_cockpitSoundVolume: " << m_cockpitSoundVolume;
    }
    if (settingsKeys.contains("audioMuted") || force) {
        ostr << " m_audioMuted: " << m_audioMuted;
    }
    if (settingsKeys.contains("displayCockpitPanels") || force) {
        ostr << " m_displayCockpitPanels: " << m_displayCockpitPanels;
    }
    if (settingsKeys.contains("displayEFIS") || force) {
        ostr << " m_displayEFIS: " << m_displayEFIS;
    }
    if (settingsKeys.contains("displayRMP") || force) {
        ostr << " m_displayRMP: " << m_displayRMP;
    }
    if (settingsKeys.contains("vhf1Channel") || force) {
        ostr << " m_vhf1Channel: " << m_vhf1Channel.toStdString();
    }
    if (settingsKeys.contains("vhf2Channel") || force) {
        ostr << " m_vhf2Channel: " << m_vhf2Channel.toStdString();
    }
    if (settingsKeys.contains("vhf1Standby") || force) {
        ostr << " m_vhf1Standby: " << m_vhf1Standby;
    }
    if (settingsKeys.contains("vhf2Standby") || force) {
        ostr << " m_vhf2Standby: " << m_vhf2Standby;
    }
    if (settingsKeys.contains("cockpitPanelGeometry") || force) {
        ostr << " m_cockpitPanelGeometry: " << m_cockpitPanelGeometry.toStdString();
    }
    if (settingsKeys.contains("efisState") || force) {
        ostr << " m_efisState: " << m_efisState.toStdString();
    }
    if (settingsKeys.contains("displayDCDU") || force) {
        ostr << " m_displayDCDU: " << m_displayDCDU;
    }
    if (settingsKeys.contains("cameraMode") || force) {
        ostr << " m_cameraMode: " << m_cameraMode;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }

    return QString(ostr.str().c_str());
}
