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

#ifndef INCLUDE_FEATURE_MAPMODEL_H_
#define INCLUDE_FEATURE_MAPMODEL_H_

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QGeoCoordinate>
#include <QGeoRectangle>
#include <QColor>

#include "util/azel.h"
#include "util/openaip.h"
#include "mapsettings.h"
#include "mapitem.h"
#include "cesiuminterface.h"

#include "SWGMapItem.h"

class MapGUI;
class CZML;

class MapModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum MarkerRoles {
        itemSettingsRole = Qt::UserRole + 1,
        nameRole =  Qt::UserRole + 2,
        labelRole = Qt::UserRole + 3,
        positionRole = Qt::UserRole + 4,
        mapImageMinZoomRole = Qt::UserRole + 5,
        lastRole = Qt::UserRole + 6
    };

    MapModel(MapGUI *gui) :
        m_gui(gui)
    {
        connect(this, &MapModel::dataChanged, this, &MapModel::update3DMap);
    }

    virtual void add(MapItem *item);
    virtual void remove(MapItem *item);
    virtual void removeAll();
    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group="");
    void updateItemSettings(QHash<QString, MapSettings::MapItemSettings *> m_itemSettings);
    void allUpdated();

    MapItem *findMapItem(const QObject *source, const QString& name);

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        (void) parent;
        return m_items.count();
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        (void) index;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    }

public slots:
    void update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());

protected:

    MapGUI *m_gui;
    QList<MapItem *> m_items;
    QHash<QString, MapItem *> m_itemsHash;

    virtual void update(MapItem *item);
    virtual void update3D(MapItem *item) = 0;
    virtual MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) = 0;

};

class ImageMapModel : public MapModel {
    Q_OBJECT

public:
    enum MarkerRoles {
        imageRole = MapModel::lastRole + 0,
        imageZoomLevelRole = MapModel::lastRole + 1,
        boundsRole = MapModel::lastRole + 2
    };

    ImageMapModel(MapGUI *gui) :
        MapModel(gui)
    {}

    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> roles = MapModel::roleNames();
        roles[imageRole] = "imageData";
        roles[imageZoomLevelRole] = "imageZoomLevel";
        roles[boundsRole] = "bounds";
        return roles;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) override;
    void update3D(MapItem *item) override;

};

class PolygonMapModel : public MapModel {
    Q_OBJECT

public:
    enum MarkerRoles {
        borderColorRole = MapModel::lastRole + 0,
        fillColorRole = MapModel::lastRole + 1,
        polygonRole = MapModel::lastRole + 2,
        boundsRole = MapModel::lastRole + 3
    };

    PolygonMapModel(MapGUI *gui) :
        MapModel(gui)
    {}

    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> roles = MapModel::roleNames();
        roles[borderColorRole] = "borderColor";
        roles[fillColorRole] = "fillColor";
        roles[polygonRole] = "polygon";
        roles[boundsRole] = "bounds";
        return roles;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    void update3D(MapItem *item) override;
    MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) override;

};

class PolylineMapModel : public MapModel {
    Q_OBJECT

public:

    enum MarkerRoles {
        lineColorRole = MapModel::lastRole + 0,
        coordinatesRole = MapModel::lastRole + 1,
        boundsRole =  MapModel::lastRole + 2,
    };

    PolylineMapModel(MapGUI *gui) :
        MapModel(gui)
    {}

    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> roles = MapModel::roleNames();
        roles[lineColorRole] = "lineColor";
        roles[coordinatesRole] = "coordinates";
        roles[boundsRole] = "bounds";
        return roles;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    void update3D(MapItem *item) override;
    MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) override;
};

// Model used for each item on the map
class ObjectMapModel : public MapModel {
    Q_OBJECT

public:

    enum MarkerRoles {
        //itemSettingsRole = Qt::UserRole + 1,
        //nameRole = Qt::UserRole + 2,
        //positionRole = Qt::UserRole + 3,
        // label? mapTextRole = Qt::UserRole + 4,
        mapTextVisibleRole =MapModel::lastRole + 0,
        mapImageVisibleRole = MapModel::lastRole + 1,
        mapImageRole = MapModel::lastRole + 2,
        mapImageRotationRole = MapModel::lastRole + 3,
        //mapImageMinZoomRole = MapModel::lastRole + 9,
        bubbleColourRole = MapModel::lastRole + 4,
        selectedRole = MapModel::lastRole + 5,
        targetRole = MapModel::lastRole + 6,
        frequencyRole = MapModel::lastRole + 7,
        frequencyStringRole = MapModel::lastRole + 8,
        predictedGroundTrack1Role = MapModel::lastRole + 9,
        predictedGroundTrack2Role = MapModel::lastRole + 10,
        groundTrack1Role = MapModel::lastRole + 11,
        groundTrack2Role = MapModel::lastRole + 12,
        groundTrackColorRole = MapModel::lastRole + 13,
        predictedGroundTrackColorRole = MapModel::lastRole + 14,
        hasTracksRole = MapModel::lastRole + 15
    };

