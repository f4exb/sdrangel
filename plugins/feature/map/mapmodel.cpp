///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include <QGeoRectangle>

#include "channel/channelwebapiutils.h"
#include "util/flightpathpalette.h"
#include "util/units.h"
#include "util/profiler.h"
#include "maincore.h"

#include "mapmodel.h"
#include "mapgui.h"
#include "map.h"

#include "SWGTargetAzimuthElevation.h"
#include "SWGMapAircraftState.h"

QVariant MapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    switch (role)
    {
    case itemSettingsRole:
        return QVariant::fromValue(m_items[row]->m_itemSettings);
    case nameRole:
        return QVariant::fromValue(m_items[row]->m_name);
    case labelRole:
        return QVariant::fromValue(m_items[row]->m_label);
    case positionRole:
    {
        // Coordinates to display the label at
        QGeoCoordinate coords;
        coords.setLatitude(m_items[row]->m_latitude);
        coords.setLongitude(m_items[row]->m_longitude);
        coords.setAltitude(m_items[row]->m_altitude);
        return QVariant::fromValue(coords);
    }
    case mapImageMinZoomRole:
        // Minimum zoom level at which this is visible
        return QVariant::fromValue(m_items[row]->m_itemSettings->m_2DMinZoom);
    default:
        return QVariant();
    }
}

bool MapModel::setData(const QModelIndex &index, const QVariant& value, int role)
{
    (void) value;
    (void) role;

    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return false;
    }
    return true;
}

void MapModel::add(MapItem *item)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    item->m_row = m_items.size();
    m_items.append(item);
    m_filterData.append(FilterData());
    fillFilterData(item, m_filterData.last());
    m_itemsHash.insert(item->m_hashKey, item);
    endInsertRows();
}

void MapModel::update(const QObject *sourcePipe, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group)
{
    QString name = *swgMapItem->getName();
    // Add, update or delete and item
    MapItem *item = findMapItem(sourcePipe, name);
    if (item != nullptr)
    {
        QString image = *swgMapItem->getImage();
        if (image.isEmpty())
        {
            // Delete the item from 2D map
            remove(item);
            // Delete from 3D map
            item->update(swgMapItem);
            update3D(item);
            delete item;
        }
        else
        {
            // Update the item
            item->update(swgMapItem);
            // Explicitly timestamped aircraft states are replay samples. They must
            // reach CZML now: the ordinary model flush deliberately coalesces every
            // update to an item, which would otherwise retain only the last PFD state
            // from a fast ADS-B log load. The item remains dirty so the 2D map can
            // still redraw once per normal batch.
            SWGSDRangel::SWGMapAircraftState *aircraftState = swgMapItem->getAircraftState();
            if (aircraftState && aircraftState->getDateTime()
                && !aircraftState->getDateTime()->isEmpty())
            {
                ObjectMapItem *objectItem = dynamic_cast<ObjectMapItem *>(item);
                if (objectItem)
                {
                    objectItem->m_replayTrackPending = true;
                    objectItem->m_replayStateUpdate = true;
                    update3D(item);
                    objectItem->m_replayStateUpdate = false;
                }
            }
            update(item);
        }
    }
    else
    {
        // Make sure not a duplicate request to delete
        QString image = *swgMapItem->getImage();
        if (!image.isEmpty())
        {
            // Add new item
            item = newMapItem(sourcePipe, group, m_gui->getItemSettings(group), swgMapItem);
            add(item);
            // Add to 3D Map (we don't appear to get a dataChanged signal when adding)
            update3D(item);
        }
    }
}

void MapModel::update(MapItem *item)
{
    markDirty(item, item->m_changed);
}

// The expensive tests - the user's name regex and the great-circle distance to the
// station - are done here, once per change to an item, rather than once per item every
// time the map moves
void MapModel::fillFilterData(MapItem *item, FilterData& data) const
{
    const MapSettings::MapItemSettings *itemSettings = item->m_itemSettings;

    data.m_latitude = item->m_latitude;
    data.m_longitude = item->m_longitude;
    data.m_minZoom = itemSettings->m_2DMinZoom;
    data.m_flags = 0;

    if (itemSettings->m_enabled) {
        data.m_flags |= FilterEnabled;
    }
    if (itemSettings->m_display2DIcon) {
        data.m_flags |= FilterDisplayIcon;
    }
    if (itemSettings->m_filterName.isEmpty() || itemSettings->m_filterNameRE.match(item->m_name).hasMatch()) {
        data.m_flags |= FilterNameMatches;
    }
    if (itemSettings->m_filterDistance <= 0)
    {
        data.m_flags |= FilterDistanceOk;
    }
    else if (m_stationPosition.isValid())
    {
        const QGeoCoordinate position(item->m_latitude, item->m_longitude);
        if (m_stationPosition.distanceTo(position) <= itemSettings->m_filterDistance) {
            data.m_flags |= FilterDistanceOk;
        }
    }
}

void MapModel::updateFilterData(int row)
{
    if ((row >= 0) && (row < m_filterData.size())) {
        fillFilterData(m_items[row], m_filterData[row]);
    }
}

void MapModel::rebuildFilterData()
{
    m_filterData.resize(m_items.size());
    for (int row = 0; row < m_items.size(); row++) {
        fillFilterData(m_items[row], m_filterData[row]);
    }
}

void MapModel::setStationPosition(const QGeoCoordinate& position)
{
    m_stationPosition = position;
    rebuildFilterData();
}

void MapModel::markDirty(MapItem *item, quint32 changed)
{
    if (item->m_row >= 0)
    {
        m_dirty[item] |= changed;
        // Kept in step here rather than at flush time, so that a pan in between the two
        // filters on where the item actually is
        updateFilterData(item->m_row);
    }
}

// Tell the map about everything that has changed since the last time, coalescing runs of
// neighbouring rows that changed in the same way in to a single signal
void MapModel::flush()
{
    if (m_dirty.isEmpty()) {
        return;
    }

    const quint32 of3DInterest = MapItem::ChangedPosition | MapItem::ChangedLabel
                               | MapItem::ChangedImage | MapItem::ChangedText
                               | MapItem::ChangedGeometry | MapItem::ChangedSelection
                               | MapItem::Changed3D;

    // Bracketed around the whole loop rather than per item: PROFILER_STOP takes a global
    // mutex, and the per item cost is already covered by Map CZML. This gives what the 3D
    // map costs per flush, which is the figure to compare against Map flush.
    PROFILER_START()
    CesiumInterface *cesium = m_gui->cesium();
    const bool batching = cesium && cesium->isConnected();
    if (batching) {
        cesium->beginBatch();
    }
    for (auto it = m_dirty.cbegin(); it != m_dirty.cend(); ++it)
    {
        updateFilterData(it.key()->m_row);

        quint32 interest = of3DInterest;
        // With a track palette set - or smoothing on - the 3D map draws the track
        // itself as polylines, so track changes matter to it. Per item rather than in
        // the mask above: everything else's 3D track is drawn by Cesium from the
        // position samples, and needs no update when the track changes.
        if (   !it.key()->m_itemSettings->m_2DTrackPalette.isEmpty()
            || (it.key()->m_itemSettings->m_smoothingWindow > 0)) {
            interest |= MapItem::ChangedTrack | MapItem::ChangedPredictedTrack;
        }
        if (it.value() & interest) {
            update3D(it.key());
        }
    }
    if (batching) {
        cesium->endBatch();
    }
    PROFILER_STOP("Map 3D update")

    QVector<QPair<int, quint32>> pending;
    pending.reserve(m_dirty.size());
    for (auto it = m_dirty.cbegin(); it != m_dirty.cend(); ++it)
    {
        if (it.key()->m_row >= 0) {
            pending.append(qMakePair(it.key()->m_row, it.value()));
        }
    }
    m_dirty.clear();
    std::sort(pending.begin(), pending.end());

    for (int i = 0; i < pending.size(); )
    {
        const quint32 changed = pending[i].second;
        int last = i;
        while ((last + 1 < pending.size())
               && (pending[last + 1].second == changed)
               && (pending[last + 1].first == pending[last].first + 1)) {
            last++;
        }
        emit dataChanged(index(pending[i].first), index(pending[last].first), rolesFor(changed));
        i = last + 1;
    }
}

