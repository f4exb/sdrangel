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
#include <QGeoCoordinate>
#include <QColor>

#include "util/azel.h"
#include "mapsettings.h"
#include "cesiuminterface.h"

#include "SWGMapItem.h"

class MapModel;
class MapGUI;
class CZML;

// Information required about each item displayed on the map
class MapItem {

public:
    MapItem(const QObject *sourcePipe, const QString &group, MapSettings::MapItemSettings *itemSettings, SWGSDRangel::SWGMapItem *mapItem);
    void update(SWGSDRangel::SWGMapItem *mapItem);
    QGeoCoordinate getCoordinates();

private:
    void findFrequency();
    void updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track);
    void updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track);

    friend MapModel;
    friend CZML;
    QString m_group;
    MapSettings::MapItemSettings *m_itemSettings;
    const QObject *m_sourcePipe;   // Channel/feature that created the item
    QString m_name;
    QString m_label;
    float m_latitude;
    float m_longitude;
    float m_altitude;                   // In metres
    QDateTime m_positionDateTime;
    bool m_useHeadingPitchRoll;
    float m_heading;
    float m_pitch;
    float m_roll;
    QDateTime m_orientationDateTime;
    QString m_image;
    int m_imageRotation;
    QString m_text;
    double m_frequency;                 // Frequency to set
    QString m_frequencyString;
    QList<QGeoCoordinate *> m_predictedTrackCoords;
    QList<QDateTime *> m_predictedTrackDateTimes;
    QVariantList m_predictedTrack;      // Line showing where the object is going
    QVariantList m_predictedTrack1;
    QVariantList m_predictedTrack2;
    QGeoCoordinate m_predictedStart1;
    QGeoCoordinate m_predictedStart2;
    QGeoCoordinate m_predictedEnd1;
    QGeoCoordinate m_predictedEnd2;
    QList<QGeoCoordinate *> m_takenTrackCoords;
    QList<QDateTime *> m_takenTrackDateTimes;
    QVariantList m_takenTrack;          // Line showing where the object has been
    QVariantList m_takenTrack1;
    QVariantList m_takenTrack2;
    QGeoCoordinate m_takenStart1;
    QGeoCoordinate m_takenStart2;
    QGeoCoordinate m_takenEnd1;
    QGeoCoordinate m_takenEnd2;

    // For 3D map
    QString m_model;
    int m_altitudeReference;
    float m_labelAltitudeOffset;
    float m_modelAltitudeOffset;
    bool m_fixedPosition;
    QList<CesiumInterface::Animation *> m_animations;
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
        mapImageVisibleRole = Qt::UserRole + 4,
        mapImageRole = Qt::UserRole + 5,
        mapImageRotationRole = Qt::UserRole + 6,
        mapImageMinZoomRole = Qt::UserRole + 7,
        bubbleColourRole = Qt::UserRole + 8,
        selectedRole = Qt::UserRole + 9,
        targetRole = Qt::UserRole + 10,
        frequencyRole = Qt::UserRole + 11,
        frequencyStringRole = Qt::UserRole + 12,
        predictedGroundTrack1Role = Qt::UserRole + 13,
        predictedGroundTrack2Role = Qt::UserRole + 14,
        groundTrack1Role = Qt::UserRole + 15,
        groundTrack2Role = Qt::UserRole + 16,
        groundTrackColorRole = Qt::UserRole + 17,
        predictedGroundTrackColorRole = Qt::UserRole + 18
    };

    MapModel(MapGUI *gui);

    void playAnimations(MapItem *item);

    Q_INVOKABLE void add(MapItem *item);
    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group="");
    void update(MapItem *item);
    void remove(MapItem *item);
    void allUpdated();
    void removeAll();
    void updateItemSettings(QHash<QString, MapSettings::MapItemSettings *> m_itemSettings);

    void updateTarget();
    void setTarget(const QString& name);
    bool isTarget(const MapItem *mapItem) const;

    Q_INVOKABLE void moveToFront(int oldRow);
    Q_INVOKABLE void moveToBack(int oldRow);

    MapItem *findMapItem(const QObject *source, const QString& name);
    MapItem *findMapItem(const QString& name);
    QModelIndex findMapItemIndex(const QString& name);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setDisplayNames(bool displayNames);
    void setDisplaySelectedGroundTracks(bool displayGroundTracks);
    void setDisplayAllGroundTracks(bool displayGroundTracks);
    Q_INVOKABLE void setFrequency(double frequency);
    Q_INVOKABLE void track3D(int index);

    void interpolateEast(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolateWest(QGeoCoordinate *c1, QGeoCoordinate *c2, double x, QGeoCoordinate *ci, bool offScreen);
    void interpolate(QGeoCoordinate *c1, QGeoCoordinate *c2, double bottomLeftLongitude, double bottomRightLongitude, QGeoCoordinate* ci, bool offScreen);

    void splitTracks(MapItem *item);
    void splitTrack(const QList<QGeoCoordinate *>& coords, const QVariantList& track,
                        QVariantList& track1, QVariantList& track2,
                        QGeoCoordinate& start1, QGeoCoordinate& start2,
                        QGeoCoordinate& end1, QGeoCoordinate& end2);
    Q_INVOKABLE void viewChanged(double bottomLeftLongitude, double bottomRightLongitude);

    QHash<int, QByteArray> roleNames() const
    {
        QHash<int, QByteArray> roles;
        roles[positionRole] = "position";
        roles[mapTextRole] = "mapText";
        roles[mapTextVisibleRole] = "mapTextVisible";
        roles[mapImageVisibleRole] = "mapImageVisible";
        roles[mapImageRole] = "mapImage";
        roles[mapImageRotationRole] = "mapImageRotation";
        roles[mapImageMinZoomRole] = "mapImageMinZoom";
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
        return roles;
    }

    // Linear interpolation
    double interpolate(double x0, double y0, double x1, double y1, double x)
    {
        return (y0*(x1-x) + y1*(x-x0)) / (x1-x0);
    }

    bool isSelected3D(const MapItem *item) const
    {
        return m_selected3D == item->m_name;
    }

    void setSelected3D(const QString &selected)
    {
        m_selected3D = selected;
    }

public slots:
    void update3DMap(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());

private:
    MapGUI *m_gui;
    QList<MapItem *> m_items;
    QList<bool> m_selected;
    int m_target;               // Row number of current target, or -1 for none
    bool m_displayNames;
    bool m_displaySelectedGroundTracks;
    bool m_displayAllGroundTracks;

    double m_bottomLeftLongitude;
    double m_bottomRightLongitude;

    QString m_selected3D;       // Name of item selected on 3D map - only supports 1 item, unlike 2D map
};


#endif // INCLUDE_FEATURE_MAPMODEL_H_
