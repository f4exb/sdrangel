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

#include <cmath>

#include <QDebug>
#include <QJsonDocument>

#include "czml.h"
#include "mapsettings.h"
#include "mapmodel.h"

#include "util/aircraft3dmodels.h"
#include "util/coordinates.h"
#include "util/flightpathpalette.h"
#include "util/units.h"

// FIXME: Cesium now has some additional options: CLAMP_TO_TERRAIN, RELATIVE_TO_TERRAIN, CLAMP_TO_3D_TILE, RELATIVE_TO_3D_TILE
// CLIP_TO_GROUND is our own addition
const QStringList CZML::m_heightReferences = {"NONE", "CLAMP_TO_GROUND", "RELATIVE_TO_GROUND", "CLIP_TO_GROUND"};

// Using CLAMP_TO_GROUND on a label, billboard or point, results in all of them being visible through the globe.
// So use CLIP_TO_GROUND instead. 
// A single CLAMP_TO_GROUND label, billboard or point latches _shaderDisableDepthDistance
// on the Cesium collection it lands in, which recompiles every item in that collection
// onto Cesium's manual globe-depth path. That path discards nothing beyond
// coarseDepthTestDistance (~635 km), so at map viewing distances labels and images on the
// far side of the Earth draw straight through the globe - including unclamped ones that
// merely share the collection - and the latch is never cleared once set. CLIP_TO_GROUND
// keeps them above the surface without tripping it, as Cesium's isHeightReferenceClamp
// covers only the CLAMP_* references. Points come too: Cesium draws a clamped point as a
// billboard, in the same collection as the images, so leaving them would keep it latched
static int clipRatherThanClamp(int altitudeReference)
{
    return altitudeReference == 1 ? 3 : altitudeReference;   // CLAMP_TO_GROUND -> CLIP_TO_GROUND
}


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

// Cesium rejects a null inside a coordinate array with a DeveloperError that stops the
// whole scene, and Qt writes null for any non-finite double - so one NaN from one sender
// takes the 3D map down rather than misplacing one item. A QGeoCoordinate built from the
// two argument constructor has altitude qQNaN() while still reporting isValid(), which
// makes this easy to introduce and invisible until Cesium sees it. Substituting zero
// puts the item somewhere wrong, which is recoverable and obvious; a null is not.
static double finiteOrZero(double value)
{
    if (!qIsFinite(value))
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            qWarning() << "CZML: a non-finite coordinate was substituted with zero -"
                       << "some map item is sending NaN or infinity";
        }
        return 0.0;
    }
    return value;
}

void CZML::clear()
{
    m_ids.clear();
    m_lastPosition.clear();
    m_hasMoved.clear();
}

