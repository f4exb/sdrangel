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

#include <algorithm>

#include <QFile>
#include <QFileInfo>

#include "mapitem.h"
#include "util/aircraft3dmodels.h"
#include "util/profiler.h"
// For TrackColorModel, which mapitem.h only forward declares
#include "mapmodel.h"

// Map item times are held in UTC. A local QDateTime works its offset out afresh on every
// comparison and difference: a call into the C runtime's time zone code under a lock Qt
// shares between all threads. The map makes thousands a second - CZML's secsTo alone was
// 15% of the GUI thread. SDRangel's own senders write UTC, which parses with no lookup
// and makes toUTC() free; a string with no offset (an older sender, or the web API) is
// local time and is converted once, here
static QDateTime parseDateTime(const QString& dateTime, Qt::DateFormat format = Qt::ISODateWithMs)
{
    return QDateTime::fromString(dateTime, format).toUTC();
}

// These have to live here rather than in the header, where TrackColorModel is an
// incomplete type. "delete m_trackColorModel" there compiled, but freed the memory
// without running ~QObject, so QQmlData::destroyed() never fired and QML kept a wrapper
// pointing into the freed block. The crash came much later and nowhere near: the first
// garbage collection to walk its weak values read the reused memory as a QObject and
// died in markWeakValues. MSVC does warn (C4150), but it is level 2 and this builds at
// the default /W1.
// deleteLater rather than delete, because a MapItemView tears its delegates down on a
// later pass of the event loop, after the item that owned the model has gone.
ObjectMapItem::~ObjectMapItem()
{
    delete m_aircraftState;
    qDeleteAll(m_takenTrackCoords);
    qDeleteAll(m_takenTrackDateTimes);
    if (m_trackColorModel) {
        m_trackColorModel->deleteLater();
    }
}

PolylineMapItem::~PolylineMapItem()
{
    if (m_trackColorModel) {
        m_trackColorModel->deleteLater();
    }
}

