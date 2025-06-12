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
    if (mapItem->getLabelDateTime()) {
        m_labelDateTime = QDateTime::fromString(*mapItem->getLabelDateTime(), Qt::ISODateWithMs);
    } else {
        m_labelDateTime = QDateTime();
    }
    m_latitude = mapItem->getLatitude();
    m_longitude = mapItem->getLongitude();
    m_altitude = mapItem->getAltitude();
}

QGeoCoordinate MapItem::getCoordinates()
{
    QGeoCoordinate coords;
    coords.setLatitude(m_latitude);
    coords.setLongitude(m_longitude);
    return coords;
}

WhittakerEilers ObjectMapItem::m_filter;

void ObjectMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);
    if (mapItem->getPositionDateTime()) {
        m_positionDateTime = QDateTime::fromString(*mapItem->getPositionDateTime(), Qt::ISODateWithMs);
    } else {
        m_positionDateTime = QDateTime();
    }
    if (mapItem->getAltitudeDateTime()) {
        m_altitudeDateTime = QDateTime::fromString(*mapItem->getAltitudeDateTime(), Qt::ISODateWithMs);
    } else {
        m_altitudeDateTime = QDateTime();
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
    // FIXME: See nodeTransformations comment in czml.cpp
    // We can't use nodeTransformations, so adjust altitude instead
    if (m_modelAltitudeOffset != 0)
    {
        m_labelAltitudeOffset -= m_modelAltitudeOffset;
        m_altitude += m_modelAltitudeOffset;
        m_modelAltitudeOffset = 0;
    }
    m_altitudeReference = mapItem->getAltitudeReference();
    m_fixedPosition = mapItem->getFixedPosition();
    QList<SWGSDRangel::SWGMapAnimation *> *animations = mapItem->getAnimations();
    if (animations)
    {
        for (auto animation : *animations) {
            m_animations.append(new CesiumInterface::Animation(animation));
        }
    }
    findFrequencies();
    if (!m_fixedPosition)
    {
        updateTrack(mapItem->getTrack(), m_itemSettings);
        updatePredictedTrack(mapItem->getPredictedTrack());
    }
    if (mapItem->getAvailableFrom()) {
        m_availableFrom = QDateTime::fromString(*mapItem->getAvailableFrom(), Qt::ISODateWithMs);
    } else {
        m_availableFrom = QDateTime();
    }
    if (mapItem->getAvailableUntil()) {
        m_availableUntil = QDateTime::fromString(*mapItem->getAvailableUntil(), Qt::ISODateWithMs);
    } else {
        m_availableUntil = QDateTime();
    }
    if (mapItem->getAircraftState()) {
        if (!m_aircraftState) {
            m_aircraftState = new MapAircraftState();
        }
        SWGSDRangel::SWGMapAircraftState *as = mapItem->getAircraftState();
        if (as->getCallsign()) {
            m_aircraftState->m_callsign = *as->getCallsign();
        }
        if (as->getAircraftType()) {
            m_aircraftState->m_aircraftType = *as->getAircraftType();
        }
        m_aircraftState->m_onSurface = as->getOnSurface();
        m_aircraftState->m_indicatedAirspeed = as->getAirspeed();
        if (as->getAirspeedDateTime()) {
            m_aircraftState->m_indicatedAirspeedDateTime = *as->getAirspeedDateTime();
        } else {
            m_aircraftState->m_indicatedAirspeedDateTime = QString();
        }
        m_aircraftState->m_trueAirspeed = as->getTrueAirspeed();
        m_aircraftState->m_groundspeed = as->getGroundspeed();
        m_aircraftState->m_mach = as->getMach();
        m_aircraftState->m_altitude = as->getAltitude();
        if (as->getAltitudeDateTime()) {
            m_aircraftState->m_altitudeDateTime = *as->getAltitudeDateTime();
        } else {
            m_aircraftState->m_altitudeDateTime = QString();
        }
        m_aircraftState->m_qnh = as->getQnh();
        m_aircraftState->m_verticalSpeed = as->getVerticalSpeed();
        m_aircraftState->m_heading = as->getHeading();
        m_aircraftState->m_track = as->getTrack();
        m_aircraftState->m_selectedAltitude = as->getSelectedAltitude();
        m_aircraftState->m_selectedHeading = as->getSelectedHeading();
        m_aircraftState->m_autopilot = as->getAutopilot();
        m_aircraftState->m_verticalMode = (MapAircraftState::VerticalMode) as->getVerticalMode();
        m_aircraftState->m_lateralMode = (MapAircraftState::LateralMode) as->getLateralMode();
        m_aircraftState->m_tcasMode = (MapAircraftState::TCASMode) as->getTcasMode();
        m_aircraftState->m_windSpeed = as->getWindSpeed();
        m_aircraftState->m_windDirection = as->getWindDirection();
        m_aircraftState->m_staticAirTemperature = as->getStaticAirTemperature();
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
    m_deleted = *mapItem->getImage() == "";

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
    m_deleted = *mapItem->getImage() == "";

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

// Look for a frequency in the text for this object
void ObjectMapItem::findFrequencies()
{
    m_frequencies.clear();
    m_frequencyStrings.clear();

    const QRegularExpression re("(([0-9]+(\\.[0-9]+)?) *([kMG])?Hz)");
    QRegularExpressionMatchIterator itr = re.globalMatch(m_text);
    while (itr.hasNext())
    {
        QRegularExpressionMatch match = itr.next();
        QStringList capture = match.capturedTexts();
        double frequency = capture[2].toDouble();

        if (capture.length() == 5)
        {
            QChar unit = capture[4][0];
            if (unit == 'k') {
                frequency *= 1000;
            } else if (unit == 'M') {
                frequency *= 1000000;
            } else if (unit == 'G') {
                frequency *= 1000000000;
            }
        }
        m_frequencies.append((qint64)frequency);
        m_frequencyStrings.append(capture[0]);
    }
}

void ObjectMapItem::extrapolatePosition(QGeoCoordinate *c, const QDateTime& dateTime)
{
    int p1;
    int p2;

    // Find last two non extrapolated position
    for (p2 = m_takenTrackPositionExtrapolated.size() - 1 ; p2 >= 0; p2--)
    {
        if (!m_takenTrackPositionExtrapolated[p2]) {
            break;
        }
    }
    for (p1 = p2 - 1 ; p1 >= 0; p1--)
    {
        if (!m_takenTrackPositionExtrapolated[p1]) {
            break;
        }
    }

    if (p1 < 0) {
        return;
    }


    qint64 t1 = m_takenTrackDateTimes[p1]->msecsTo(*m_takenTrackDateTimes[p2]);
    qint64 t2 = m_takenTrackDateTimes[p2]->msecsTo(dateTime);

    double latV = (m_takenTrackCoords[p2]->latitude() - m_takenTrackCoords[p1]->latitude()) / t1;
    double lonV = (m_takenTrackCoords[p2]->longitude() - m_takenTrackCoords[p1]->longitude()) / t1;

    double newLat = m_takenTrackCoords[p2]->latitude() + latV * t2;
    double newLon = m_takenTrackCoords[p2]->longitude() + lonV * t2;

    c->setLatitude(newLat);
    c->setLongitude(newLon);
}

void ObjectMapItem::extrapolateAltitude(QGeoCoordinate *c, const QDateTime& dateTime)
{
    int p1;
    int p2;

    // Find last two non extrapolated position
    for (p2 = m_takenTrackPositionExtrapolated.size() - 1 ; p2 >= 0; p2--)
    {
        if (!m_takenTrackPositionExtrapolated[p2]) {
            break;
        }
    }
    for (p1 = p2 - 1 ; p1 >= 0; p1--)
    {
        if (!m_takenTrackPositionExtrapolated[p1]) {
            break;
        }
    }

    if (p1 < 0) {
        return;
    }


    qint64 t1 = m_takenTrackDateTimes[p1]->msecsTo(*m_takenTrackDateTimes[p2]);
    qint64 t2 = m_takenTrackDateTimes[p2]->msecsTo(dateTime);

    double vertV = (m_takenTrackCoords[p2]->altitude() - m_takenTrackCoords[p1]->altitude()) / t1;

    double newAlt = m_takenTrackCoords[p2]->latitude() + vertV * t2;

    c->setAltitude(newAlt);
}

void ObjectMapItem::interpolatePosition(int p2, const float p3Latitude, const float p3Longitude, const QDateTime &p3DateTime)
{
    // p1 last non extrapolated position
    // p2 interpolated position
    // p3 current non extrapolated position

    // Find last non extrapolated position
    int p1;

    for (p1 = p2 - 1; p1 >= 0; p1--)
    {
        if (!m_takenTrackPositionExtrapolated[p1]) {
            break;
        }
    }
    if (p1 < 0) {
        return;
    }

    qint64 t1 = m_takenTrackDateTimes[p1]->msecsTo(p3DateTime);
    qint64 t2 = m_takenTrackDateTimes[p1]->msecsTo(*m_takenTrackDateTimes[p2]);

    double latV = (p3Latitude - m_takenTrackCoords[p1]->latitude()) / t1;
    double lonV = (p3Longitude - m_takenTrackCoords[p1]->longitude()) / t1;

    double newLat = m_takenTrackCoords[p1]->latitude() + latV * t2;
    double newLon = m_takenTrackCoords[p1]->longitude() + lonV * t2;

    m_takenTrackCoords[p2]->setLatitude(newLat);
    m_takenTrackCoords[p2]->setLongitude(newLon);

    m_interpolatedCoords.append(m_takenTrackCoords[p2]);
    m_interpolatedDateTimes.append(m_takenTrackDateTimes[p2]);
}

void ObjectMapItem::interpolateAltitude(int p2, const float p3Altitude, const QDateTime &p3DateTime)
{
    // p1 last non extrapolated position
    // p2 interpolated position
    // p3 current non extrapolated position

    // Find last non extrapolated position
    int p1;

    for (p1 = p2 - 1; p1 >= 0; p1--)
    {
        if (!m_takenTrackPositionExtrapolated[p1]) {
            break;
        }
    }
    if (p1 < 0) {
        return;
    }

    qint64 t1 = m_takenTrackDateTimes[p1]->msecsTo(p3DateTime);
    qint64 t2 = m_takenTrackDateTimes[p1]->msecsTo(*m_takenTrackDateTimes[p2]);

    double vertV = (p3Altitude - m_takenTrackCoords[p1]->altitude()) / t1;

    double newAlt = m_takenTrackCoords[p1]->altitude() + vertV * t2;

    m_takenTrackCoords[p2]->setAltitude(newAlt);

    m_interpolatedCoords.append(m_takenTrackCoords[p2]);
    m_interpolatedDateTimes.append(m_takenTrackDateTimes[p2]);
}

void ObjectMapItem::updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track, MapSettings::MapItemSettings *itemSettings)
{
    if (track != nullptr)
    {
        qDeleteAll(m_takenTrackCoords);
        m_takenTrackCoords.clear();
        qDeleteAll(m_takenTrackDateTimes);
        m_takenTrackDateTimes.clear();
        m_takenTrackPositionExtrapolated.clear();
        m_takenTrackAltitudeExtrapolated.clear();
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
            m_takenTrackPositionExtrapolated.push_back(false);
            m_takenTrackAltitudeExtrapolated.push_back(false);
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
            if (m_altitudeDateTime.isValid()) {
                m_takenTrackDateTimes.push_back(new QDateTime(m_altitudeDateTime));
            } else if (m_positionDateTime.isValid()) {
                m_takenTrackDateTimes.push_back(new QDateTime(m_positionDateTime));
            } else {
                m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTime()));
            }
            m_takenTrackPositionExtrapolated.push_back(false);
            m_takenTrackAltitudeExtrapolated.push_back(false);
            m_takenTrack.push_back(QVariant::fromValue(*c));
        }
        else
        {
            // For Whittaker-Eilers filtering, we need to make sure we don't have 2 data with the same time
            // so we just update the last item if the prev time is the same
            // To reduce size of list for stationary items, we only store two items with same position
            // We store two, rather than one, so that we have the times this position was arrived at and left

            const bool interpolate = false;
            const bool onlyActual2D = true; // Extrapolation / smoothing doesn't look good on 2D map, so only use actual data from aircraft, not extrapolated/interpolated points

            QGeoCoordinate *prev1 = m_takenTrackCoords.last();
            bool samePos1 = (prev1->latitude() == m_latitude) && (prev1->longitude() == m_longitude) && (prev1->altitude() == m_altitude);
            QGeoCoordinate *prev2 = m_takenTrackCoords.size() > 1 ? m_takenTrackCoords[m_takenTrackCoords.size() - 2] : nullptr;
            bool samePos2 = prev2 && samePos1 ? (prev2->latitude() == m_latitude) && (prev2->longitude() == m_longitude) && (prev2->altitude() == m_altitude) : false;

            QDateTime *prevDateTime = m_takenTrackDateTimes.last();

            QGeoCoordinate c(m_latitude, m_longitude, m_altitude);
            QGeoCoordinate cActual = c;
            int prevSize = m_takenTrackPositionExtrapolated.size();

            if (m_altitudeDateTime.isValid() && m_positionDateTime.isValid() && (m_altitudeDateTime > m_positionDateTime))
            {
                if (interpolate)
                {
                    for (int i = m_takenTrackAltitudeExtrapolated.size() - 1; (i >= 0) && m_takenTrackAltitudeExtrapolated[i]; i--)
                    {
                        interpolateAltitude(i, m_altitude, m_altitudeDateTime);
                        m_takenTrackAltitudeExtrapolated[i] = false;
                    }
                }

                if (samePos2)
                {
                    *m_takenTrackDateTimes.last() = m_altitudeDateTime;
                }
                else
                {
                    extrapolatePosition(&c, m_altitudeDateTime);
                    if (m_altitudeDateTime == *prevDateTime)
                    {
                        m_takenTrackPositionExtrapolated[m_takenTrackPositionExtrapolated.size() - 1] = true;
                        *m_takenTrackCoords.last() = c;
                        m_takenTrack.last() = QVariant::fromValue(onlyActual2D ? cActual : c);
                    }
                    else
                    {
                        m_takenTrackDateTimes.push_back(new QDateTime(m_altitudeDateTime));
                        m_takenTrackPositionExtrapolated.push_back(true);
                        m_takenTrackAltitudeExtrapolated.push_back(false);
                        m_takenTrackCoords.push_back(new QGeoCoordinate(c));
                        m_takenTrack.push_back(QVariant::fromValue(onlyActual2D ? cActual : c));
                    }
                }
            }
            else if (m_positionDateTime.isValid())
            {
                if (interpolate)
                {
                    for (int i = m_takenTrackPositionExtrapolated.size() - 1; (i >= 0) && m_takenTrackPositionExtrapolated[i]; i--)
                    {
                        interpolatePosition(i, m_latitude, m_longitude, m_positionDateTime);
                        m_takenTrackPositionExtrapolated[i] = false;
                    }
                }

                if (m_positionDateTime > *m_takenTrackDateTimes.last())
                {
                    if (samePos2)
                    {
                        *m_takenTrackDateTimes.last() = m_positionDateTime;
                    }
                    else
                    {
                        bool extrapolateAlt = m_altitudeDateTime.isValid() && (m_positionDateTime > m_altitudeDateTime);
                        if (extrapolateAlt) {
                            extrapolateAltitude(&c, m_positionDateTime);
                        }
                        if (m_positionDateTime == *prevDateTime)
                        {
                            m_takenTrackAltitudeExtrapolated[m_takenTrackPositionExtrapolated.size() - 1] = extrapolateAlt;
                            *m_takenTrackCoords.last() = c;
                            m_takenTrack.last() = QVariant::fromValue(onlyActual2D ? cActual : c);
                        }
                        else
                        {
                            m_takenTrackDateTimes.push_back(new QDateTime(m_positionDateTime));
                            m_takenTrackPositionExtrapolated.push_back(false);
                            m_takenTrackAltitudeExtrapolated.push_back(extrapolateAlt);
                            m_takenTrackCoords.push_back(new QGeoCoordinate(c));
                            m_takenTrack.push_back(QVariant::fromValue(onlyActual2D ? cActual : c));
                        }
                    }
                }
                else
                {
                    //qDebug() << "m_positionDateTime matches last datetime" << samePos1 << samePos2;
                }
            }
            else
            {
                m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTime()));
                m_takenTrackPositionExtrapolated.push_back(false);
                m_takenTrackAltitudeExtrapolated.push_back(false);
                m_takenTrackCoords.push_back(new QGeoCoordinate(c));
                m_takenTrack.push_back(QVariant::fromValue(onlyActual2D ? cActual : c));
            }

            /*if (m_takenTrackDateTimes.size() >= 2) {
                if (*m_takenTrackDateTimes[m_takenTrackDateTimes.size() - 1] < *m_takenTrackDateTimes[m_takenTrackDateTimes.size() - 2]) {
                    qDebug() << "ObjectMapItem::updateTrack: Out of order";
                }
            }*/

            if ((m_takenTrackPositionExtrapolated.size() > 0) && (prevSize != m_takenTrackPositionExtrapolated.size()))
            {
                const int filterLen = itemSettings->m_smoothingWindow;
                if ((filterLen > 0) && (m_takenTrackCoords.size() >= filterLen) && (m_takenTrackCoords.size() % (filterLen/2)) == 0)
                {
                    // Filter last filterLen coords
                    QVector<double> x(filterLen);
                    QVector<double> y1(filterLen);
                    QVector<double> y2(filterLen);
                    QVector<double> y3(filterLen);
                    QVector<double> w1(filterLen);
                    QVector<double> w3(filterLen);

                    //qDebug() << "Filter from" << (m_takenTrackCoords.size() - (filterLen - 0)) << "to" << (m_takenTrackCoords.size() - 1);
                    for (int i = 0; i < filterLen; i++)
                    {
                        int idx = m_takenTrackCoords.size() - (filterLen - i);
                        x[i] = (m_takenTrackDateTimes[idx]->toMSecsSinceEpoch() - m_takenTrackDateTimes[0]->toMSecsSinceEpoch()) / 1000.0;
                        y1[i] = m_takenTrackCoords[idx]->latitude();
                        y2[i] = m_takenTrackCoords[idx]->longitude();
                        y3[i] = m_takenTrackCoords[idx]->altitude();
                        if (i < (filterLen / 4))
                        {
                            w1[i] = 10.0; // Try to avoid discontinuities between windows
                            w3[i] = 10.0;
                        }
                        else if (i == filterLen - 1)
                        {
                            w1[i] = 1.0;
                            w3[i] = 1.0;
                        }
                        else
                        {
                            w1[i] = m_takenTrackPositionExtrapolated[idx] ? 0.0 : 1.0;
                            w3[i] = m_takenTrackAltitudeExtrapolated[idx] ? 0.0 : 1.0;
                        }
                    }

                    const double lambda = itemSettings->m_smoothingLambda;
                    m_filter.filter(x.data(), y1.data(), w1.data(), filterLen, lambda);
                    m_filter.filter(x.data(), y2.data(), w1.data(), filterLen, lambda);
                    m_filter.filter(x.data(), y3.data(), w3.data(), filterLen, lambda);

                    for (int i = 0; i < filterLen; i++)
                    {
                        int idx = m_takenTrackCoords.size() - (filterLen - i);
                        m_takenTrackCoords[idx]->setLatitude(y1[i]);
                        m_takenTrackCoords[idx]->setLongitude(y2[i]);
                        m_takenTrackCoords[idx]->setAltitude(y3[i]);
                        m_takenTrackPositionExtrapolated[idx] = false;
                        m_takenTrackAltitudeExtrapolated[idx] = false;
                        if (!onlyActual2D) {
                            m_takenTrack[idx] = QVariant::fromValue(QGeoCoordinate(y1[i], y2[i], y3[i]));
                        }

                        m_interpolatedCoords.append(m_takenTrackCoords[idx]);
                        m_interpolatedDateTimes.append(m_takenTrackDateTimes[idx]);
                    }

                    // Update current position - Don't do this, as it can make aircraft move backwards on 2D map
                    if (!onlyActual2D)
                    {
                        m_latitude = m_takenTrackCoords.back()->latitude();
                        m_longitude = m_takenTrackCoords.back()->longitude();
                        m_altitude = m_takenTrackCoords.back()->altitude();
                    }
                }
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

