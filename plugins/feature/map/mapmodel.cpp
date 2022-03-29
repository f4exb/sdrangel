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

MapItem::MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) :
    m_altitude(0.0)
{
    m_sourcePipe = sourcePipe;
    m_group = group;
    m_itemSettings = itemSettings;
    m_name = *mapItem->getName();
    update(mapItem);
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
    updateTrack(mapItem->getTrack());
    updatePredictedTrack(mapItem->getPredictedTrack());
}

QGeoCoordinate MapItem::getCoordinates()
{
    QGeoCoordinate coords;
    coords.setLatitude(m_latitude);
    coords.setLongitude(m_longitude);
    return coords;
}

void MapItem::findFrequency()
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

void MapItem::updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
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

void MapItem::updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
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

MapModel::MapModel(MapGUI *gui) :
    m_gui(gui),
    m_target(-1)
{
    connect(this, &MapModel::dataChanged, this, &MapModel::update3DMap);
}

Q_INVOKABLE void MapModel::add(MapItem *item)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_items.append(item);
    m_selected.append(false);
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
            splitTracks(item);
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
            item = new MapItem(sourcePipe, group, m_gui->getItemSettings(group), swgMapItem);
            add(item);
            // Add to 3D Map (we don't appear to get a dataChanged signal when adding)
            CesiumInterface *cesium = m_gui->cesium();
            if (cesium) {
                cesium->update(item, isTarget(item), isSelected3D(item));
            }
            playAnimations(item);
        }
    }
}

// Slot called on dataChanged signal, to update 3D map
void MapModel::update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    (void) roles;

    CesiumInterface *cesium = m_gui->cesium();
    if (cesium)
    {
        for (int row = topLeft.row(); row <= bottomRight.row(); row++)
        {
            cesium->update(m_items[row], isTarget(m_items[row]), isSelected3D(m_items[row]));
            playAnimations(m_items[row]);
        }
    }
}

void MapModel::playAnimations(MapItem *item)
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

void MapModel::update(MapItem *item)
{
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
        QModelIndex idx = index(row);
        emit dataChanged(idx, idx);
        if (row == m_target) {
            updateTarget();
        }
    }
}

void MapModel::remove(MapItem *item)
{
    int row = m_items.indexOf(item);
    if (row >= 0)
    {
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        m_selected.removeAt(row);
        if (row == m_target) {
            m_target = -1;
        } else if (row < m_target) {
            m_target--;
        }
        endRemoveRows();
    }
}

void MapModel::allUpdated()
{
    for (int i = 0; i < m_items.count(); i++)
    {
        // Updates both 2D and 3D Map
        QModelIndex idx = index(i);
        emit dataChanged(idx, idx);
    }
}

void MapModel::removeAll()
{
    if (m_items.count() > 0)
    {
        beginRemoveRows(QModelIndex(), 0, m_items.count());
        m_items.clear();
        m_selected.clear();
        endRemoveRows();
    }
}

// After new settings are deserialised, we need to update
// pointers to item settings for all existing items
void MapModel::updateItemSettings(QHash<QString, MapSettings::MapItemSettings *> m_itemSettings)
{
    for (auto item : m_items) {
        item->m_itemSettings = m_itemSettings[item->m_group];
    }
}

void MapModel::updateTarget()
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

void MapModel::setTarget(const QString& name)
{
    if (name.isEmpty())
    {
        QModelIndex idx = index(-1);
        setData(idx, QVariant(-1), MapModel::targetRole);
    }
    else
    {
        QModelIndex idx = findMapItemIndex(name);
        setData(idx, QVariant(idx.row()), MapModel::targetRole);
    }
}

bool MapModel::isTarget(const MapItem *mapItem) const
{
    if (m_target < 0) {
        return false;
    } else {
        return m_items[m_target] == mapItem;
    }
}