MapItem::MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
    m_latitude(0.0f),
    m_longitude(0.0f),
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
    m_changed = 0;

    const QString label = mapItem->getLabel() ? *mapItem->getLabel() : QString();
    if (label != m_label)
    {
        m_label = label;
        m_changed |= ChangedLabel;
    }
    if (mapItem->getLabelDateTime()) {
        m_labelDateTime = parseDateTime(*mapItem->getLabelDateTime());
    } else {
        m_labelDateTime = QDateTime();
    }
    if (   (m_latitude != mapItem->getLatitude())
        || (m_longitude != mapItem->getLongitude())
        || (m_altitude != mapItem->getAltitude()))
    {
        m_latitude = mapItem->getLatitude();
        m_longitude = mapItem->getLongitude();
        m_altitude = mapItem->getAltitude();
        m_changed |= ChangedPosition;
    }
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
    // An empty image means take it off the map. The 2D side reads that in the model, but
    // the 3D side is driven from the item, so it has to be recorded here or Cesium keeps
    // the entity - which is what left a route's waypoint markers and their names on the
    // 3D map after the route itself had gone.
    //
    // Null checked, unlike the polygon and polyline versions of this: those are only ever
    // built from the Map feature's own dispatch, which has already tested the image, while
    // an object is built from several places and some of them set no image at all.
    m_deleted = mapItem->getImage() && mapItem->getImage()->isEmpty();

    PROFILER_START()
    MapItem::update(mapItem);
    if (mapItem->getPositionDateTime()) {
        m_positionDateTime = parseDateTime(*mapItem->getPositionDateTime());
        m_positionDateTimeString = *mapItem->getPositionDateTime();
    } else {
        m_positionDateTime = QDateTime();
        m_positionDateTimeString.clear();
    }
    if (mapItem->getAltitudeDateTime()) {
        m_altitudeDateTime = parseDateTime(*mapItem->getAltitudeDateTime());
    } else {
        m_altitudeDateTime = QDateTime();
    }
    m_useHeadingPitchRoll = mapItem->getOrientation() == 1;
    m_heading = mapItem->getHeading();
    m_pitch = mapItem->getPitch();
    m_roll = mapItem->getRoll();
    if (mapItem->getOrientationDateTime()) {
        m_orientationDateTime = parseDateTime(*mapItem->getOrientationDateTime());
    } else {
        m_orientationDateTime = QDateTime();
    }
    if ((m_image != *mapItem->getImage()) || (m_imageRotation != mapItem->getImageRotation()))
    {
        m_image = *mapItem->getImage();
        m_imageRotation = mapItem->getImageRotation();
        m_changed |= ChangedImage;
    }
    QString *text = mapItem->getText();
    QString newText = text != nullptr ? text->replace("\n", "<br>") : QString();  // Convert to HTML
    bool textChanged = newText != m_text;
    if (textChanged)
    {
        m_text = newText;
        m_changed |= ChangedText;
    }
    QString model = mapItem->getModel() ? *mapItem->getModel() : QString();
    if (model != m_model)
    {
        m_model = model;
        // The livery icons are named after the 3D models they were rendered from
        // (e.g. BB_Airbus_png/A320/A320_BAW.gltf -> A320_BAW.png), so an aircraft's
        // icon follows its model's basename. The resources come from the plugin that
        // sent the item (such as the ADS-B demod), so may not be registered
        m_liveryImage = "";
        if (!m_model.isEmpty())
        {
            QString base = Aircraft3DModels::liveryIconName(m_model);
            if (QFile::exists(":/map/liveries/" + base + ".png")) {
                m_liveryImage = "qrc:///map/liveries/" + base + ".png";
            }
        }
        m_changed |= ChangedImage;
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
    if (textChanged) {
        findFrequencies();
    }
    if (!m_fixedPosition)
    {
        PROFILER_START()
        updateTrack(mapItem->getTrack(), m_itemSettings);
        updateProvisionalTrackPoint();
        updatePredictedTrack(mapItem->getPredictedTrack());
        updateTakenTrackBounds();
        PROFILER_STOP("Map item track")
        m_changed |= ChangedTrack;
        if (mapItem->getPredictedTrack()) {
            m_changed |= ChangedPredictedTrack;
        }
    }

    // Anything else in here is only of interest to the 3D map
    m_changed |= Changed3D;
    if (mapItem->getAvailableFrom()) {
        m_availableFrom = parseDateTime(*mapItem->getAvailableFrom());
    } else {
        m_availableFrom = QDateTime();
    }
    if (mapItem->getAvailableUntil()) {
        m_availableUntil = parseDateTime(*mapItem->getAvailableUntil());
    } else {
        m_availableUntil = QDateTime();
    }
    if (mapItem->getLoopSound()) {
        m_loopSound = *mapItem->getLoopSound();
    }
    if (mapItem->getAircraftState()) {
        if (!m_aircraftState) {
            m_aircraftState = new MapAircraftState();
        }
        SWGSDRangel::SWGMapAircraftState *as = mapItem->getAircraftState();
        if (as->getDateTime()) {
            m_aircraftState->m_dateTime = parseDateTime(*as->getDateTime());
        } else {
            m_aircraftState->m_dateTime = QDateTime();
        }
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
        m_aircraftState->m_tcasRA = as->getTcasRa();
        m_aircraftState->m_tcasThreatIcao = as->getTcasThreatIcao() ? *as->getTcasThreatIcao() : QString();
        m_aircraftState->m_squawk = as->getSquawk();
        m_aircraftState->m_icao = as->getIcao() ? *as->getIcao() : QString();
        m_aircraftState->m_windSpeed = as->getWindSpeed();
        m_aircraftState->m_windDirection = as->getWindDirection();
        m_aircraftState->m_staticAirTemperature = as->getStaticAirTemperature();
        MapFlightPlan& plan = m_aircraftState->m_flightPlan;
        if (as->getFlightPlan() && as->getFlightPlan()->isSet())
        {
            SWGSDRangel::SWGMapFlightPlan *fp = as->getFlightPlan();
            plan.m_valid = true;
            plan.m_departure = fp->getDeparture() ? *fp->getDeparture() : QString();
            plan.m_arrival = fp->getArrival() ? *fp->getArrival() : QString();
            plan.m_arrivalRunway = fp->getArrivalRunway() ? *fp->getArrivalRunway() : QString();
            plan.m_approach = fp->getApproach() ? *fp->getApproach() : QString();
            plan.m_activeWaypoint = fp->getActiveWaypoint();
            plan.m_waypoints.clear();
            if (fp->getWaypoints())
            {
                for (const auto wp : *fp->getWaypoints())
                {
                    MapRouteWaypoint waypoint;
                    waypoint.m_name = wp->getName() ? *wp->getName() : QString();
                    waypoint.m_latitude = wp->getLatitude();
                    waypoint.m_longitude = wp->getLongitude();
                    waypoint.m_altitudeFt = wp->getAltitude();
                    waypoint.m_eta = wp->getEta() ? *wp->getEta() : QString();
                    plan.m_waypoints.append(waypoint);
                }
            }
        }
        else
        {
            plan.m_valid = false;
            plan.m_waypoints.clear();
        }
        MapDatalinkMessage& message = m_aircraftState->m_datalinkMessage;
        if (as->getDatalinkMessage() && as->getDatalinkMessage()->isSet())
        {
            SWGSDRangel::SWGMapDatalinkMessage *dm = as->getDatalinkMessage();
            message.m_valid = true;
            message.m_id = dm->getId();
            message.m_received = dm->getReceived() ? *dm->getReceived() : QString();
            message.m_uplink = dm->getUplink() != 0;
            message.m_station = dm->getStation() ? *dm->getStation() : QString();
            message.m_text = dm->getText() ? *dm->getText() : QString();
        }
        else
        {
            message.m_valid = false;
        }
    }
    PROFILER_STOP("Map item update")
}

