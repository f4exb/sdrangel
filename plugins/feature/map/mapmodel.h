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

#ifndef INCLUDE_FEATURE_MAPMODEL_H_
#define INCLUDE_FEATURE_MAPMODEL_H_

#include <QAbstractListModel>
#include <QQmlEngine>
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
    }

    ~MapModel() override
    {
        qDeleteAll(m_items);
    }

    virtual void add(MapItem *item);
    virtual void remove(MapItem *item);
    virtual void removeAll();

    // Map items arrive far faster than the map can usefully be redrawn - an aircraft
    // sends one per decoded frame - so changes are collected here and handed to the map
    // in batches, rather than a signal per item per message
    void markDirty(MapItem *item, quint32 changed);
    virtual void flush();
    bool hasDirtyItems() const { return !m_dirty.isEmpty(); }

    // Roles the map should re-read for a given set of changes. Empty means all of them
    virtual QVector<int> rolesFor(quint32 changed) const;

    // Everything the filters need in order to decide whether to show an item, worked out
    // when the item changes rather than every time the map is panned. The whole model is
    // re-filtered on a view change, and it holds every airport and navaid in the world,
    // so this is on the path that decides how smoothly the map pans
    enum FilterFlags {
        FilterEnabled     = 1u << 0,
        FilterDisplayIcon = 1u << 1,   // Item settings say to draw the image
        FilterNameMatches = 1u << 2,   // Passes the user's name filter
        FilterDistanceOk  = 1u << 3,   // Within the user's distance filter of the station
        FilterHasTracks   = 1u << 4    // Keep it even when off screen, as its track may be on screen
    };

    struct FilterData {
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        float m_boundsTop = 0.0f;      // Only used by the items that have an extent
        float m_boundsBottom = 0.0f;
        float m_boundsLeft = 0.0f;
        float m_boundsRight = 0.0f;
        qint32 m_minZoom = 0;
        quint32 m_flags = 0;
    };

    const FilterData& filterData(int row) const { return m_filterData.at(row); }
    void setStationPosition(const QGeoCoordinate& position);
    void rebuildFilterData();
    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group="");
    void updateItemSettings(QHash<QString, MapSettings::MapItemSettings *> m_itemSettings);
    virtual void allUpdated();

    MapItem *findMapItem(const QObject *source, const QString& name);
    MapItem *findMapItem(const QString& name);
    QModelIndex findMapItemIndex(const QString& name);

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

protected:

    MapGUI *m_gui;
    QList<MapItem *> m_items;
    QHash<QString, MapItem *> m_itemsHash;
    QHash<MapItem *, quint32> m_dirty;              // Items changed since the last flush
    QVector<FilterData> m_filterData;               // Parallel to m_items
    QGeoCoordinate m_stationPosition;
    mutable QHash<quint32, QVector<int>> m_roleCache;

    virtual void fillFilterData(MapItem *item, FilterData& data) const;
    void updateFilterData(int row);

    virtual void update(MapItem *item);
    virtual void update3D(MapItem *item) = 0;
    virtual MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) = 0;

};

// A MapPolyline can only be drawn in a single colour, so a track that is coloured
// according to altitude has to be drawn as a series of them. This is the model of those
// segments for one item, used by a MapItemView nested inside the item's delegate.
class TrackColorModel : public QAbstractListModel {
    Q_OBJECT

public:

    TrackColorModel()
    {
        // This model is handed to QML from ObjectMapModel::data() and has no parent, and
        // QML takes ownership of any such QObject it is given: its garbage collector can
        // then delete the model while the map item still holds the pointer. That crashed
        // in update(), emitting dataChanged on freed memory, and raced the delete in
        // ~ObjectMapItem to a double free. The item owns it, so say so.
        QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    }

    enum SegmentRoles {
        segmentCoordinatesRole = Qt::UserRole + 1,
        segmentColorRole = Qt::UserRole + 2
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        (void) parent;
        return m_segments.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override
    {
        QHash<int, QByteArray> roles;
        roles[segmentCoordinatesRole] = "segmentCoordinates";
        roles[segmentColorRole] = "segmentColor";
        return roles;
    }

    // firstChanged is the first segment that differs from what the model already has,
    // so that appending to a track doesn't make the map redraw every segment of it
    void update(const QList<QVariantList>& segments, const QList<QVariant>& colors, int firstChanged = 0);
    void clear();

private:
    QList<QVariantList> m_segments;
    QList<QVariant> m_colors;
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

    void fillFilterData(MapItem *item, FilterData& data) const override;

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

    void fillFilterData(MapItem *item, FilterData& data) const override;

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
        trackModelRole = MapModel::lastRole + 3,
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
        roles[trackModelRole] = "polylineTrackModel";
        return roles;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void fillFilterData(MapItem *item, FilterData& data) const override;
    void flush() override;
    void allUpdated() override;

protected:
    void update3D(MapItem *item) override;
    MapItem *newMapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem) override;

private:
    void updateTrackSegments(PolylineMapItem *item);

