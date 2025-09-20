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

#include <QDebug>

#include "czml.h"
#include "mapsettings.h"
#include "mapmodel.h"

#include "util/coordinates.h"

// FIXME: Cesium now has some additional options: CLAMP_TO_TERRAIN, RELATIVE_TO_TERRAIN, CLAMP_TO_3D_TILE, RELATIVE_TO_3D_TILE
// CLIP_TO_GROUND is our own addition
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
    // Start a few seconds in the past, to allow for data to be received
    QString start = QDateTime::currentDateTimeUtc().addSecs(-4).toString(Qt::ISODate);
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

static void insertConstantProperty(QJsonObject& properties, const QString& name, const QString& value)
{
    properties.insert(name, value);
}

static void insertProperty(QJsonObject& properties, const QString& name, const QString& dateTime, float value)
{
    if (!std::isnan(value))
    {
        QJsonArray ar;
        ar.push_back(dateTime);
        ar.push_back(value);
        QJsonObject obj {
            {"number", ar},
            {"backwardExtrapolationType", "HOLD"},
            {"backwardExtrapolationDuration", 30},
            {"forwardExtrapolationType", "HOLD"},
            {"forwardExtrapolationDuration", 30},
        };
        properties.insert(name, obj);
    }
}

// SampledProperties are interpolated
// Need to use intervals to avoid interpolation
// See: https://sandcastle.cesium.com/?src=CZML%20Custom%20Properties.html&label=All
static void insertProperty0(QJsonObject& properties, const QString& name, const QString& dateTime, float value)
{
    if (!std::isnan(value))
    {
        QJsonObject obj {
            {"interval", dateTime + "/3000"}, // Year 3000
            {"number", value}
        };

        QJsonArray array {
            obj
        };
        properties.insert(name, array);
    }
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

    if (mapItem->m_availableFrom.isValid()) {
        dt = mapItem->m_availableFrom.toString(Qt::ISODateWithMs);
    } else if (mapItem->m_takenTrackDateTimes.size() > 0) {
        dt = mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs);
    } else {
        dt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    // Keep a hash of the time we first saw each item
    bool existingId = m_ids.contains(id);
    State& state = m_ids[id];
    if (!existingId) {
        state.m_firstSeenDateTime = dt;
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
            bool useDateTime = !fixedPosition && mapItem->m_positionDateTime.isValid();

            // Update positions that have been recalculated with interpolation
            if (!mapItem->m_interpolatedCoords.isEmpty())
            {
                for (int i = 0; i < mapItem->m_interpolatedCoords.size(); i++)
                {
                    if (useDateTime) {
                        coords.push_back(mapItem->m_interpolatedDateTimes[i]->toString(Qt::ISODateWithMs));
                    }
                    coords.push_back(mapItem->m_interpolatedCoords[i]->longitude());
                    coords.push_back(mapItem->m_interpolatedCoords[i]->latitude());
                    coords.push_back(mapItem->m_interpolatedCoords[i]->altitude());
                }
                mapItem->m_interpolatedCoords.clear();
                mapItem->m_interpolatedDateTimes.clear();
            }
            else
            {
                // Only send latest position, to reduce processing
                if (!mapItem->m_takenTrackPositionExtrapolated.isEmpty() && mapItem->m_takenTrackPositionExtrapolated.back())
                {
                    if (useDateTime) {
                        coords.push_back(mapItem->m_takenTrackDateTimes.back()->toString(Qt::ISODateWithMs));
                    }
                    coords.push_back(mapItem->m_takenTrackCoords.back()->longitude());
                    coords.push_back(mapItem->m_takenTrackCoords.back()->latitude());
                }
                else
                {
                    if (useDateTime) {
                        coords.push_back(mapItem->m_positionDateTime.toString(Qt::ISODateWithMs));
                    }
                    coords.push_back(mapItem->m_longitude);
                    coords.push_back(mapItem->m_latitude);
                }
                if (!mapItem->m_takenTrackAltitudeExtrapolated.isEmpty() && mapItem->m_takenTrackAltitudeExtrapolated.back()) {
                    coords.push_back(mapItem->m_takenTrackCoords.back()->altitude());
                } else {
                    coords.push_back(mapItem->m_altitude + mapItem->m_modelAltitudeOffset); // See nodeTransformations comment below, as to why we use this here
                }
            }
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
                //position.insert("forwardExtrapolationType", "LINEAR_EXTRAPOLATE"); // LAGRANGE is poor for extrapolation
                //position.insert("forwardExtrapolationType", "HOLD"); // Keeps at last position
                position.insert("forwardExtrapolationDuration", mapItem->m_itemSettings->m_extrapolate);
                // To calc acceleration, we need to use non-linear interpolation.
                position.insert("interpolationAlgorithm", "LINEAR");
                position.insert("interpolationDegree", 2);
                //position.insert("interpolationAlgorithm", "HERMITE");
                //position.insert("interpolationDegree", 2);
                //position.insert("interpolationAlgorithm", "LAGRANGE");
                //position.insert("interpolationDegree", 5); // crazy interpolation for LAGRANGE
            }
            else
            {
                position.insert("forwardExtrapolationType", "HOLD");
            }
        }
        else
        {
            // Interpolation goes wrong at end points. FIXME: Check if still true
            //position.insert("interpolationAlgorithm", "LAGRANGE");
            //position.insert("interpolationDegree", 5);
            //position.insert("interpolationAlgorithm", "HERMITE");
            //position.insert("interpolationDegree", 2);
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

    if (!removeObj)
    {
        if (mapItem->m_aircraftState)
        {
            QJsonObject properties;
            QDateTime aircraftStateDateTime = mapItem->m_positionDateTime;
            QString dateTime = aircraftStateDateTime.toString(Qt::ISODateWithMs);
            bool aircraftStateDateTimeChanged = aircraftStateDateTime != state.m_aircraftStateDateTime;
            QString aircraftCallsign = mapItem->m_aircraftState->m_callsign;
            QString aircraftType = mapItem->m_aircraftState->m_aircraftType;
            QString aircraftIndicatedAirspeedDateTime = mapItem->m_aircraftState->m_indicatedAirspeedDateTime;
            float aircraftIndicatedAirspeed = mapItem->m_aircraftState->m_indicatedAirspeed;
            float aircraftTrueAirspeed = mapItem->m_aircraftState->m_trueAirspeed;
            float aircraftGroundspeed = mapItem->m_aircraftState->m_groundspeed;
            QString aircraftAltitudeDateTime = mapItem->m_aircraftState->m_altitudeDateTime;
            float aircraftAltitude = mapItem->m_aircraftState->m_altitude;
            int aircraftOnSurface = mapItem->m_aircraftState->m_onSurface;
            float aircraftMach = mapItem->m_aircraftState->m_mach;
            float aircraftQNH = mapItem->m_aircraftState->m_qnh;
            float aircraftVerticalSpeed = mapItem->m_aircraftState->m_verticalSpeed;
            float aircraftHeading = mapItem->m_aircraftState->m_heading;
            float aircraftTrack = mapItem->m_aircraftState->m_track;
            float aircraftRoll = mapItem->m_roll;
            float aircraftSelectedAltitude = mapItem->m_aircraftState->m_selectedAltitude;
            float aircraftSelectedHeading = mapItem->m_aircraftState->m_selectedHeading;
            int aircraftAutopilot = mapItem->m_aircraftState->m_autopilot;
            MapAircraftState::VerticalMode aircraftVerticalMode = mapItem->m_aircraftState->m_verticalMode;
            MapAircraftState::LateralMode aircraftLateralMode = mapItem->m_aircraftState->m_lateralMode;
            MapAircraftState::TCASMode aircraftTCASMode = mapItem->m_aircraftState->m_tcasMode;
            float aircraftWindSpeed = mapItem->m_aircraftState->m_windSpeed;
            float aircraftWindDirection = mapItem->m_aircraftState->m_windDirection;
            float aircraftStaticAirTemperature = mapItem->m_aircraftState->m_staticAirTemperature;

            if (   (!existingId && !aircraftCallsign.isEmpty())
                || (aircraftCallsign != state.m_aircraftState.m_callsign)
                )
            {
                insertConstantProperty(properties, "pfdCallsign", aircraftCallsign);
                state.m_aircraftState.m_callsign = aircraftCallsign;
            }
            if (   (!existingId && !aircraftType.isEmpty())
                || (aircraftType != state.m_aircraftState.m_aircraftType)
                )
            {
                insertConstantProperty(properties, "pfdAircraftType", aircraftType);
                state.m_aircraftState.m_aircraftType = aircraftType;
            }
            if (   (!existingId && !aircraftIndicatedAirspeedDateTime.isEmpty())
                || (aircraftIndicatedAirspeedDateTime != state.m_aircraftState.m_indicatedAirspeedDateTime)
                || (aircraftIndicatedAirspeed != state.m_aircraftState.m_indicatedAirspeed)
                )
            {
                insertProperty(properties, "pfdIndicatedAirspeed", aircraftIndicatedAirspeedDateTime, aircraftIndicatedAirspeed);
                state.m_aircraftState.m_indicatedAirspeedDateTime = aircraftIndicatedAirspeedDateTime;
                state.m_aircraftState.m_indicatedAirspeed = aircraftIndicatedAirspeed;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftTrueAirspeed != state.m_aircraftState.m_trueAirspeed)
                )
            {
                insertProperty(properties, "pfdTrueAirspeed", dateTime, aircraftTrueAirspeed);
                state.m_aircraftState.m_trueAirspeed = aircraftTrueAirspeed;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftGroundspeed != state.m_aircraftState.m_groundspeed)
                )
            {
                insertProperty(properties, "pfdGroundspeed", dateTime, aircraftGroundspeed);
                state.m_aircraftState.m_groundspeed = aircraftGroundspeed;
            }
            if (   (!existingId && !aircraftAltitudeDateTime.isEmpty())
                || (aircraftAltitudeDateTime != state.m_aircraftState.m_altitudeDateTime)
                || (aircraftAltitude != state.m_aircraftState.m_altitude)
                )
            {
                insertProperty(properties, "pfdAltitude", aircraftAltitudeDateTime, aircraftAltitude);
                state.m_aircraftState.m_altitudeDateTime = aircraftAltitudeDateTime;
                state.m_aircraftState.m_altitude = aircraftAltitude;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftOnSurface != state.m_aircraftState.m_onSurface)
                )
            {
                insertProperty0(properties, "pfdOnSurface", dateTime, aircraftOnSurface);
                state.m_aircraftState.m_onSurface = aircraftOnSurface;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftMach != state.m_aircraftState.m_mach)
                )
            {
                insertProperty(properties, "pfdMach", dateTime, aircraftMach);
                state.m_aircraftState.m_mach = aircraftMach;

            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftQNH != state.m_aircraftState.m_qnh)
                )
            {
                insertProperty0(properties, "pfdQNH", dateTime, aircraftQNH);
                state.m_aircraftState.m_qnh = aircraftQNH;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftVerticalSpeed != state.m_aircraftState.m_verticalSpeed)
                )
            {
                insertProperty(properties, "pfdVerticalSpeed", dateTime, aircraftVerticalSpeed);
                state.m_aircraftState.m_verticalSpeed = aircraftVerticalSpeed;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftHeading != state.m_aircraftState.m_heading)
                )
            {
                insertProperty(properties, "pfdHeading", dateTime, aircraftHeading);
                state.m_aircraftState.m_heading = aircraftHeading;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftTrack != state.m_aircraftState.m_track)
                )
            {
                insertProperty(properties, "pfdTrack", dateTime, aircraftTrack);
                state.m_aircraftState.m_track = aircraftTrack;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftRoll != state.m_aircraftRoll)
                )
            {
                insertProperty(properties, "pfdRoll", dateTime, aircraftRoll);
                state.m_aircraftRoll = aircraftRoll;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftSelectedAltitude != state.m_aircraftState.m_selectedAltitude)
                )
            {
                insertProperty0(properties, "pfdSelectedAltitude", dateTime, aircraftSelectedAltitude);
                state.m_aircraftState.m_selectedAltitude = aircraftSelectedAltitude;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftSelectedHeading != state.m_aircraftState.m_selectedHeading)
                )
            {
                insertProperty0(properties, "pfdSelectedHeading", dateTime, aircraftSelectedHeading);
                state.m_aircraftState.m_selectedHeading = aircraftSelectedHeading;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftAutopilot != state.m_aircraftState.m_autopilot)
                )
            {
                insertProperty0(properties, "pfdAutopilot", dateTime, aircraftAutopilot);
                state.m_aircraftState.m_autopilot = aircraftAutopilot;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftVerticalMode != state.m_aircraftState.m_verticalMode)
                )
            {
                insertProperty0(properties, "pfdVerticalMode", dateTime, aircraftVerticalMode);
                state.m_aircraftState.m_verticalMode = aircraftVerticalMode;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftLateralMode != state.m_aircraftState.m_lateralMode)
                )
            {
                insertProperty0(properties, "pfdLateralMode", dateTime, aircraftLateralMode);
                state.m_aircraftState.m_lateralMode = aircraftLateralMode;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftTCASMode != state.m_aircraftState.m_tcasMode)
                )
            {
                insertProperty0(properties, "pfdTCASMode", dateTime, aircraftTCASMode);
                state.m_aircraftState.m_tcasMode = aircraftTCASMode;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftWindSpeed != state.m_aircraftState.m_windSpeed)
                )
            {
                insertProperty0(properties, "pfdWindSpeed", dateTime, aircraftWindSpeed);
                state.m_aircraftState.m_windSpeed = aircraftWindSpeed;
            }
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftWindDirection != state.m_aircraftState.m_windDirection)
                )
            {
                insertProperty0(properties, "pfdWindDirection", dateTime, aircraftWindDirection);
                state.m_aircraftState.m_windDirection = aircraftWindDirection;
            }

            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftStaticAirTemperature != state.m_aircraftState.m_staticAirTemperature)
                )
            {
                insertProperty0(properties, "pfdStaticAirTemperature", dateTime, aircraftStaticAirTemperature);
                state.m_aircraftState.m_staticAirTemperature = aircraftStaticAirTemperature;
            }

            //QJsonObject speedProperty {
            //    {"velocityReference", "#position"},
            //};
            //properties.insert("pfdSpeed", speedProperty);

            if (   !existingId
                || aircraftStateDateTimeChanged
                )
            {
                state.m_aircraftStateDateTime = aircraftStateDateTime;
            }

            if (properties.size() > 0) {
                obj.insert("properties", properties);
            }
        }

        obj.insert("position", position);
        if (!fixedPosition)
        {
            if (mapItem->m_useHeadingPitchRoll) {
                obj.insert("orientation", orientation);
            } else {
                obj.insert("orientation", orientationPosition);
            }
        }

        // Point
        quint32 pointColorInt = mapItem->m_itemSettings->m_3DPointColor;
        int pointAltitudeReference = mapItem->m_altitudeReference;
        bool pointShow = mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DPoint;

        if (   !existingId
            || (pointColorInt != state.m_pointColorInt)
            || (pointAltitudeReference != state.m_pointAltitudeReference)
            || (pointShow != state.m_pointShow)
            )
        {
            QColor pointColor = QColor::fromRgba(pointColorInt);
            QJsonArray pointRGBA {
                pointColor.red(), pointColor.green(), pointColor.blue(), pointColor.alpha()
            };
            QJsonObject pointColorObj {
                {"rgba", pointRGBA}
            };
            QJsonObject point {
                {"pixelSize", 8},
                {"color", pointColorObj},
                {"heightReference", heightReferences[pointAltitudeReference]},
                {"show", pointShow}
            };
            obj.insert("point", point);

            state.m_pointColorInt = pointColorInt;
            state.m_pointAltitudeReference = pointAltitudeReference;
            state.m_pointShow = pointShow;
        }

        // Label

        float labelAltitudeOffset = mapItem->m_labelAltitudeOffset;
        QString labelText = mapItem->m_label;
        bool labelShow = m_settings->m_displayNames && mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DLabel;
        float labelScale = mapItem->m_itemSettings->m_3DLabelScale;
        int labelAltitudeReference = mapItem->m_altitudeReference;

        if (   !existingId
            || (labelAltitudeOffset != state.m_lableAltitudeOffset)
            || (labelText != state.m_labelText)
            || (labelShow != state.m_labelShow)
            || (labelScale != state.m_labelScale)
            || (labelAltitudeReference != state.m_labelAltitudeReference)
            )
        {
            // Prevent labels from being too cluttered when zoomed out
            // FIXME: These values should come from mapItem or mapItemSettings
            float displayDistanceMax = std::numeric_limits<float>::max();
            if ((mapItem->m_group == "Beacons")
                || (mapItem->m_group == "AM") || (mapItem->m_group == "FM") || (mapItem->m_group == "DAB")
                || (mapItem->m_group == "NavAid")
                || (mapItem->m_group == "Waypoints")
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
                0, labelAltitudeOffset, 0     // Position above the object, dependent on the height of the model
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

            labelText.replace("<br>", "\n");
            QJsonObject label {
                //{"text", labelText},
                {"show", labelShow},
                {"scale", labelScale},
                {"pixelOffset", labelPixelOffset},
                {"pixelOffsetScaleByDistance", labelPixelOffsetScaleObject},
                {"eyeOffset", labelEyeOffset},
                {"verticalOrigin",  "BASELINE"},
                {"horizontalOrigin",  "LEFT"},
                {"heightReference", heightReferences[labelAltitudeReference]},
                {"style", "FILL_AND_OUTLINE"},
            };
            if (!mapItem->m_labelDateTime.isValid())
            {
                label.insert("text", labelText);
            }
            else
            {
                QString interval = mapItem->m_labelDateTime.toString(Qt::ISODateWithMs) + "/2999-12-31";
                QJsonObject labelInterval {
                    {"interval", interval},
                    {"string", labelText}
                };
                QJsonArray labelIntervalArray {
                    labelInterval
                };
                label.insert("text", labelIntervalArray);
            }
            if (displayDistanceMax != std::numeric_limits<float>::max()) {
                label.insert("distanceDisplayCondition", labelDistanceDisplayCondition);
            }

            obj.insert("label", label);

            state.m_lableAltitudeOffset = labelAltitudeOffset;
            state.m_labelText = labelText;
            state.m_labelShow = labelShow;
            state.m_labelScale = labelScale;
            state.m_labelAltitudeReference = labelAltitudeReference;
        }

        if (!mapItem->m_model.isEmpty())
        {
            // Model
            QString modelGLTF = m_settings->m_modelURL + mapItem->m_model;
            int modelAltitudeReference = mapItem->m_altitudeReference;
            bool modelShow = mapItem->m_itemSettings->m_enabled && mapItem->m_itemSettings->m_display3DModel;
            int modelMinimumPixelSize = mapItem->m_itemSettings->m_3DModelMinPixelSize;

            if (   !existingId
                || (modelGLTF != state.m_modelGLTF)
                || (modelAltitudeReference != state.m_modelAltitudeReference)
                || (modelShow != state.m_modelShow)
                || (modelMinimumPixelSize != state.m_modelMinimumPixelSize)
               )
            {
                QJsonObject model {
                    {"gltf", modelGLTF},
                    //{"incrementallyLoadTextures", false}, // Aircraft will flash as they appear without textures if this is the default of true
                    {"heightReference", heightReferences[modelAltitudeReference]},
                    {"runAnimations", false},
                    {"show", modelShow},
                    {"minimumPixelSize", modelMinimumPixelSize},
                    {"maximumScale", 20000} // Stop it getting too big when zoomed really far out
                };
                // Using nodeTransformations stops animations from running.
                // See: https://github.com/CesiumGS/cesium/issues/11566
                /*QJsonArray node0Cartesian {
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
                if (mapItem->m_modelAltitudeOffset != 0.0) {
                model.insert("nodeTransformations", nodeTransforms);
                }*/

                obj.insert("model", model);

                state.m_modelGLTF = modelGLTF;
                state.m_modelAltitudeReference = modelAltitudeReference;
                state.m_modelShow = modelShow;
                state.m_modelMinimumPixelSize = modelMinimumPixelSize;
            }
        }
        else
        {
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

            obj.insert("billboard", billboard);
        }

        // Description
        QString description = mapItem->m_text;
        if (   !existingId
            || (description != state.m_description)
            )
        {
            obj.insert("description", description);
            state.m_description = description;
        }

        // Path
        if (!fixedPosition)
        {
            quint32 pathColorInt = mapItem->m_itemSettings->m_3DTrackColor;
            bool pathShow =    mapItem->m_itemSettings->m_enabled
                            && mapItem->m_itemSettings->m_display3DTrack
                            && (    m_settings->m_displayAllGroundTracks
                                || (m_settings->m_displaySelectedGroundTracks && isSelected));

            if (   !existingId
                || (pathColorInt != state.m_pathColorInt)
                || (pathShow != state.m_pathShow)
               )
            {
                QColor pathColor = QColor::fromRgba(pathColorInt);
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
                QJsonObject path {
                    // We want full paths for sat tracker, so leadTime and trailTime should be 0
                    // Should be configurable.. 6000=100mins ~> 1 orbit for LEO
                    //{"leadTime", "6000"},
                    //{"trailTime", "6000"},
                    {"width", "3"},
                    {"material", pathMaterial},
                    {"show", pathShow},
                    {"resolution", 1}
                };

                obj.insert("path", path);

                state.m_pathColorInt = pathColorInt;
                state.m_pathShow = pathShow;
            }
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
                    QString period = QString("%1/%2").arg(state.m_firstSeenDateTime).arg(mapItem->m_availableUntil.toString(Qt::ISODateWithMs));
                    obj.insert("availability", period);
                }
            }
        }
        else
        {
            if (mapItem->m_availableUntil.isValid())
            {
                QString period = QString("%1/%2").arg(state.m_firstSeenDateTime).arg(mapItem->m_availableUntil.toString(Qt::ISODateWithMs));
                obj.insert("availability", period);
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
    // https://github.com/CesiumGS/cesium/issues/4049 - Has now been fixed
    //if (mapItem->m_altitudeReference == 3) {
    //    obj.insert("altitudeReference", "CLIP_TO_GROUND");
    //}

    //qDebug() << obj;

    /*if (id == "400b00")
    {
        QJsonDocument doc(obj);
        if (!m_file.isOpen())
        {
            m_file.setFileName(QString("%1.czml").arg(id));
            m_file.open(QIODeviceBase::WriteOnly);
        }
        m_file.write(doc.toJson());
        m_file.write(",\n");
    }*/

    return obj;
}
