///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "mapsettings.h"
#include "cesiuminterface.h"

#include "SWGMapItem.h"

class MapModel;
class ObjectMapModel;
class PolygonMapModel;
class PolylineMapModel;
class ImageMapModel;
class CZML;

class MapItem {

public:

    MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem);
    virtual void update(SWGSDRangel::SWGMapItem *mapItem);

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
    float m_latitude;                   // Position for label
    float m_longitude;
    float m_altitude;                   // In metres
    QDateTime m_availableUntil;         // Date & time this item is visible until (for 3D map). Invalid date/time is forever
};


// Information required about each item displayed on the map
class ObjectMapItem : public MapItem {

public:
    ObjectMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem)
    {
        update(mapItem);
    }
    void update(SWGSDRangel::SWGMapItem *mapItem) override;
    QGeoCoordinate getCoordinates();

protected:
    void findFrequency();
    void updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track);
    void updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track);

    friend ObjectMapModel;
    friend CZML;
    QDateTime m_positionDateTime;
    bool m_useHeadingPitchRoll;
    float m_heading;
    float m_pitch;
    float m_roll;
    QDateTime m_orientationDateTime;
    QString m_image;
    int m_imageRotation;
    QString m_text;
    double m_frequency;                 // Frequency to set
    QString m_frequencyString;
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
    QVariantList m_takenTrack;          // Line showing where the object has been
    QVariantList m_takenTrack1;
    QVariantList m_takenTrack2;
    QGeoCoordinate m_takenStart1;
    QGeoCoordinate m_takenStart2;
    QGeoCoordinate m_takenEnd1;
    QGeoCoordinate m_takenEnd2;

    // For 3D map
    QString m_model;
    int m_altitudeReference;
    float m_labelAltitudeOffset;
    float m_modelAltitudeOffset;
    QList<CesiumInterface::Animation *> m_animations;
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
};

class PolylineMapItem : public MapItem {

public:
    PolylineMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
        MapItem(sourcePipe, group, itemSettings, mapItem)
    {
        update(mapItem);
    }
    void update(SWGSDRangel::SWGMapItem *mapItem) override;

protected:
    friend PolylineMapModel;
    friend CZML;

    QList<QGeoCoordinate *> m_points;   // FIXME: Remove?
    QVariantList m_polyline;
    QGeoRectangle m_bounds;             // Bounding boxes for the polyline, for view clipping
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

