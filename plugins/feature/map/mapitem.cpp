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

#include "mapitem.h"

MapItem::MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
    m_altitude(0.0)
{
    m_sourcePipe = sourcePipe;
    m_group = group;
    m_itemSettings = itemSettings;
    m_name = *mapItem->getName();
    m_hashKey = m_sourcePipe->objectName() + m_name;
}

void MapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    if (mapItem->getLabel()) {
        m_label = *mapItem->getLabel();
    } else {
        m_label = "";
    }
    m_latitude = mapItem->getLatitude();
    m_longitude = mapItem->getLongitude();
    m_altitude = mapItem->getAltitude();
}

void ObjectMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);
    if (mapItem->getPositionDateTime()) {
        m_positionDateTime = QDateTime::fromString(*mapItem->getPositionDateTime(), Qt::ISODateWithMs);
    } else {
        m_positionDateTime = QDateTime();
    }
    m_useHeadingPitchRoll = mapItem->getOrientation() == 1;
    m_heading = mapItem->getHeading();
    m_pitch = mapItem->getPitch();
    m_roll = mapItem->getRoll();
    if (mapItem->getOrientationDateTime()) {
        m_orientationDateTime = QDateTime::fromString(*mapItem->getOrientationDateTime(), Qt::ISODateWithMs);
    } else {
        m_orientationDateTime = QDateTime();
    }
    m_image = *mapItem->getImage();
    m_imageRotation = mapItem->getImageRotation();
    QString *text = mapItem->getText();
    if (text != nullptr) {
        m_text = text->replace("\n", "<br>");  // Convert to HTML
    } else {
        m_text = "";
    }
    if (mapItem->getModel()) {
        m_model = *mapItem->getModel();
    } else {
        m_model = "";
    }
    m_labelAltitudeOffset = mapItem->getLabelAltitudeOffset();
    m_modelAltitudeOffset = mapItem->getModelAltitudeOffset();
    m_altitudeReference = mapItem->getAltitudeReference();
    m_fixedPosition = mapItem->getFixedPosition();
    QList<SWGSDRangel::SWGMapAnimation *> *animations = mapItem->getAnimations();
    if (animations)
    {
        for (auto animation : *animations) {
            m_animations.append(new CesiumInterface::Animation(animation));
        }
    }
    findFrequency();
    if (!m_fixedPosition)
    {
        updateTrack(mapItem->getTrack());
        updatePredictedTrack(mapItem->getPredictedTrack());
    }
    if (mapItem->getAvailableUntil()) {
        m_availableUntil = QDateTime::fromString(*mapItem->getAvailableUntil(), Qt::ISODateWithMs);
    } else {
        m_availableUntil = QDateTime();
    }
}

void ImageMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);

    m_image = "data:image/png;base64," + *mapItem->getImage();
    m_imageZoomLevel = mapItem->getImageZoomLevel();

    float east = mapItem->getImageTileEast();
    float west = mapItem->getImageTileWest();
    float north = mapItem->getImageTileNorth();
    float south = mapItem->getImageTileSouth();
    m_latitude = north - (north - south) / 2.0;
    m_longitude = east - (east - west) / 2.0;
    m_bounds = QGeoRectangle(QGeoCoordinate(north, west), QGeoCoordinate(south, east));
}

void PolygonMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);
    m_extrudedHeight = mapItem->getExtrudedHeight();
    m_colorValid = mapItem->getColorValid();
    m_color = mapItem->getColor();
    m_altitudeReference = mapItem->getAltitudeReference();

    qDeleteAll(m_points);
    m_points.clear();
    QList<SWGSDRangel::SWGMapCoordinate *> *coords = mapItem->getCoordinates();
    if (coords)
    {
        for (int i = 0; i < coords->size(); i++)
        {
            SWGSDRangel::SWGMapCoordinate* p = coords->at(i);
            QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
            m_points.append(c);
        }
    }

    // Calculate bounds
    m_polygon.clear();
    qreal latMin = 90.0, latMax = -90.0, lonMin = 180.0, lonMax = -180.0;
    for (const auto p : m_points)
    {
        QGeoCoordinate coord = *p;
        latMin = std::min(latMin, coord.latitude());
        latMax = std::max(latMax, coord.latitude());
        lonMin = std::min(lonMin, coord.longitude());
        lonMax = std::max(lonMax, coord.longitude());
        m_polygon.push_back(QVariant::fromValue(coord));
    }
    m_bounds = QGeoRectangle(QGeoCoordinate(latMax, lonMin), QGeoCoordinate(latMin, lonMax));
}

void PolylineMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);
    m_colorValid = mapItem->getColorValid();
    m_color = mapItem->getColor();
    m_altitudeReference = mapItem->getAltitudeReference();

    qDeleteAll(m_points);
    m_points.clear();
    QList<SWGSDRangel::SWGMapCoordinate *> *coords = mapItem->getCoordinates();
    if (coords)
    {
        for (int i = 0; i < coords->size(); i++)
        {
            SWGSDRangel::SWGMapCoordinate* p = coords->at(i);
            QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
            m_points.append(c);
        }
    }

    // Calculate bounds
    m_polyline.clear();
    qreal latMin = 90.0, latMax = -90.0, lonMin = 180.0, lonMax = -180.0;
    for (const auto p : m_points)
    {
        QGeoCoordinate coord = *p;
        latMin = std::min(latMin, coord.latitude());
        latMax = std::max(latMax, coord.latitude());
        lonMin = std::min(lonMin, coord.longitude());
        lonMax = std::max(lonMax, coord.longitude());
        m_polyline.push_back(QVariant::fromValue(coord));
    }
    m_bounds = QGeoRectangle(QGeoCoordinate(latMax, lonMin), QGeoCoordinate(latMin, lonMax));
}

QGeoCoordinate ObjectMapItem::getCoordinates()
{
    QGeoCoordinate coords;
    coords.setLatitude(m_latitude);
    coords.setLongitude(m_longitude);
    return coords;
}

void ObjectMapItem::findFrequency()
{
    // Look for a frequency in the text for this object
    QRegExp re("(([0-9]+(\\.[0-9]+)?) *([kMG])?Hz)");
    if (re.indexIn(m_text) != -1)
    {
        QStringList capture = re.capturedTexts();
        m_frequency = capture[2].toDouble();
        if (capture.length() == 5)
        {
            QChar unit = capture[4][0];
            if (unit == 'k')
                m_frequency *= 1000.0;
            else if (unit == 'M')
                m_frequency *= 1000000.0;
            else if (unit == 'G')
                m_frequency *= 1000000000.0;
        }
        m_frequencyString = capture[0];
    }
    else
    {
        m_frequency = 0.0;
    }
}

void ObjectMapItem::updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
{
    if (track != nullptr)
    {
        qDeleteAll(m_takenTrackCoords);
        m_takenTrackCoords.clear();
        qDeleteAll(m_takenTrackDateTimes);
        m_takenTrackDateTimes.clear();
        m_takenTrack.clear();
        m_takenTrack1.clear();
        m_takenTrack2.clear();
        for (int i = 0; i < track->size(); i++)
        {
            SWGSDRangel::SWGMapCoordinate* p = track->at(i);
            QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
            QDateTime *d = new QDateTime(QDateTime::fromString(*p->getDateTime(), Qt::ISODate));
            m_takenTrackCoords.push_back(c);
            m_takenTrackDateTimes.push_back(d);
            m_takenTrack.push_back(QVariant::fromValue(*c));
        }
    }
    else
    {
        // Automatically create a track
        if (m_takenTrackCoords.size() == 0)
        {
            QGeoCoordinate *c = new QGeoCoordinate(m_latitude, m_longitude, m_altitude);
            m_takenTrackCoords.push_back(c);
            if (m_positionDateTime.isValid()) {
                m_takenTrackDateTimes.push_back(new QDateTime(m_positionDateTime));
            } else {
                m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTime()));
            }
            m_takenTrack.push_back(QVariant::fromValue(*c));
        }
        else
        {
            QGeoCoordinate *prev = m_takenTrackCoords.last();
            QDateTime *prevDateTime = m_takenTrackDateTimes.last();
            if ((prev->latitude() != m_latitude) || (prev->longitude() != m_longitude)
                || (prev->altitude() != m_altitude) || (*prevDateTime != m_positionDateTime))
            {
                QGeoCoordinate *c = new QGeoCoordinate(m_latitude, m_longitude, m_altitude);
                m_takenTrackCoords.push_back(c);
                if (m_positionDateTime.isValid()) {
                    m_takenTrackDateTimes.push_back(new QDateTime(m_positionDateTime));
                } else {
                    m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTime()));
                }
                m_takenTrack.push_back(QVariant::fromValue(*c));
            }
        }
    }
}

void ObjectMapItem::updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
{
    if (track != nullptr)
    {
        qDeleteAll(m_predictedTrackCoords);
        m_predictedTrackCoords.clear();
        qDeleteAll(m_predictedTrackDateTimes);
        m_predictedTrackDateTimes.clear();
        m_predictedTrack.clear();
        m_predictedTrack1.clear();
        m_predictedTrack2.clear();
        for (int i = 0; i < track->size(); i++)
        {
            SWGSDRangel::SWGMapCoordinate* p = track->at(i);
            QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
            QDateTime *d = new QDateTime(QDateTime::fromString(*p->getDateTime(), Qt::ISODate));
            m_predictedTrackCoords.push_back(c);
            m_predictedTrackDateTimes.push_back(d);
            m_predictedTrack.push_back(QVariant::fromValue(*c));
        }
    }
}

