///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_MAPITEM_H_
#define INCLUDE_FEATURE_MAPITEM_H_

#include <QDateTime>
#include <QGeoCoordinate>
#include <QGeoRectangle>
#include <QColor>

#include "mapsettings.h"
#include "cesiuminterface.h"
#include "util/whittakereilers.h"
#include "mapaircraftstate.h"

#include "SWGMapItem.h"

class MapModel;
class ObjectMapModel;
class TrackColorModel;
class PolygonMapModel;
class PolylineMapModel;
class ImageMapModel;
class CZML;

class MapItem {

public:

    MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem);
    virtual ~MapItem() = default;

    // What has changed about this item since the model last told the map about it.
    // Lets us tell the map which roles to re-read, rather than all of them
    enum Change : quint32 {
        ChangedPosition       = 1u << 0,
        ChangedLabel          = 1u << 1,
        ChangedImage          = 1u << 2,
        ChangedText           = 1u << 3,   // Bubble text, and the frequencies in it
        ChangedTrack          = 1u << 4,
        ChangedPredictedTrack = 1u << 5,
        ChangedGeometry       = 1u << 6,   // Polygon/polyline points and bounds
        ChangedSelection      = 1u << 7,   // Selected or targeted in the GUI
        ChangedZOrder         = 1u << 9,   // Moved in front of or behind other items
        Changed3D             = 1u << 8,   // Only of interest to the 3D map
        ChangedAll            = 0xffffffffu
    };

    quint32 m_changed = ChangedAll;   // Set by update(), consumed by the model's flush

    // What the map draws this item in front of. Qt Quick orders siblings by z, and map
    // items are plain QQuickItems, so this is all it takes to raise or lower one
    qreal m_zOrder = 0.0;

    // Row this item is at in the model it belongs to, or -1 if it isn't in one.
    // Saves a linear search of the model, which holds every airport and navaid in the
    // world as well as the objects we're currently hearing
    int m_row = -1;
    virtual void update(SWGSDRangel::SWGMapItem *mapItem);
    QGeoCoordinate getCoordinates();

protected:

    friend CZML;
    friend MapModel;
    friend ObjectMapModel;
    friend PolygonMapModel;
    friend PolylineMapModel;

    QString m_group;
    MapSettings::MapItemSettings *m_itemSettings;
    const QObject *m_sourcePipe;        // Channel/feature that created the item
    QString m_hashKey;

    QString m_name;                     // Unique id
    QString m_label;
    QDateTime m_labelDateTime;          // Date & time from which this label is valid from (for 3D map). Invalid date/time is forever
    float m_latitude;                   // Position for label
    float m_longitude;
    float m_altitude;                   // In metres
    QDateTime m_availableFrom;          // Date & time this item is visible from. Invalid date/time is forever
    QDateTime m_availableUntil;         // Date & time this item is visible until (for 3D map). Invalid date/time is forever
};

// Information required about each item displayed on the map
class ObjectMapItem : public MapItem {
public:
    //!< Taken off the map, which an empty image is how a sender says. A polygon and a
    //!< polyline have carried this for a while; an object did not, so the 3D map was
    //!< never told and every marker a removed item had drawn stayed on it.
    bool m_deleted = false;
public:
    ObjectMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem),
        m_aircraftState(nullptr),
        m_trackColorModel(nullptr)
    {
        update(mapItem);
    }

    ~ObjectMapItem();

    void update(SWGSDRangel::SWGMapItem *mapItem) override;

