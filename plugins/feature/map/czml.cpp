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

#include <QDebug>

#include "czml.h"
#include "mapsettings.h"
#include "mapmodel.h"

#include "util/coordinates.h"

const QStringList CZML::m_heightReferences = {"NONE", "CLAMP_TO_GROUND", "RELATIVE_TO_GROUND", "CLIP_TO_GROUND"};

CZML::CZML(const MapSettings *settings) :
    m_settings(settings)
{
}

// Set position from which distance filter is calculated
void CZML::setPosition(const QGeoCoordinate& position)
{
    m_position = position;
}

bool CZML::filter(const MapItem *mapItem) const
{
    return (   !mapItem->m_itemSettings->m_filterName.isEmpty()
            && !mapItem->m_itemSettings->m_filterNameRE.match(mapItem->m_name).hasMatch()
           )
        || (   (mapItem->m_itemSettings->m_filterDistance > 0)
            && (m_position.distanceTo(QGeoCoordinate(mapItem->m_latitude, mapItem->m_longitude, mapItem->m_altitude)) > mapItem->m_itemSettings->m_filterDistance)
           );
}

QJsonObject CZML::init()
{
    QString start = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString stop = QDateTime::currentDateTimeUtc().addSecs(60*60).toString(Qt::ISODate);
    QString interval = QString("%1/%2").arg(start).arg(stop);

    QJsonObject spec {
        {"interval", interval},
        {"currentTime", start},
        {"range", "UNBOUNDED"}
    };
    QJsonObject doc {
        {"id", "document"},
        {"version", "1.0"},
        {"clock", spec}
    };

    return doc;
}

QJsonObject CZML::update(PolygonMapItem *mapItem)
{
    QString id = mapItem->m_name;

    QJsonObject obj {
        {"id", id}         // id must be unique
    };

    if (   !mapItem->m_itemSettings->m_enabled
        || !mapItem->m_itemSettings->m_display3DTrack
        || filter(mapItem)
        || mapItem->m_deleted
       )
    {
        // Delete obj completely (including any history)
        obj.insert("delete", true);
        return obj;
    }

    // Need to use perPositionHeight for vertical polygons
    bool perPosition = mapItem->m_extrudedHeight == 0;

    QJsonArray positions;
    for (const auto c : mapItem->m_points)
    {
        positions.append(c->longitude());
        positions.append(c->latitude());
        positions.append(c->altitude());
    }

    QJsonObject positionList {
        {"cartographicDegrees", positions},
    };

    QColor color;
    if (mapItem->m_colorValid) {
        color = QColor::fromRgba(mapItem->m_color);
    } else {
        color = QColor::fromRgba(mapItem->m_itemSettings->m_3DTrackColor);
    }
    QJsonArray colorRGBA {
        color.red(), color.green(), color.blue(), color.alpha()
    };
    QJsonObject colorObj {
        {"rgba", colorRGBA}
    };

    QJsonObject solidColor {
        {"color", colorObj},
    };

    QJsonObject material {
        {"solidColor", solidColor}
    };

    QJsonArray outlineColorRGBA {
        0, 0, 0, 255
    };
    QJsonObject outlineColor {
        {"rgba", outlineColorRGBA}
    };

    QJsonObject polygon {
        {"positions", positionList},
        {"material", material},
        {"outline", true},
        {"outlineColor", outlineColor},
    };
    if (perPosition)
    {
        polygon.insert("perPositionHeight", true);
        if (mapItem->m_altitudeReference != 0) {
            polygon.insert("altitudeReference", m_heightReferences[mapItem->m_altitudeReference]); // Custom code in map3d.html
        }
    }
    else
    {
        polygon.insert("height", mapItem->m_altitude);
        polygon.insert("heightReference", m_heightReferences[mapItem->m_altitudeReference]);
        polygon.insert("extrudedHeight", mapItem->m_extrudedHeight);
    }

    // We need to have a position, otherwise viewer entity tracking doesn't seem to work
    QJsonArray coords {
        mapItem->m_longitude, mapItem->m_latitude, mapItem->m_altitude
    };
    QJsonObject position {
        {"cartographicDegrees", coords},
    };
    obj.insert("position", position);

    obj.insert("polygon", polygon);
    obj.insert("description", mapItem->m_label);

    //qDebug() << "Polygon " << obj;
    return obj;
}

