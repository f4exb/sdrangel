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

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "ssbmod.h"
#include "ssbmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class SSBModGUI;
}

class SSBModGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    static SSBModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::SSBModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    SSBModSettings m_settings;
    bool m_doApplySettings;
	int m_spectrumRate;

    SpectrumVis* m_spectrumVis;
    SSBMod* m_ssbMod;
    MovingAverage<double> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    SSBMod::SSBModInputAF m_modAFInput;
    MessageQueue m_inputMessageQueue;

    explicit SSBModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~SSBModGUI();

    bool blockApplySettings(bool block);
    void applySettings(bool force = false);
    void applyBandwidths(bool force = false);
    void displaySettings();
    void displayAGCPowerThreshold();
    void updateWithStreamData();
    void updateWithStreamTime();
    void channelMarkerUpdate();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_dsb_toggled(bool checked);
    void on_audioBinaural_toggled(bool checked);
    void on_audioFlipChannels_toggled(bool checked);
    void on_spanLog2_valueChanged(int value);
    void on_BW_valueChanged(int value);
    void on_lowCut_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_toneFrequency_valueChanged(int value);
    void on_mic_toggled(bool checked);
    void on_agc_toggled(bool checked);
    void on_agcOrder_valueChanged(int value);
    void on_agcTime_valueChanged(int value);
    void on_agcThreshold_valueChanged(int value);
    void on_agcThresholdGate_valueChanged(int value);
    void on_agcThresholdDelay_valueChanged(int value);
    void on_play_toggled(bool checked);
    void on_playLoop_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);

    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void onWidgetRolled(QWidget* widget, bool rollDown);

    void configureFileName();
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODSSB_SSBMODGUI_H_ */