void ImageMapItem::update(SWGSDRangel::SWGMapItem *mapItem)
{
    MapItem::update(mapItem);
    m_changed = ChangedAll;   // Low rate, so not worth working out what changed

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
    m_changed = ChangedAll;   // Low rate, so not worth working out what changed
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
    m_changed = ChangedAll;   // Low rate, so not worth working out what changed
    m_colorValid = mapItem->getColorValid();
    m_color = mapItem->getColor();
    m_altitudeColored = mapItem->getType() == 4;
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
    PROFILER_START()
    m_frequencies.clear();
    m_frequencyStrings.clear();

    static const QRegularExpression re("(([0-9]+(\\.[0-9]+)?) *([kMG])?Hz)");
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
    PROFILER_STOP("Map frequencies")
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
    if (t1 <= 0) {
        return;
    }

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
    if (t1 <= 0) {
        return;
    }

    double vertV = (m_takenTrackCoords[p2]->altitude() - m_takenTrackCoords[p1]->altitude()) / t1;

    double newAlt = m_takenTrackCoords[p2]->altitude() + vertV * t2;

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
    if (t1 <= 0) {
        return;
    }

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
    if (t1 <= 0) {
        return;
    }

    double vertV = (p3Altitude - m_takenTrackCoords[p1]->altitude()) / t1;

    double newAlt = m_takenTrackCoords[p1]->altitude() + vertV * t2;

    m_takenTrackCoords[p2]->setAltitude(newAlt);

    m_interpolatedCoords.append(m_takenTrackCoords[p2]);
    m_interpolatedDateTimes.append(m_takenTrackDateTimes[p2]);
}

void ObjectMapItem::updateTakenTrackBounds()
{
    for (int i = m_takenTrackBoundsCount; i < m_takenTrackCoords.size(); i++)
    {
        const double longitude = m_takenTrackCoords[i]->longitude();
        m_takenTrackMinLongitude = std::min(m_takenTrackMinLongitude, longitude);
        m_takenTrackMaxLongitude = std::max(m_takenTrackMaxLongitude, longitude);
    }
    m_takenTrackBoundsCount = m_takenTrackCoords.size();
}