QVector<int> MapModel::rolesFor(quint32 changed) const
{
    if (changed == MapItem::ChangedAll) {
        return QVector<int>();   // All of them
    }

    if (m_roleCache.contains(changed)) {
        return m_roleCache.value(changed);
    }

    QVector<int> roles;
    if (changed & MapItem::ChangedPosition) {
        roles.append(positionRole);
    }
    if (changed & (MapItem::ChangedLabel | MapItem::ChangedText | MapItem::ChangedSelection)) {
        roles.append(labelRole);
    }
    m_roleCache.insert(changed, roles);

    return roles;
}

void MapModel::remove(MapItem *item)
{
    int row = item->m_row;
    if (row >= 0)
    {
        QString key = m_items[row]->m_hashKey;
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        m_filterData.remove(row);
        m_itemsHash.remove(key);
        m_dirty.remove(item);
        item->m_row = -1;
        // Everything after it has moved up one
        for (int i = row; i < m_items.size(); i++) {
            m_items[i]->m_row = i;
        }
        endRemoveRows();
    }
}

void MapModel::removeAll()
{
    if (m_items.count() > 0)
    {
        beginRemoveRows(QModelIndex(), 0, m_items.count() - 1);
        for (auto item : m_items) {
            item->m_row = -1;
        }
        m_dirty.clear();
        qDeleteAll(m_items);
        m_items.clear();
        m_filterData.clear();
        m_itemsHash.clear();
        endRemoveRows();
    }
}

// After new settings are deserialised, we need to update
// pointers to item settings for all existing items
void MapModel::updateItemSettings(QHash<QString, MapSettings::MapItemSettings *> m_itemSettings)
{
    for (auto item : m_items)
    {
        if (m_itemSettings.contains(item->m_group)) {
            item->m_itemSettings = m_itemSettings[item->m_group];
        }
    }
    rebuildFilterData();
}

void MapModel::allUpdated()
{
    // Settings are what most of the filter data is derived from, and this is called
    // whenever they change
    rebuildFilterData();

    if (m_items.count() > 0)
    {
        emit dataChanged(index(0), index(m_items.count()-1));

        // The 3D map is no longer driven by dataChanged, so it has to be told here.
        // update3D() returns immediately when the 3D map isn't running.
        //
        // Batched, because this walks EVERY item - of the order of 200,000 once the
        // airport, navaid, airspace and waypoint databases are loaded. Most of them will
        // produce nothing at all now that an unchanged item emits only its id, but the
        // ones that do produce something should still travel together.
        CesiumInterface *cesium = m_gui->cesium();
        const bool batching = cesium && cesium->isConnected();
        if (batching) {
            cesium->beginBatch();
        }
        for (auto item : m_items) {
            update3D(item);
        }
        if (batching) {
            cesium->endBatch();
        }
    }
}

MapItem *MapModel::findMapItem(const QObject *source, const QString& name)
{
    QString key = source->objectName() + name;
    if (m_itemsHash.contains(key)) {
        return m_itemsHash.value(key);
    }
    return nullptr;
}

// FIXME: This should potentially return a list, as we have have multiple items with the same name
// from different sources
MapItem *MapModel::findMapItem(const QString& name)
{
    QListIterator<MapItem *> i(m_items);
    while (i.hasNext())
    {
        MapItem *item = i.next();
        if (!item->m_name.compare(name, Qt::CaseInsensitive)) {
            return item;
        }
    }
    return nullptr;
}

QModelIndex MapModel::findMapItemIndex(const QString& name)
{
    int idx = 0;
    QListIterator<MapItem *> i(m_items);
    while (i.hasNext())
    {
        MapItem *item = i.next();
        if (item->m_name == name) {
            return index(idx);
        }
        idx++;
    }
    return index(-1);
}

QHash<int, QByteArray> MapModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[itemSettingsRole] = "itemSettings";
    roles[nameRole] = "name";
    roles[labelRole] = "label";
    roles[positionRole] = "position";
    roles[mapImageMinZoomRole] = "mapImageMinZoom";
    return roles;
}

// Slot called on dataChanged signal, to update 3D map
MapItem *ImageMapModel::newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem)
{
    return new ImageMapItem(sourcePipe, group, itemSettings, mapItem);
}

QVariant ImageMapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    ImageMapItem *mapItem = (ImageMapItem*)m_items[row];
    switch (role)
    {
    case imageRole:
         return QVariant::fromValue(mapItem->m_image);
    case imageZoomLevelRole:
         return QVariant::fromValue(mapItem->m_imageZoomLevel);
    case boundsRole:
        return QVariant::fromValue(mapItem->m_bounds);
    default:
        return MapModel::data(index, role);
    }
}

// These items have an extent, so the filter tests that rather than a single position
void ImageMapModel::fillFilterData(MapItem *item, FilterData& data) const
{
    MapModel::fillFilterData(item, data);

    const QGeoRectangle& bounds = ((const ImageMapItem *) item)->m_bounds;
    data.m_boundsTop = bounds.topLeft().latitude();
    data.m_boundsBottom = bounds.bottomRight().latitude();
    data.m_boundsLeft = bounds.topLeft().longitude();
    data.m_boundsRight = bounds.bottomRight().longitude();
}

void ImageMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium)
    {
        ImageMapItem *imageItem = (ImageMapItem *)item;
        if (!imageItem->m_image.isEmpty())
        {
            /*qDebug() << "ImageMapModel::update3D - " << imageItem->m_name
                                                     << imageItem->m_bounds.right()
                                                     << imageItem->m_bounds.left()
                                                     << imageItem->m_bounds.top()
                                                     << imageItem->m_bounds.bottom()
                                                     ;  */
            cesium->updateImage(imageItem->m_name,
                                imageItem->m_bounds.topRight().longitude(),
                                imageItem->m_bounds.bottomLeft().longitude(),
                                imageItem->m_bounds.topRight().latitude(),
                                imageItem->m_bounds.bottomLeft().latitude(),
                                imageItem->m_altitude,
                                imageItem->m_image);
        }
        else
        {
            qDebug() << "ImageMapModel::update3D - removeImage " << imageItem->m_name;
            cesium->removeImage(imageItem->m_name);
        }
    }
}

MapItem *PolygonMapModel::newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem)
{
    return new PolygonMapItem(sourcePipe, group, itemSettings, mapItem);
}

QVariant PolygonMapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    PolygonMapItem *polygonItem = ((PolygonMapItem *)m_items[row]);
    switch (role)
    {
    case borderColorRole:
         return QVariant::fromValue(QColor(0x00, 0x00, 0x00, 0x00)); // Transparent
    case fillColorRole:
        if (polygonItem->m_itemSettings->m_display2DTrack)
        {
            if (polygonItem->m_colorValid) {
                return QVariant::fromValue(QColor::fromRgba(polygonItem->m_color));
            } else {
                return QVariant::fromValue(QColor::fromRgba(polygonItem->m_itemSettings->m_2DTrackColor));
            }
        }
        else
        {
            return QVariant::fromValue(QColor(0x00, 0x00, 0x00, 0x00)); // Transparent
        }
    case polygonRole:
        return polygonItem->m_polygon;
    case boundsRole:
        return QVariant::fromValue(polygonItem->m_bounds);
    default:
        return MapModel::data(index, role);
    }
}

// These items have an extent, so the filter tests that rather than a single position
void PolygonMapModel::fillFilterData(MapItem *item, FilterData& data) const
{
    MapModel::fillFilterData(item, data);

    const QGeoRectangle& bounds = ((const PolygonMapItem *) item)->m_bounds;
    data.m_boundsTop = bounds.topLeft().latitude();
    data.m_boundsBottom = bounds.bottomRight().latitude();
    data.m_boundsLeft = bounds.topLeft().longitude();
    data.m_boundsRight = bounds.bottomRight().longitude();
}

void PolygonMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium) {
        cesium->update((PolygonMapItem *)item);
    }
}

MapItem *PolylineMapModel::newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem)
{
    PolylineMapItem *item = new PolylineMapItem(sourcePipe, group, itemSettings, mapItem);
    // Before it is added, because adding it is what makes the map read its roles - an
    // item is only marked dirty when it CHANGES, so a new one would never reach flush()
    updateTrackSegments(item);
    return item;
}

