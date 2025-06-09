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
            m_pathShow(false)
        { }
    };

private:
    const MapSettings *m_settings;
    QHash<QString, State> m_ids;
    QHash<QString, QJsonArray> m_lastPosition;
    QHash<QString, bool> m_hasMoved;
    QGeoCoordinate m_position;
    static const QStringList m_heightReferences;
    QFile m_file;
    QFile m_csvFile;

public:
    CZML(const MapSettings *settings);
    QJsonObject init();
    QJsonObject update(ObjectMapItem *mapItem, bool isTarget, bool isSelected);
    QJsonObject update(PolygonMapItem *mapItem);
    QJsonObject update(PolylineMapItem *mapItem);
    bool filter(const MapItem *mapItem) const;
    void setPosition(const QGeoCoordinate& position);

signals:
    void connected();
};

#endif // INCLUDE_FEATURE_CZML_H_