// A sender that owns its track only adds a point when the aircraft has moved far enough
// to be worth storing - the Aircraft feature's threshold is 200 m - so the last point it
// has given us sits behind where the aircraft actually is. The Map used to hide that by
// appending a point of its own on every update, but those points became part of the
// stored track and were what made it jump (see updateTrack).
//
// So carry exactly ONE point beyond the sender's track, holding the live position, and
// replace it in place rather than accumulating. The track drawn on the 2D map reaches the
// aircraft as it did before, while the history behind it stays exactly what the sender
// said it was, and stays prefix-stable.
// Whittaker-Eilers penalises the second differences of z with respect to x, and those
// carry units of 1/x^2 - so the same lambda smooths far harder over a window spanning
// ten minutes than over one spanning twenty seconds. That is the difference between an
// aircraft overhead and one at the edge of range, and it would cut the corners off a
// distant aircraft's turns at a lambda that looks right for a near one.
//
// Rescaling the window to [0, n-1] fixes it. The RELATIVE spacing of the points is
// preserved, which is what the filter needs in order to weight uneven gaps properly,
// and evenly spaced data lands on 0, 1, 2, ... - the textbook formulation, where the
// familiar lambda range means what it usually does. Returns false if the window has no
// extent, which leaves nothing to smooth along.
static bool normaliseAbscissae(QVector<double>& x, int n)
{
    const double span = x[n - 1] - x[0];
    if (span <= 0.0) {
        return false;
    }
    const double scale = (n - 1) / span;
    for (int i = 0; i < n; i++) {
        x[i] = (x[i] - x[0]) * scale;
    }
    return true;
}