// A line drawn as a track is split in to runs of constant colour, the colour of each point
// coming from its altitude and the group's palette. The same mapping an object's ground
// track uses, so a track drawn on its own looks like one drawn under an aircraft.
void PolylineMapModel::updateTrackSegments(PolylineMapItem *item)
{
    const QString& paletteName = item->m_itemSettings->m_2DTrackPalette;

    if (!item->m_altitudeColored || paletteName.isEmpty())
    {
        // Drawn in a single colour instead, by the plain polyline delegate
        if (item->m_trackColorModel) {
            item->m_trackColorModel->clear();
        }
        return;
    }

    const QVariant *palette = FlightPathPalette::getPalette(paletteName);
    QList<QVariantList> segments;
    QList<QVariant> colors;
    int lastColor = -1;

    for (const QVariant& point : item->m_polyline)
    {
        const QGeoCoordinate coord = point.value<QGeoCoordinate>();
        const int colorIndex = FlightPathPalette::getColorIndex(Units::metresToFeet(coord.altitude()));

        if (colorIndex != lastColor)
        {
            // Repeat the point the colour changes at in the segment that ends there, so
            // there's no gap between the two
            if (lastColor >= 0) {
                segments.last().append(point);
            }
            segments.append(QVariantList());
            colors.append(palette[colorIndex]);
            lastColor = colorIndex;
        }
        segments.last().append(point);
    }

    if (!item->m_trackColorModel) {
        item->m_trackColorModel = new TrackColorModel();
    }
    // No firstChanged: unlike a ground track, which grows a point at a time, a line is
    // re-sent whole whenever it changes at all
    item->m_trackColorModel->update(segments, colors);
}

void PolylineMapModel::flush()
{
    for (auto it = m_dirty.cbegin(); it != m_dirty.cend(); ++it) {
        updateTrackSegments((PolylineMapItem *) it.key());
    }
    MapModel::flush();
}

// The palette is a setting, so a change to it recolours every line already drawn
void PolylineMapModel::allUpdated()
{
    for (auto item : m_items) {
        updateTrackSegments((PolylineMapItem *) item);
    }
    MapModel::allUpdated();
}

QVariant PolylineMapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    PolylineMapItem *polylineItem = (PolylineMapItem *)m_items[row];
    switch (role)
    {
    case lineColorRole:
        if (polylineItem->m_colorValid) {
            return QVariant::fromValue(QColor::fromRgba(polylineItem->m_color));
        } else {
            return QVariant::fromValue(QColor::fromRgba(polylineItem->m_itemSettings->m_2DTrackColor));
        }
    case coordinatesRole:
        // Nothing for the plain delegate to draw when the segments are drawing it, or the
        // two would be drawn one on top of the other and the single colour would win
        if (polylineItem->m_trackColorModel && (polylineItem->m_trackColorModel->rowCount() > 0)) {
            return QVariant::fromValue(QVariantList());
        }
        return QVariant::fromValue(polylineItem->m_polyline);
    case trackModelRole:
        if (polylineItem->m_trackColorModel) {
            return QVariant::fromValue(polylineItem->m_trackColorModel);
        }
        return QVariant::fromValue(&m_noTrackColors);
    case boundsRole:
        return QVariant::fromValue(polylineItem->m_bounds);
    default:
        return MapModel::data(index, role);
    }
}

// These items have an extent, so the filter tests that rather than a single position
void PolylineMapModel::fillFilterData(MapItem *item, FilterData& data) const
{
    MapModel::fillFilterData(item, data);

    const QGeoRectangle& bounds = ((const PolylineMapItem *) item)->m_bounds;
    data.m_boundsTop = bounds.topLeft().latitude();
    data.m_boundsBottom = bounds.bottomRight().latitude();
    data.m_boundsLeft = bounds.topLeft().longitude();
    data.m_boundsRight = bounds.bottomRight().longitude();
}

void PolylineMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium) {
        cesium->update((PolylineMapItem *)item);
    }
}

void ObjectMapModel::allUpdated()
{
    for (auto item : m_items) {
        updateTrackSegments((ObjectMapItem *) item);
    }
    MapModel::allUpdated();
}

QVariant TrackColorModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();

    if ((row < 0) || (row >= m_segments.size())) {
        return QVariant();
    }

    if (role == segmentCoordinatesRole) {
        return m_segments[row];
    } else if (role == segmentColorRole) {
        return m_colors[row];
    }

    return QVariant();
}

void TrackColorModel::update(const QList<QVariantList>& segments, const QList<QVariant>& colors, int firstChanged)
{
    if (segments.size() == m_segments.size())
    {
        // Same segments as last time, just longer or shorter, so update them in place
        // rather than having the map delete and recreate a polyline for each one
        m_segments = segments;
        m_colors = colors;
        if (!m_segments.isEmpty() && (firstChanged < m_segments.size())) {
            emit dataChanged(index(qMax(0, firstChanged)), index(m_segments.size() - 1));
        }
    }
    else
    {
        beginResetModel();
        m_segments = segments;
        m_colors = colors;
        endResetModel();
    }
}

void TrackColorModel::clear()
{
    if (!m_segments.isEmpty())
    {
        beginResetModel();
        m_segments.clear();
        m_colors.clear();
        endResetModel();
    }
}

ObjectMapModel::ObjectMapModel(MapGUI *gui) :
    MapModel(gui),
    m_target(-1),
    m_displayNames(false),
    m_displaySelectedGroundTracks(false),
    m_displayAllGroundTracks(false),
    m_bottomLeftLongitude(0.0),
    m_bottomRightLongitude(0.0),
    m_frontZOrder(0.0),
    m_backZOrder(0.0)
{
    //connect(this, &ObjectMapModel::dataChanged, this, &ObjectMapModel::update3DMap);
}

Q_INVOKABLE void ObjectMapModel::add(MapItem *item)
{
    m_selected.append(false);
    MapModel::add(item);
}

/*void ObjectMapModel::update(const QObject *sourcePipe, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group)
{
    QString name = *swgMapItem->getName();
    // Add, update or delete and item
    ObjectMapItem *item = (ObjectMapItem *)MapModel::findMapItem(sourcePipe, name);
    if (item != nullptr)
    {
        QString image = *swgMapItem->getImage();
        if (image.isEmpty())
        {
            // Delete the item
            remove(item);
            // Need to call update, for it to be removed in 3D map
            // Item is set to not be available from this point in time
            // It will still be available if time is set in the past
            item->update(swgMapItem);
        }
        else
        {
            // Update the item
            item->update(swgMapItem);
            splitTracks(item);            // ******** diff from base   FIXME
            update(item);
        }
    }
    else
    {
        // Make sure not a duplicate request to delete
        QString image = *swgMapItem->getImage();
        if (!image.isEmpty())
        {
            // Add new item
            item = (ObjectMapItem *)newMapItem(sourcePipe, group, m_gui->getItemSettings(group), swgMapItem);
            add(item);
            // Add to 3D Map (we don't appear to get a dataChanged signal when adding)
            update3D(item);
        }
    }
}*/

MapItem *ObjectMapModel::newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem)
{
    return new ObjectMapItem(sourcePipe, group, itemSettings, mapItem);
}

void ObjectMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium && cesium->isConnected()) // Wait until connected, otherwise initial czml will be lost
    {
        ObjectMapItem *objectMapItem = (ObjectMapItem *)item;
        cesium->update(objectMapItem, isTarget(objectMapItem), isSelected3D(objectMapItem));
        // An immediate replay-state update intentionally skipped track serialization.
        // The normal batched update clears the debt after sending the complete tail.
        if (!objectMapItem->m_replayStateUpdate) {
            objectMapItem->m_replayTrackPending = false;
        }
        playAnimations(objectMapItem);
    }
}

// Slot called on dataChanged signal, to update 3D map
/*void ObjectMapModel::update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    (void) roles;

    CesiumInterface *cesium = m_gui->cesium();
    if (cesium)
    {
        for (int row = topLeft.row(); row <= bottomRight.row(); row++)
        {
            ObjectMapItem *item = (ObjectMapItem *)m_items[row];
            cesium->update(item, isTarget(item), isSelected3D(item));
            playAnimations(item);
        }
    }
} */

