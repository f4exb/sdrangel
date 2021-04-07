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

#ifndef INCLUDE_IEEE_802_15_4_MODGUI_H
#define INCLUDE_IEEE_802_15_4_MODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"

#include "ieee_802_15_4_mod.h"
#include "ieee_802_15_4_modsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;
class ScopeVis;
class ScopeVisXY;

namespace Ui {
    class IEEE_802_15_4_ModGUI;
}

class IEEE_802_15_4_ModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static IEEE_802_15_4_ModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::IEEE_802_15_4_ModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    IEEE_802_15_4_ModSettings m_settings;
    bool m_doApplySettings;
    SpectrumVis* m_spectrumVis;
    ScopeVis* m_scopeVis;
    int m_basebandSampleRate;

    IEEE_802_15_4_Mod* m_IEEE_802_15_4_Mod;
    MovingAverageUtil<double, double, 2> m_channelPowerDbAvg; // Less than other mods, as frames are short

    MessageQueue m_inputMessageQueue;

    explicit IEEE_802_15_4_ModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~IEEE_802_15_4_ModGUI();

    void checkSampleRate();
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
    void on_phy_currentIndexChanged(int value);
    void on_rfBW_valueChanged(int index);
    void on_gain_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_txButton_clicked();
    void on_frame_editingFinished();
    void on_frame_returnPressed();
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

#endif /* INCLUDE_IEEE_802_15_4_MODGUI_H */