    ObjectMapModel(MapGUI *gui);

    Q_INVOKABLE void add(MapItem *item) override;
    //void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group="");
    using MapModel::update;
    void remove(MapItem *item) override;
    void removeAll() override;
    QHash<int, QByteArray> roleNames() const override;

    void updateTarget();
    void setTarget(const QString& name);
    bool isTarget(const ObjectMapItem *mapItem) const;

    Q_INVOKABLE void moveToFront(int oldRow);
    Q_INVOKABLE void moveToBack(int oldRow);

    ObjectMapItem *findMapItem(const QString& name);
    QModelIndex findMapItemIndex(const QString& name);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    void setDisplayNames(bool displayNames);
    void setDisplaySelectedGroundTracks(bool displayGroundTracks);
    void setDisplayAllGroundTracks(bool displayGroundTracks);
    Q_INVOKABLE void setFrequency(double frequency);
    Q_INVOKABLE void track3D(int index);

    Q_INVOKABLE void viewChanged(double bottomLeftLongitude, double bottomRightLongitude);

    bool isSelected3D(const ObjectMapItem *item) const
    {
        return m_selected3D == item->m_name;
    }

    void setSelected3D(const QString &selected)
    {
        m_selected3D = selected;
    }

//public slots:
//    void update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>()) override;

protected:
    void playAnimations(ObjectMapItem *item);
    MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) override;
    void update(MapItem *item) override;
    void update3D(MapItem *item) override;

    // Linear interpolation
    double interpolate(double x0, double y0, double x1, double y1, double x) const
    {
        return (y0*(x1-x) + y1*(x-x0)) / (x1-x0);
    }

private:
    QList<bool> m_selected;     // Whether each item on 2D map is selected
    QString m_selected3D;       // Name of item selected on 3D map - only supports 1 item, unlike 2D map
    int m_target;               // Row number of current target, or -1 for none
    bool m_displayNames;
    bool m_displaySelectedGroundTracks;
    bool m_displayAllGroundTracks;

    double m_bottomLeftLongitude;
    double m_bottomRightLongitude;

    void interpolateEast(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolateWest(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolate(QGeoCoordinate *c1, QGeoCoordinate *c2, double bottomLeftLongitude, double bottomRightLongitude, QGeoCoordinate* ci, bool offScreen);

    void splitTracks(ObjectMapItem *item);
    void splitTrack(const QList<QGeoCoordinate *>& coords, const QVariantList& track,
                        QVariantList& track1, QVariantList& track2,
                        QGeoCoordinate& start1, QGeoCoordinate& start2,
                        QGeoCoordinate& end1, QGeoCoordinate& end2);
};

class PolygonFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    PolygonFilter() :
        m_zoomLevel(10),
        m_settings(nullptr)
    {
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    Q_INVOKABLE void viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel);
    Q_INVOKABLE int mapRowToSource(int row);
    void setPosition(const QGeoCoordinate& position);

private:
    QGeoRectangle m_view;
    double m_zoomLevel;
    MapSettings *m_settings;
    QGeoCoordinate m_position;;

};

class PolylineFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    PolylineFilter() :
        m_zoomLevel(10),
        m_settings(nullptr)
    {
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    Q_INVOKABLE void viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel);
    Q_INVOKABLE int mapRowToSource(int row);
    void setPosition(const QGeoCoordinate& position);

private:
    QGeoRectangle m_view;
    double m_zoomLevel;
    MapSettings *m_settings;
    QGeoCoordinate m_position;;

};

class ObjectMapFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    ObjectMapFilter() :
        m_topLeftLongitude(0),
        m_topLeftLatitude(0),
        m_bottomRightLongitude(0),
        m_bottomRightLatitude(0),
        m_zoomLevel(10),
        m_settings(nullptr)
    {
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    Q_INVOKABLE void viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel);
    void applySettings(MapSettings *settings);
    Q_INVOKABLE int mapRowToSource(int row);
    void setPosition(const QGeoCoordinate& position);

private:
    double m_topLeftLongitude;
    double m_topLeftLatitude;
    double m_bottomRightLongitude;
    double m_bottomRightLatitude;
    double m_zoomLevel;
    MapSettings *m_settings;
    QGeoCoordinate m_position;

};

class ImageFilter : public QSortFilterProxyModel {
    Q_OBJECT
public:
    ImageFilter() :
        m_zoomLevel(10),
        m_settings(nullptr)
    {
    }

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
    Q_INVOKABLE void viewChanged(double topLeftLongitude, double topLeftLatitude, double bottomRightLongitude, double bottomRightLatitude, double zoomLevel);
    Q_INVOKABLE int mapRowToSource(int row);
    void setPosition(const QGeoCoordinate& position);

private:
    QGeoRectangle m_view;
    double m_zoomLevel;
    MapSettings *m_settings;
    QGeoCoordinate m_position;;

};

#endif // INCLUDE_FEATURE_MAPMODEL_H_