void ObjectMapModel::playAnimations(ObjectMapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium)
    {
        for (auto animation : item->m_animations) {
            m_gui->cesium()->playAnimation(item->m_name, animation);
        }
    }
    qDeleteAll(item->m_animations);
    item->m_animations.clear();
}

void ObjectMapModel::update(MapItem *item)
{
    // The track split and the target azimuth/elevation are done in flush(), so they
    // happen once per batch rather than once per message
    MapModel::update(item);
}

// An object that is off screen is still drawn if its track might be on screen
void ObjectMapModel::fillFilterData(MapItem *item, FilterData& data) const
{
    MapModel::fillFilterData(item, data);

    const ObjectMapItem *objectItem = (const ObjectMapItem *) item;
    const bool selected = (item->m_row >= 0) && (item->m_row < m_selected.size()) && m_selected[item->m_row];

    // Based on the split of the track that actually gets drawn, so that this keeps the
    // same set of items on the map as before. The split is redone when the batch is
    // flushed, so this can lag what the item holds by one flush - only ever for an item
    // that is off screen, whose track isn't drawable until that flush anyway
    if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && selected))
        && (   (objectItem->m_predictedTrack1.size() > 1)
            || (objectItem->m_predictedTrack2.size() > 1)
            || (objectItem->m_takenTrack1.size() > 1)
            || (objectItem->m_takenTrack2.size() > 1))) {
        data.m_flags |= FilterHasTracks;
    }
}

void ObjectMapModel::flush()
{
    if (!hasDirtyItems()) {
        return;
    }

    bool targetMoved = false;
    ObjectMapItem *targetItem = nullptr;

    for (auto it = m_dirty.cbegin(); it != m_dirty.cend(); ++it)
    {
        ObjectMapItem *item = (ObjectMapItem *) it.key();

        if (it.value() & (MapItem::ChangedTrack | MapItem::ChangedPredictedTrack)) {
            splitTracks(item);
        }
        if (item->m_row == m_target)
        {
            targetMoved = true;
            targetItem = item;
        }
    }

    if (targetMoved)
    {
        updateTarget();
        // The target's label carries the azimuth, elevation, range and coordinates that
        // updateTarget() has just recomputed, so for THIS row a change of position is a
        // change of label as well. Saying so in rolesFor() instead would reassign the
        // label of every moving item on every flush, which is the cost that scoping the
        // roles exists to avoid - here it is one row, once a flush. Safe after the loop
        // above has finished, and picked up by the flush below.
        markDirty(targetItem, MapItem::ChangedLabel);
    }

    MapModel::flush();
}

QVector<int> ObjectMapModel::rolesFor(quint32 changed) const
{
    if (changed == MapItem::ChangedAll) {
        return QVector<int>();   // All of them
    }

    if (m_roleCache.contains(changed)) {
        return m_roleCache.value(changed);
    }

    QVector<int> roles;

    if (changed & MapItem::ChangedPosition) {
        roles.append(positionRole);
    }
    if (changed & (MapItem::ChangedLabel | MapItem::ChangedText | MapItem::ChangedSelection)) {
        roles.append(labelRole);
        roles.append(mapTextVisibleRole);
    }
    if (changed & MapItem::ChangedText)
    {
        roles.append(frequenciesRole);
        roles.append(frequencyStringsRole);
    }
    if (changed & MapItem::ChangedImage)
    {
        roles.append(mapImageRole);
        roles.append(mapImageRotationRole);
        roles.append(mapImageVisibleRole);
    }
    if (changed & MapItem::ChangedZOrder) {
        roles.append(zOrderRole);
    }
    if (changed & MapItem::ChangedSelection)
    {
        roles.append(selectedRole);
        roles.append(targetRole);
        roles.append(bubbleColourRole);
    }
    // Which tracks are drawn depends on what is selected, as well as on the track itself
    if (changed & (MapItem::ChangedTrack | MapItem::ChangedSelection))
    {
        roles.append(groundTrack1Role);
        roles.append(groundTrack2Role);
        roles.append(groundTrackModelRole);
        roles.append(groundTrackColorRole);
    }
    if (changed & (MapItem::ChangedPredictedTrack | MapItem::ChangedSelection))
    {
        roles.append(predictedGroundTrack1Role);
        roles.append(predictedGroundTrack2Role);
        roles.append(predictedGroundTrackColorRole);
    }
    if (changed & (MapItem::ChangedTrack | MapItem::ChangedPredictedTrack | MapItem::ChangedSelection)) {
        roles.append(hasTracksRole);
    }

    m_roleCache.insert(changed, roles);

    return roles;
}

void ObjectMapModel::remove(MapItem *item)
{
    int row = item->m_row;
    if (row >= 0)
    {
        ObjectMapItem *objectItem = (ObjectMapItem *) item;
        if (objectItem->m_tracked)
        {
            m_trackedItems.removeOne(objectItem);
            objectItem->m_tracked = false;
        }
        m_selected.removeAt(row);
        if (row == m_target) {
            m_target = -1;
        } else if (row < m_target) {
            m_target--;
        }
        MapModel::remove(item);
    }
}

void ObjectMapModel::removeAll()
{
    for (auto item : m_trackedItems) {
        item->m_tracked = false;
    }
    m_trackedItems.clear();
    MapModel::removeAll();
    m_selected.clear();
}

QHash<int, QByteArray> ObjectMapModel::roleNames() const
{
    QHash<int, QByteArray> roles = MapModel::roleNames();
    //roles[itemSettingsRole] = "itemSettings";
    //roles[nameRole] = "name";
    //roles[positionRole] = "position";
    //roles[mapTextRole] = "mapText";
    roles[MapModel::labelRole] = "mapText";
    roles[mapTextVisibleRole] = "mapTextVisible";
    roles[mapImageVisibleRole] = "mapImageVisible";
    roles[mapImageRole] = "mapImage";
    roles[mapImageRotationRole] = "mapImageRotation";
    //roles[mapImageMinZoomRole] = "mapImageMinZoom";
    roles[bubbleColourRole] = "bubbleColour";
    roles[selectedRole] = "selected";
    roles[targetRole] = "target";
    roles[frequenciesRole] = "frequencies";
    roles[frequencyStringsRole] = "frequencyStrings";
    roles[predictedGroundTrack1Role] = "predictedGroundTrack1";
    roles[predictedGroundTrack2Role] = "predictedGroundTrack2";
    roles[groundTrack1Role] = "groundTrack1";
    roles[groundTrack2Role] = "groundTrack2";
    roles[groundTrackColorRole] = "groundTrackColor";
    roles[predictedGroundTrackColorRole] = "predictedGroundTrackColor";
    roles[hasTracksRole] = "hasTracks";
    roles[groundTrackModelRole] = "groundTrackModel";
    roles[zOrderRole] = "zOrder";
    return roles;
}

void ObjectMapModel::updateTarget()
{
    // Calculate range, azimuth and elevation to object from station
    AzEl *azEl = m_gui->getAzEl();
    azEl->setTarget(m_items[m_target]->m_latitude, m_items[m_target]->m_longitude, m_items[m_target]->m_altitude);
    azEl->calculate();

    // Send to Rotator Controllers
    QList<ObjectPipe*> rotatorPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "target", rotatorPipes);

    for (const auto& pipe : rotatorPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
        swgTarget->setName(new QString(m_items[m_target]->m_name));
        swgTarget->setAzimuth(azEl->getAzimuth());
        swgTarget->setElevation(azEl->getElevation());
        messageQueue->push(MainCore::MsgTargetAzimuthElevation::create(m_gui->getMap(), swgTarget));
    }
}

void ObjectMapModel::setTarget(const QString& name)
{
    if (name.isEmpty())
    {
        QModelIndex idx = index(-1);
        setData(idx, QVariant(-1), ObjectMapModel::targetRole);
    }
    else
    {
        QModelIndex idx = findMapItemIndex(name);
        setData(idx, QVariant(idx.row()), ObjectMapModel::targetRole);
    }
}

