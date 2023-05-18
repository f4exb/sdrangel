///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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
#include "maincore.h"

#include "mapmodel.h"
#include "mapgui.h"
#include "map.h"

#include "SWGTargetAzimuthElevation.h"

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
    m_items.append(item);
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
        }
        else
        {
            // Update the item
            item->update(swgMapItem);
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
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
    }
}

void MapModel::remove(MapItem *item)
{
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
        QString key = m_items[row]->m_hashKey;
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        m_itemsHash.remove(key);
        endRemoveRows();
    }
}

void MapModel::removeAll()
{
    if (m_items.count() > 0)
    {
        beginRemoveRows(QModelIndex(), 0, m_items.count() - 1);
        m_items.clear();
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
}

void MapModel::allUpdated()
{
    if (m_items.count() > 0) {
        emit dataChanged(index(0), index(m_items.count()-1));
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
void MapModel::update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    (void) roles;

    for (int row = topLeft.row(); row <= bottomRight.row(); row++) {
        update3D(m_items[row]);
    }
}

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

void PolygonMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium) {
        cesium->update((PolygonMapItem *)item);
    }
}

MapItem *PolylineMapModel::newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem)
{
    return new PolylineMapItem(sourcePipe, group, itemSettings, mapItem);
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
        return QVariant::fromValue(polylineItem->m_polyline);
    case boundsRole:
        return QVariant::fromValue(polylineItem->m_bounds);
    default:
        return MapModel::data(index, role);
    }
}

void PolylineMapModel::update3D(MapItem *item)
{
    CesiumInterface *cesium = m_gui->cesium();
    if (cesium) {
        cesium->update((PolylineMapItem *)item);
    }
}

ObjectMapModel::ObjectMapModel(MapGUI *gui) :
    MapModel(gui),
    m_target(-1)
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
    if (cesium)
    {
        ObjectMapItem *objectMapItem = (ObjectMapItem *)item;
        cesium->update(objectMapItem, isTarget(objectMapItem), isSelected3D(objectMapItem));
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
    splitTracks((ObjectMapItem *)item);
    MapModel::update(item);
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
        if (row == m_target) {
            updateTarget();
        }
    }
}

void ObjectMapModel::remove(MapItem *item)
{
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
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
    roles[frequencyRole] = "frequency";
    roles[frequencyStringRole] = "frequencyString";
    roles[predictedGroundTrack1Role] = "predictedGroundTrack1";
    roles[predictedGroundTrack2Role] = "predictedGroundTrack2";
    roles[groundTrack1Role] = "groundTrack1";
    roles[groundTrack2Role] = "groundTrack2";
    roles[groundTrackColorRole] = "groundTrackColor";
    roles[predictedGroundTrackColorRole] = "predictedGroundTrackColor";
    roles[hasTracksRole] = "hasTracks";
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

// FIXME: This should use Z order - rather than adding/removing
// but I couldn't quite get it to work
Q_INVOKABLE void ObjectMapModel::moveToFront(int oldRow)
{
    // Last item in list is drawn on top, so remove than add to end of list
    if (oldRow < m_items.size() - 1)
    {
        bool wasTarget = m_target == oldRow;
        ObjectMapItem *item = (ObjectMapItem *)m_items[oldRow];
        bool wasSelected = m_selected[oldRow];
        remove(item);
        add(item);
        int newRow = m_items.size() - 1;
        if (wasTarget) {
            m_target = newRow;
        }
        m_selected[newRow] = wasSelected;
        QModelIndex idx = index(newRow);
        emit dataChanged(idx, idx);
    }
}

Q_INVOKABLE void ObjectMapModel::moveToBack(int oldRow)
{
    // First item in list is drawn first, so remove item then add to front of list
    if ((oldRow < m_items.size()) && (oldRow > 0))
    {
        bool wasTarget = m_target == oldRow;
        int newRow = 0;
        // See: https://forum.qt.io/topic/122991/changing-the-order-mapquickitems-are-drawn-on-a-map
        //QModelIndex parent;
        //beginMoveRows(parent, oldRow, oldRow, parent, newRow);
        beginResetModel();
        m_items.move(oldRow, newRow);
        m_selected.move(oldRow, newRow);
        if (wasTarget) {
            m_target = newRow;
        } else if (m_target >= 0) {
            m_target++;
        }
        //endMoveRows();
        endResetModel();
        //emit dataChanged(index(oldRow), index(newRow));
    }
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
        return QVariant::fromValue(mapItem->m_image); // Set an image to use
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
    case frequencyRole:
        return QVariant::fromValue(mapItem->m_frequency);
    case frequencyStringRole:
        return QVariant::fromValue(mapItem->m_frequencyString);
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
            && mapItem->m_itemSettings->m_display2DTrack) {
            return mapItem->m_takenTrack1;
        } else {
            return QVariantList();
        }
    }
    case groundTrack2Role:
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && mapItem->m_itemSettings->m_display2DTrack) {
            return mapItem->m_takenTrack2;
        } else {
            return QVariantList();
        }
    }
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
        emit dataChanged(idx, idx);
        return true;
    }
    else if (role == targetRole)
    {
        if (m_target >= 0)
        {
            // Update text bubble for old target
            QModelIndex oldIdx = index(m_target);
            m_target = -1;
            emit dataChanged(oldIdx, oldIdx);
        }
        m_target = row;
        updateTarget();
        emit dataChanged(idx, idx);
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
    m_displaySelectedGroundTracks = displayGroundTracks;
    allUpdated();
}

