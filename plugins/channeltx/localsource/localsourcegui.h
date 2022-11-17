///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_LOCALSOURCE_LOCALSOURCEGUI_H_
#define PLUGINS_CHANNELTX_LOCALSOURCE_LOCALSOURCEGUI_H_

#include <stdint.h>

#include <QObject>

#include "dsp/channelmarker.h"
#include "channel/channelgui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "localsourcesettings.h"

class PluginAPI;
class DeviceUISet;
class LocalSource;
class BasebandSampleSource;

namespace Ui {
    class LocalSourceGUI;
}

class LocalSourceGUI : public ChannelGUI {
    Q_OBJECT
public:
    static LocalSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel);
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

private:
    Ui::LocalSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    LocalSourceSettings m_settings;
    int m_basebandSampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor
    bool m_doApplySettings;

    LocalSource* m_localSource;
    MessageQueue m_inputMessageQueue;

    uint32_t m_tickCount;

    explicit LocalSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *txChannel, QWidget* parent = 0);
    virtual ~LocalSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayRateAndShift();
    void updateLocalDevices();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void applyInterpolation();
    void applyPosition();

private slots:
    void handleSourceMessages();
    void on_interpolationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_localDevice_currentIndexChanged(int index);
    void on_localDevicesRefresh_clicked(bool checked);
    void on_localDevicePlay_toggled(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};



#endif /* PLUGINS_CHANNELRX_LOCALSINK_LOCALSINKGUI_H_ */