bool ObjectMapModel::isTarget(const ObjectMapItem *mapItem) const
{
    if (m_target < 0) {
        return false;
    } else {
        return m_items[m_target] == mapItem;
    }
}

Q_INVOKABLE void ObjectMapModel::moveToFront(int row)
{
    if ((row >= 0) && (row < m_items.size()))
    {
        m_frontZOrder += 1.0;
        m_items[row]->m_zOrder = m_frontZOrder;
        markDirty(m_items[row], MapItem::ChangedZOrder);
        flush();
    }
}

Q_INVOKABLE void ObjectMapModel::moveToBack(int row)
{
    if ((row >= 0) && (row < m_items.size()))
    {
        m_backZOrder -= 1.0;
        m_items[row]->m_zOrder = m_backZOrder;
        markDirty(m_items[row], MapItem::ChangedZOrder);
        flush();
    }
}

Q_INVOKABLE void ObjectMapModel::link(const QString& url)
{
    emit linkClicked(url);
}

QVariant ObjectMapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    ObjectMapItem *mapItem = (ObjectMapItem*)m_items[row];
    switch (role)
    {
    case labelRole: // mapTextRole)
    {
        // Create the text to go in the bubble next to the image
        QString name = mapItem->m_label.isEmpty() ? mapItem->m_name :mapItem->m_label;
        if (row == m_target)
        {
            AzEl *azEl = m_gui->getAzEl();
            QString text = QString("%1<br>Az: %2%5 El: %3%5 Dist: %4 km<br>Coords: %6, %7")
                                .arg(m_selected[row] ? mapItem->m_text : name)
                                .arg(std::round(azEl->getAzimuth()))
                                .arg(std::round(azEl->getElevation()))
                                .arg(std::round(azEl->getDistance() / 1000.0))
                                .arg(QChar(0xb0))
                                .arg(mapItem->m_latitude)
                                .arg(mapItem->m_longitude);
            return QVariant::fromValue(text);
        }
        else if (m_selected[row])
        {
            return QVariant::fromValue(mapItem->m_text);
        }
        else
        {
            return QVariant::fromValue(name);
        }
    }
    case mapTextVisibleRole:
        return QVariant::fromValue((m_selected[row] || m_displayNames) && mapItem->m_itemSettings->m_display2DLabel);
    case mapImageVisibleRole:
        return QVariant::fromValue(mapItem->m_itemSettings->m_display2DIcon);
    case mapImageRole:
        // Set an image to use, preferring the livery icon when enabled and available
        if (!mapItem->m_liveryImage.isEmpty() && m_gui->useLiveryIcons()) {
            return QVariant::fromValue(mapItem->m_liveryImage);
        }
        return QVariant::fromValue(mapItem->m_image);
    case mapImageRotationRole:
        return QVariant::fromValue(mapItem->m_imageRotation);  // Angle to rotate image by
    case bubbleColourRole:
    {
        // Select a background colour for the text bubble next to the item
        if (m_selected[row]) {
            return QVariant::fromValue(QColor("lightgreen"));
        } else {
            return QVariant::fromValue(QColor("lightblue"));
        }
    }
    case selectedRole:
        return QVariant::fromValue(m_selected[row]);
    case targetRole:
        return QVariant::fromValue(m_target == row);
    case frequenciesRole:
        return QVariant::fromValue(mapItem->m_frequencies);
    case frequencyStringsRole:
        return QVariant::fromValue(mapItem->m_frequencyStrings);
    case predictedGroundTrack1Role:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack) {
            return mapItem->m_predictedTrack1;
        } else {
            return QVariantList();
        }
    }
    case predictedGroundTrack2Role:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack) {
            return mapItem->m_predictedTrack2;
        } else {
            return QVariantList();
        }
    }
    case groundTrack1Role:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack
            && mapItem->m_itemSettings->m_2DTrackPalette.isEmpty()) {
            return mapItem->m_takenTrack1;
        } else {
            return QVariantList();
        }
    }
    case groundTrack2Role:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack
            && mapItem->m_itemSettings->m_2DTrackPalette.isEmpty()) {
            return mapItem->m_takenTrack2;
        } else {
            return QVariantList();
        }
    }
    case groundTrackModelRole:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack
            && !mapItem->m_itemSettings->m_2DTrackPalette.isEmpty()
            && mapItem->m_trackColorModel) {
            return QVariant::fromValue(mapItem->m_trackColorModel);
        } else {
            return QVariant::fromValue(&m_noTrackColors);
        }
    }
    case zOrderRole:
        return QVariant::fromValue(mapItem->m_zOrder);
    case groundTrackColorRole:
        return QVariant::fromValue(QColor::fromRgb(mapItem->m_itemSettings->m_2DTrackColor));
    case predictedGroundTrackColorRole:
        return QVariant::fromValue(QColor::fromRgb(mapItem->m_itemSettings->m_2DTrackColor).lighter());
    case hasTracksRole:
    {
        bool hasTracks =    (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
                         && (   (mapItem->m_predictedTrack1.size() > 1)
                             || (mapItem->m_predictedTrack2.size() > 1)
                             || (mapItem->m_takenTrack1.size() > 1)
                             || (mapItem->m_takenTrack2.size() > 1)
                            );
        return QVariant::fromValue(hasTracks);
    }
    default:
        return MapModel::data(index, role);
    }
}

bool ObjectMapModel::setData(const QModelIndex &idx, const QVariant& value, int role)
{
    int row = idx.row();
    if ((row < 0) || (row >= m_items.count()))
        return false;
    if (role == selectedRole)
    {
        m_selected[row] = value.toBool();
        markDirty(m_items[row], MapItem::ChangedSelection);
        flush();
        return true;
    }
    else if (role == targetRole)
    {
        if (m_target >= 0)
        {
            // Update text bubble for old target
            markDirty(m_items[m_target], MapItem::ChangedSelection);
            m_target = -1;
        }
        m_target = row;
        updateTarget();
        markDirty(m_items[row], MapItem::ChangedSelection);
        flush();
        return true;
    }
    return true;
}

void ObjectMapModel::setDisplayNames(bool displayNames)
{
    m_displayNames = displayNames;
    allUpdated();
}

void ObjectMapModel::setDisplaySelectedGroundTracks(bool displayGroundTracks)
{
    // Whether an off-screen item is kept for its track depends on this
    m_displaySelectedGroundTracks = displayGroundTracks;
    allUpdated();
}

void ObjectMapModel::setDisplayAllGroundTracks(bool displayGroundTracks)
{
    // Whether an off-screen item is kept for its track depends on this
    m_displayAllGroundTracks = displayGroundTracks;
    allUpdated();
}

void ObjectMapModel::track3D(int index)
{
    if (index < m_items.count())
    {
        MapItem *item = m_items[index];
        m_gui->track3D(item->m_name);
    }
}

QStringList ObjectMapModel::getDeviceSets() const
{
    return MainCore::instance()->getDeviceSetIds(true, true, false); // FIXME: MIMO currently disabled, as we can't get channel stream indexes
}

void ObjectMapModel::setFrequency(qint64 frequency, const QString& deviceSetId)
{
    unsigned int deviceSetIndex;

    if (MainCore::getDeviceSetIndexFromId(deviceSetId, deviceSetIndex))
    {
        // Set as centre frequency
        ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, frequency);
    }
}

