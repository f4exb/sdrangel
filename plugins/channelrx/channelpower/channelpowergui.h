///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_CHANNELPOWERGUI_H
#define INCLUDE_CHANNELPOWERGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "channelpowersettings.h"
#include "channelpower.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ChannelPower;
class ChannelPowerGUI;

namespace Ui {
    class ChannelPowerGUI;
}

class ChannelPowerGUI : public ChannelGUI {
    Q_OBJECT

public:
    static ChannelPowerGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::ChannelPowerGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    ChannelPowerSettings m_settings;
    QStringList m_settingsKeys;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    ChannelPower* m_channelPower;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    const static QStringList m_averagePeriodTexts;

    explicit ChannelPowerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ChannelPowerGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void calcOffset();
    void updateAbsoluteCenterFrequency();
    void on_clearMeasurements_clicked();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_frequencyMode_currentIndexChanged(int index);
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_changed(qint64 value);
    void on_clearChannelPower_clicked();
    void on_pulseTH_valueChanged(int value);
    void on_averagePeriod_valueChanged(int value);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_CHANNELPOWERGUI_H