// Smoothing for both supplied and automatically collected tracks. The smoothed values
// are computed from the raw points and published separately. Besides keeping a supplied
// track comparable with its sender, this prevents the automatic path from feeding an
// earlier smoothed result back into its next fit and progressively amplifying an error.
//
// Only the 3D map sees them. The 2D map deliberately draws raw positions
// (onlyActual2D below), and the entity position is what Cesium draws its path from, so
// smoothing the samples smooths the 3D track.
void ObjectMapItem::smoothTrack(MapSettings::MapItemSettings *itemSettings)
{
    // These are one-shot corrections for the 3D entity. Clear them before every pass,
    // including passes that return early, or a rejected/short window can resend values
    // calculated for the previous version of the track.
    m_smoothedWindow.clear();
    m_smoothedWindowTimes.clear();
    m_interpolatedCoords.clear();
    m_interpolatedDateTimes.clear();

    const int filterLen = itemSettings ? itemSettings->m_smoothingWindow : 0;
    if (filterLen <= 0)
    {
        m_lastSmoothedSize = 0;
        m_smoothedTrackCoords.clear();
        return;
    }
    // Keep the smoothed shadow aligned with the track: new points start as their raw
    // selves and are overwritten below as smoothing windows complete. Trims and
    // rebuilds truncate it where they truncate the track - see updateTrack().
    while (m_smoothedTrackCoords.size() < m_takenTrackCoords.size()) {
        m_smoothedTrackCoords.append(*m_takenTrackCoords[m_smoothedTrackCoords.size()]);
    }
    // The window actually filtered is twice the setting, and only its later three
    // quarters are committed. Fitting exactly the last "window" points and committing
    // all of them put a visible corner at every window boundary - measured on a real
    // track, the heading kink at the seams averaged 4 degrees against 0.7 inside the
    // windows, which is precisely the jaggedness smoothing is meant to remove. A
    // point committed here has context on both sides of it in the fit.
    const int windowLen = 2 * filterLen;
    if (m_takenTrackCoords.size() < windowLen) {
        return;
    }
    // Run every half setting-window, measured against where the last pass ended rather
    // than testing the length for divisibility. The automatic track grows one point at
    // a time so a "% (filterLen/2) == 0" test always came round; a supplied track
    // arrives in batches - the Aircraft feature appends several points between sends -
    // and the length steps straight over the multiples, so smoothing hardly ever ran.
    if ((m_takenTrackCoords.size() - m_lastSmoothedSize) < (filterLen / 2)) {
        return;
    }
    m_lastSmoothedSize = m_takenTrackCoords.size();

    QVector<double> x(windowLen), y1(windowLen), y2(windowLen), y3(windowLen), w(windowLen);
    for (int i = 0; i < windowLen; i++)
    {
        const int idx = m_takenTrackCoords.size() - (windowLen - i);
        x[i] = (m_takenTrackDateTimes[idx]->toMSecsSinceEpoch()
                - m_takenTrackDateTimes[0]->toMSecsSinceEpoch()) / 1000.0;
        // The first half of the window was smoothed by earlier passes and is already
        // on screen. The fit is anchored hard to those SMOOTHED values rather than the
        // raw ones, which is what joins this window's curve onto the last one's -
        // anchoring to the raw values put a corner at every seam.
        if (i < (windowLen / 2))
        {
            const QGeoCoordinate& c = m_smoothedTrackCoords[idx];
            y1[i] = c.latitude();
            y2[i] = c.longitude();
            y3[i] = c.altitude();
            w[i] = 100.0;
        }
        else
        {
            y1[i] = m_takenTrackCoords[idx]->latitude();
            y2[i] = m_takenTrackCoords[idx]->longitude();
            y3[i] = m_takenTrackCoords[idx]->altitude();
            w[i] = 1.0;
        }
        // Two points at the same instant make the divided differences below divide by
        // zero. Nudging them apart was worse than useless: it leaves a gap a thousand
        // times smaller than its neighbours, and the fit over such uneven spacing is
        // ill conditioned - the window that produced 15 km errors had x values of
        // 2.283, 2.284, 2.285 next to gaps of five seconds. Skip the window instead,
        // and let the counter say it is happening.
        if ((i > 0) && (x[i] <= x[i - 1])) {
            return;
        }
    }

    // A window straddling a hole in coverage is just as ill conditioned: fitting
    // across half-second points either side of a minute-long gap threw a point of a
    // real track 1.9 km sideways. The aircraft was not being received - there is
    // nothing true to draw across the gap, so leave those points as they came.
    QVector<double> gaps(windowLen - 1);
    for (int i = 1; i < windowLen; i++) {
        gaps[i - 1] = x[i] - x[i - 1];
    }
    QVector<double> sortedGaps = gaps;
    std::nth_element(sortedGaps.begin(), sortedGaps.begin() + sortedGaps.size() / 2, sortedGaps.end());
    const double medianGap = sortedGaps[sortedGaps.size() / 2];
    const double maxGap = *std::max_element(gaps.constBegin(), gaps.constEnd());
    if ((maxGap > 30.0) || (maxGap > 20.0 * medianGap)) {
        return;
    }

    QVector<double> xn = x;
    if (!normaliseAbscissae(xn, windowLen)) {
        return;
    }

    const double lambda = itemSettings->m_smoothingLambda;
    m_filter.filter(xn.data(), y1.data(), w.data(), windowLen, lambda);
    m_filter.filter(xn.data(), y2.data(), w.data(), windowLen, lambda);
    m_filter.filter(xn.data(), y3.data(), w.data(), windowLen, lambda);

    for (int i = 0; i < windowLen; i++)
    {
        if (!std::isfinite(y1[i]) || !std::isfinite(y2[i]) || !std::isfinite(y3[i])) {
            return;
        }
    }

    // Commit from a quarter of the way in: the anchored first quarter is already
    // final, and everything after it has been fitted with points both sides.
    // Filled completely before any pointer into it is taken, so nothing dangles when
    // the lists grow. Cleared each pass, which also drops anything the 3D map did not
    // collect because it was closed
    const int commitFrom = windowLen / 4;
    m_smoothedWindow.reserve(windowLen - commitFrom);
    m_smoothedWindowTimes.reserve(windowLen - commitFrom);
    for (int i = commitFrom; i < windowLen; i++)
    {
        const int idx = m_takenTrackCoords.size() - (windowLen - i);
        m_smoothedWindow.append(QGeoCoordinate(y1[i], y2[i], y3[i]));
        m_smoothedWindowTimes.append(*m_takenTrackDateTimes[idx]);
        // And into the shadow track, which is what the 3D map's altitude coloured
        // segments draw
        m_smoothedTrackCoords[idx] = QGeoCoordinate(y1[i], y2[i], y3[i]);
    }
    for (int i = 0; i < m_smoothedWindow.size(); i++)
    {
        m_interpolatedCoords.append(&m_smoothedWindow[i]);
        m_interpolatedDateTimes.append(&m_smoothedWindowTimes[i]);
    }
}