void ObjectMapModel::splitTracks(ObjectMapItem *item)
{
    PROFILER_START()
    if (!item->m_tracked && ((item->m_takenTrackCoords.size() > 1) || (item->m_predictedTrackCoords.size() > 1)))
    {
        item->m_tracked = true;
        m_trackedItems.append(item);
    }
    const bool hasTaken = item->m_takenTrackCoords.size() > 1;
    const bool hasPredicted = item->m_predictedTrackCoords.size() > 1;

    if (!hasTaken && !hasPredicted)
    {
        PROFILER_STOP("Map splitTracks")
        return;
    }

    const ViewLongitudes view = viewLongitudes();

    if (hasTaken)
    {
        splitTakenTrack(item, view);
        updateTrackSegments(item);
    }
    if (hasPredicted)
    {
        splitTrack(view, item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                        item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
    }
    PROFILER_STOP("Map splitTracks")
}

// Split the ground track in to segments of constant colour, so it can be drawn
// showing the altitude the object was at. The track has already been split where it
// crosses the edge of the map, so we split each of those parts again whenever the
// altitude moves in to a different band.
// Add points to the item's colour segments, starting a new segment wherever the altitude
// moves in to a different band
void ObjectMapModel::colorTrackPoints(ObjectMapItem *item, const QVariantList& track, int first, const QVariant *palette)
{
    for (int i = first; i < track.size(); i++)
    {
        const QVariant& point = track.at(i);
        const QGeoCoordinate coord = point.value<QGeoCoordinate>();
        const int colorIndex = FlightPathPalette::getColorIndex(Units::metresToFeet(coord.altitude()));

        if (colorIndex != item->m_trackSegmentLastColor)
        {
            // Repeat the point the colour changes at in the segment that ends here, so
            // there's no gap between the two
            if (item->m_trackSegmentLastColor >= 0) {
                item->m_trackSegments.last().append(point);
            }
            item->m_trackSegments.append(QVariantList());
            item->m_trackSegmentColors.append(palette[colorIndex]);
            item->m_trackSegmentLastColor = colorIndex;
        }
        item->m_trackSegments.last().append(point);
    }
}

void ObjectMapModel::updateTrackSegments(ObjectMapItem *item)
{
    if (item->m_itemSettings->m_2DTrackPalette.isEmpty())
    {
        // Not coloured by altitude - the track is drawn as a plain polyline instead
        if (item->m_trackColorModel) {
            item->m_trackColorModel->clear();
        }
        item->m_trackSegmentPoints = 0;
        item->m_trackSegmentLastCoord = QGeoCoordinate();
        item->m_trackSegmentRevision = item->m_takenTrackRevision;
        return;
    }

    PROFILER_START()

    const QString& paletteName = item->m_itemSettings->m_2DTrackPalette;
    const QVariant *palette = FlightPathPalette::getPalette(paletteName);

    // Nothing before the last point we looked at can have changed if the whole track is
    // on screen (so the split is just the track itself) and points have only been added
    // to the end of it. Then we only have to colour what is new
    const bool extend = item->m_takenTrackWholeOnScreen
                     && item->m_takenTrack2.isEmpty()
                     && (item->m_trackSegmentPoints > 0)
                     && (item->m_takenTrack1.size() >= item->m_trackSegmentPoints)
                     && (item->m_takenTrack1[item->m_trackSegmentPoints - 1].value<QGeoCoordinate>()
                         == item->m_trackSegmentLastCoord)
                     && (item->m_trackSegmentRevision == item->m_takenTrackRevision)
                     && (item->m_trackSegmentPalette == paletteName);

    if (!extend)
    {
        item->m_trackSegments.clear();
        item->m_trackSegmentColors.clear();
        item->m_trackSegmentPoints = 0;
        item->m_trackSegmentLastColor = -1;
        item->m_trackSegmentPalette = paletteName;
    }

    // Colour the first part, continuing from where we left off if we can. Only the
    // segment we carry on from, and anything after it, can have changed
    const int firstChanged = extend ? qMax(0, (int) item->m_trackSegments.size() - 1) : 0;

    colorTrackPoints(item, item->m_takenTrack1, extend ? item->m_trackSegmentPoints : 0, palette);
    item->m_trackSegmentPoints = item->m_takenTrack1.size();
    item->m_trackSegmentLastCoord = item->m_takenTrack1.isEmpty()
        ? QGeoCoordinate() : item->m_takenTrack1.last().value<QGeoCoordinate>();
    item->m_trackSegmentRevision = item->m_takenTrackRevision;

    // The second part starts at the edge of the screen, so it doesn't join on to the
    // first. It is only ever non-empty when the track had to be split, which is also
    // when we rebuilt from scratch above
    if (!item->m_takenTrack2.isEmpty())
    {
        item->m_trackSegmentLastColor = -1;
        colorTrackPoints(item, item->m_takenTrack2, 0, palette);
    }

    if (!item->m_trackColorModel) {
        item->m_trackColorModel = new TrackColorModel();
    }
    item->m_trackColorModel->update(item->m_trackSegments, item->m_trackSegmentColors, firstChanged);
    PROFILER_STOP("Map trackSegments")
}

void ObjectMapModel::interpolateEast(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen)
{
    double x1 = c1->longitude();
    double y1 = c1->latitude();
    double x2 = c2->longitude();
    double y2 = c2->latitude();
    double y;
    if (x2 < x1)
        x2 += 360.0;
    if (x < x1)
        x += 360.0;
    y = interpolate(x1, y1, x2, y2, x);
    if (x > 180)
        x -= 360.0;
    if (offScreen)
        x -= 0.000000001;
    else
        x += 0.000000001;
    ci->setLongitude(x);
    ci->setLatitude(y);
    ci->setAltitude(c1->altitude());
}

void ObjectMapModel::interpolateWest(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen)
{
    double x1 = c1->longitude();
    double y1 = c1->latitude();
    double x2 = c2->longitude();
    double y2 = c2->latitude();
    double y;
    if (x2 > x1)
        x2 -= 360.0;
    if (x > x1)
        x -= 360.0;
    y = interpolate(x1, y1, x2, y2, x);
    if (x < -180)
        x += 360.0;
    if (offScreen)
        x += 0.000000001;
    else
        x -= 0.000000001;
    ci->setLongitude(x);
    ci->setLatitude(y);
    ci->setAltitude(c1->altitude());
}

static bool isOnScreen(double lon, double bottomLeftLongitude, double bottomRightLongitude, double width, bool antimed)
{
    bool onScreen = false;
    if (width == 360)
        onScreen = true;
    else if (!antimed)
        onScreen = (lon > bottomLeftLongitude) && (lon <= bottomRightLongitude);
    else
        onScreen = (lon > bottomLeftLongitude) || (lon <= bottomRightLongitude);
    return onScreen;
}

static bool crossesAntimeridian(double prevLon, double lon)
{
    bool crosses = false;
    if ((prevLon > 90) && (lon < -90))
        crosses = true; // West to East
    else if ((prevLon < -90) && (lon > 90))
        crosses = true; // East to West
    return crosses;
}

static bool crossesAntimeridianEast(double prevLon, double lon)
{
    bool crosses = false;
    if ((prevLon > 90) && (lon < -90))
        crosses = true; // West to East
    return crosses;
}

static bool crossesAntimeridianWest(double prevLon, double lon)
{
    bool crosses = false;
    if ((prevLon < -90) && (lon > 90))
        crosses = true; // East to West
    return crosses;
}

static bool crossesEdge(double lon, double prevLon, double bottomLeftLongitude, double bottomRightLongitude)
{
    // Determine if antimerdian is between the two points
    if (!crossesAntimeridian(prevLon, lon))
    {
        bool crosses = false;
        if ((prevLon <= bottomRightLongitude) && (lon > bottomRightLongitude))
            crosses = true; // Crosses right edge East
        else if ((prevLon >= bottomRightLongitude) && (lon < bottomRightLongitude))
            crosses = true; // Crosses right edge West
        else if ((prevLon >= bottomLeftLongitude) && (lon < bottomLeftLongitude))
            crosses = true; // Crosses left edge West
        else if ((prevLon <= bottomLeftLongitude) && (lon > bottomLeftLongitude))
            crosses = true; // Crosses left edge East
        return crosses;
    }
    else
    {
        // Determine which point and the edge the antimerdian is between
        bool prevLonToRightCrossesAnti = crossesAntimeridianEast(prevLon, bottomRightLongitude);
        bool rightToLonCrossesAnti = crossesAntimeridianEast(bottomRightLongitude, lon);
        bool prevLonToLeftCrossesAnti = crossesAntimeridianWest(prevLon, bottomLeftLongitude);
        bool leftToLonCrossesAnti = crossesAntimeridianWest(bottomLeftLongitude, lon);

        bool crosses = false;
        if (   ((prevLon > bottomRightLongitude) && prevLonToRightCrossesAnti && (lon > bottomRightLongitude))
            || ((prevLon <= bottomRightLongitude) && (lon <= bottomRightLongitude) && rightToLonCrossesAnti)
           )
            crosses = true; // Crosses right edge East
        else if (   ((prevLon < bottomRightLongitude) && prevLonToRightCrossesAnti && (lon < bottomRightLongitude))
                 || ((prevLon >= bottomRightLongitude) && (lon >= bottomRightLongitude) && rightToLonCrossesAnti)
                )
            crosses = true; // Crosses right edge West
        else if (   ((prevLon < bottomLeftLongitude) && prevLonToLeftCrossesAnti && (lon < bottomLeftLongitude))
                 || ((prevLon >= bottomLeftLongitude) && (lon >= bottomLeftLongitude) && leftToLonCrossesAnti)
                )
            crosses = true; // Crosses left edge West
        else if (   ((prevLon > bottomLeftLongitude) && prevLonToLeftCrossesAnti && (lon > bottomLeftLongitude))
                 || ((prevLon <= bottomLeftLongitude) && (lon <= bottomLeftLongitude) && leftToLonCrossesAnti)
                )
            crosses = true; // Crosses left edge East
        return crosses;
    }
}

void ObjectMapModel::interpolate(QGeoCoordinate *c1, QGeoCoordinate *c2, double bottomLeftLongitude, double bottomRightLongitude, QGeoCoordinate* ci, bool offScreen)
{
    double x1 = c1->longitude();
    double x2 = c2->longitude();
    double crossesAnti = crossesAntimeridian(x1, x2);
    double x;

    // Need to work out which edge we're interpolating too
    // and whether antimeridian is in the way, as that flips x1<x2 to x1>x2

    if (((x1 < x2) && !crossesAnti) || ((x1 > x2) && crossesAnti))
    {
        x = offScreen ? bottomRightLongitude : bottomLeftLongitude;
        interpolateEast(c1, c2, x, ci, offScreen);
    }
    else
    {
        x = offScreen ? bottomLeftLongitude : bottomRightLongitude;
        interpolateWest(c1, c2, x, ci, offScreen);
    }
}

void ObjectMapModel::splitTakenTrack(ObjectMapItem *item, const ViewLongitudes& view)
{
    if (wholeTrackOnScreen(view, item->m_takenTrackMinLongitude, item->m_takenTrackMaxLongitude))
    {
        // Nothing to split. The lists are implicitly shared, so this is just a pointer
        // copy, rather than a walk over every point in the track
        item->m_takenTrack1 = item->m_takenTrack;
        item->m_takenTrack2.clear();
        item->m_takenTrackWholeOnScreen = true;
    }
    else
    {
        splitTrack(view, item->m_takenTrackCoords, item->m_takenTrack, item->m_takenTrack1, item->m_takenTrack2,
                        item->m_takenStart1, item->m_takenStart2, item->m_takenEnd1, item->m_takenEnd2);
        item->m_takenTrackWholeOnScreen = false;
    }
}

// The split only exists to stop MapPolyline drawing lines that cross the edge of the map
// in the wrong place, so a track that is comfortably within what's on screen doesn't need
// it. The margin covers the last track point being moved slightly by extrapolation after
// the bounds were last extended
bool ObjectMapModel::wholeTrackOnScreen(const ViewLongitudes& view, double minLongitude, double maxLongitude)
{
    const double margin = 1.0;

    return !view.m_antimeridian
        && (view.m_width < 360)
        && (minLongitude - margin > view.m_bottomLeft)
        && (maxLongitude + margin <= view.m_bottomRight);
}

// Ask the map what it is showing. This is a synchronous call in to QML, so it's done
// once per pass over the items rather than once per track
ObjectMapModel::ViewLongitudes ObjectMapModel::viewLongitudes() const
{
    QQuickItem* map = m_gui->getMapItem();
    QVariant rectVariant;
    QMetaObject::invokeMethod(map, "mapRect", Q_RETURN_ARG(QVariant, rectVariant));
    QGeoRectangle rect = qvariant_cast<QGeoRectangle>(rectVariant);

    ViewLongitudes view;
    view.m_bottomLeft = rect.bottomLeft().longitude();
    view.m_bottomRight = rect.bottomRight().longitude();

    view.m_width = round(rect.width());
    view.m_antimeridian = (view.m_width == 360) || (view.m_bottomLeft > view.m_bottomRight);

    return view;
}

void ObjectMapModel::splitTrack(const ViewLongitudes& view,
                        const QList<QGeoCoordinate *>& coords, const QVariantList& track,
                        QVariantList& track1, QVariantList& track2,
                        QGeoCoordinate& start1, QGeoCoordinate& start2,
                        QGeoCoordinate& end1, QGeoCoordinate& end2)
{
    /*
    QStringList l;
    for (int i = 0; i < track.size(); i++)
    {
        QGeoCoordinate c = track[i].value<QGeoCoordinate>();
        l.append(QString("%1").arg((int)c.longitude()));
    }
    qDebug() << "Init T: " << l;
    */

    const double bottomLeftLongitude = view.m_bottomLeft;
    const double bottomRightLongitude = view.m_bottomRight;
    const int width = view.m_width;
    const bool antimed = view.m_antimeridian;

    /*
    qDebug() << "Anitmed visible: " << antimed;
    qDebug() << "bottomLeftLongitude: " << bottomLeftLongitude;
    qDebug() << "bottomRightLongitude: " << bottomRightLongitude;
    */

    track1.clear();
    track2.clear();

    double lon, prevLon;
    bool onScreen, prevOnScreen;
    QList<QVariantList *> tracks({&track1, &track2});
    QList<QGeoCoordinate *> ends({&end1, &end2});
    QList<QGeoCoordinate *> starts({&start1, &start2});
    int trackIdx = 0;
    for (int i = 0; i < coords.size(); i++)
    {
        lon = coords[i]->longitude();
        if (i == 0)
        {
            prevLon = lon;
            prevOnScreen = true; // To avoid interpolation for first point
        }
        // Can be onscreen after having crossed edge from other side
        // Or can be onscreen after previously having been off screen
        onScreen = isOnScreen(lon, bottomLeftLongitude, bottomRightLongitude, width, antimed);
        bool crossedEdge = crossesEdge(lon, prevLon, bottomLeftLongitude, bottomRightLongitude);
        if ((onScreen && !crossedEdge) || (onScreen && !prevOnScreen))
        {
            if ((i > 0) && (tracks[trackIdx]->size() == 0)) // Could also use (onScreen && !prevOnScreen)?
            {
                if (trackIdx >= starts.size())
                    break;
                // Interpolate from edge of screen
                interpolate(coords[i-1], coords[i], bottomLeftLongitude, bottomRightLongitude, starts[trackIdx], false);
                tracks[trackIdx]->append(QVariant::fromValue(*starts[trackIdx]));
            }
            tracks[trackIdx]->append(track[i]);
        }
        else if (tracks[trackIdx]->size() > 0)
        {
            // Either we've crossed to the other side, or have gone off screen
            if (trackIdx >= ends.size())
                break;
            // Interpolate to edge of screen
            interpolate(coords[i-1], coords[i], bottomLeftLongitude, bottomRightLongitude, ends[trackIdx], true);
            tracks[trackIdx]->append(QVariant::fromValue(*ends[trackIdx]));
            // Start new track
            trackIdx++;
            if (trackIdx >= tracks.size())
            {
                // This can happen with highly retrograde orbits, where trace 90% of period
                // will cover more than 360 degrees - delete last point as Map
                // will not be able to display it properly
                tracks[trackIdx-1]->removeLast();
                break;
            }
            if (onScreen)
            {
                // Interpolate from edge of screen
                interpolate(coords[i-1], coords[i], bottomLeftLongitude, bottomRightLongitude, starts[trackIdx], false);
                tracks[trackIdx]->append(QVariant::fromValue(*starts[trackIdx]));
                tracks[trackIdx]->append(track[i]);
            }
        }
        prevLon = lon;
        prevOnScreen = onScreen;
    }

    /*
    l.clear();
    for (int i = 0; i < track1.size(); i++)
    {
        QGeoCoordinate c = track1[i].value<QGeoCoordinate>();
        if (!c.isValid())
            l.append("Invalid!");
        else
            l.append(QString("%1").arg(c.longitude(), 0, 'f', 1));
    }
    qDebug() << "T1: " << l;

    l.clear();
    for (int i = 0; i < track2.size(); i++)
    {
        QGeoCoordinate c = track2[i].value<QGeoCoordinate>();
        if (!c.isValid())
            l.append("Invalid!");
        else
        l.append(QString("%1").arg(c.longitude(), 0, 'f', 1));
    }
    qDebug() << "T2: " << l;
    */
}

void ObjectMapModel::viewChanged(double bottomLeftLongitude, double bottomRightLongitude)
{
    PROFILER_START()
    (void) bottomRightLongitude;

    if (!std::isnan(bottomLeftLongitude))
    {
        const ViewLongitudes view = viewLongitudes();

        for (ObjectMapItem *item : m_trackedItems)
        {
            const int row = item->m_row;
            if ((row >= 0) && item->m_itemSettings->m_enabled)
            {
                if (item->m_takenTrackCoords.size() > 1)
                {
                    const bool wholeOnScreen = wholeTrackOnScreen(view, item->m_takenTrackMinLongitude, item->m_takenTrackMaxLongitude);

                    // If it was wholly on screen before and still is, the split it already
                    // has is the whole track, so the pan hasn't changed anything
                    if (!wholeOnScreen || !item->m_takenTrackWholeOnScreen)
                    {
                        splitTakenTrack(item, view);
                        updateTrackSegments(item);
                        QModelIndex idx = index(row);
                        QVector<int> roles = {groundTrack1Role, groundTrack2Role, groundTrackModelRole};
                        emit dataChanged(idx, idx, roles);
                    }
                }
                if (item->m_predictedTrackCoords.size() > 1)
                {
                    splitTrack(view, item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                                    item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
                    QModelIndex idx = index(row);
                    QVector<int> roles = {predictedGroundTrack1Role, predictedGroundTrack2Role};
                    emit dataChanged(idx, idx, roles);
                }
            }
        }
    }
    PROFILER_STOP("Map viewChanged")
}

// Longitude wraps at the antimeridian, and a QGeoRectangle that crosses it reports a
// LEFT edge GREATER than its right - a view centred on the Pacific runs from 170 to
// -170. That is why the tests below cannot be plain comparisons: "longitude >= 170 and
// longitude <= -170" is false everywhere, so everything actually on screen was filtered
// out and the map went empty. QGeoRectangle::intersects() handles the wrap, but it was
// replaced by these scalar tests because it cost 10.5 ms a pan against 1.7 ms; these
// keep that and handle the wrap too.
//
// An item can wrap as well as the view - a flight plan from Anchorage to Tokyo does -
// so both sides are checked for it.
static inline bool longitudeInView(double longitude, double left, double right)
{
    return (left <= right) ? ((longitude >= left) && (longitude <= right))
                           : ((longitude >= left) || (longitude <= right));
}

static inline bool longitudesOverlap(double aLeft, double aRight, double bLeft, double bRight)
{
    const bool aWraps = aLeft > aRight;
    const bool bWraps = bLeft > bRight;
    if (aWraps && bWraps) {
        return true;            // both contain the antimeridian, so they share it
    }
    if (aWraps) {
        return (bRight >= aLeft) || (bLeft <= aRight);
    }
    if (bWraps) {
        return (aRight >= bLeft) || (aLeft <= bRight);
    }
    return (aLeft <= bRight) && (aRight >= bLeft);
}

bool ObjectMapFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    (void) sourceParent;

    // Everything expensive was worked out when the item last changed - see
    // MapModel::fillFilterData. This runs for every row every time the map moves
    const MapModel::FilterData& data = ((const MapModel *) sourceModel())->filterData(sourceRow);

    if (!(data.m_flags & MapModel::FilterEnabled)) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    if (data.m_minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Don't show off-screen items, unless they have tracks, as these may be on screen
    const bool onScreen = (data.m_latitude >= m_bottomRightLatitude) && (data.m_latitude <= m_topLeftLatitude)
                       && longitudeInView(data.m_longitude, m_topLeftLongitude, m_bottomRightLongitude);
    if (!onScreen && !(data.m_flags & MapModel::FilterHasTracks)) {
        return false;
    }

    // The user's name and distance filters
    return (data.m_flags & (MapModel::FilterNameMatches | MapModel::FilterDistanceOk))
        == (MapModel::FilterNameMatches | MapModel::FilterDistanceOk);
}

void ObjectMapFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    PROFILER_START()
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude))
    {
        m_topLeftLongitude = topLeftLongitude;
        m_topLeftLatitude = topLeftLatitude;
        m_bottomRightLongitude = bottomRightLongitude;
        m_bottomRightLatitude = bottomRightLatitude;
    }
    invalidateFilter();
    PROFILER_STOP("Map filter objects")
}