// FIXME: This should use Z order - rather than adding/removing
// but I couldn't quite get it to work
Q_INVOKABLE void MapModel::moveToFront(int oldRow)
{
    // Last item in list is drawn on top, so remove than add to end of list
    if (oldRow < m_items.size() - 1)
    {
        bool wasTarget = m_target == oldRow;
        MapItem *item = m_items[oldRow];
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

Q_INVOKABLE void MapModel::moveToBack(int oldRow)
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

MapItem *MapModel::findMapItem(const QObject *source, const QString& name)
{
    // FIXME: Should consider adding a QHash for this
    QListIterator<MapItem *> i(m_items);
    while (i.hasNext())
    {
        MapItem *item = i.next();
        if ((item->m_name == name) && (item->m_sourcePipe == source))
            return item;
    }
    return nullptr;
}

MapItem *MapModel::findMapItem(const QString& name)
{
    QListIterator<MapItem *> i(m_items);
    while (i.hasNext())
    {
        MapItem *item = i.next();
        if (item->m_name == name)
            return item;
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

int MapModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_items.count();
}

QVariant MapModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();

    if ((row < 0) || (row >= m_items.count())) {
        return QVariant();
    }
    if (role == MapModel::positionRole)
    {
        // Coordinates to display the item at
        QGeoCoordinate coords;
        coords.setLatitude(m_items[row]->m_latitude);
        coords.setLongitude(m_items[row]->m_longitude);
        return QVariant::fromValue(coords);
    }
    else if (role == MapModel::mapTextRole)
    {
        // Create the text to go in the bubble next to the image
        QString name = m_items[row]->m_label.isEmpty() ? m_items[row]->m_name : m_items[row]->m_label;
        if (row == m_target)
        {
            AzEl *azEl = m_gui->getAzEl();
            QString text = QString("%1<br>Az: %2%5 El: %3%5 Dist: %4 km")
                                .arg(m_selected[row] ? m_items[row]->m_text : name)
                                .arg(std::round(azEl->getAzimuth()))
                                .arg(std::round(azEl->getElevation()))
                                .arg(std::round(azEl->getDistance() / 1000.0))
                                .arg(QChar(0xb0));
            return QVariant::fromValue(text);
        }
        else if (m_selected[row])
        {
            return QVariant::fromValue(m_items[row]->m_text);
        }
        else
        {
            return QVariant::fromValue(name);
        }
    }
    else if (role == MapModel::mapTextVisibleRole)
    {
        return QVariant::fromValue((m_selected[row] || m_displayNames) && m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DLabel);
    }
    else if (role == MapModel::mapImageVisibleRole)
    {
        return QVariant::fromValue(m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DIcon);
    }
    else if (role == MapModel::mapImageRole)
    {
        // Set an image to use
        return QVariant::fromValue(m_items[row]->m_image);
    }
    else if (role == MapModel::mapImageRotationRole)
    {
        // Angle to rotate image by
        return QVariant::fromValue(m_items[row]->m_imageRotation);
    }
    else if (role == MapModel::mapImageMinZoomRole)
    {
        // Minimum zoom level
        //return QVariant::fromValue(m_items[row]->m_imageMinZoom);
        return QVariant::fromValue(m_items[row]->m_itemSettings->m_2DMinZoom);
    }
    else if (role == MapModel::bubbleColourRole)
    {
        // Select a background colour for the text bubble next to the item
        if (m_selected[row]) {
            return QVariant::fromValue(QColor("lightgreen"));
        } else {
            return QVariant::fromValue(QColor("lightblue"));
        }
    }
    else if (role == MapModel::selectedRole)
    {
        return QVariant::fromValue(m_selected[row]);
    }
    else if (role == MapModel::targetRole)
    {
        return QVariant::fromValue(m_target == row);
    }
    else if (role == MapModel::frequencyRole)
    {
        return QVariant::fromValue(m_items[row]->m_frequency);
    }
    else if (role == MapModel::frequencyStringRole)
    {
        return QVariant::fromValue(m_items[row]->m_frequencyString);
    }
    else if (role == MapModel::predictedGroundTrack1Role)
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DTrack) {
            return m_items[row]->m_predictedTrack1;
        } else {
            return QVariantList();
        }
    }
    else if (role == MapModel::predictedGroundTrack2Role)
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DTrack) {
            return m_items[row]->m_predictedTrack2;
        } else {
            return QVariantList();
        }
    }
    else if (role == MapModel::groundTrack1Role)
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DTrack) {
            return m_items[row]->m_takenTrack1;
        } else {
            return QVariantList();
        }
    }
    else if (role == MapModel::groundTrack2Role)
    {
        if (   (m_displayAllGroundTracks || (m_displaySelectedGroundTracks && m_selected[row]))
            && m_items[row]->m_itemSettings->m_enabled && m_items[row]->m_itemSettings->m_display2DTrack) {
            return m_items[row]->m_takenTrack2;
        } else {
            return QVariantList();
        }
    }
    else if (role == groundTrackColorRole)
    {
        return QVariant::fromValue(QColor::fromRgb(m_items[row]->m_itemSettings->m_2DTrackColor));
    }
    else if (role == predictedGroundTrackColorRole)
    {
        return QVariant::fromValue(QColor::fromRgb(m_items[row]->m_itemSettings->m_2DTrackColor).lighter());
    }
    return QVariant();
}

