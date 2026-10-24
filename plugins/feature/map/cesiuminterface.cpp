///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QJsonObject>

#include "cesiuminterface.h"
#include "util/profiler.h"

CesiumInterface::CesiumInterface(const MapSettings *settings, QObject *parent) :
    MapWebSocketServer(parent),
    m_czml(settings)
{
}

// Set the view displayed when the Home button is pressed
// Angle in degrees of longitude or latitude either side of central location
void CesiumInterface::setHomeView(float latitude, float longitude, float angle)
{
    QJsonObject obj {
        {"command", "setHomeView"},
        {"latitude", latitude},
        {"longitude", longitude},
        {"angle", angle}
    };
    send(obj);
}

// Set the current camera view to a given location
void CesiumInterface::setView(float latitude, float longitude, float altitude)
{
    QJsonObject obj {
        {"command", "setView"},
        {"latitude", latitude},
        {"longitude", longitude},
        {"altitude", altitude}
    };
    send(obj);
}

// How the camera follows the tracked object: MapSettings::CameraMode
void CesiumInterface::setCameraMode(int mode)
{
    QJsonObject obj {
        {"command", "setCameraMode"},
        {"mode", mode}
    };
    send(obj);
}

// Play glTF model animation for the map item with the specified name
void CesiumInterface::playAnimation(const QString &name, Animation *animation)
{
    QJsonObject obj {
        {"command", "playAnimation"},
        {"id", name},
        {"animation", animation->m_name},
        {"startDateTime", animation->m_startDateTime},
        {"reverse", animation->m_reverse},
        {"loop", animation->m_loop},
        {"stop", animation->m_stop},
        {"startOffset", animation->m_startOffset},
        {"duration", animation->m_duration},
        {"multiplier", animation->m_multiplier},
        {"sound", animation->m_sound}
    };
    if (m_batching) {
        // Queued so it cannot overtake the entity update it refers to
        m_pendingCommands.append(obj);
        return;
    }
    send(obj);
}

// Set date and time for the map
void CesiumInterface::setDateTime(QDateTime dateTime)
{
    QJsonObject obj {
        {"command", "setDateTime"},
        {"dateTime", dateTime.toString(Qt::ISODate)}
    };
    send(obj);
}

// Get current date and time from the map
void CesiumInterface::getDateTime()
{
    QJsonObject obj {
        {"command", "getDateTime"}
    };
    send(obj);
}

// Set the camera to track the map item with the specified name
void CesiumInterface::track(const QString& name)
{
    QJsonObject obj {
        {"command", "trackId"},
        {"id", name}
    };
    send(obj);
}

void CesiumInterface::setDefaultImagery(const QString &imagery)
{
    QJsonObject obj {
        {"command", "setDefaultImagery"},
        {"imagery", imagery}
    };
    send(obj);
}

void CesiumInterface::setTerrain(const QString &terrain, const QString &maptilerAPIKey, bool lighting, bool water)
{
    QString provider;
    QString url;
    if (terrain == "Maptiler")
    {
        provider = "CesiumTerrainProvider";
        url = "https://api.maptiler.com/tiles/terrain-quantized-mesh/?key=" + maptilerAPIKey;
    }
    else if (terrain == "ArcGIS")
    {
        provider = "ArcGISTiledElevationTerrainProvider";
        url = "https://elevation3d.arcgis.com/arcgis/rest/services/WorldElevation3D/Terrain3D/ImageServer";
    }
    else
    {
        provider = terrain;
    }
    QJsonObject obj {
        {"command", "setTerrain"},
        {"provider", provider},
        {"url", url},
        // Not "lighting", which is the Sun/camera lighting in setLighting
        {"terrainLighting", lighting},
        {"water", water}
    };
    send(obj);
}

void CesiumInterface::setBuildings(const QString &buildings)
{
    QJsonObject obj {
        {"command", "setBuildings"},
        {"buildings", buildings}
    };
    send(obj);
}

void CesiumInterface::setGooglePhotorealistic3DTiles(bool enabled)
{
    QJsonObject obj {
        {"command", "setGooglePhotorealistic3DTiles"},
        {"enabled", enabled}
    };
    send(obj);
}

void CesiumInterface::setLighting(bool useSunLight, float cameraLightIntensity)
{
    QJsonObject obj {
        {"command", "setLighting"},
        {"useSunLight", useSunLight},
        {"cameraLightIntensity", cameraLightIntensity}
    };
    send(obj);
}