Q_INVOKABLE int ObjectMapFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}


bool PolygonFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    (void) sourceParent;

    const MapModel::FilterData& data = ((const MapModel *) sourceModel())->filterData(sourceRow);

    if (!(data.m_flags & MapModel::FilterEnabled)) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    if (data.m_minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items, by overlap of their extent with what's visible
    if (   (data.m_boundsBottom > m_view.topLeft().latitude())
        || (data.m_boundsTop < m_view.bottomRight().latitude())
        || !longitudesOverlap(data.m_boundsLeft, data.m_boundsRight,
                              m_view.topLeft().longitude(), m_view.bottomRight().longitude())) {
        return false;
    }

    // The user's name and distance filters
    return (data.m_flags & (MapModel::FilterNameMatches | MapModel::FilterDistanceOk))
        == (MapModel::FilterNameMatches | MapModel::FilterDistanceOk);
}

void PolygonFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    PROFILER_START()
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
    PROFILER_STOP("Map filter polygons")
}

Q_INVOKABLE int PolygonFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}

bool PolylineFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    (void) sourceParent;

    const MapModel::FilterData& data = ((const MapModel *) sourceModel())->filterData(sourceRow);

    if (!(data.m_flags & MapModel::FilterEnabled)) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    if (data.m_minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items, by overlap of their extent with what's visible
    if (   (data.m_boundsBottom > m_view.topLeft().latitude())
        || (data.m_boundsTop < m_view.bottomRight().latitude())
        || !longitudesOverlap(data.m_boundsLeft, data.m_boundsRight,
                              m_view.topLeft().longitude(), m_view.bottomRight().longitude())) {
        return false;
    }

    // The user's name and distance filters
    return (data.m_flags & (MapModel::FilterNameMatches | MapModel::FilterDistanceOk))
        == (MapModel::FilterNameMatches | MapModel::FilterDistanceOk);
}

void PolylineFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    PROFILER_START()
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
    PROFILER_STOP("Map filter polylines")
}

Q_INVOKABLE int PolylineFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}

bool ImageFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    (void) sourceParent;

    const MapModel::FilterData& data = ((const MapModel *) sourceModel())->filterData(sourceRow);

    if (!(data.m_flags & MapModel::FilterEnabled)) {
        return false;
    }

    if (!(data.m_flags & MapModel::FilterDisplayIcon)) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    if (data.m_minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items, by overlap of their extent with what's visible
    if (   (data.m_boundsBottom > m_view.topLeft().latitude())
        || (data.m_boundsTop < m_view.bottomRight().latitude())
        || !longitudesOverlap(data.m_boundsLeft, data.m_boundsRight,
                              m_view.topLeft().longitude(), m_view.bottomRight().longitude())) {
        return false;
    }

    // The user's name and distance filters
    return (data.m_flags & (MapModel::FilterNameMatches | MapModel::FilterDistanceOk))
        == (MapModel::FilterNameMatches | MapModel::FilterDistanceOk);
}

void ImageFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    PROFILER_START()
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
    PROFILER_STOP("Map filter images")
}

Q_INVOKABLE int ImageFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}

void PolygonFilter::setPosition(const QGeoCoordinate& position)
{
    m_position = position;
    invalidateFilter();
}

void PolylineFilter::setPosition(const QGeoCoordinate& position)
{
    m_position = position;
    invalidateFilter();
}

void ObjectMapFilter::setPosition(const QGeoCoordinate& position)
{
    m_position = position;
    invalidateFilter();
}

void ImageFilter::setPosition(const QGeoCoordinate& position)
{
    m_position = position;
    invalidateFilter();
}
