///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2017, 2019-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#ifndef INCLUDE_FEATURE_CZML_H_
#define INCLUDE_FEATURE_CZML_H_

#include <QHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QGeoCoordinate>

#include "mapaircraftstate.h"

struct MapSettings;
class MapItem;
class ObjectMapItem;
class PolygonMapItem;
class PolylineMapItem;

class CZML
{
    // Record previous state of object, so we only send changes in state
    struct State {
        QString m_firstSeenDateTime;

        // For the early-out in update(): what the item and its settings looked like the
        // last time a packet was actually built for it
        bool m_seen = false;
        bool m_deleted = false;     // Cesium has already been told to delete this one
        quint32 m_settingsSignature = 0;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        float m_altitude = 0.0f;
        QDateTime m_altitudeDateTime;   // When m_altitude was set, to work out how fast it is being lost
        QString m_lastLabel;
        QString m_lastImage;

        QString m_modelGLTF;
        int m_modelAltitudeReference;
        bool m_modelShow;
        int m_modelMinimumPixelSize;

        quint32 m_pointColorInt;
        int m_pointAltitudeReference;
        bool m_pointShow;

        float m_lableAltitudeOffset;
        QString m_labelText;
        bool m_labelShow;
        float m_labelScale;
        int m_labelAltitudeReference;

        quint32 m_pathColorInt;
        bool m_pathShow;

        QString m_description;

        QDateTime m_aircraftStateDateTime;
        MapAircraftState m_aircraftState;
        float m_aircraftRoll;
        float m_aircraftPitch = 0.0f;
        QString m_ndFlightPlan;     // JSON, as last sent in the ndFlightPlan property
        QString m_dcduMessage;      // JSON, as last sent in the dcduMessage property
        QString m_loopSound;        // Sound-set directory or legacy file, as last sent

        // The 3D altitude coloured track, as last emitted - one polyline entity per
        // altitude band, ids "<item name> t3d <n>". See updateTrackSegments3D().
        QString m_trackPalette;          // Palette the segments were built with
        int m_trackPointCount = 0;       // Taken track points segmented so far
        int m_trackLastColorIndex = -1;
        QList<int> m_trackSegmentSizes;  // Points per segment (boundary points repeated)
        QList<int> m_trackSegmentBands;  // Altitude band per segment
        int m_trackSegmentsEmitted = 0;  // Entities created in Cesium, for deletion
        bool m_trackSegmentsShow = false;
        QGeoCoordinate m_trackFirstCoord; // Trim/rebuild detection
        QGeoCoordinate m_trackLastCoord;  // Same-size provisional-tail replacement
        quint64 m_trackRevision = 0;      // Any rewrite, including timestamp-only changes
        QDateTime m_trackTailEmit;       // Throttle for the growing tail segment

        // Orientation and availability as last sent, so a packet raised by something
        // else - a new altitude, say - does not carry them again unchanged
        int m_orientationMode = -1;      // -1 never sent, 0 velocityReference, 1 sampled quaternion
        QJsonArray m_orientationQuaternion;
        int m_orientationExtrapolate = -1;
        QString m_availability;

        State() :
            m_modelAltitudeReference(0),
            m_modelShow(false),
            m_modelMinimumPixelSize(0),
            m_pointColorInt(0),
            m_pointAltitudeReference(0),
            m_pointShow(false),
            m_lableAltitudeOffset(0.0),
            m_labelShow(false),
            m_labelScale(0.0),
            m_labelAltitudeReference(0),
            m_pathColorInt(0),
            m_pathShow(false),
            m_aircraftState{},
            m_aircraftRoll(0.0f)
        { }
    };

private:
    const MapSettings *m_settings;
    QHash<QString, State> m_ids;
    // Whether a delete needs sending for this id, and if so records that it has been
    bool deleteNeeded(const QString& id);
    // Records that Cesium has been told about this id, so a later delete is sent
    void described(const QString& id);
    QHash<QString, QJsonArray> m_lastPosition;
    QHash<QString, bool> m_hasMoved;
    QGeoCoordinate m_position;
    static const QStringList m_heightReferences;
    QFile m_file;
    QFile m_csvFile;

public:
    CZML(const MapSettings *settings);
    QJsonObject init();
    // Forget everything cached about what Cesium has been told. Call whenever Cesium is
    // made to drop its entities, or the next description of each one is a difference
    // against a state that no longer exists there - see update(ObjectMapItem *).
    void clear();
    QJsonObject update(ObjectMapItem *mapItem, bool isTarget, bool isSelected);
    // The altitude coloured track segments for the 3D map. Call BEFORE update(), which
    // resets the item's cached state when the item is deleted.
    QList<QJsonObject> updateTrackSegments3D(ObjectMapItem *mapItem, bool isSelected);
    QJsonObject update(PolygonMapItem *mapItem);
    QJsonObject update(PolylineMapItem *mapItem);
    bool filter(const MapItem *mapItem) const;
    void setPosition(const QGeoCoordinate& position);

signals:
    void connected();
};

#endif // INCLUDE_FEATURE_CZML_H_