QJsonObject CZML::update(PolylineMapItem *mapItem)
{
    QString id = mapItem->m_name;

    QJsonObject obj {
        {"id", id}         // id must be unique
    };

    if (   !mapItem->m_itemSettings->m_enabled
        || !mapItem->m_itemSettings->m_display3DTrack
        || filter(mapItem)
        || mapItem->m_deleted
       )
    {
        // Delete obj completely (including any history)
        obj.insert("delete", true);
        return obj;
    }

    QJsonArray positions;
    for (const auto c : mapItem->m_points)
    {
        positions.append(c->longitude());
        positions.append(c->latitude());
        positions.append(c->altitude());
    }

    QJsonObject positionList {
        {"cartographicDegrees", positions},
    };

    QColor color;
    if (mapItem->m_colorValid) {
        color = QColor::fromRgba(mapItem->m_color);
    } else {
        color = QColor::fromRgba(mapItem->m_itemSettings->m_3DTrackColor);
    }
    QJsonArray colorRGBA {
        color.red(), color.green(), color.blue(), color.alpha()
    };
    QJsonObject colorObj {
        {"rgba", colorRGBA}
    };

    QJsonObject solidColor {
        {"color", colorObj},
    };

    QJsonObject material {
        {"solidColor", solidColor}
    };

    QJsonObject polyline {
        {"positions", positionList},
        {"material", material}
    };
    polyline.insert("clampToGround", mapItem->m_altitudeReference == 1);
    if (mapItem->m_altitudeReference == 3) {
        polyline.insert("altitudeReference", m_heightReferences[mapItem->m_altitudeReference]); // Custom code in map3d.html
    }

    // We need to have a position, otherwise viewer entity tracking doesn't seem to work
    QJsonArray coords {
        mapItem->m_longitude, mapItem->m_latitude, mapItem->m_altitude
    };
    QJsonObject position {
        {"cartographicDegrees", coords},
    };
    obj.insert("position", position);

    obj.insert("polyline", polyline);
    obj.insert("description", mapItem->m_label);

    //qDebug() << "Polyline " << obj;
    return obj;
}

