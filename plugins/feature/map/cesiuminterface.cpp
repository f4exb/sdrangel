///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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
        {"multiplier", animation->m_multiplier}
    };
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

void CesiumInterface::setTerrain(const QString &terrain, const QString &maptilerAPIKey)
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
        {"url", url}
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

void CesiumInterface::setSunLight(bool useSunLight)
{
    QJsonObject obj {
        {"command", "setSunLight"},
        {"useSunLight", useSunLight}
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

void CesiumInterface::setAntiAliasing(const QString &antiAliasing)
{
    QJsonObject obj {
        {"command", "setAntiAliasing"},
        {"antiAliasing", antiAliasing}
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
}

void CesiumInterface::initCZML()
{
    QJsonObject obj = m_czml.init();
    czml(obj);
}

// Send and process CZML
void CesiumInterface::czml(QJsonObject &obj)
{
    obj.insert("command", "czml");
    send(obj);
}

void CesiumInterface::update(ObjectMapItem *mapItem, bool isTarget, bool isSelected)
{
    QJsonObject obj = m_czml.update(mapItem, isTarget, isSelected);
    czml(obj);
}

void CesiumInterface::update(PolygonMapItem *mapItem)
{
    QJsonObject obj = m_czml.update(mapItem);
    czml(obj);
}

void CesiumInterface::update(PolylineMapItem *mapItem)
{
    QJsonObject obj = m_czml.update(mapItem);
    czml(obj);
}

void CesiumInterface::setPosition(const QGeoCoordinate& position)
{
    m_czml.setPosition(position);
}
