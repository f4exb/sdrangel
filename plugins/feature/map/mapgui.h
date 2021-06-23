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
#include <QGeoRectangle>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "pipes/pipeendpoint.h"
#include "mapsettings.h"
#include "SWGMapItem.h"
#include "mapbeacondialog.h"
#include "mapradiotimedialog.h"

class PluginAPI;
class FeatureUISet;
class Map;

namespace Ui {
    class MapGUI;
}

class MapGUI;
class MapModel;
class QQuickItem;
struct Beacon;

struct RadioTimeTransmitter {
    QString m_callsign;
    int m_frequency;        // In Hz
    float m_latitude;       // In degrees
    float m_longitude;      // In degrees
    int m_power;            // In kW
};

// Information required about each item displayed on the map
class MapItem {

public:
    MapItem(const PipeEndPoint *sourcePipe, quint32 sourceMask, SWGSDRangel::SWGMapItem *mapItem)
    {
        m_sourcePipe = sourcePipe;
        m_sourceMask = sourceMask;
        m_name = *mapItem->getName();
        m_latitude = mapItem->getLatitude();
        m_longitude = mapItem->getLongitude();
        m_altitude = mapItem->getAltitude();
        m_image = *mapItem->getImage();
        m_imageRotation = mapItem->getImageRotation();
        m_imageMinZoom = mapItem->getImageMinZoom();
        QString *text = mapItem->getText();
        if (text != nullptr)
            m_text = *text;
        findFrequency();
        updateTrack(mapItem->getTrack());
        updatePredictedTrack(mapItem->getPredictedTrack());
    }

    void update(SWGSDRangel::SWGMapItem *mapItem)
    {
        m_latitude = mapItem->getLatitude();
        m_longitude = mapItem->getLongitude();
        m_altitude = mapItem->getAltitude();
        m_image = *mapItem->getImage();
        m_imageRotation = mapItem->getImageRotation();
        m_imageMinZoom = mapItem->getImageMinZoom();
        QString *text = mapItem->getText();
        if (text != nullptr)
            m_text = *text;
        findFrequency();
        updateTrack(mapItem->getTrack());
        updatePredictedTrack(mapItem->getPredictedTrack());
    }

    QGeoCoordinate getCoordinates()
    {
        QGeoCoordinate coords;
        coords.setLatitude(m_latitude);
        coords.setLongitude(m_longitude);
        return coords;
    }

private:

    void findFrequency();

    void updateTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
    {
        if (track != nullptr)
        {
            qDeleteAll(m_takenTrackCoords);
            m_takenTrackCoords.clear();
            m_takenTrack.clear();
            m_takenTrack1.clear();
            m_takenTrack2.clear();
            for (int i = 0; i < track->size(); i++)
            {
                SWGSDRangel::SWGMapCoordinate* p = track->at(i);
                QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
                m_takenTrackCoords.push_back(c);
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
                m_takenTrack.push_back(QVariant::fromValue(*c));
            }
            else
            {
                QGeoCoordinate *prev = m_takenTrackCoords.last();
                if ((prev->latitude() != m_latitude) || (prev->longitude() != m_longitude) || (prev->altitude() != m_altitude))
                {
                    QGeoCoordinate *c = new QGeoCoordinate(m_latitude, m_longitude, m_altitude);
                    m_takenTrackCoords.push_back(c);
                    m_takenTrack.push_back(QVariant::fromValue(*c));
                }
            }
        }
    }

    void updatePredictedTrack(QList<SWGSDRangel::SWGMapCoordinate *> *track)
    {
        if (track != nullptr)
        {
            qDeleteAll(m_predictedTrackCoords);
            m_predictedTrackCoords.clear();
            m_predictedTrack.clear();
            m_predictedTrack1.clear();
            m_predictedTrack2.clear();
            for (int i = 0; i < track->size(); i++)
            {
                SWGSDRangel::SWGMapCoordinate* p = track->at(i);
                QGeoCoordinate *c = new QGeoCoordinate(p->getLatitude(), p->getLongitude(), p->getAltitude());
                m_predictedTrackCoords.push_back(c);
                m_predictedTrack.push_back(QVariant::fromValue(*c));
            }
        }
    }

    friend MapModel;
    const PipeEndPoint *m_sourcePipe;   // Channel/feature that created the item
    quint32 m_sourceMask;               // Source bitmask as per MapSettings::SOURCE_* constants
    QString m_name;
    float m_latitude;
    float m_longitude;
    float m_altitude;                   // In metres
    QString m_image;
    int m_imageRotation;
    int m_imageMinZoom;
    QString m_text;
    double m_frequency;                 // Frequency to set
    QString m_frequencyString;
    QList<QGeoCoordinate *> m_predictedTrackCoords;
    QVariantList m_predictedTrack;      // Line showing where the object is going
    QVariantList m_predictedTrack1;
    QVariantList m_predictedTrack2;
    QGeoCoordinate m_predictedStart1;
    QGeoCoordinate m_predictedStart2;
    QGeoCoordinate m_predictedEnd1;
    QGeoCoordinate m_predictedEnd2;
    QList<QGeoCoordinate *> m_takenTrackCoords;
    QVariantList m_takenTrack;          // Line showing where the object has been
    QVariantList m_takenTrack1;
    QVariantList m_takenTrack2;
    QGeoCoordinate m_takenStart1;
    QGeoCoordinate m_takenStart2;
    QGeoCoordinate m_takenEnd1;
    QGeoCoordinate m_takenEnd2;
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