    mutable TrackColorModel m_noTrackColors;  // Empty, for lines that aren't coloured by altitude
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
        frequenciesRole = MapModel::lastRole + 7,
        frequencyStringsRole = MapModel::lastRole + 8,
        predictedGroundTrack1Role = MapModel::lastRole + 9,
        predictedGroundTrack2Role = MapModel::lastRole + 10,
        groundTrack1Role = MapModel::lastRole + 11,
        groundTrack2Role = MapModel::lastRole + 12,
        groundTrackColorRole = MapModel::lastRole + 13,
        predictedGroundTrackColorRole = MapModel::lastRole + 14,
        hasTracksRole = MapModel::lastRole + 15,
        groundTrackModelRole = MapModel::lastRole + 16,
        zOrderRole = MapModel::lastRole + 17
    };

    ObjectMapModel(MapGUI *gui);

    Q_INVOKABLE void add(MapItem *item) override;
    //void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group="");
    using MapModel::update;
    void remove(MapItem *item) override;
    void removeAll() override;
    QHash<int, QByteArray> roleNames() const override;

    void allUpdated() override;
    void flush() override;
    void fillFilterData(MapItem *item, FilterData& data) const override;
    QVector<int> rolesFor(quint32 changed) const override;

    void updateTarget();
    void setTarget(const QString& name);
    bool isTarget(const ObjectMapItem *mapItem) const;

    Q_INVOKABLE void moveToFront(int row);
    Q_INVOKABLE void moveToBack(int row);

    Q_INVOKABLE void link(const QString& link);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;

    void setDisplayNames(bool displayNames);
    void setDisplaySelectedGroundTracks(bool displayGroundTracks);
    void setDisplayAllGroundTracks(bool displayGroundTracks);
    Q_INVOKABLE void track3D(int index);
    Q_INVOKABLE QStringList getDeviceSets() const;
    Q_INVOKABLE void setFrequency(qint64 frequency, const QString& deviceSet);

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

signals:
    void linkClicked(const QString& url);

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
    mutable TrackColorModel m_noTrackColors;  // Empty, for items whose track isn't coloured by altitude
    QList<ObjectMapItem *> m_trackedItems;  // Items with a track, so a pan doesn't walk every airport in the world
    QList<bool> m_selected;     // Whether each item on 2D map is selected
    QString m_selected3D;       // Name of item selected on 3D map - only supports 1 item, unlike 2D map
    int m_target;               // Row number of current target, or -1 for none
    qreal m_frontZOrder;        // Draw order handed out by moveToFront
    qreal m_backZOrder;         //   and by moveToBack
    bool m_displayNames;
    bool m_displaySelectedGroundTracks;
    bool m_displayAllGroundTracks;

    double m_bottomLeftLongitude;
    double m_bottomRightLongitude;

    void interpolateEast(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolateWest(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolate(QGeoCoordinate *c1, QGeoCoordinate *c2, double bottomLeftLongitude, double bottomRightLongitude, QGeoCoordinate* ci, bool offScreen);

    // What's currently visible on the 2D map, which tracks are split against
    struct ViewLongitudes {
        double m_bottomLeft;
        double m_bottomRight;
        int m_width;           // Degrees of longitude visible
        bool m_antimeridian;   // Whether the antimeridian is on screen
    };

    ViewLongitudes viewLongitudes() const;
    static bool wholeTrackOnScreen(const ViewLongitudes& view, double minLongitude, double maxLongitude);

    void splitTracks(ObjectMapItem *item);
    void splitTakenTrack(ObjectMapItem *item, const ViewLongitudes& view);
    void updateTrackSegments(ObjectMapItem *item);
    static void colorTrackPoints(ObjectMapItem *item, const QVariantList& track, int first, const QVariant *palette);
    void splitTrack(const ViewLongitudes& view,
                        const QList<QGeoCoordinate *>& coords, const QVariantList& track,
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

protected:
    // Qt 6.10 brought these, deprecating invalidateFilter() in their favour. On anything
    // older they stand in for it, as in AcarsMessageFilter
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    void beginFilterChange() {}
    void endFilterChange() { invalidateFilter(); }
#endif

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

protected:
    // As in PolygonFilter
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    void beginFilterChange() {}
    void endFilterChange() { invalidateFilter(); }
#endif

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

protected:
    // As in PolygonFilter
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    void beginFilterChange() {}
    void endFilterChange() { invalidateFilter(); }
#endif

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

protected:
    // As in PolygonFilter
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
    void beginFilterChange() {}
    void endFilterChange() { invalidateFilter(); }
#endif

private:
    QGeoRectangle m_view;
    double m_zoomLevel;
    MapSettings *m_settings;
    QGeoCoordinate m_position;;

};

#endif // INCLUDE_FEATURE_MAPMODEL_H_

