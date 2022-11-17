///////////////////////////////////////////////////////////////////////////////////
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

#ifndef PLUGINS_CHANNELTX_MODLORA_LORAMODGUI_H_
#define PLUGINS_CHANNELTX_MODLORA_LORAMODGUI_H_

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "chirpchatmod.h"
#include "chirpchatmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;

namespace Ui {
    class ChirpChatModGUI;
}

class ChirpChatModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static ChirpChatModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::ChirpChatModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    ChirpChatModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    ChirpChatMod* m_chirpChatMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    std::size_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    explicit ChirpChatModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = nullptr);
    virtual ~ChirpChatModGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayCurrentPayloadMessage();
    void displayBinaryMessage();
    void setBandwidths();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_bw_valueChanged(int value);
	void on_spread_valueChanged(int value);
    void on_deBits_valueChanged(int value);
    void on_preambleChirps_valueChanged(int value);
    void on_idleTime_valueChanged(int value);
    void on_syncWord_editingFinished();
    void on_channelMute_toggled(bool checked);
    void on_scheme_currentIndexChanged(int index);
    void on_fecParity_valueChanged(int value);
    void on_crc_stateChanged(int state);
    void on_header_stateChanged(int state);
    void on_myCall_editingFinished();
    void on_urCall_editingFinished();
    void on_myLocator_editingFinished();
    void on_report_editingFinished();
    void on_msgType_currentIndexChanged(int index);
    void on_resetMessages_clicked(bool checked);
    void on_playMessage_clicked(bool checked);
    void on_repeatMessage_valueChanged(int value);
    void on_generateMessages_clicked(bool checked);
    void on_messageText_editingFinished();
    void on_hexText_editingFinished();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODLORA_LORAMODGUI_H_ */
