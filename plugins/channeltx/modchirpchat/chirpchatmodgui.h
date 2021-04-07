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

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::ChirpChatModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    ChirpChatModSettings m_settings;
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
    void displayStreamIndex();
    void displayCurrentPayloadMessage();
    void displayBinaryMessage();
    void setBandwidths();
    bool handleMessage(const Message& message);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

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
