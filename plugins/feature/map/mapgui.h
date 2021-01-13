///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_MAPGUI_H_
#define INCLUDE_FEATURE_MAPGUI_H_

#include <QTimer>
#include <QAbstractListModel>
#include <QGeoCoordinate>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "pipes/pipeendpoint.h"
#include "mapsettings.h"
#include "SWGMapItem.h"

class PluginAPI;
class FeatureUISet;
class Map;

namespace Ui {
    class MapGUI;
}

class MapGUI;
class MapModel;

// Information required about each item displayed on the map
class MapItem {

public:
    MapItem(const PipeEndPoint *source, SWGSDRangel::SWGMapItem *mapItem)
    {
        m_source = source;
        m_name = *mapItem->getName();
        m_latitude = mapItem->getLatitude();
        m_longitude = mapItem->getLongitude();
        m_image = *mapItem->getImage();
        m_imageRotation = mapItem->getImageRotation();
        m_imageFixedSize = mapItem->getImageFixedSize() == 1;
        m_text = *mapItem->getText();
    }

    void update(SWGSDRangel::SWGMapItem *mapItem)
    {
        m_latitude = mapItem->getLatitude();
        m_longitude = mapItem->getLongitude();
        m_image = *mapItem->getImage();
        m_imageRotation = mapItem->getImageRotation();
        m_imageFixedSize = mapItem->getImageFixedSize() == 1;
        m_text = *mapItem->getText();
    }

    QGeoCoordinate getCoordinates()
    {
        QGeoCoordinate coords;
        coords.setLatitude(m_latitude);
        coords.setLongitude(m_longitude);
        return coords;
    }

private:
    friend MapModel;
    const PipeEndPoint *m_source;       // Channel/feature that created the item
    QString m_name;
    float m_latitude;
    float m_longitude;
    QString m_image;
    int m_imageRotation;
    bool m_imageFixedSize;              // Keep image same size when map is zoomed
    QString m_text;
};

// Model used for each item on the map
class MapModel : public QAbstractListModel {
    Q_OBJECT

public:
    using QAbstractListModel::QAbstractListModel;
    enum MarkerRoles {
        positionRole = Qt::UserRole + 1,
        mapTextRole = Qt::UserRole + 2,
        mapTextVisibleRole = Qt::UserRole + 3,
        mapImageRole = Qt::UserRole + 4,
        mapImageRotationRole = Qt::UserRole + 5,
        mapImageFixedSizeRole = Qt::UserRole + 6,
        bubbleColourRole = Qt::UserRole + 7,
        selectedRole = Qt::UserRole + 8
    };

    MapModel(MapGUI *gui) :
        m_gui(gui)
    {
    }

    Q_INVOKABLE void add(MapItem *item)
    {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_items.append(item);
        m_selected.append(false);
        endInsertRows();
    }

    void update(const PipeEndPoint *source, SWGSDRangel::SWGMapItem *swgMapItem)
    {
        QString name = *swgMapItem->getName();
        // Add, update or delete and item
        MapItem *item = findMapItem(source, name);
        if (item != nullptr)
        {
            QString image = *swgMapItem->getImage();
            if (image.isEmpty())
            {
                // Delete the item
                remove(item);
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
                add(new MapItem(source, swgMapItem));
            }
        }
    }

    void update(MapItem *item)
    {
        int row = m_items.indexOf(item);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
        }
    }

    void remove(MapItem *item)
    {
        int row = m_items.indexOf(item);
        if (row >= 0)
        {
            beginRemoveRows(QModelIndex(), row, row);
            m_items.removeAt(row);
            m_selected.removeAt(row);
            endRemoveRows();
        }
     }

    MapItem *findMapItem(const PipeEndPoint *source, const QString& name)
    {
        // FIXME: Should consider adding a QHash for this
        QListIterator<MapItem *> i(m_items);
        while (i.hasNext())
        {
            MapItem *item = i.next();
            if ((item->m_name == name) && (item->m_source == source))
                return item;
        }
        return nullptr;
    }

    MapItem *findMapItem(const QString& name)
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

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return m_items.count();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

    void allUpdated()
    {
        for (int i = 0; i < m_items.count(); i++)
        {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx);
        }
    }

    void removeAll()
    {
        beginRemoveRows(QModelIndex(), 0, m_items.count());
        m_items.clear();
        m_selected.clear();
        endRemoveRows();
    }

    void setDisplayNames(bool displayNames)
    {
        m_displayNames = displayNames;
        allUpdated();
    }

    QHash<int, QByteArray> roleNames() const
    {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[mapTextRole] = "mapText";
        roles[mapTextVisibleRole] = "mapTextVisible";
        roles[mapImageRole] = "mapImage";
        roles[mapImageRotationRole] = "mapImageRotation";
        roles[mapImageFixedSizeRole] = "mapImageFixedSize";
        roles[bubbleColourRole] = "bubbleColour";
        roles[selectedRole] = "selected";
        return roles;
    }

private:
    MapGUI *m_gui;
    QList<MapItem *> m_items;
    QList<bool> m_selected;
    bool m_displayNames;
};

class MapGUI : public FeatureGUI {
    Q_OBJECT
public:
    static MapGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::MapGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    MapSettings m_settings;
    bool m_doApplySettings;
    QList<PipeEndPoint::AvailablePipeSource> m_availablePipes;

    Map* m_map;
    MessageQueue m_inputMessageQueue;
    MapModel m_mapModel;

    explicit MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~MapGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updatePipeList();
    bool handleMessage(const Message& message);
    void find(const QString& target);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_displayNames_clicked(bool checked=false);
    void on_find_returnPressed();
    void on_deleteAll_clicked();
};


#endif // INCLUDE_FEATURE_MAPGUI_H_
