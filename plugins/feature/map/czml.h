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

#ifndef INCLUDE_FEATURE_CZML_H_
#define INCLUDE_FEATURE_CZML_H_

#include <QHash>
#include <QJsonArray>
#include <QMatrix3x3>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <QJsonObject>

struct MapSettings;
class MapItem;

class CZML
{
private:
    const MapSettings *m_settings;
    QHash<QString, QString> m_ids;
    QHash<QString, QJsonArray> m_lastPosition;
    QHash<QString, bool> m_hasMoved;

public:
    CZML(const MapSettings *settings, QObject *parent = nullptr);
    QJsonObject init();
    QJsonObject update(MapItem *mapItem, bool isTarget, bool isSelected);

protected:
    QVector3D cartesian3FromDegrees(double longitude, double latitude, double height=0.0) const;
    QVector3D cartesianFromRadians(double longitude, double latitude, double height=0.0) const;
    QQuaternion fromHeadingPitchRoll(double heading, double pitch, double roll) const;
    QMatrix4x4 eastNorthUpToFixedFrame(QVector3D origin) const;
    QQuaternion fromRotation(QMatrix3x3 mat) const;
    QQuaternion orientation(double longitude, double latitude, double altitude, double heading, double pitch, double roll) const;

signals:
    void connected();
};

#endif // INCLUDE_FEATURE_CZML_H_