void CesiumInterface::setCameraReferenceFrame(bool eci)
{
    QJsonObject obj {
        {"command", "setCameraReferenceFrame"},
        {"eci", eci}
    };
    send(obj);
}

void CesiumInterface::setAntiAliasing(bool fxaa, int msaa)
{
    QJsonObject obj {
        {"command", "setAntiAliasing"},
        {"fxaa", fxaa},
        {"msaa", msaa}
    };
    send(obj);
}

void CesiumInterface::setHDR(bool enabled)
{
    QJsonObject obj {
        {"command", "setHDR"},
        {"hdr", enabled}
    };
    send(obj);
}

void CesiumInterface::setFog(bool enabled)
{
    QJsonObject obj {
        {"command", "setFog"},
        {"fog", enabled}
    };
    send(obj);
}

void CesiumInterface::showFPS(bool show)
{
    QJsonObject obj {
        {"command", "showFPS"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showPFD(bool show)
{
    QJsonObject obj {
        {"command", "showPFD"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showND(bool show)
{
    QJsonObject obj {
        {"command", "showND"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showEFIS(bool show)
{
    QJsonObject obj {
        {"command", "showEFIS"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showRMP(bool show)
{
    QJsonObject obj {
        {"command", "showRMP"},
        {"show", show}
    };
    send(obj);
}

// The RMP's two VHF channels: {"vhf1": {"active": Hz, "standby": Hz, "available": bool},
// "vhf2": {...}}. active is null when the linked AM Demod cannot be read
void CesiumInterface::setRMPState(const QJsonObject& state)
{
    QJsonObject obj = state;
    obj.insert("command", "setRMPState");
    send(obj);
}

// Hiding also suppresses the DCDU's pop-on-message behaviour
void CesiumInterface::showDCDU(bool show)
{
    QJsonObject obj {
        {"command", "showDCDU"},
        {"show", show}
    };
    send(obj);
}

// The airport database for the ND's airport overlay: [{n, lat, lon, t}] with
// t 0 small, 1 medium, 2 large
// Where each cockpit panel is to be placed: {"pfd": {"l": x, "t": y, "w": w, "h": h}, ...}
// in CSS pixels, as the page last reported them
void CesiumInterface::setPanelGeometry(const QJsonObject& panels)
{
    QJsonObject obj {
        {"command", "setPanelGeometry"},
        {"panels", panels}
    };
    send(obj);
}

// What the EFIS knobs and pushbuttons are set to: {"mode", "range", "wx", "terr",
// "arpt", "vor", "tcas"}, as the page last reported them
void CesiumInterface::setEFISState(const QJsonObject& state)
{
    QJsonObject obj {
        {"command", "setEFISState"},
        {"state", state}
    };
    send(obj);
}

void CesiumInterface::setNDAirports(const QJsonArray& airports)
{
    QJsonObject obj {
        {"command", "setNDAirports"},
        {"airports", airports}
    };
    send(obj);
}

// The VOR-family navaids for the ND's overlay: [{n, lat, lon}]
void CesiumInterface::setNDNavAids(const QJsonArray& navAids)
{
    QJsonObject obj {
        {"command", "setNDNavAids"},
        {"navAids", navAids}
    };
    send(obj);
}

// Volumes are [0..100]
void CesiumInterface::setAudioVolume(int mapItemVolume, int cockpitVolume)
{
    QJsonObject obj {
        {"command", "setAudioVolume"},
        {"mapItem", mapItemVolume / 100.0},
        {"cockpit", cockpitVolume / 100.0}
    };
    send(obj);
}

void CesiumInterface::showMUF(bool show)
{
    QJsonObject obj {
        {"command", "showMUF"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showfoF2(bool show)
{
    QJsonObject obj {
        {"command", "showfoF2"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showMagDec(bool show)
{
    QJsonObject obj {
        {"command", "showMagneticDeclination"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showMaidenheadGrid(bool show)
{
    QJsonObject obj {
        {"command", "showMaidenheadGrid"},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::showLayer(const QString& layer, bool show)
{
    QJsonObject obj {
        {"command", "showLayer"},
        {"layer", layer},
        {"show", show}
    };
    send(obj);
}

void CesiumInterface::setLayerSettings(const QString& layer, const QStringList& settings, const QList<QVariant>& values)
{
    QJsonObject obj {
        {"command", "setLayerSettings"},
        {"layer", layer},
    };
    for (int i = 0; i < settings.size(); i++) {
        obj.insert(settings[i], QJsonValue::fromVariant(values[i]));
    }
    send(obj);
}

void CesiumInterface::updateImage(const QString &name, float east, float west, float north, float south, float altitude, const QString &data)
{
    QJsonObject obj {
        {"command", "updateImage"},
        {"name", name},
        {"east", east},
        {"west", west},
        {"north", north},
        {"south", south},
        {"altitude", altitude},
        {"data", data},
    };
    send(obj);
}

void CesiumInterface::removeImage(const QString &name)
{
    QJsonObject obj {
        {"command", "removeImage"},
        {"name", name}
    };
    send(obj);
}

void CesiumInterface::removeAllImages()
{
    QJsonObject obj {
        {"command", "removeAllImages"}
    };
    send(obj);
}

// Remove all entities created by CZML
void CesiumInterface::removeAllCZMLEntities()
{
    QJsonObject obj {
        {"command", "removeAllCZMLEntities"}
    };
    send(obj);
    // Cesium keeps nothing after this, so neither may we: describing an item against
    // what it used to look like would leave out of the packet everything that has not
    // changed, and none of it is there any more
    m_czml.clear();
}

void CesiumInterface::initCZML()
{
    QJsonObject obj = m_czml.init();
    czml(obj);
}

// Send and process CZML
void CesiumInterface::czml(QJsonObject &obj)
{
    if (m_batching)
    {
        // The command key is added once, to the array, not to every packet
        m_batch.append(obj);
        return;
    }
    obj.insert("command", "czml");
    send(obj);
}

void CesiumInterface::beginBatch()
{
    m_batching = true;
    m_batch = QJsonArray();
}

void CesiumInterface::endBatch()
{
    m_batching = false;

    // The CZML goes first, then anything that was issued while it was being collected -
    // an animation must not arrive before the entity update it belongs to
    if (m_batch.isEmpty())
    {
        flushPendingCommands();
        return;
    }
    // One packet is not worth an array
    if (m_batch.size() == 1)
    {
        QJsonObject obj = m_batch.first().toObject();
        obj.insert("command", "czml");
        send(obj);
    }
    else
    {
        QJsonObject obj {
            {"command", "czmlArray"},
            {"packets", m_batch}
        };
        send(obj);
    }
    m_batch = QJsonArray();
    flushPendingCommands();
}

void CesiumInterface::flushPendingCommands()
{
    for (auto& obj : m_pendingCommands) {
        send(obj);
    }
    m_pendingCommands.clear();
}

void CesiumInterface::update(ObjectMapItem *mapItem, bool isTarget, bool isSelected)
{
    PROFILER_START()
    // Track segments first: update() resets the item's cached state when the item is
    // deleted, and the segments' cleanup packets are built from that state
    const QList<QJsonObject> trackSegments = m_czml.updateTrackSegments3D(mapItem, isSelected);
    PROFILER_STOP("Map CZML segments")
    PROFILER_RESTART()
    QJsonObject obj = m_czml.update(mapItem, isTarget, isSelected);
    PROFILER_STOP("Map CZML update")
    PROFILER_RESTART()
    // Nothing changed: the packet holds an id and nothing else, so there is nothing for
    // Cesium to do with it. Sending it anyway costs a websocket frame, a JSON parse and a
    // czmlStream.process() call in the page, per item - which with 200,000 static items
    // in the model was most of the traffic the 3D map ever saw.
    if (obj.size() > 1) {
        czml(obj);
    }
    for (QJsonObject segment : trackSegments) { // czml() takes a non-const reference
        czml(segment);
    }
    PROFILER_STOP("Map CZML emit")
}

void CesiumInterface::update(PolygonMapItem *mapItem)
{
    QJsonObject obj = m_czml.update(mapItem);
    if (obj.size() > 1) {   // id only means nothing to do - see update(ObjectMapItem *)
        czml(obj);
    }
}

void CesiumInterface::update(PolylineMapItem *mapItem)
{
    QJsonObject obj = m_czml.update(mapItem);
    if (obj.size() > 1) {   // id only means nothing to do - see update(ObjectMapItem *)
        czml(obj);
    }
}

void CesiumInterface::setPosition(const QGeoCoordinate& position)
{
    m_czml.setPosition(position);
}

void CesiumInterface::save(const QString& filename, const QString& dataDir)
{
    QJsonObject obj {
        {"command", "save"},
        {"filename", filename},
        {"dataDir", dataDir}
    };
    send(obj);
}