protected:
    void findFrequencies();
    void updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track, MapSettings::MapItemSettings *itemSettings);
    void smoothTrack(MapSettings::MapItemSettings *itemSettings);
    void updateProvisionalTrackPoint();
    void dropProvisionalTrackPoint();
    void updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track);
    void extrapolatePosition(QGeoCoordinate *c, const QDateTime& dateTime);
    void extrapolateAltitude(QGeoCoordinate *c, const QDateTime& dateTime);
    void interpolatePosition(int i, const float latitude, const float longitude, const QDateTime &dateTime);
    void interpolateAltitude(int i, const float altitude, const QDateTime &dateTime);

    friend MapModel;
    friend ObjectMapModel;
    friend CZML;
    QDateTime m_positionDateTime;
    QString m_positionDateTimeString;   // As the sender wrote it, which m_positionDateTime is not - see updateTrack
    QDateTime m_altitudeDateTime;
    bool m_useHeadingPitchRoll;
    float m_heading;
    float m_pitch;
    float m_roll;
    QDateTime m_orientationDateTime;
    QString m_image;
    int m_imageRotation;
    // 2D icon showing the livery of the matched 3D model (e.g. A320 in British Airways
    // colours), for when the user prefers those to the plain icons in m_image.
    // Empty when there is no model or no livery icon matching it
    QString m_liveryImage;
    QString m_text;
    QList<qint64> m_frequencies;        // Frequencies that can be tuned (currently only extracted from text)
    QStringList m_frequencyStrings;
    bool m_fixedPosition;               // Don't record/display track
    QList<QGeoCoordinate *> m_predictedTrackCoords;
    QList<QDateTime *> m_predictedTrackDateTimes;
    QVariantList m_predictedTrack;      // Line showing where the object is going
    QVariantList m_predictedTrack1;
    QVariantList m_predictedTrack2;
    QGeoCoordinate m_predictedStart1;
    QGeoCoordinate m_predictedStart2;
    QGeoCoordinate m_predictedEnd1;
    QGeoCoordinate m_predictedEnd2;
    QList<QGeoCoordinate *> m_takenTrackCoords;
    QList<QDateTime *> m_takenTrackDateTimes;
    // Exact strings received with a supplied track. They make the cheap prefix check in
    // updateTrack() sensitive to timestamp corrections without reparsing every old point.
    QList<QString> m_takenTrackDateTimeStrings;
    QList<bool> m_takenTrackPositionExtrapolated;
    QList<bool> m_takenTrackAltitudeExtrapolated;
    // Incremented when points already presented to the Map are rewritten. Appends are
    // deliberately excluded so the 2D and 3D segment caches can retain their fast path.
    quint64 m_takenTrackRevision = 0;
    // Longitude range of the taken track, so splitTrack() can skip tracks that are
    // wholly on screen. Extended as points are appended, recomputed if it is rebuilt
    double m_takenTrackMinLongitude = 180.0;
    double m_takenTrackMaxLongitude = -180.0;
    int m_takenTrackBoundsCount = 0;    // Points the bounds above account for
    bool m_trackSupplied = false;       // The sender owns this track - see updateTrack()
    bool m_trackProvisional = false;    // Last point is the live position, not the sender's

    void updateTakenTrackBounds();

    bool m_tracked = false;             // In the model's list of items that have a track
    bool m_takenTrackWholeOnScreen = false;   // Whether the last split found the whole track on screen

    // Taken track split in to runs of constant colour, when coloured by altitude
    QList<QVariantList> m_trackSegments;
    QList<QVariant> m_trackSegmentColors;
    QString m_trackSegmentPalette;      // Palette they were built with
    int m_trackSegmentPoints = 0;       // Points of m_takenTrack1 they account for
    int m_trackSegmentLastColor = -1;
    QGeoCoordinate m_trackSegmentLastCoord; // Detect an in-place provisional tail update
    quint64 m_trackSegmentRevision = 0;

    QVariantList m_takenTrack;          // Line showing where the object has been
    QVariantList m_takenTrack1;
    QVariantList m_takenTrack2;
    TrackColorModel *m_trackColorModel; // m_takenTrack1 and 2 split in to segments of constant colour, when coloured by altitude
    QGeoCoordinate m_takenStart1;
    QGeoCoordinate m_takenStart2;
    QGeoCoordinate m_takenEnd1;
    QGeoCoordinate m_takenEnd2;
    // The smoothed tail of a track. Owned here, and pointed at by
    // m_interpolatedCoords, which the 3D map reads and then clears - see
    // smoothTrack()
    int m_lastSmoothedSize = 0;         // Track length when smoothing last ran
    QList<QGeoCoordinate> m_smoothedWindow;
    QList<QDateTime> m_smoothedWindowTimes;
    // The smoothed shadow of a track, aligned with m_takenTrackCoords: raw
    // values where no smoothing pass has reached yet, overwritten as windows complete.
    // The stored track itself stays raw (and, for a supplied track, exactly as sent), so
    // the 3D map's altitude coloured segments draw from this instead.
    // Empty when smoothing is off - the segments then draw the raw track.
    QList<QGeoCoordinate> m_smoothedTrackCoords;
    QList<QGeoCoordinate *> m_interpolatedCoords;
    QList<QDateTime *> m_interpolatedDateTimes;

    // For 3D map
    QString m_model;
    int m_altitudeReference;
    float m_labelAltitudeOffset;
    float m_modelAltitudeOffset;
    QList<CesiumInterface::Animation *> m_animations;
    MapAircraftState *m_aircraftState;
    // Engine sound-set directory below 3d/Sounds, or a single file in 3d/Sounds/aircraft.
    QString m_loopSound;    // Empty for none
    // Fast replay sends PFD state immediately but lets the ordinary model flush send
    // the accumulated altitude-coloured track once per batch. These flags keep those
    // two jobs separate, so replay does not serialize the growing track for every frame.
    bool m_replayStateUpdate = false;
    bool m_replayTrackPending = false;

    static WhittakerEilers m_filter; // For smoothing/interpolating position
};

