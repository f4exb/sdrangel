///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_FEATURE_SKYMAPGUI_H_
#define INCLUDE_FEATURE_SKYMAPGUI_H_

#include <QTimer>
#include <QJsonObject>
#include <QWebEngineFullScreenRequest>
#include <QWebEnginePage>
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#include <QWebEngineLoadingInfo>
#endif
#include <QGeoCoordinate>

#include <math.h>
#include <limits>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "availablechannelorfeaturehandler.h"
#include "maincore.h"

#include "skymapsettings.h"
#include "webinterface.h"
#include "webserver.h"
#include "wtml.h"

class PluginAPI;
class FeatureUISet;
class SkyMap;

namespace Ui {
    class SkyMapGUI;
}

class SkyMapGUI;

class SkyMapGUI : public FeatureGUI {
    Q_OBJECT
public:
    static SkyMapGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }
    SkyMap *getSkyMap() { return m_skymap; }

private:
    Ui::SkyMapGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    SkyMapSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;
    QObject *m_source;
    AvailableChannelOrFeatureList m_availableChannelOrFeatures;
    AvailableChannelOrFeatureHandler m_availableChannelOrFeatureHandler;

    SkyMap* m_skymap;
    MessageQueue m_inputMessageQueue;
    quint16 m_skymapTileServerPort;

    WebServer *m_webServer;
    quint16 m_webPort;
    WTML m_wtml;
    WebInterface *m_webInterface;
    bool m_ready;               //!< Web app is ready
    QString m_find;

    double m_ra;                //!< Target from source plugin
    double m_dec;
    QDateTime m_dateTime;       //!< Date time from source plugin

    QStringList m_wwtBackgrounds;

    explicit SkyMapGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~SkyMapGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void setDateTime(QDateTime dateTime);
    QDateTime getDateTime() const;
    void setPosition(float latitude, float longitude, float altitude);
    QGeoCoordinate getPosition() const;
    void initSkyMap();
    QString backgroundID(const QString& name);
    void updateBackgrounds();
    void updateToolbar();
    void updateProjection();
    void find(const QString& text);
    void sendToRotator(const QString& name, double az, double alt);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_tabs_tabCloseRequested(int index);
    void on_find_returnPressed();
    void on_displaySettings_clicked();
    void on_displayNames_clicked(bool checked);
    void on_displayConstellations_clicked(bool checked);
    void on_displayReticle_clicked(bool checked);
    void on_displayGrid_clicked(bool checked);
    void on_displayAntennaFoV_clicked(bool checked);
    void on_map_currentIndexChanged(int index);
    void on_background_currentIndexChanged(int index);
    void on_projection_currentIndexChanged(int index);
    void on_source_currentIndexChanged(int index);
    void on_track_clicked(bool checked);
    void fullScreenRequested(QWebEngineFullScreenRequest fullScreenRequest);
    void renderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus terminationStatus, int exitCode);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    void loadingChanged(const QWebEngineLoadingInfo &loadingInfo);
#endif
    void preferenceChanged(int elementType);
    void receivedEvent(const QJsonObject &obj);
    void wtmlUpdated(const QList<WTML::ImageSet>& dataSets);
    void updateSourceList(const QStringList& renameFrom, const QStringList& renameTo);
    void handlePipeMessageQueue(MessageQueue* messageQueue);

};

#endif // INCLUDE_FEATURE_SKYMAPGUI_H_