    MapModel(MapGUI *gui) :
        m_gui(gui),
        m_target(-1),
        m_sources(-1)
    {
        setGroundTrackColor(0);
        setPredictedGroundTrackColor(0);
    }

    Q_INVOKABLE void add(MapItem *item)
    {
        beginInsertRows(QModelIndex(), rowCount(), rowCount());
        m_items.append(item);
        m_selected.append(false);
        endInsertRows();
    }

    void update(const PipeEndPoint *source, SWGSDRangel::SWGMapItem *swgMapItem, quint32 sourceMask=0);

    void updateTarget();

    void update(MapItem *item)
    {
        int row = m_items.indexOf(item);
        if (row >= 0)
        {
            QModelIndex idx = index(row);
            emit dataChanged(idx, idx);
            if (row == m_target)
                updateTarget();
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
            if (row == m_target)
                m_target = -1;
            endRemoveRows();
        }
     }

    Q_INVOKABLE void moveToFront(int oldRow)
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
            if (wasTarget)
                m_target = newRow;
            m_selected[newRow] = wasSelected;
            QModelIndex idx = index(newRow);
            emit dataChanged(idx, idx);
        }
    }

    Q_INVOKABLE void moveToBack(int oldRow)
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
            if (wasTarget)
                m_target = newRow;
            //endMoveRows();
            endResetModel();
            //emit dataChanged(index(oldRow), index(newRow));
        }
    }

    MapItem *findMapItem(const PipeEndPoint *source, const QString& name)
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
        if (m_items.count() > 0)
        {
            beginRemoveRows(QModelIndex(), 0, m_items.count());
            m_items.clear();
            m_selected.clear();
            endRemoveRows();
        }
    }

    void setDisplayNames(bool displayNames)
    {
        m_displayNames = displayNames;
        allUpdated();
    }

    void setDisplaySelectedGroundTracks(bool displayGroundTracks)
    {
        m_displaySelectedGroundTracks = displayGroundTracks;
        allUpdated();
    }

    void setDisplayAllGroundTracks(bool displayGroundTracks)
    {
        m_displayAllGroundTracks = displayGroundTracks;
        allUpdated();
    }

    void setGroundTrackColor(quint32 color)
    {
        m_groundTrackColor = QVariant::fromValue(QColor::fromRgb(color));
    }

    void setPredictedGroundTrackColor(quint32 color)
    {
        m_predictedGroundTrackColor = QVariant::fromValue(QColor::fromRgb(color));
    }

    Q_INVOKABLE void setFrequency(double frequency);

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

    // Set the sources of data we should display
    void setSources(quint32 sources)
    {
        m_sources = sources;
        allUpdated();
    }

    // Linear interpolation
    double interpolate(double x0, double y0, double x1, double y1, double x)
    {
        return (y0*(x1-x) + y1*(x-x0)) / (x1-x0);
    }

private:
    MapGUI *m_gui;
    QList<MapItem *> m_items;
    QList<bool> m_selected;
    int m_target;               // Row number of current target, or -1 for none
    bool m_displayNames;
    bool m_displaySelectedGroundTracks;
    bool m_displayAllGroundTracks;
    quint32 m_sources;
    QVariant m_groundTrackColor;
    QVariant m_predictedGroundTrackColor;

    double m_bottomLeftLongitude;
    double m_bottomRightLongitude;
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
    AzEl *getAzEl() { return &m_azEl; }
    Map *getMap() { return m_map; }
    QQuickItem *getMapItem();
    quint32 getSourceMask(const PipeEndPoint *sourcePipe);
    static QString getBeaconFilename();
    QList<Beacon *> *getBeacons() { return m_beacons; }
    void setBeacons(QList<Beacon *> *beacons);
    QList<RadioTimeTransmitter> getRadioTimeTransmitters() { return m_radioTimeTransmitters; }
    void addRadioTimeTransmitters();
    void find(const QString& target);

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
    AzEl m_azEl;                        // Position of station
    QList<Beacon *> *m_beacons;
    MapBeaconDialog m_beaconDialog;
    MapRadioTimeDialog m_radioTimeDialog;

    explicit MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~MapGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void applyMapSettings();
    void displaySettings();
    bool handleMessage(const Message& message);
    void geoReply();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

    static QString getDataDir();
    static const QList<RadioTimeTransmitter> m_radioTimeTransmitters;

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_displayNames_clicked(bool checked=false);
    void on_displayAllGroundTracks_clicked(bool checked=false);
    void on_displaySelectedGroundTracks_clicked(bool checked=false);
    void on_find_returnPressed();
    void on_maidenhead_clicked();
    void on_deleteAll_clicked();
    void on_displaySettings_clicked();
    void on_mapTypes_currentIndexChanged(int index);
    void on_beacons_clicked();
    void on_radiotime_clicked();
};


#endif // INCLUDE_FEATURE_MAPGUI_H_