void ObjectMapItem::updateProvisionalTrackPoint()
{
    if (!m_trackSupplied || m_takenTrackCoords.isEmpty()) {
        return;
    }

    const int last = m_takenTrackCoords.size() - 1;
    const int real = m_trackProvisional ? last - 1 : last;
    if (real < 0) {
        return;         // Nothing from the sender yet to hang it off
    }

    // Nothing to show while the aircraft is still on the sender's last point
    const QGeoCoordinate *r = m_takenTrackCoords.at(real);
    if ((r->latitude() == m_latitude) && (r->longitude() == m_longitude)
        && (r->altitude() == m_altitude))
    {
        dropProvisionalTrackPoint();
        return;
    }

    const QGeoCoordinate live(m_latitude, m_longitude, m_altitude);
    const QDateTime when = m_positionDateTime.isValid() ? m_positionDateTime
                                                        : QDateTime::currentDateTimeUtc();
    // The sender's own string, not when.toString(): updateTrack() compares the strings
    // to recognise the sender's point replacing this one, and when is held in UTC while
    // a sender may write local time
    const QString whenString = m_positionDateTime.isValid() ? m_positionDateTimeString
                                                            : when.toString(Qt::ISODateWithMs);
    if (m_trackProvisional)
    {
        if ((*m_takenTrackCoords[last] != live) || (*m_takenTrackDateTimes[last] != when))
        {
            *m_takenTrackCoords[last] = live;
            *m_takenTrackDateTimes[last] = when;
            m_takenTrackDateTimeStrings[last] = whenString;
            m_takenTrack[last] = QVariant::fromValue(live);
            m_takenTrackRevision++;
        }
    }
    else
    {
        m_takenTrackCoords.push_back(new QGeoCoordinate(live));
        m_takenTrackDateTimes.push_back(new QDateTime(when));
        m_takenTrackDateTimeStrings.push_back(whenString);
        m_takenTrackPositionExtrapolated.push_back(false);
        m_takenTrackAltitudeExtrapolated.push_back(false);
        m_takenTrack.push_back(QVariant::fromValue(live));
        m_trackProvisional = true;
    }
}

void ObjectMapItem::dropProvisionalTrackPoint()
{
    if (!m_trackProvisional) {
        return;
    }
    delete m_takenTrackCoords.takeLast();
    delete m_takenTrackDateTimes.takeLast();
    m_takenTrackDateTimeStrings.removeLast();
    m_takenTrackPositionExtrapolated.removeLast();
    m_takenTrackAltitudeExtrapolated.removeLast();
    m_takenTrack.removeLast();
    m_trackProvisional = false;
    // The longitude bounds were extended over the point that has just gone. They are only
    // used to decide whether a track is wholly on screen, so leaving them slightly wide
    // costs a split that could have been skipped - never a wrong answer
    m_takenTrackBoundsCount = std::min(m_takenTrackBoundsCount, (int) m_takenTrackCoords.size());
}