QJsonObject CZML::init()
{
    // Start a few seconds in the past, to allow for data to be received.
    //
    // Only the interval is read: the page creates its viewer with
    // automaticallyTrackDataSourceClocks false, so the currentTime below is discarded and
    // map3d.html gives the clock the same lag as its STARTING value instead. Left here
    // because the interval has to begin at or before that time, and because a document
    // packet without a clock would be odd - but do not change this expecting the map's
    // clock to follow
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

// A delete is only worth sending for an entity Cesium has actually been told about, and
// only once. Everything else was the great majority of the CZML traffic: in a measured run
// 231,680 of 231,779 deletes were for entities the page had never had - every airport in
// the database, filtered out by distance, deleted on first sight - and every airspace was
// deleted afresh on each settings pass. 91% of all packets.
//
// When a delete is sent, everything cached about the entity is reset, because Cesium is
// being told to throw it away entirely - history and all. Kept, it makes the next
// description a difference against a state Cesium no longer holds, and every unchanged
// property is then left out of a packet describing an entity that no longer has any of
// them. The position is the one that bites: it is suppressed when it has not moved, so a
// waypoint deleted and re-added in the same place - which is what selecting away from a
// flight and back does - came back as a billboard with nowhere to be drawn, and never
// appeared. Clearing m_seen alone was not enough; that only governs the early-out, while
// the rest of the packet is built against existingId, which is what m_firstSeenDateTime
// carries - so it is the whole State that goes.
bool CZML::deleteNeeded(const QString& id)
{
    auto it = m_ids.find(id);

    // Never described (an entry may exist without one: the track segments create them),
    // or already deleted - which also clears m_firstSeenDateTime, so that check covers both
    if ((it == m_ids.end()) || it->m_deleted || it->m_firstSeenDateTime.isEmpty()) {
        return false;
    }
    *it = State();
    it->m_deleted = true;
    m_lastPosition.remove(id);
    m_hasMoved.remove(id);
    return true;
}

void CZML::described(const QString& id)
{
    State& state = m_ids[id];

    if (state.m_firstSeenDateTime.isEmpty()) {
        state.m_firstSeenDateTime = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }
    state.m_deleted = false;    // visible again, so a later filter must re-delete it
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
        // Delete obj completely (including any history) - if Cesium has it
        if (deleteNeeded(id)) {
            obj.insert("delete", true);
        }
        return obj;         // id only otherwise, so nothing is sent
    }
    described(id);

    // Need to use perPositionHeight for vertical polygons
    bool perPosition = mapItem->m_extrudedHeight == 0;
    // A flat polygon clamped to the ground is draped over it instead: terrain, 3D tiles
    // and all. Given per position heights, Cesium neither subdivides a polygon nor
    // projects it for its size - it fills between the vertices it was given with flat
    // triangles. Sampling the ground at each vertex, as map3d.html does for
    // altitudeReference, lifts the vertices and nothing between them, so the fill cut
    // under the curve of the Earth as soon as the vertices were far apart. The Aircraft
    // feature's coverage, spanning thousands of km, showed only as slivers along its
    // outer edge, and HFDL's not at all.
    const bool draped = perPosition && (mapItem->m_altitudeReference == 1);

    QJsonArray positions;
    for (const auto c : mapItem->m_points)
    {
        positions.append(finiteOrZero(c->longitude()));
        positions.append(finiteOrZero(c->latitude()));
        positions.append(finiteOrZero(c->altitude()));
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

    // Outlined only when the polygon is FLAT, which is the difference between an outline
    // that helps and one that ruins the picture.
    //
    // Cesium draws the outline of an extruded polygon at every vertex - not just around
    // the top and bottom but a vertical edge at each point of the perimeter - in opaque
    // black whatever colour the polygon is. An airspace is a faint translucent wall built
    // from a great many vertices, so that came out as a row of hard dark bars across it.
    // A flat polygon has no verticals, and its outline is simply its boundary: the ILS
    // demodulator's beams and the Aircraft feature's coverage wedges both want that.
    //
    // Cesium has one flag for the whole outline, so this is decided from the item rather
    // than asked of it - perPosition is already the test for "not extruded", and an
    // explicit flag would mean a new SWGMapItem field and a swagger regeneration.
    QJsonArray outlineColorRGBA {
        0, 0, 0, 255
    };
    QJsonObject outlineColor {
        {"rgba", outlineColorRGBA}
    };

    // Cesium cannot outline a polygon on the ground, and says so on the console if asked
    QJsonObject polygon {
        {"positions", positionList},
        {"material", material},
        {"outline", perPosition && !draped},
        {"outlineColor", outlineColor},
    };
    if (draped)
    {
        polygon.insert("heightReference", m_heightReferences[mapItem->m_altitudeReference]);
    }
    else if (perPosition)
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
        finiteOrZero(mapItem->m_longitude), finiteOrZero(mapItem->m_latitude),
        finiteOrZero(mapItem->m_altitude)
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
        // Delete obj completely (including any history) - if Cesium has it
        if (deleteNeeded(id)) {
            obj.insert("delete", true);
        }
        return obj;         // id only otherwise, so nothing is sent
    }
    described(id);

    QJsonArray positions;
    for (const auto c : mapItem->m_points)
    {
        positions.append(finiteOrZero(c->longitude()));
        positions.append(finiteOrZero(c->latitude()));
        positions.append(finiteOrZero(c->altitude()));
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
        finiteOrZero(mapItem->m_longitude), finiteOrZero(mapItem->m_latitude),
        finiteOrZero(mapItem->m_altitude)
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

// A sample needs a time. An item can legitimately not have one - a position datetime
// is invalid until a protocol that carries times has been heard, and the per-field
// datetimes are sender strings that serialize an invalid QDateTime as "" - and an
// empty string here made Cesium reject the ENTIRE packet ("Invalid ISO 8601 date."),
// state, position, model and all. The state being described is current, so 'now' is
// the honest anchor when the sender gave none.
static QString sampleTime(const QString& dateTime)
{
    return dateTime.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) : dateTime;
}

static void insertProperty(QJsonObject& properties, const QString& name, const QString& dateTime, float value)
{
    if (!std::isnan(value))
    {
        QJsonArray ar;
        ar.push_back(sampleTime(dateTime));
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

// Two samples in one packet. CZML takes a sampled number as a flat
// [time, value, time, value] array. Used to close a run of identical values with the last
// sample that still carried the old value, immediately before the sample that changes it,
// so the run reconstructs as a flat line instead of a ramp from where the run began.
static void insertProperty2(QJsonObject& properties, const QString& name,
                            const QString& dateTime0, float value0,
                            const QString& dateTime1, float value1)
{
    if (!std::isnan(value0) && !std::isnan(value1))
    {
        QJsonArray ar;
        ar.push_back(sampleTime(dateTime0));
        ar.push_back(value0);
        ar.push_back(sampleTime(dateTime1));
        ar.push_back(value1);
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

void CZML::SampledChannel::write(QJsonObject& properties, const QString& name,
    const QString& dateTime, const QDateTime& frameTime, float value, bool force)
{
    if (std::isnan(value)) {
        return;             // insertProperty() would drop it anyway
    }

    const bool first = force || std::isnan(m_emitted) || !m_sentFrame.isValid();

    // How often this aircraft is being updated, which bounds how early the keepalive has
    // to fire. Measured from the previous call rather than assumed: it varies from about
    // 0.5 s to tens of seconds across the traffic on the map
    const qint64 interval = m_lastFrame.isValid()
        ? qAbs(m_lastFrame.secsTo(frameTime)) : 1;
    m_lastFrame = frameTime;

    if (first || (value != m_emitted))
    {
        // Close the run that has just ended before opening the new one, so the old value
        // is still held at the time it was last measured and the change stays a step
        if (!first && !m_heldTime.isEmpty() && m_heldFrame.isValid()
            && (m_heldFrame < frameTime))
        {
            insertProperty2(properties, name, m_heldTime, m_emitted, dateTime, value);
        }
        else
        {
            insertProperty(properties, name, dateTime, value);
        }
        m_emitted = value;
        m_heldTime.clear();
        m_heldFrame = QDateTime();
        m_sentFrame = frameTime;
        return;
    }

    // Unchanged, so withhold it - but keep the trailing edge of the run inside the
    // forward HOLD, or the PFD field would blank mid-cruise. Emitting at `threshold`
    // lands at up to threshold + interval, so the threshold has to leave room for the
    // next update to be late; at a large enough interval it goes non-positive and every
    // update is emitted, which is the only thing that can work at that cadence
    const qint64 threshold = qMin<qint64>(m_keepaliveMaxSecs,
                                          m_holdSecs - 2 * interval);
    m_heldTime = dateTime;
    m_heldFrame = frameTime;
    if (qAbs(m_sentFrame.secsTo(frameTime)) >= threshold)
    {
        insertProperty(properties, name, dateTime, value);
        m_sentFrame = frameTime;
        m_heldTime.clear();
        m_heldFrame = QDateTime();
    }
}

// The flight plan is structured data of variable length, so it is sent as a single
// constant string property containing compact JSON, which the ND parses
static QString flightPlanJson(const MapFlightPlan& plan)
{
    if (!plan.m_valid) {
        return QString();
    }
    QJsonObject obj;
    if (!plan.m_departure.isEmpty()) {
        obj.insert("dep", plan.m_departure);
    }
    if (!plan.m_arrival.isEmpty()) {
        obj.insert("arr", plan.m_arrival);
    }
    if (!plan.m_arrivalRunway.isEmpty()) {
        obj.insert("rwy", plan.m_arrivalRunway);
    }
    if (!plan.m_approach.isEmpty()) {
        obj.insert("app", plan.m_approach);
    }
    obj.insert("act", plan.m_activeWaypoint);
    QJsonArray waypoints;
    for (const auto& wp : plan.m_waypoints)
    {
        QJsonObject waypoint;
        waypoint.insert("n", wp.m_name);
        if (!std::isnan(wp.m_latitude) && !std::isnan(wp.m_longitude))
        {
            waypoint.insert("lat", wp.m_latitude);
            waypoint.insert("lon", wp.m_longitude);
        }
        if (!std::isnan(wp.m_altitudeFt)) {
            waypoint.insert("alt", wp.m_altitudeFt);
        }
        if (!wp.m_eta.isEmpty()) {
            waypoint.insert("eta", wp.m_eta);
        }
        waypoints.append(waypoint);
    }
    obj.insert("wpts", waypoints);
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// A datalink (CPDLC/PDC) message for the DCDU, as a single constant string property of
// compact JSON, like the flight plan above
static QString datalinkMessageJson(const MapDatalinkMessage& message)
{
    if (!message.m_valid) {
        return QString();
    }
    QJsonObject obj;
    obj.insert("id", message.m_id);
    if (!message.m_received.isEmpty()) {
        obj.insert("rx", message.m_received);
    }
    obj.insert("up", message.m_uplink ? 1 : 0);
    if (!message.m_station.isEmpty()) {
        obj.insert("stn", message.m_station);
    }
    obj.insert("txt", message.m_text);
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// SampledProperties are interpolated
// Need to use intervals to avoid interpolation
// See: https://sandcastle.cesium.com/?src=CZML%20Custom%20Properties.html&label=All
static void insertProperty0(QJsonObject& properties, const QString& name, const QString& dateTime, float value)
{
    if (!std::isnan(value))
    {
        QJsonObject obj {
            {"interval", sampleTime(dateTime) + "/3000"}, // Year 3000
            {"number", value}
        };

        QJsonArray array {
            obj
        };
        properties.insert(name, array);
    }
}

// The 3D map's altitude coloured track: the taken track split into one polyline entity
// per altitude band, sharing the 2D map's palette and its 5000 ft bands. A Cesium path
// has a single material, so a multi coloured track cannot be drawn with the entity's
// own path graphic - when a palette is set the path is hidden (see the path block in
// update()) and these entities, named "<item name> t3d <n>", are drawn instead.
//
// Items with a predicted track (satellites) keep the path, which also draws the future
// leg - something a polyline of taken positions cannot.
//
// Called BEFORE update(), which resets the item's cached state when it is deleted.
QList<QJsonObject> CZML::updateTrackSegments3D(ObjectMapItem *mapItem, bool isSelected)
{
    QList<QJsonObject> packets;
    const QString id = mapItem->m_name;

    // PFD samples from a fast log replay are sent immediately, while the accumulated
    // track is sent by the model's normal flush. Rebuilding the growing tail for every
    // frame would be quadratic work and can swamp the 3D page on a large log.
    if (mapItem->m_replayStateUpdate) {
        return packets;
    }

    const QString& palette = mapItem->m_itemSettings->m_2DTrackPalette;
    // With no palette but smoothing on, the track is still drawn as polylines - in the
    // single track colour - rather than left to the entity's path. The path is drawn
    // from the POSITION SAMPLES, which carry every raw live report interleaved between
    // the smoothed ones at different timestamps, so it stays jagged whatever lambda is.
    // Only the polylines below draw the smoothed track and nothing else.
    const bool smoothedPlain = palette.isEmpty()
        && (mapItem->m_itemSettings->m_smoothingWindow > 0)
        && mapItem->m_trackSupplied;
    const bool segmented = (!palette.isEmpty() || smoothedPlain)
        && !mapItem->m_fixedPosition
        && mapItem->m_predictedTrackCoords.isEmpty()
        && mapItem->m_itemSettings->m_enabled
        && !mapItem->m_deleted
        && (mapItem->m_image != "")
        && !filter(mapItem);

    if (!segmented)
    {
        // Take down anything drawn. Without creating a state entry: the settings walks
        // visit every one of the model's items, most of which never segment a track.
        auto it = m_ids.find(id);
        if ((it == m_ids.end()) || (it->m_trackSegmentsEmitted == 0)) {
            return packets;
        }
        State& state = *it;
        for (int i = 0; i < state.m_trackSegmentsEmitted; i++)
        {
            packets.append(QJsonObject {
                {"id", QString("%1 t3d %2").arg(id).arg(i)},
                {"delete", true}
            });
        }
        state.m_trackSegmentsEmitted = 0;
        state.m_trackSegmentSizes.clear();
        state.m_trackSegmentBands.clear();
        state.m_trackPointCount = 0;
        state.m_trackLastColorIndex = -1;
        state.m_trackPalette.clear();
        state.m_trackSegmentsShow = false;
        return packets;
    }

    State& state = m_ids[id];
    const QList<QGeoCoordinate *>& coords = mapItem->m_takenTrackCoords;

    const bool show = mapItem->m_itemSettings->m_display3DTrack
        && (   m_settings->m_displayAllGroundTracks
            || (m_settings->m_displaySelectedGroundTracks && isSelected));
    const bool showChanged = (show != state.m_trackSegmentsShow)
        && (state.m_trackSegmentsEmitted > 0);

    // Rebuild from scratch when the palette changed or the track is not simply longer
    // than before - it shrank, or its first point moved, which is what a trim or a
    // flight change looks like. In plain mode the key carries the track colour, so a
    // colour change also recolours the already closed chunks
    const QString paletteKey = smoothedPlain
        ? QString("#plain %1").arg(mapItem->m_itemSettings->m_3DTrackColor)
        : palette;
    const bool tailChanged = (state.m_trackPointCount > 0)
        && (coords.size() >= state.m_trackPointCount)
        && (*coords[state.m_trackPointCount - 1] != state.m_trackLastCoord);
    const bool rebuild = (paletteKey != state.m_trackPalette)
        || (coords.size() < state.m_trackPointCount)
        || tailChanged
        || (state.m_trackRevision != mapItem->m_takenTrackRevision)
        || (   (state.m_trackPointCount > 0) && !coords.isEmpty()
            && (*coords[0] != state.m_trackFirstCoord));
    if (rebuild)
    {
        state.m_trackSegmentSizes.clear();
        state.m_trackSegmentBands.clear();
        state.m_trackPointCount = 0;
        state.m_trackLastColorIndex = -1;
        state.m_trackPalette = paletteKey;
    }
    if (!coords.isEmpty()) {
        state.m_trackFirstCoord = *coords[0];
    }

    // Extend the segmentation over the new points, mirroring the 2D map's
    // ObjectMapModel::colorTrackPoints(): the point a colour change happens at is
    // repeated, in the segment that ends there and the one that starts there, so
    // consecutive segments join without a gap
    // A segment is also split when it reaches a maximum size. A closed segment is never
    // re-sent, but the growing tail is - and a band only changes when the aircraft
    // crosses 5000 ft, so at cruise the tail segment was the whole cruise, thousands of
    // points re-serialized on the GUI thread every few seconds for every aircraft. That
    // is what made position updates jerky with a few hundred tracks on screen. The cap
    // bounds the tail: at most this many points per re-send, whatever the flight phase.
    const int maxSegmentSize = 250;
    const int previousCount = state.m_trackSegmentSizes.size();
    for (int i = state.m_trackPointCount; i < coords.size(); i++)
    {
        // Plain mode has no altitude bands - everything is one band in the single
        // track colour, and the size cap alone chunks it
        const int band = smoothedPlain
            ? 0
            : FlightPathPalette::getColorIndex(Units::metresToFeet(coords[i]->altitude()));
        if (   (band != state.m_trackLastColorIndex)
            || (state.m_trackSegmentSizes.last() >= maxSegmentSize))
        {
            if (state.m_trackLastColorIndex >= 0) {
                state.m_trackSegmentSizes.last()++;
            }
            state.m_trackSegmentSizes.append(0);
            state.m_trackSegmentBands.append(band);
            state.m_trackLastColorIndex = band;
        }
        state.m_trackSegmentSizes.last()++;
    }
    const bool grew = coords.size() > state.m_trackPointCount;
    state.m_trackPointCount = coords.size();
    state.m_trackLastCoord = coords.isEmpty() ? QGeoCoordinate() : *coords.last();
    state.m_trackRevision = mapItem->m_takenTrackRevision;

    const int segmentCount = state.m_trackSegmentSizes.size();
    if (segmentCount == 0)
    {
        state.m_trackSegmentsShow = show;
        return packets;
    }

    // While the track is hidden, nothing is emitted at all - the segmentation state
    // above still advances, and showing the track re-emits everything current in one
    // go (the show change below makes firstEmit 0). Hiding just flips the show flag
    // on what is already drawn, rather than re-sending every position hidden
    if (!show)
    {
        if (showChanged)
        {
            for (int k = 0; k < state.m_trackSegmentsEmitted; k++)
            {
                packets.append(QJsonObject {
                    {"id", QString("%1 t3d %2").arg(id).arg(k)},
                    {"polyline", QJsonObject {{"show", false}}}
                });
            }
        }
        state.m_trackSegmentsShow = false;
        return packets;
    }

    // Decide which segments go out. Closed segments never change again, so in the
    // steady state only the growing tail is re-sent - and that at most once a second.
    // The throttle was 5 s when the tail was a CZML polyline entity, whose every update
    // re-batched static geometry on the page's main thread; the page now draws these
    // through a PolylineCollection, where an update rewrites one line's vertex data
    // (measured ~30 us a packet), so the track can afford to hug the aircraft.
    int firstEmit;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (rebuild || showChanged || (state.m_trackSegmentsEmitted == 0)) {
        firstEmit = 0;
    } else if (segmentCount > previousCount) {
        // A segment closed: it gained its boundary point, so re-send from it
        firstEmit = qMax(0, previousCount - 1);
    } else if (grew && (mapItem->m_replayTrackPending
        || !state.m_trackTailEmit.isValid() || (state.m_trackTailEmit.msecsTo(now) >= 1000))) {
        firstEmit = segmentCount - 1;
    } else {
        return packets; // Nothing changed, or the tail is throttled
    }
    // The Whittaker-Eilers smoother refines the last two windows of points in place
    // (smoothTrack() filters twice the setting), which can reach back beyond
    // the segment that grew - re-send far enough to cover it
    const int smoothingWindow = 2 * mapItem->m_itemSettings->m_smoothingWindow;
    if (smoothingWindow > 0)
    {
        int covered = (firstEmit < segmentCount) ? state.m_trackSegmentSizes[firstEmit] : 0;
        while ((firstEmit > 0) && (covered < smoothingWindow)) {
            firstEmit--;
            covered += state.m_trackSegmentSizes[firstEmit] - 1;
        }
    }

    const QVariant *paletteColors = smoothedPlain ? nullptr : FlightPathPalette::getPalette(palette);
    const QColor plainColor = QColor::fromRgba(mapItem->m_itemSettings->m_3DTrackColor);

    // Draw the smoothed shadow of the track where one is being maintained - the bands
    // above stay keyed on the raw altitudes, which never change under us. The provisional
    // live point rides one past the shadow (updateProvisionalTrackPoint() does not shadow
    // it), and it is there on almost every flush - requiring exact equality here meant
    // the raw track was what actually got drawn, quantized altitudes and all.
    const int shadowSize = mapItem->m_smoothedTrackCoords.size();
    const bool smoothed = (shadowSize == coords.size())
        || (mapItem->m_trackProvisional && (shadowSize == coords.size() - 1));

    // Consecutive segments share their boundary point, so segment k starts at the sum
    // of the earlier sizes less one shared point per join
    int start = 0;
    for (int k = 0; k < firstEmit; k++) {
        start += state.m_trackSegmentSizes[k] - 1;
    }
    for (int k = firstEmit; k < segmentCount; k++)
    {
        const int size = state.m_trackSegmentSizes[k];
        QJsonArray positions;
        for (int j = start; j < start + size; j++)
        {
            const QGeoCoordinate& coord = (smoothed && (j < shadowSize))
                ? mapItem->m_smoothedTrackCoords[j] : *coords[j];
            // Rounded, not full precision: a raw double serializes to up to 17
            // significant digits, which is most of the packet's bytes. 1e-7 degrees
            // is about a centimetre
            positions.push_back(std::round(finiteOrZero(coord.longitude()) * 1e7) / 1e7);
            positions.push_back(std::round(finiteOrZero(coord.latitude()) * 1e7) / 1e7);
            positions.push_back(std::round(finiteOrZero(coord.altitude()) * 100.0) / 100.0);
        }
        const QColor color = smoothedPlain ? plainColor : paletteColors[state.m_trackSegmentBands[k]].value<QColor>();
        QJsonArray colorRGBA {
            color.red(), color.green(), color.blue(), color.alpha()
        };
        QJsonObject polyline {
            {"positions", QJsonObject {{"cartographicDegrees", positions}}},
            {"material", QJsonObject {{"solidColor", QJsonObject {{"color", QJsonObject {{"rgba", colorRGBA}}}}}}},
            {"width", 2},
            {"show", show}
        };
        packets.append(QJsonObject {
            {"id", QString("%1 t3d %2").arg(id).arg(k)},
            {"polyline", polyline}
        });
        start += size - 1;
    }

    // A rebuild can produce fewer segments than were drawn
    for (int k = segmentCount; k < state.m_trackSegmentsEmitted; k++)
    {
        packets.append(QJsonObject {
            {"id", QString("%1 t3d %2").arg(id).arg(k)},
            {"delete", true}
        });
    }

    state.m_trackSegmentsEmitted = segmentCount;
    state.m_trackSegmentsShow = show;
    state.m_trackTailEmit = now;
    return packets;
}

QJsonObject CZML::update(ObjectMapItem *mapItem, bool isTarget, bool isSelected)
{
    (void) isTarget;

    QString id = mapItem->m_name;

    QJsonObject obj {
        {"id", id}         // id must be unique
    };

    if (!mapItem->m_itemSettings->m_enabled || filter(mapItem) || mapItem->m_deleted)
    {
        // Delete obj completely (including any history) - if Cesium has it, and only
        // once. See deleteNeeded() for what that was costing.
        if (deleteNeeded(id)) {
            obj.insert("delete", true);
        }
        return obj;         // id only otherwise, so nothing is sent
    }

    // Models, labels, points and billboards are all kept above the surface by the
    // CLIP_TO_GROUND height reference added to our Cesium, which re-queries the surface
    // under each every frame - as terrain and tiles load, and between the positions we
    // send, where Cesium interpolates and extrapolates. That costs: against Google's 3D
    // tiles each query is a ray pick through the tile geometry, measured at 21 ms a
    // frame for 90 models and far more for their labels. The alternative, clipping the
    // positions we send in map3d.html from a height sampled off the depth buffer, was
    // cheaper but wrong on those tiles - a sample that lands on a hangar roof or a parked
    // aircraft's mesh lifts everything crossing that cell into the air - so Cesium's
    // is used for everything until that is right, and this comment records the cost
    const QStringList heightReferences = {"NONE", "CLAMP_TO_GROUND", "RELATIVE_TO_GROUND", "CLIP_TO_GROUND"};
    QString dt;

    if (mapItem->m_availableFrom.isValid()) {
        dt = mapItem->m_availableFrom.toString(Qt::ISODateWithMs);
    } else if (mapItem->m_takenTrackDateTimes.size() > 0) {
        dt = mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs);
    } else {
        dt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    }

    // Keep a hash of the time we first saw each item
    State& state = m_ids[id];
    // An item that was previously filtered out already has an entry, so the presence of
    // one no longer means we have described it
    bool existingId = !state.m_firstSeenDateTime.isEmpty();
    if (!existingId) {
        state.m_firstSeenDateTime = dt;
    }
    state.m_deleted = false;    // visible again, so a later filter must re-delete it

    bool removeObj = false;
    bool fixedPosition = mapItem->m_fixedPosition;

    // EARLY OUT, before any JSON is built.
    //
    // allUpdated() walks every item whenever settings are applied, and the model holds of
    // the order of 200,000 - airports, navaids, airspaces, waypoints - none of which ever
    // move. Filtering the resulting packet was enough to stop them being SENT, but each
    // one was still assembled into a QJsonObject and then discarded: measured at 217,510
    // constructions to produce 1,242 batches worth sending.
    //
    // A fixed item that we have already described, whose position, label, image and the
    // settings CZML reads are all as they were, cannot produce anything but its own id.
    const quint32 settingsSignature = mapItem->m_itemSettings->czmlSignature();
    if (state.m_seen
        && fixedPosition
        && (state.m_settingsSignature == settingsSignature)
        && (state.m_latitude == mapItem->m_latitude)
        && (state.m_longitude == mapItem->m_longitude)
        && (state.m_altitude == mapItem->m_altitude)
        && (state.m_lastLabel == mapItem->m_label)
        && (state.m_lastImage == mapItem->m_image))
    {
        return obj;     // id only, which CesiumInterface::update() will not send
    }
    // How fast height is being lost, from the last update, so that the extrapolation
    // below can be stopped before it flies the item into the ground. Worked out here
    // because the previous altitude is about to be overwritten
    double descentRate = 0.0;   // m/s, positive while descending
    if (state.m_altitudeDateTime.isValid() && mapItem->m_positionDateTime.isValid())
    {
        const double seconds = state.m_altitudeDateTime.msecsTo(mapItem->m_positionDateTime) / 1000.0;
        if (seconds > 0.0)
        {
            const double rate = (state.m_altitude - mapItem->m_altitude) / seconds;
            if ((rate > 0.0) && (rate < 100.0)) {    // Anything faster is a bad altitude, not a descent
                descentRate = rate;
            }
        }
    }

    state.m_seen = true;
    state.m_settingsSignature = settingsSignature;
    state.m_latitude = mapItem->m_latitude;
    state.m_longitude = mapItem->m_longitude;
    state.m_altitude = mapItem->m_altitude;
    state.m_altitudeDateTime = mapItem->m_positionDateTime;
    state.m_lastLabel = mapItem->m_label;
    state.m_lastImage = mapItem->m_image;
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
                    coords.push_back(finiteOrZero(mapItem->m_interpolatedCoords[i]->longitude()));
                    coords.push_back(finiteOrZero(mapItem->m_interpolatedCoords[i]->latitude()));
                    // The same offset the live position gets below - 0,0,0 is the
                    // middle of a 3D model, not the bottom of its undercarriage, so
                    // without it these samples sit lower than the ones around them
                    coords.push_back(finiteOrZero(mapItem->m_interpolatedCoords[i]->altitude() + mapItem->m_modelAltitudeOffset));
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
                    coords.push_back(finiteOrZero(mapItem->m_takenTrackCoords.back()->longitude()));
                    coords.push_back(finiteOrZero(mapItem->m_takenTrackCoords.back()->latitude()));
                }
                else
                {
                    if (useDateTime) {
                        coords.push_back(mapItem->m_positionDateTime.toString(Qt::ISODateWithMs));
                    }
                    coords.push_back(finiteOrZero(mapItem->m_longitude));
                    coords.push_back(finiteOrZero(mapItem->m_latitude));
                }
                if (!mapItem->m_takenTrackAltitudeExtrapolated.isEmpty() && mapItem->m_takenTrackAltitudeExtrapolated.back()) {
                    coords.push_back(finiteOrZero(mapItem->m_takenTrackCoords.back()->altitude()));
                } else {
                    coords.push_back(finiteOrZero(mapItem->m_altitude + mapItem->m_modelAltitudeOffset)); // See nodeTransformations comment below, as to why we use this here
                }
            }
        }
    }
    else
    {
        coords = m_lastPosition.value(id);
    }
    // A position that has not moved does not need re-sending. For a fixed item - an
    // airport, a navaid, an airspace - that is every update after the first, and there
    // are of the order of 200,000 of those in the model.
    const bool positionUnchanged = existingId && m_lastPosition.contains(id)
                                && (m_lastPosition.value(id) == coords);

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
                // Extrapolation carries the last vertical rate on, so an aircraft still
                // descending when its updates stop is flown down through the ground and
                // keeps going - they were seen 82 m under it. It may only be extrapolated
                // for as long as the height it had would last at the rate it was losing
                // it: an aircraft at cruise is unaffected, one on approach stops being
                // extrapolated at the point it would have arrived. Only for items drawn
                // at an absolute altitude that are meant to sit above the ground, so
                // shipping, which is clamped to it, is untouched
                int extrapolationSeconds = mapItem->m_itemSettings->m_extrapolate;
                if ((mapItem->m_altitudeReference == 3) && (descentRate > 0.0) && (mapItem->m_altitude > 0.0f))
                {
                    const int secondsToGround = (int) (mapItem->m_altitude / descentRate);
                    extrapolationSeconds = qMin(extrapolationSeconds, qMax(1, secondsToGround));
                }
                position.insert("forwardExtrapolationDuration", extrapolationSeconds);
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
        QJsonObject properties;
        if (mapItem->m_aircraftState)
        {
            // A replay sample has its own frame time. Live/coalesced senders omit it
            // and retain the historical behaviour of tying state to the position time.
            QDateTime aircraftStateDateTime = mapItem->m_aircraftState->m_dateTime.isValid()
                ? mapItem->m_aircraftState->m_dateTime : mapItem->m_positionDateTime;
            QString dateTime = aircraftStateDateTime.toString(Qt::ISODateWithMs);
            bool aircraftStateDateTimeChanged = aircraftStateDateTime != state.m_aircraftStateDateTime;
            const bool replayState = mapItem->m_aircraftState->m_dateTime.isValid();
            QString aircraftCallsign = mapItem->m_aircraftState->m_callsign;
            QString aircraftType = mapItem->m_aircraftState->m_aircraftType;
            // During replay this is a complete snapshot, so even fields whose most
            // recent measurement has its own time are sampled at the frame time. That
            // preserves their held value throughout the replay instead of letting the
            // CZML 30-second extrapolation expire between sparse measurements.
            QString aircraftIndicatedAirspeedDateTime = replayState
                ? dateTime : mapItem->m_aircraftState->m_indicatedAirspeedDateTime;
            float aircraftIndicatedAirspeed = mapItem->m_aircraftState->m_indicatedAirspeed;
            float aircraftTrueAirspeed = mapItem->m_aircraftState->m_trueAirspeed;
            float aircraftGroundspeed = mapItem->m_aircraftState->m_groundspeed;
            QString aircraftAltitudeDateTime = replayState
                ? dateTime : mapItem->m_aircraftState->m_altitudeDateTime;
            float aircraftAltitude = mapItem->m_aircraftState->m_altitude;
            int aircraftOnSurface = mapItem->m_aircraftState->m_onSurface;
            float aircraftMach = mapItem->m_aircraftState->m_mach;
            float aircraftQNH = mapItem->m_aircraftState->m_qnh;
            float aircraftVerticalSpeed = mapItem->m_aircraftState->m_verticalSpeed;
            float aircraftHeading = mapItem->m_aircraftState->m_heading;
            float aircraftTrack = mapItem->m_aircraftState->m_track;
            float aircraftRoll = mapItem->m_roll;
            // The same estimate that orients the 3D model, so the attitude indicator and
            // the aircraft on the map agree
            float aircraftPitch = mapItem->m_pitch;
            float aircraftSelectedAltitude = mapItem->m_aircraftState->m_selectedAltitude;
            float aircraftSelectedHeading = mapItem->m_aircraftState->m_selectedHeading;
            int aircraftAutopilot = mapItem->m_aircraftState->m_autopilot;
            MapAircraftState::VerticalMode aircraftVerticalMode = mapItem->m_aircraftState->m_verticalMode;
            MapAircraftState::LateralMode aircraftLateralMode = mapItem->m_aircraftState->m_lateralMode;
            MapAircraftState::TCASMode aircraftTCASMode = mapItem->m_aircraftState->m_tcasMode;
            int aircraftTcasRA = mapItem->m_aircraftState->m_tcasRA;
            QString aircraftTcasThreatIcao = mapItem->m_aircraftState->m_tcasThreatIcao;
            int aircraftSquawk = mapItem->m_aircraftState->m_squawk;
            QString aircraftIcao = mapItem->m_aircraftState->m_icao;
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
            state.m_chIndicatedAirspeed.write(properties, "pfdIndicatedAirspeed",
                aircraftIndicatedAirspeedDateTime, aircraftStateDateTime, aircraftIndicatedAirspeed,
                !existingId && !aircraftIndicatedAirspeedDateTime.isEmpty());
            state.m_chTrueAirspeed.write(properties, "pfdTrueAirspeed",
                dateTime, aircraftStateDateTime, aircraftTrueAirspeed,
                !existingId);
            state.m_chGroundspeed.write(properties, "pfdGroundspeed",
                dateTime, aircraftStateDateTime, aircraftGroundspeed,
                !existingId);
            state.m_chAltitude.write(properties, "pfdAltitude",
                aircraftAltitudeDateTime, aircraftStateDateTime, aircraftAltitude,
                !existingId && !aircraftAltitudeDateTime.isEmpty());
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftOnSurface != state.m_aircraftState.m_onSurface)
                )
            {
                insertProperty0(properties, "pfdOnSurface", dateTime, aircraftOnSurface);
                state.m_aircraftState.m_onSurface = aircraftOnSurface;
            }
            state.m_chMach.write(properties, "pfdMach",
                dateTime, aircraftStateDateTime, aircraftMach,
                !existingId);
            if (   !existingId
                || aircraftStateDateTimeChanged
                || (aircraftQNH != state.m_aircraftState.m_qnh)
                )
            {
                insertProperty0(properties, "pfdQNH", dateTime, aircraftQNH);
                state.m_aircraftState.m_qnh = aircraftQNH;
            }
            state.m_chVerticalSpeed.write(properties, "pfdVerticalSpeed",
                dateTime, aircraftStateDateTime, aircraftVerticalSpeed,
                !existingId);
            state.m_chHeading.write(properties, "pfdHeading",
                dateTime, aircraftStateDateTime, aircraftHeading,
                !existingId);
            state.m_chTrack.write(properties, "pfdTrack",
                dateTime, aircraftStateDateTime, aircraftTrack,
                !existingId);
            state.m_chRoll.write(properties, "pfdRoll",
                dateTime, aircraftStateDateTime, aircraftRoll,
                !existingId);
            state.m_chPitch.write(properties, "pfdPitch",
                dateTime, aircraftStateDateTime, aircraftPitch,
                !existingId);
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
                || (aircraftTcasRA != state.m_aircraftState.m_tcasRA)
                )
            {
                insertProperty0(properties, "pfdTcasRA", dateTime, aircraftTcasRA);
                state.m_aircraftState.m_tcasRA = aircraftTcasRA;
            }
            if (   (!existingId && !aircraftTcasThreatIcao.isEmpty())
                || (aircraftTcasThreatIcao != state.m_aircraftState.m_tcasThreatIcao)
                )
            {
                insertConstantProperty(properties, "pfdThreatIcao", aircraftTcasThreatIcao);
                state.m_aircraftState.m_tcasThreatIcao = aircraftTcasThreatIcao;
            }
            if (   (!existingId && (aircraftSquawk >= 0))
                || (aircraftSquawk != state.m_aircraftState.m_squawk)
                )
            {
                insertProperty0(properties, "pfdSquawk", dateTime, aircraftSquawk);
                state.m_aircraftState.m_squawk = aircraftSquawk;
            }
            if (   (!existingId && !aircraftIcao.isEmpty())
                || (aircraftIcao != state.m_aircraftState.m_icao)
                )
            {
                insertConstantProperty(properties, "pfdIcao", aircraftIcao);
                state.m_aircraftState.m_icao = aircraftIcao;
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

            QString ndPlan = flightPlanJson(mapItem->m_aircraftState->m_flightPlan);
            if (   (!existingId && !ndPlan.isEmpty())
                || (existingId && (ndPlan != state.m_ndFlightPlan))    // Empty when a plan goes away, so the ND clears it
                )
            {
                insertConstantProperty(properties, "ndFlightPlan", ndPlan);
                state.m_ndFlightPlan = ndPlan;
            }

            QString dcduMessage = datalinkMessageJson(mapItem->m_aircraftState->m_datalinkMessage);
            if (   (!existingId && !dcduMessage.isEmpty())
                || (existingId && (dcduMessage != state.m_dcduMessage))
                )
            {
                insertConstantProperty(properties, "dcduMessage", dcduMessage);
                state.m_dcduMessage = dcduMessage;
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

        }

        // The engine sound looped at the item, which is on the item rather than the
        // aircraft state, so it works for anything the sender knows a sound for
        if (   (!existingId && !mapItem->m_loopSound.isEmpty())
            || (existingId && (mapItem->m_loopSound != state.m_loopSound))
            )
        {
            insertConstantProperty(properties, "loopSound", mapItem->m_loopSound);
            state.m_loopSound = mapItem->m_loopSound;
        }

        if (properties.size() > 0) {
            obj.insert("properties", properties);
        }

        // ... while a moving aircraft still sends one every update, which is the point
        if (!positionUnchanged) {
            obj.insert("position", position);
        }

        if (!fixedPosition)
        {
            // Orientation is a sampled property, so a sample Cesium already has is a no-op
            // there - and a packet raised by something else, an altitude message say,
            // carries the same sample as the last one. That was 36% of the orientations
            // sent in a measured run, at ~190 bytes each.
            if (mapItem->m_useHeadingPitchRoll)
            {
                const int extrapolate = mapItem->m_itemSettings->m_extrapolate;
                if (   (state.m_orientationMode != 1)
                    || (quaternion != state.m_orientationQuaternion)
                    || (extrapolate != state.m_orientationExtrapolate))
                {
                    obj.insert("orientation", orientation);
                    state.m_orientationMode = 1;
                    state.m_orientationQuaternion = quaternion;
                    state.m_orientationExtrapolate = extrapolate;
                }
            }
            else if (state.m_orientationMode != 0)   // A reference to the position: constant
            {
                obj.insert("orientation", orientationPosition);
                state.m_orientationMode = 0;
                state.m_orientationQuaternion = QJsonArray();
            }
        }

        // Point
        quint32 pointColorInt = mapItem->m_itemSettings->m_3DPointColor;
        int pointAltitudeReference = clipRatherThanClamp(mapItem->m_altitudeReference);
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
        int labelAltitudeReference = clipRatherThanClamp(mapItem->m_altitudeReference);

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
            } else if ((mapItem->m_group == "Station") || (mapItem->m_group == "Radar") || (mapItem->m_group == "Radio Time Transmitters")
                || (mapItem->m_group == "WEFAX Transmitters")) {
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

            // On a copy: replace() is in place, and the state must keep the raw text it is
            // compared against, or the guard above never matches and the whole label -
            // ~390 bytes of styling - goes out on every packet, as it did until measured
            QString labelDisplayText = labelText;
            labelDisplayText.replace("<br>", "\n");
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
                label.insert("text", labelDisplayText);
            }
            else
            {
                QString interval = sampleTime(mapItem->m_labelDateTime.toString(Qt::ISODateWithMs)) + "/2999-12-31";
                QJsonObject labelInterval {
                    {"interval", interval},
                    {"string", labelDisplayText}
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
                    // With the default of true an aircraft flashes untextured as it appears,
                    // and every shader it has is built twice: once without its textures and
                    // again when they arrive
                    {"incrementallyLoadTextures", false},
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

                // Not CZML: map3d.html takes it out, and switches the entity to this
                // model while the aircraft is small on screen
                const QString lowDetail = Aircraft3DModels::instance().lowDetailModel(mapItem->m_model);
                if (!lowDetail.isEmpty()) {
                    model.insert("lowDetailGltf", m_settings->m_modelURL + lowDetail);
                }

                obj.insert("model", model);

                state.m_modelGLTF = modelGLTF;
                state.m_modelAltitudeReference = modelAltitudeReference;
                state.m_modelShow = modelShow;
                state.m_modelMinimumPixelSize = modelMinimumPixelSize;
            }
            // A packet only adds to what Cesium has for an entity, so an item that was a
            // billboard and now has a model would otherwise show both
            if (state.m_billboardShown)
            {
                obj.insert("billboard", QJsonObject{{"show", false}});
                state.m_billboardShown = false;
            }
        }
        else
        {
            // And the other way round: an aircraft whose model is taken away - its stand-in
            // replaced by one for its emitter category that is not installed - was left
            // drawn as the old model with the billboard on top of it
            if (!state.m_modelGLTF.isEmpty())
            {
                obj.insert("model", QJsonObject{{"show", false}});
                state.m_modelGLTF.clear();
            }
            state.m_billboardShown = true;

            // Use billboard for APRS as we don't currently have 3D objects
            QString imageURL = mapItem->m_image;
            if (imageURL.startsWith("qrc://")) {
                imageURL = imageURL.mid(6); // Redirect to our embedded webserver, which will check resources
            }
            QJsonObject billboard {
                {"image", imageURL},
                {"show", true},     // In case a model had hidden it
                {"heightReference", heightReferences[clipRatherThanClamp(mapItem->m_altitudeReference)]},
                {"verticalOrigin", "BOTTOM"} // To stop it being cut in half when zoomed out
            };

            obj.insert("billboard", billboard);
        }

        // Description
        // Images in the description are qrc:/// URLs, which Qt Quick and the widgets
        // resolve themselves, but the 3D map is a page served over http and Chromium
        // refuses to load a local scheme resource from it - "Not allowed to load local
        // resource". Point them at the embedded web server, which looks the path up in
        // our resources, exactly as the billboard image above is redirected
        QString description = mapItem->m_text;
        description.replace("qrc://", "");
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
            // When a palette is set - or the supplied track is smoothed - the track is
            // drawn as polylines instead - see updateTrackSegments3D(). Items with a
            // predicted track keep the path, which also draws the future leg.
            if (   (   !mapItem->m_itemSettings->m_2DTrackPalette.isEmpty()
                    || (   (mapItem->m_itemSettings->m_smoothingWindow > 0)
                        && mapItem->m_trackSupplied))
                && mapItem->m_predictedTrackCoords.isEmpty()) {
                pathShow = false;
            }

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

        // Availability, when it has changed. It moves on with each position - which is
        // what keeps an aircraft that has gone quiet from being drawn for ever - but a
        // packet raised by anything else carried the same interval again: 63% of them
        QString availability;
        if (!fixedPosition && (mapItem->m_takenTrackDateTimes.size() > 0) && (mapItem->m_predictedTrackDateTimes.size() > 0))
        {
            availability = QString("%1/%2")
                            .arg(mapItem->m_takenTrackDateTimes.last()->toString(Qt::ISODateWithMs))
                            .arg(mapItem->m_predictedTrackDateTimes.last()->toString(Qt::ISODateWithMs));
        }
        else if (mapItem->m_availableUntil.isValid())
        {
            availability = QString("%1/%2").arg(state.m_firstSeenDateTime).arg(mapItem->m_availableUntil.toString(Qt::ISODateWithMs));
        }
        if (!availability.isEmpty() && (availability != state.m_availability))
        {
            obj.insert("availability", availability);
            state.m_availability = availability;
        }
        m_lastPosition.insert(id, coords);
    }
    else
    {
        // Disable forward extrapolation
        obj.insert("position", noPosition);
    }

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
