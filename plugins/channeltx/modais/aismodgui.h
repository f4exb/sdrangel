///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODAIS_AISMODGUI_H_
#define PLUGINS_CHANNELTX_MODAIS_AISMODGUI_H_

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "aismod.h"
#include "aismodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;
class ScopeVis;

namespace Ui {
    class AISModGUI;
}

class AISModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static AISModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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

private:
    Ui::AISModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    AISModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    SpectrumVis* m_spectrumVis;
    ScopeVis* m_scopeVis;

    AISMod* m_aisMod;
    MovingAverageUtil<double, double, 2> m_channelPowerDbAvg; // Less than other mods, as messages are short

    MessageQueue m_inputMessageQueue;

    explicit AISModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~AISModGUI();

    void transmit();
    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_mode_currentIndexChanged(int value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_bt_valueChanged(int value);
    void on_gain_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_txButton_clicked();
    void on_encode_clicked();
    void on_msgId_currentIndexChanged(int index);
    void on_mmsi_editingFinished();
    void on_status_currentIndexChanged(int index);
    void on_latitude_valueChanged(double value);
    void on_longitude_valueChanged(double value);
    void on_insertPosition_clicked();
    void on_course_valueChanged(double value);
    void on_speed_valueChanged(double value);
    void on_heading_valueChanged(int value);
    void on_message_editingFinished();
    void on_message_returnPressed();
    void on_repeat_toggled(bool checked);
    void repeatSelect();
    void txSettingsSelect();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODAIS_AISMODGUI_H_ */
