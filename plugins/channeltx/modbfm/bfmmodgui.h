///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_CHANNELTX_MODBFM_BFMMODGUI_H_
#define PLUGINS_CHANNELTX_MODBFM_BFMMODGUI_H_

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "bfmmod.h"
#include "bfmmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;

namespace Ui {
    class BFMModGUI;
}

class BFMModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static BFMModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::BFMModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    BFMModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    BFMMod* m_bfmMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    int m_audioSampleRate;
    int m_feedbackAudioSampleRate;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    MessageQueue m_inputMessageQueue;

    explicit BFMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~BFMModGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    void selectInput(BFMModSettings::BFMModInputAF input, bool checked);
    void displayInput();
    void displayFileName();
    void updateWithStreamData();
    void updateWithStreamTime();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void updateSampleRateStatus();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    static int requiredBW(int rfBW)
    {
        if (rfBW <= 96000) {
            return 96000;
        } else {
            return (3*rfBW)/2;
        }
    }

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_currentIndexChanged(int index);
    void on_afBW_valueChanged(int value);
    void on_fmDev_valueChanged(int value);
    void on_toneFrequency_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_stereo_toggled(bool checked);
    void on_preEmphasis_currentIndexChanged(int index);
    void on_rdsActive_toggled(bool checked);
    void on_pilotLevel_valueChanged(double value);
    void on_rdsLevel_valueChanged(double value);
    void on_rdsPTY_currentIndexChanged(int index);
    void on_rdsPI_editingFinished();
    void on_rdsPS_editingFinished();
    void on_rdsRadioText_editingFinished();
    void on_channelMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);

    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void on_feedbackEnable_toggled(bool checked);
    void on_feedbackVolume_valueChanged(int value);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void audioSelect(const QPoint& p);
    void audioFeedbackSelect(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODBFM_BFMMODGUI_H_ */