void ObjectMapModel::setDisplayAllGroundTracks(bool displayGroundTracks)
{
    m_displayAllGroundTracks = displayGroundTracks;
    allUpdated();
}

void ObjectMapModel::setFrequency(double frequency)
{
    // Set as centre frequency
    ChannelWebAPIUtils::setCenterFrequency(0, frequency);
}

void ObjectMapModel::track3D(int index)
{
    if (index < m_items.count())
    {
        MapItem *item = m_items[index];
        m_gui->track3D(item->m_name);
    }
}

void ObjectMapModel::splitTracks(ObjectMapItem *item)
{
    if (item->m_takenTrackCoords.size() > 1)
    {
        splitTrack(item->m_takenTrackCoords, item->m_takenTrack, item->m_takenTrack1, item->m_takenTrack2,
                        item->m_takenStart1, item->m_takenStart2, item->m_takenEnd1, item->m_takenEnd2);
    }
    if (item->m_predictedTrackCoords.size() > 1)
    {
        splitTrack(item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                        item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
    }
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

void ObjectMapModel::splitTrack(const QList<QGeoCoordinate *>& coords, const QVariantList& track,
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

    QQuickItem* map = m_gui->getMapItem();
    QVariant rectVariant;
    QMetaObject::invokeMethod(map, "mapRect", Q_RETURN_ARG(QVariant, rectVariant));
    QGeoRectangle rect = qvariant_cast<QGeoRectangle>(rectVariant);
    double bottomLeftLongitude = rect.bottomLeft().longitude();
    double bottomRightLongitude = rect.bottomRight().longitude();

    int width = round(rect.width());
    bool antimed = (width == 360) || (bottomLeftLongitude > bottomRightLongitude);

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
    (void) bottomRightLongitude;

    if (!std::isnan(bottomLeftLongitude))
    {
        for (int row = 0; row < m_items.size(); row++)
        {
            ObjectMapItem *item = (ObjectMapItem *)m_items[row];
            if (m_items[row]->m_itemSettings->m_enabled)
            {
                if (item->m_takenTrackCoords.size() > 1)
                {
                    splitTrack(item->m_takenTrackCoords, item->m_takenTrack, item->m_takenTrack1, item->m_takenTrack2,
                                    item->m_takenStart1, item->m_takenStart2, item->m_takenEnd1, item->m_takenEnd2);
                    QModelIndex idx = index(row);
                    QVector<int> roles = {groundTrack1Role, groundTrack2Role};
                    emit dataChanged(idx, idx, roles);
                }
                if (item->m_predictedTrackCoords.size() > 1)
                {
                    splitTrack(item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                                    item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
                    QModelIndex idx = index(row);
                    QVector<int> roles = {predictedGroundTrack1Role, predictedGroundTrack2Role};
                    emit dataChanged(idx, idx, roles);
                }
            }
        }
    }
}

bool ObjectMapFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    MapSettings::MapItemSettings *itemSettings = sourceModel()->data(index0, ObjectMapModel::itemSettingsRole).value<MapSettings::MapItemSettings *>();
    if (!itemSettings->m_enabled) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    int minZoom = sourceModel()->data(index0, ObjectMapModel::mapImageMinZoomRole).toInt();
    if (minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Don't show off-screen items, unless they have tracks, as these may be on screen
    // In the future, we could calculate a bounding box for tracks, if helpful for performance
    QGeoCoordinate coord = sourceModel()->data(index0, ObjectMapModel::positionRole).value<QGeoCoordinate>();
    float lat = coord.latitude();
    float lon = coord.longitude();
    bool onScreen = (lat >= m_bottomRightLatitude) && (lat <= m_topLeftLatitude) && (lon >= m_topLeftLongitude) && (lon <= m_bottomRightLongitude);
    if (!onScreen)
    {
        bool hasTracks = sourceModel()->data(index0, ObjectMapModel::hasTracksRole).toBool();
        if (!hasTracks) {
            return false;
        }
    }

    // Apply user filter
    if (!itemSettings->m_filterName.isEmpty())
    {
        QString name = sourceModel()->data(index0, ObjectMapModel::nameRole).toString();
        QRegularExpressionMatch match = itemSettings->m_filterNameRE.match(name);
        if (!match.hasMatch()) {
            return false;
        }
    }
    if (itemSettings->m_filterDistance > 0)
    {
        QGeoCoordinate position = sourceModel()->data(index0, ObjectMapModel::positionRole).value<QGeoCoordinate>();
        if (m_position.distanceTo(position) > itemSettings->m_filterDistance) {
            return false;
        }
    }

    return true;
}


void ObjectMapFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude))
    {
        m_topLeftLongitude = topLeftLongitude;
        m_topLeftLatitude = topLeftLatitude;
        m_bottomRightLongitude = bottomRightLongitude;
        m_bottomRightLatitude = bottomRightLatitude;
    }
    invalidateFilter();
}