QJsonObject CZML::update(ObjectMapItem *mapItem, bool isTarget, bool isSelected)
{
    (void) isTarget;

    QString id = mapItem->m_name;

    QJsonObject obj {
        {"id", id}         // id must be unique
    };

    if (!mapItem->m_itemSettings->m_enabled || filter(mapItem))
    {
        // Delete obj completely (including any history)
        obj.insert("delete", true);
        return obj;
    }

    // Don't currently use CLIP_TO_GROUND in Cesium due to Jitter bug
    // https://github.com/CesiumGS/cesium/issues/4049
    // Instead we implement our own clipping code in map3d.html
    const QStringList heightReferences = {"NONE", "CLAMP_TO_GROUND", "RELATIVE_TO_GROUND", "NONE"};
    QString dt;

    if (mapItem->m_takenTrackDateTimes.size() > 0) {
        dt = mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs);
    } else {
        dt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    // Keep a hash of the time we first saw each item
    bool existingId = m_ids.contains(id);
    if (!existingId) {
        m_ids.insert(id, dt);
    }

    bool removeObj = false;
    bool fixedPosition = mapItem->m_fixedPosition;
    if (mapItem->m_image == "")
    {
        // Need to remove this from the map (but history is retained)
        removeObj = true;
    }

    QJsonArray coords;
    if (!removeObj)
    {
        if (!fixedPosition && (mapItem->m_predictedTrackCoords.size() > 0))
        {
            QListIterator<QGeoCoordinate *> i(mapItem->m_takenTrackCoords);
            QListIterator<QDateTime *> j(mapItem->m_takenTrackDateTimes);
            while (i.hasNext())
            {
                QGeoCoordinate *c = i.next();
                coords.append(j.next()->toString(Qt::ISODateWithMs));
                coords.append(c->longitude());
                coords.append(c->latitude());
                coords.append(c->altitude());
            }
            if (mapItem->m_predictedTrackCoords.size() > 0)
            {
                QListIterator<QGeoCoordinate *> k(mapItem->m_predictedTrackCoords);
                QListIterator<QDateTime *> l(mapItem->m_predictedTrackDateTimes);
                k.toBack();
                l.toBack();
                while (k.hasPrevious())
                {
                    QGeoCoordinate *c = k.previous();
                    coords.append(l.previous()->toString(Qt::ISODateWithMs));
                    coords.append(c->longitude());
                    coords.append(c->latitude());
                    coords.append(c->altitude());
                }
            }
        }
        else
        {
            // Only send latest position, to reduce processing
            if (!fixedPosition && mapItem->m_positionDateTime.isValid()) {
                coords.push_back(mapItem->m_positionDateTime.toString(Qt::ISODateWithMs));
            }
            coords.push_back(mapItem->m_longitude);
            coords.push_back(mapItem->m_latitude);
            coords.push_back(mapItem->m_altitude);
        }
    }
    else
    {
        coords = m_lastPosition.value(id);
    }
    QJsonObject position {
        {"cartographicDegrees", coords},
    };
    if (!fixedPosition)
    {
        // Don't use forward extrapolation for satellites (with predicted tracks), as
        // it seems to jump about. We use it for AIS and ADS-B that don't have predicted tracks
        if (mapItem->m_predictedTrackCoords.size() == 0)
        {
            // Need 2 different positions to enable extrapolation, otherwise entity may not appear
            bool hasMoved = m_hasMoved.contains(id);
            if (!hasMoved && m_lastPosition.contains(id) && (m_lastPosition.value(id) != coords))
            {
                hasMoved = true;
                m_hasMoved.insert(id, true);
            }
            if (hasMoved && (mapItem->m_itemSettings->m_extrapolate > 0))
            {
                position.insert("forwardExtrapolationType", "EXTRAPOLATE");
                position.insert("forwardExtrapolationDuration", mapItem->m_itemSettings->m_extrapolate);
                // Use linear interpolation for now - other two can go crazy with aircraft on the ground
                //position.insert("interpolationAlgorithm", "HERMITE");
                //position.insert("interpolationDegree", "2");
                //position.insert("interpolationAlgorithm", "LAGRANGE");
                //position.insert("interpolationDegree", "5");
            }
            else
            {
                position.insert("forwardExtrapolationType", "HOLD");
            }
        }
        else
        {
            // Interpolation goes wrong at end points
            //position.insert("interpolationAlgorithm", "LAGRANGE");
            //position.insert("interpolationDegree", "5");
            //position.insert("interpolationAlgorithm", "HERMITE");
            //position.insert("interpolationDegree", "2");
        }
    }

    QQuaternion q = Coordinates::orientation(mapItem->m_longitude, mapItem->m_latitude, mapItem->m_altitude,
                                             mapItem->m_heading, mapItem->m_pitch, mapItem->m_roll);
    QJsonArray quaternion;
    if (!fixedPosition && mapItem->m_orientationDateTime.isValid()) {
        quaternion.push_back(mapItem->m_orientationDateTime.toString(Qt::ISODateWithMs));
    }
    quaternion.push_back(q.x());
    quaternion.push_back(q.y());
    quaternion.push_back(q.z());
    quaternion.push_back(q.scalar());

    QJsonObject orientation {
        {"unitQuaternion", quaternion},
        {"forwardExtrapolationType", "HOLD"}, // If we extrapolate, aircraft tend to spin around
        {"forwardExtrapolationDuration", mapItem->m_itemSettings->m_extrapolate},
       // {"interpolationAlgorithm", "LAGRANGE"}
    };
    QJsonObject orientationPosition {
        {"velocityReference", "#position"},
    };
    QJsonObject noPosition {
        {"cartographicDegrees", coords},
        {"forwardExtrapolationType", "NONE"}
    };

    // Point
    QColor pointColor = QColor::fromRgba(mapItem->m_itemSettings->m_3DPointColor);
    QJsonArray pointRGBA {
        pointColor.red(), pointColor.green(), pointColor.blue(), pointColor.alpha()
    };
    QJsonObject pointColorObj {
        {"rgba", pointRGBA}
    };
    QJsonObject point {
        {"pixelSize", 8},
        {"color", pointColorObj},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"show", mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DPoint}
    };

    // Model
    QJsonArray node0Cartesian {
        {0.0, mapItem->m_modelAltitudeOffset, 0.0}
    };
    QJsonObject node0Translation {
        {"cartesian", node0Cartesian}
    };
    QJsonObject node0Transform {
        {"translation", node0Translation}
    };
    QJsonObject nodeTransforms {
        {"node0", node0Transform},
    };
    QJsonObject model {
        {"gltf", m_settings->m_modelURL + mapItem->m_model},
        {"incrementallyLoadTextures", false}, // Aircraft will flash as they appear without textures if this is the default of true
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"runAnimations", false},
        {"show", mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DModel},
        {"minimumPixelSize", mapItem->m_itemSettings->m_3DModelMinPixelSize},
        {"maximumScale", 20000} // Stop it getting too big when zoomed really far out
    };
    if (mapItem->m_modelAltitudeOffset != 0.0) {
        model.insert("nodeTransformations", nodeTransforms);
    }

    // Path
    QColor pathColor = QColor::fromRgba(mapItem->m_itemSettings->m_3DTrackColor);
    QJsonArray pathColorRGBA {
        pathColor.red(), pathColor.green(), pathColor.blue(), pathColor.alpha()
    };
    QJsonObject pathColorObj {
        {"rgba", pathColorRGBA}
    };
    // Paths can't be clamped to ground, so AIS paths can be underground if terrain is used
    // See: https://github.com/CesiumGS/cesium/issues/7133
    QJsonObject pathSolidColorMaterial {
        {"color", pathColorObj}
    };
    QJsonObject pathMaterial {
        {"solidColor", pathSolidColorMaterial}
    };
    bool showPath = mapItem->m_itemSettings->m_enabled
                        && mapItem->m_itemSettings->m_display3DTrack
                        && (    m_settings->m_displayAllGroundTracks
                            || (m_settings->m_displaySelectedGroundTracks && isSelected));
    QJsonObject path {
        // We want full paths for sat tracker, so leadTime and trailTime should be 0
        // Should be configurable.. 6000=100mins ~> 1 orbit for LEO
        //{"leadTime", "6000"},
        //{"trailTime", "6000"},
        {"width", "3"},
        {"material", pathMaterial},
        {"show", showPath}
    };

    // Label

    // Prevent labels from being too cluttered when zoomed out
    // FIXME: These values should come from mapItem or mapItemSettings
    float displayDistanceMax = std::numeric_limits<float>::max();
    if ((mapItem->m_group == "Beacons")
        || (mapItem->m_group == "AM") || (mapItem->m_group == "FM") || (mapItem->m_group == "DAB")
        || (mapItem->m_group == "NavAid")
       ) {
        displayDistanceMax = 1000000;
    } else if ((mapItem->m_group == "Station") || (mapItem->m_group == "Radar") || (mapItem->m_group == "Radio Time Transmitters")) {
        displayDistanceMax = 10000000;
    } else if (mapItem->m_group == "Ionosonde Stations") {
        displayDistanceMax = 30000000;
    }

    QJsonArray labelPixelOffsetScaleArray {
        1000000, 20, 10000000, 5
    };
    QJsonObject labelPixelOffsetScaleObject {
        {"nearFarScalar", labelPixelOffsetScaleArray}
    };
    QJsonArray labelPixelOffsetArray {
        1, 0
    };
    QJsonObject labelPixelOffset {
        {"cartesian2", labelPixelOffsetArray}
    };
    QJsonArray labelEyeOffsetArray {
        0, mapItem->m_labelAltitudeOffset, 0     // Position above the object, dependent on the height of the model
    };
    QJsonObject labelEyeOffset {
        {"cartesian", labelEyeOffsetArray}
    };
    QJsonObject labelHorizontalOrigin  {
        {"horizontalOrigin", "LEFT"}
    };
    QJsonArray labelDisplayDistance {
        0, displayDistanceMax
    };
    QJsonObject labelDistanceDisplayCondition {
        {"distanceDisplayCondition", labelDisplayDistance}
    };
    QJsonObject label {
        {"text", mapItem->m_label},
        {"show", m_settings->m_displayNames && mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DLabel},
        {"scale", mapItem->m_itemSettings->m_3DLabelScale},
        {"pixelOffset", labelPixelOffset},
        {"pixelOffsetScaleByDistance", labelPixelOffsetScaleObject},
        {"eyeOffset", labelEyeOffset},
        {"verticalOrigin",  "BASELINE"},
        {"horizontalOrigin",  "LEFT"},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
    };
    if (displayDistanceMax != std::numeric_limits<float>::max()) {
        label.insert("distanceDisplayCondition", labelDistanceDisplayCondition);
    }

    // Use billboard for APRS as we don't currently have 3D objects
    QString imageURL = mapItem->m_image;
    if (imageURL.startsWith("qrc://")) {
        imageURL = imageURL.mid(6); // Redirect to our embedded webserver, which will check resources
    }
    QJsonObject billboard {
        {"image", imageURL},
        {"heightReference", heightReferences[mapItem->m_altitudeReference]},
        {"verticalOrigin", "BOTTOM"} // To stop it being cut in half when zoomed out
    };

    if (!removeObj)
    {
        obj.insert("position", position);
        if (!fixedPosition)
        {
            if (mapItem->m_useHeadingPitchRoll) {
                obj.insert("orientation", orientation);
            } else {
                obj.insert("orientation", orientationPosition);
            }
        }
        obj.insert("point", point);
        if (!mapItem->m_model.isEmpty()) {
            obj.insert("model", model);
        } else {
            obj.insert("billboard", billboard);
        }
        obj.insert("label", label);
        obj.insert("description", mapItem->m_text);
        if (!fixedPosition) {
            obj.insert("path", path);
        }

        if (!fixedPosition)
        {
            if (mapItem->m_takenTrackDateTimes.size() > 0 && mapItem->m_predictedTrackDateTimes.size() > 0)
            {
                QString availability = QString("%1/%2")
                                        .arg(mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs))
                                        .arg(mapItem->m_predictedTrackDateTimes.last()->toString(Qt::ISODateWithMs));
                obj.insert("availability", availability);
            }
            else
            {
                if (mapItem->m_availableUntil.isValid())
                {
                    QString period = QString("%1/%2").arg(m_ids[id]).arg(mapItem->m_availableUntil.toString(Qt::ISODateWithMs));
                    obj.insert("availability", period);
                }
            }
        }
        m_lastPosition.insert(id, coords);
    }
    else
    {
        // Disable forward extrapolation
        obj.insert("position", noPosition);
    }

    // Use our own clipping routine, due to
    // https://github.com/CesiumGS/cesium/issues/4049
    if (mapItem->m_altitudeReference == 3) {
        obj.insert("altitudeReference", "CLIP_TO_GROUND");
    }

    //qDebug() << obj;

    return obj;
}

