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

#ifndef PLUGINS_CHANNELTX_MODPACKET_PACKETMODGUI_H_
#define PLUGINS_CHANNELTX_MODPACKET_PACKETMODGUI_H_

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "packetmod.h"
#include "packetmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class PacketModGUI;
}

class PacketModGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    static PacketModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::PacketModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    PacketModSettings m_settings;
    bool m_doApplySettings;
    SpectrumVis* m_spectrumVis;

    PacketMod* m_packetMod;
    MovingAverageUtil<double, double, 2> m_channelPowerDbAvg; // Less than other mods, as packets are short

    MessageQueue m_inputMessageQueue;

    explicit PacketModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~PacketModGUI();

    void transmit();
    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_mode_currentIndexChanged(int value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_gain_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_txButton_clicked(bool checked);
    void on_callsign_editingFinished();
    void on_to_currentTextChanged(const QString &text);
    void on_via_currentTextChanged(const QString &text);
    void on_packet_editingFinished();
    void on_insertPosition_clicked(bool checked);
    void on_packet_returnPressed();
    void on_repeat_toggled(bool checked);
    void on_preEmphasis_toggled(bool checked);
    void on_bpf_toggled(bool checked);
    void preEmphasisSelect();
    void bpfSelect();
    void repeatSelect();
    void txSettingsSelect();

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODPACKET_PACKETMODGUI_H_ */