class PolygonMapItem  : public MapItem {

public:
    PolygonMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem)
    {
        update(mapItem);
    }
    void update(SWGSDRangel::SWGMapItem *mapItem) override;

protected:
    friend PolygonMapModel;
    friend CZML;

    QList<QGeoCoordinate *> m_points;   // FIXME: Remove?
    float m_extrudedHeight;             // In metres
    QVariantList m_polygon;
    QGeoRectangle m_bounds;             // Bounding boxes for the polygons, for view clipping
    bool m_colorValid;
    QRgb m_color;
    int m_altitudeReference;
    bool m_deleted = false;
};

class PolylineMapItem : public MapItem {

public:
    PolylineMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem),
        m_trackColorModel(nullptr)
    {
        update(mapItem);
    }

    // As ObjectMapItem, defined in mapitem.cpp
    ~PolylineMapItem();

    void update(SWGSDRangel::SWGMapItem *mapItem) override;

protected:
    friend PolylineMapModel;
    friend CZML;

    QList<QGeoCoordinate *> m_points;   // FIXME: Remove?
    QVariantList m_polyline;
    QGeoRectangle m_bounds;             // Bounding boxes for the polyline, for view clipping
    bool m_colorValid;
    QRgb m_color;
    // Item type 4: drawn as a series of lines coloured by the altitude of their points,
    // the way an object's ground track is, rather than in one colour. That lets a track
    // with no aircraft on it - an archived flight, say - still be drawn as a track. The
    // segments are built by PolylineMapModel, which is where the palette lives.
    bool m_altitudeColored;
    TrackColorModel *m_trackColorModel;
    int m_altitudeReference;
    bool m_deleted = false;
};

class ImageMapItem : public MapItem {

public:
    ImageMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem)
    {
        update(mapItem);
    }
    void update(SWGSDRangel::SWGMapItem *mapItem) override;

protected:
    friend ImageMapModel;
    friend CZML;

    QString m_image;
    float m_imageZoomLevel;
    QGeoRectangle m_bounds;

};

#endif // INCLUDE_FEATURE_MAPITEM_H_
