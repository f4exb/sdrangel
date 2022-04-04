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
#include <QQuickItem>
#include <QJsonObject>
#include <QWebEngineFullScreenRequest>

#include <math.h>
#include <limits>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "settings/rollupstate.h"

#include "SWGMapItem.h"

#include "mapsettings.h"
#include "mapbeacondialog.h"
#include "mapibpbeacondialog.h"
#include "mapradiotimedialog.h"
#include "cesiuminterface.h"
#include "osmtemplateserver.h"
#include "webserver.h"
#include "mapmodel.h"

class PluginAPI;
class FeatureUISet;
class Map;

namespace Ui {
    class MapGUI;
}

class MapGUI;
struct Beacon;

struct RadioTimeTransmitter {
    QString m_callsign;
    int m_frequency;        // In Hz
    float m_latitude;       // In degrees
    float m_longitude;      // In degrees
    int m_power;            // In kW
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
    static QString getBeaconFilename();
    QList<Beacon *> *getBeacons() { return m_beacons; }
    void setBeacons(QList<Beacon *> *beacons);
    void addIBPBeacons();
    QList<RadioTimeTransmitter> getRadioTimeTransmitters() { return m_radioTimeTransmitters; }
    void addRadioTimeTransmitters();
    void addRadar();
    void addDAB();
    void find(const QString& target);
    void track3D(const QString& target);
    Q_INVOKABLE void supportedMapsChanged();
    MapSettings::MapItemSettings *getItemSettings(const QString &group) { return m_settings.m_itemSettings[group]; }
    CesiumInterface *cesium() { return m_cesium; }

private:
    Ui::MapGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    MapSettings m_settings;
    RollupState m_rollupState;
    bool m_doApplySettings;
    QList<MapSettings::AvailableChannelOrFeature> m_availableChannelOrFeatures;

    Map* m_map;
    MessageQueue m_inputMessageQueue;
    MapModel m_mapModel;
    AzEl m_azEl;                        // Position of station
    SWGSDRangel::SWGMapItem m_antennaMapItem;
    QList<Beacon *> *m_beacons;
    MapBeaconDialog m_beaconDialog;
    MapIBPBeaconDialog m_ibpBeaconDialog;
    MapRadioTimeDialog m_radioTimeDialog;
    quint16 m_osmPort;
    OSMTemplateServer *m_templateServer;
    QTimer m_redrawMapTimer;

    CesiumInterface *m_cesium;
    WebServer *m_webServer;
    quint16 m_webPort;

    explicit MapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~MapGUI();

    void update(const QObject *source, SWGSDRangel::SWGMapItem *swgMapItem, const QString &group);
    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void applyMap2DSettings(bool reloadMap);
    void applyMap3DSettings(bool reloadMap);
    QString osmCachePath();
    void clearOSMCache();
    void displaySettings();
    bool handleMessage(const Message& message);
    void geoReply();
    QString thunderforestAPIKey() const;
    QString maptilerAPIKey() const;
    QString cesiumIonAPIKey() const;
    void redrawMap();
    void makeUIConnections();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

    static QString getDataDir();
    static const QList<RadioTimeTransmitter> m_radioTimeTransmitters;

private slots:
    void init3DMap();
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
    void on_ibpBeacons_clicked();
    void on_radiotime_clicked();
    void receivedCesiumEvent(const QJsonObject &obj);
    virtual void showEvent(QShowEvent *event);
    virtual bool eventFilter(QObject *obj, QEvent *event);
    void fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest);
    void preferenceChanged(int elementType);

};

#endif // INCLUDE_FEATURE_MAPGUI_H_