void ObjectMapItem::updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track, MapSettings::MapItemSettings *itemSettings)
{
    if (track != nullptr)
    {
        // From here on this track belongs to the sender, and the Map must not add points
        // of its own to it - see the else branch below
        m_trackSupplied = true;

        // The incoming sender point commonly replaces the provisional live point at
        // the same index. Treat it as a rewrite only when its value actually differs;
        // otherwise forcing both segment caches to rebuild on every append would throw
        // away their main performance optimisation.
        if (m_trackProvisional && (track->size() >= m_takenTrackCoords.size()))
        {
            const int i = m_takenTrackCoords.size() - 1;
            SWGSDRangel::SWGMapCoordinate *p = track->at(i);
            const QGeoCoordinate *c = m_takenTrackCoords.at(i);
            const bool timeDiffers = !p->getDateTime()
                || (m_takenTrackDateTimeStrings.at(i) != *p->getDateTime());
            if ((c->latitude() != p->getLatitude())
                || (c->longitude() != p->getLongitude())
                || (c->altitude() != p->getAltitude()) || timeDiffers)
            {
                m_takenTrackRevision++;
            }
        }
        dropProvisionalTrackPoint();

        // The sender hands over the WHOLE track on every message, but in the ordinary
        // case it has simply grown by one point. Rebuilding it from nothing - two heap
        // allocations and an ISO 8601 date parse per point - was measured at 173 us per
        // message and 83% of all item update time, and it gets steadily worse the longer
        // an aircraft is watched, which is exactly when the map feels slow.
        //
        // So check whether the points we already hold still match, and if they do, copy
        // only what is new. The check is four comparisons per point with no
        // allocation and no parsing, cheap enough to do exhaustively rather than assume
        // the sender only ever appends - a sender that rewrote a track would otherwise
        // leave a wrong one on the map, and that is not a bug worth risking to save a
        // few hundred nanoseconds.
        int keep = 0;
        if ((track->size() >= m_takenTrackCoords.size())
            && (m_takenTrackDateTimeStrings.size() == m_takenTrackCoords.size()))
        {
            keep = m_takenTrackCoords.size();
            for (int i = 0; i < keep; i++)
            {
                // The generated accessors are not const, so neither is this
                SWGSDRangel::SWGMapCoordinate *p = track->at(i);
                const QGeoCoordinate *c = m_takenTrackCoords.at(i);
                const bool latDiffers = (c->latitude() != p->getLatitude());
                const bool lonDiffers = (c->longitude() != p->getLongitude());
                const bool altDiffers = (c->altitude() != p->getAltitude());
                const bool timeDiffers = !p->getDateTime()
                    || (m_takenTrackDateTimeStrings.at(i) != *p->getDateTime());
                if (latDiffers || lonDiffers || altDiffers || timeDiffers)
                {
                    // Keep everything before the divergence rather than starting again
                    keep = i;
                    break;
                }
            }
        }

        if (keep == 0)
        {
            if (!m_takenTrackCoords.isEmpty()) {
                m_takenTrackRevision++;
            }
            qDeleteAll(m_takenTrackCoords);
            m_takenTrackCoords.clear();
            m_takenTrackMinLongitude = 180.0;
            m_takenTrackMaxLongitude = -180.0;
            m_takenTrackBoundsCount = 0;
            qDeleteAll(m_takenTrackDateTimes);
            m_takenTrackDateTimes.clear();
            m_takenTrackDateTimeStrings.clear();
            m_takenTrackPositionExtrapolated.clear();
            m_takenTrackAltitudeExtrapolated.clear();
            m_takenTrack.clear();
            m_takenTrack1.clear();
            m_takenTrack2.clear();
            m_lastSmoothedSize = 0;
            m_smoothedTrackCoords.clear();
        }
        else if (keep < m_takenTrackCoords.size())
        {
            // Diverged part way: drop the tail from there and re-copy it. The longitude
            // bounds were accumulated over points that are going away, so they have to be
            // recomputed from scratch - updateTakenTrackBounds() resumes from this count.
            m_takenTrackRevision++;
            while (m_takenTrackCoords.size() > keep)
            {
                delete m_takenTrackCoords.takeLast();
                delete m_takenTrackDateTimes.takeLast();
                m_takenTrackDateTimeStrings.removeLast();
                m_takenTrackPositionExtrapolated.removeLast();
                m_takenTrackAltitudeExtrapolated.removeLast();
                m_takenTrack.removeLast();
            }
            m_takenTrackMinLongitude = 180.0;
            m_takenTrackMaxLongitude = -180.0;
            m_takenTrackBoundsCount = 0;
            m_takenTrack1.clear();
            m_takenTrack2.clear();
            m_lastSmoothedSize = std::min(m_lastSmoothedSize, keep);
            while (m_smoothedTrackCoords.size() > keep) {
                m_smoothedTrackCoords.removeLast();
            }
        }
        else if (keep == track->size())
        {
            // Nothing new. The extrapolation flags below are still recomputed by the
            // caller, so there is simply nothing to copy.
            return;
        }

        // Appending leaves m_takenTrackBoundsCount alone, and updateTakenTrackBounds()
        // resumes from it, so the longitude bounds extend over just the new points
        for (int i = keep; i < track->size(); i++)
        {
            SWGSDRangel::SWGMapCoordinate* p = track->at(i);
            QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
            const QString dateTime = p->getDateTime() ? *p->getDateTime() : QString();
            QDateTime *d = new QDateTime(parseDateTime(dateTime));
            m_takenTrackCoords.push_back(c);
            m_takenTrackDateTimes.push_back(d);
            m_takenTrackDateTimeStrings.push_back(dateTime);
            m_takenTrackPositionExtrapolated.push_back(false);
            m_takenTrackAltitudeExtrapolated.push_back(false);
            m_takenTrack.push_back(QVariant::fromValue(*c));
        }

        smoothTrack(itemSettings);
    }
    else if (m_trackSupplied)
    {
        // This item supplies its own track, and simply has not changed it. Do NOT fall
        // through to the automatic track below: rebuilding the whole track for every
        // report is expensive, so a sender like the Aircraft feature sends it only when
        // it grows and sends nothing in between - and the Map was appending its own
        // point on each of those, so the stored track became the sender's points with
        // Map-generated ones (including EXTRAPOLATED positions, and positions rewritten
        // in place by the Whittaker-Eilers smoother) spliced in after them. The next real
        // track then disagreed with what was held and rewrote the tail, which is the
        // backwards jump seen in taken tracks on the 3D map.
        //
        // It showed in the counters as 8578 of 9916 track updates finding the stored
        // track already different, diverging on average 39 points in - a sender that
        // only ever appends cannot do that on its own.
        }
    else
    {
        // Automatically create a track
        bool trackGrew = false;
        if (m_takenTrackCoords.size() == 0)
        {
            QGeoCoordinate *c = new QGeoCoordinate(m_latitude, m_longitude, m_altitude);
            m_takenTrackCoords.push_back(c);
            // This coordinate says where the object is, so its time is the position
            // time. Preferring a newer altitude time falsely moved the position sample
            // forward and made later extrapolation use the wrong interval.
            if (m_positionDateTime.isValid()) {
                m_takenTrackDateTimes.push_back(new QDateTime(m_positionDateTime));
            } else if (m_altitudeDateTime.isValid()) {
                m_takenTrackDateTimes.push_back(new QDateTime(m_altitudeDateTime));
            } else {
                m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTimeUtc()));
            }
            m_takenTrackPositionExtrapolated.push_back(false);
            m_takenTrackAltitudeExtrapolated.push_back(false);
            m_takenTrack.push_back(QVariant::fromValue(*c));
            trackGrew = true;
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

            // A new altitude is not a new position. The old code extrapolated a
            // position to every altitude report and inserted it into the 3D track. With
            // millisecond source timestamps, two close position fixes can legitimately
            // be only a few ms apart; projecting that short-baseline velocity even one
            // second forward produces the long spokes seen in the Map. The 2D list kept
            // the actual coordinate while its parallel geometry list kept the projected
            // one, so edge splitting could corrupt the 2D path as well. Only a newer
            // position timestamp is allowed to grow a spatial track.
            if (m_positionDateTime.isValid())
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
                        const qint64 elapsedMs = prevDateTime->msecsTo(m_positionDateTime);
                        const double segmentKm = prev1->distanceTo(cActual) / 1000.0;
                        const double speedKn = elapsedMs > 0
                            ? segmentKm * 3600000.0 / elapsedMs / 1.852 : 0.0;
                        if (!cActual.isValid() || !std::isfinite(segmentKm)
                            || ((segmentKm > 20.0) && (speedKn > 1200.0)))
                        {
                            qWarning() << "MAP_TRACK_DIAG implausible input segment"
                                       << m_name
                                       << "from" << *prev1 << prevDateTime->toString(Qt::ISODateWithMs)
                                       << "to" << cActual << m_positionDateTime.toString(Qt::ISODateWithMs)
                                       << "distance-km" << segmentKm << "speed-kn" << speedKn;
                        }
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
                            trackGrew = true;
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
                m_takenTrackDateTimes.push_back(new QDateTime(QDateTime::currentDateTimeUtc()));
                m_takenTrackPositionExtrapolated.push_back(false);
                m_takenTrackAltitudeExtrapolated.push_back(false);
                m_takenTrackCoords.push_back(new QGeoCoordinate(c));
                m_takenTrack.push_back(QVariant::fromValue(onlyActual2D ? cActual : c));
                trackGrew = true;
            }

            /*if (m_takenTrackDateTimes.size() >= 2) {
                if (*m_takenTrackDateTimes[m_takenTrackDateTimes.size() - 1] < *m_takenTrackDateTimes[m_takenTrackDateTimes.size() - 2]) {
                    qDebug() << "ObjectMapItem::updateTrack: Out of order";
                }
            }*/

        }
        // The same robust, non-destructive smoothing as a sender-owned track. It rejects
        // duplicate/ill-conditioned time windows and gaps in coverage, and leaves the
        // raw geometry used by the 2D map untouched.
        if (trackGrew) {
            smoothTrack(itemSettings);
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
            QDateTime *d = new QDateTime(parseDateTime(*p->getDateTime(), Qt::ISODate));
            m_predictedTrackCoords.push_back(c);
            m_predictedTrackDateTimes.push_back(d);
            m_predictedTrack.push_back(QVariant::fromValue(*c));
        }
    }
}