Q_INVOKABLE int ObjectMapFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}


bool PolygonFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    MapSettings::MapItemSettings *itemSettings = sourceModel()->data(index0, PolygonMapModel::itemSettingsRole).value<MapSettings::MapItemSettings *>();
    if (!itemSettings->m_enabled) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    int minZoom = sourceModel()->data(index0, PolygonMapModel::mapImageMinZoomRole).toInt();
    if (minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items
    QGeoRectangle bounds = sourceModel()->data(index0, PolygonMapModel::boundsRole).value<QGeoRectangle>();
    if (!m_view.intersects(bounds)) {
        return false;
    }

    // Apply user filter
    if (!itemSettings->m_filterName.isEmpty())
    {
        QString name = sourceModel()->data(index0, PolygonMapModel::nameRole).toString();
        QRegularExpressionMatch match = itemSettings->m_filterNameRE.match(name);
        if (!match.hasMatch()) {
            return false;
        }
    }
    if (itemSettings->m_filterDistance > 0)
    {
        QGeoCoordinate position = sourceModel()->data(index0, PolygonMapModel::positionRole).value<QGeoCoordinate>();
        if (m_position.distanceTo(position) > itemSettings->m_filterDistance) {
            return false;
        }
    }

    return true;
}

void PolygonFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
}

Q_INVOKABLE int PolygonFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}

bool PolylineFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    MapSettings::MapItemSettings *itemSettings = sourceModel()->data(index0, PolylineMapModel::itemSettingsRole).value<MapSettings::MapItemSettings *>();
    if (!itemSettings->m_enabled) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    int minZoom = sourceModel()->data(index0, PolylineMapModel::mapImageMinZoomRole).toInt();
    if (minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items
    QGeoRectangle bounds = sourceModel()->data(index0, PolylineMapModel::boundsRole).value<QGeoRectangle>();
    if (!m_view.intersects(bounds)) {
        return false;
    }

    // Apply user filter
    if (!itemSettings->m_filterName.isEmpty())
    {
        QString name = sourceModel()->data(index0, PolylineMapModel::nameRole).toString();
        QRegularExpressionMatch match = itemSettings->m_filterNameRE.match(name);
        if (!match.hasMatch()) {
            return false;
        }
    }
    if (itemSettings->m_filterDistance > 0)
    {
        QGeoCoordinate position = sourceModel()->data(index0, PolylineMapModel::positionRole).value<QGeoCoordinate>();
        if (m_position.distanceTo(position) > itemSettings->m_filterDistance) {
            return false;
        }
    }

    return true;
}

void PolylineFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
}

Q_INVOKABLE int PolylineFilter::mapRowToSource(int row)
{
    QModelIndex sourceIndex = mapToSource(index(row, 0));
    return sourceIndex.row();
}

bool ImageFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);

    MapSettings::MapItemSettings *itemSettings = sourceModel()->data(index0, ImageMapModel::itemSettingsRole).value<MapSettings::MapItemSettings *>();
    if (!itemSettings->m_enabled || !itemSettings->m_display2DIcon) {
        return false;
    }

    // Don't show tiny items when we're zoomed out
    int minZoom = sourceModel()->data(index0, ImageMapModel::mapImageMinZoomRole).toInt();
    if (minZoom - 3 >= m_zoomLevel) {
        return false;
    }

    // Filter off-screen items
    QGeoRectangle bounds = sourceModel()->data(index0, ImageMapModel::boundsRole).value<QGeoRectangle>();
    if (!m_view.intersects(bounds)) {
        return false;
    }

    // Apply user filter
    if (!itemSettings->m_filterName.isEmpty())
    {
        QString name = sourceModel()->data(index0, ImageMapModel::nameRole).toString();
        QRegularExpressionMatch match = itemSettings->m_filterNameRE.match(name);
        if (!match.hasMatch()) {
            return false;
        }
    }
    if (itemSettings->m_filterDistance > 0)
    {
        QGeoCoordinate position = sourceModel()->data(index0, ImageMapModel::positionRole).value<QGeoCoordinate>();
        if (m_position.distanceTo(position) > itemSettings->m_filterDistance) {
            return false;
        }
    }

    return true;
}

void ImageFilter::viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel)
{
    m_zoomLevel = zoomLevel;
    if (!std::isnan(topLeftLongitude)) {
        m_view = QGeoRectangle(QGeoCoordinate(topLeftLatitude, topLeftLongitude), QGeoCoordinate(bottomRightLatitude, bottomRightLongitude));
    }
    invalidateFilter();
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