bool MapModel::setData(const QModelIndex &idx, const QVariant& value, int role)
{
    int row = idx.row();
    if ((row < 0) || (row >= m_items.count()))
        return false;
    if (role == MapModel::selectedRole)
    {
        m_selected[row] = value.toBool();
        emit dataChanged(idx, idx);
        return true;
    }
    else if (role == MapModel::targetRole)
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

Qt::ItemFlags MapModel::flags(const QModelIndex &index) const
{
    (void) index;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void MapModel::setDisplayNames(bool displayNames)
{
    m_displayNames = displayNames;
    allUpdated();
}

void MapModel::setDisplaySelectedGroundTracks(bool displayGroundTracks)
{
    m_displaySelectedGroundTracks = displayGroundTracks;
    allUpdated();
}

void MapModel::setDisplayAllGroundTracks(bool displayGroundTracks)
{
    m_displayAllGroundTracks = displayGroundTracks;
    allUpdated();
}

void MapModel::setFrequency(double frequency)
{
    // Set as centre frequency
    ChannelWebAPIUtils::setCenterFrequency(0, frequency);
}

void MapModel::track3D(int index)
{
    if (index < m_items.count())
    {
        MapItem *item = m_items[index];
        m_gui->track3D(item->m_name);
    }
}

void MapModel::splitTracks(MapItem *item)
{
    if (item->m_takenTrackCoords.size() > 1)
        splitTrack(item->m_takenTrackCoords, item->m_takenTrack, item->m_takenTrack1, item->m_takenTrack2,
                        item->m_takenStart1, item->m_takenStart2, item->m_takenEnd1, item->m_takenEnd2);
    if (item->m_predictedTrackCoords.size() > 1)
        splitTrack(item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                        item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
}

void MapModel::interpolateEast(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen)
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

void MapModel::interpolateWest(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen)
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

void MapModel::interpolate(QGeoCoordinate *c1, QGeoCoordinate *c2, double bottomLeftLongitude, double bottomRightLongitude, QGeoCoordinate* ci, bool offScreen)
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

void MapModel::splitTrack(const QList<QGeoCoordinate *>& coords, const QVariantList& track,
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

void MapModel::viewChanged(double bottomLeftLongitude, double bottomRightLongitude)
{
    (void) bottomRightLongitude;
    if (!std::isnan(bottomLeftLongitude))
    {
        for (int row = 0; row < m_items.size(); row++)
        {
            MapItem *item = m_items[row];
            if (item->m_takenTrackCoords.size() > 1)
            {
                splitTrack(item->m_takenTrackCoords, item->m_takenTrack, item->m_takenTrack1, item->m_takenTrack2,
                                item->m_takenStart1, item->m_takenStart2, item->m_takenEnd1, item->m_takenEnd2);
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx);
            }
            if (item->m_predictedTrackCoords.size() > 1)
            {
                splitTrack(item->m_predictedTrackCoords, item->m_predictedTrack, item->m_predictedTrack1, item->m_predictedTrack2,
                                item->m_predictedStart1, item->m_predictedStart2, item->m_predictedEnd1, item->m_predictedEnd2);
                QModelIndex idx = index(row);
                emit dataChanged(idx, idx);
            }
        }
    }
}

