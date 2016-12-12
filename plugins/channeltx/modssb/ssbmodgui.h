///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODSSB_SSBMODGUI_H_
#define PLUGINS_CHANNELTX_MODSSB_SSBMODGUI_H_

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "ssbmod.h"

class PluginAPI;
class DeviceSinkAPI;

class ThreadedBasebandSampleSource;
class UpChannelizer;
class SSBMod;

namespace Ui {
    class SSBModGUI;
}

class SSBModGUI : public RollupWidget, public PluginGUI {
    Q_OBJECT

public:
    static SSBModGUI* create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI);
    void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    virtual bool handleMessage(const Message& message);

    static const QString m_channelID;

private slots:
    void viewChanged();
    void handleSourceMessages();

    void on_deltaFrequency_changed(quint64 value);
    void on_deltaMinus_toggled(bool minus);
    void on_BW_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_toneFrequency_valueChanged(int value);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);

    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();

    void configureFileName();
    void tick();

private:
    Ui::SSBModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceSinkAPI* m_deviceAPI;
    ChannelMarker m_channelMarker;
    bool m_basicSettingsShown;
    bool m_doApplySettings;
	int m_rate;
	int m_spanLog2;

    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    SSBMod* m_ssbMod;
    MovingAverage<Real> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    SSBMod::SSBModInputAF m_modAFInput;

    explicit SSBModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent = NULL);
    virtual ~SSBModGUI();

    bool setNewRate(int spanLog2);

    void blockApplySettings(bool block);
    void applySettings();
    void updateWithStreamData();
    void updateWithStreamTime();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);
};

#endif /* PLUGINS_CHANNELTX_MODSSB_SSBMODGUI_H_ */
