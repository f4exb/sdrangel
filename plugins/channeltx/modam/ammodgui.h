///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_
#define PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "ammod.h"
#include "ammodsettings.h"

class PluginAPI;
class DeviceUISet;

class AMMod;
class BasebandSampleSource;
class CRightClickEnabler;

namespace Ui {
    class AMModGUI;
}

class AMModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static AMModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void resetToDefaults() final;
    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;
    MessageQueue *getInputMessageQueue() final { return &m_inputMessageQueue; }
    void setWorkspaceIndex(int index) final { m_settings.m_workspaceIndex = index; };
    int getWorkspaceIndex() const final { return m_settings.m_workspaceIndex; };
    void setGeometryBytes(const QByteArray& blob) final { m_settings.m_geometryBytes = blob; };
    QByteArray getGeometryBytes() const final { return m_settings.m_geometryBytes; };
    QString getTitle() const final { return m_settings.m_title; };
    QColor getTitleColor() const final  { return m_settings.m_rgbColor; };
    void zetHidden(bool hidden) final { m_settings.m_hidden = hidden; }
    bool getHidden() const final { return m_settings.m_hidden; }
    ChannelMarker& getChannelMarker() final { return m_channelMarker; }
    int getStreamIndex() const final { return m_settings.m_streamIndex; }
    void setStreamIndex(int streamIndex) final { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::AMModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    AMModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    CRightClickEnabler *m_audioMuteRightClickEnabler;
    CRightClickEnabler *m_feedbackRightClickEnabler;

    AMMod* m_amMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    int m_audioSampleRate;
    int m_feedbackAudioSampleRate;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    MessageQueue m_inputMessageQueue;

    explicit AMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = nullptr);
    ~AMModGUI() final;

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateWithStreamData();
    void updateWithStreamTime();
    bool handleMessage(const Message& message);
    void makeUIConnections() const;
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*) final;
    void enterEvent(EnterEventType*) final;

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_modPercent_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_toneFrequency_valueChanged(int value);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);

    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void on_feedbackEnable_toggled(bool checked);
    void on_feedbackVolume_valueChanged(int value);

    void onWidgetRolled(const QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void configureFileName();
    void audioSelect(const QPoint& p);
    void audioFeedbackSelect(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_ */
