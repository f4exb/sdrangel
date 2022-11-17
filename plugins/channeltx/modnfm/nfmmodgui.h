///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_
#define PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_

#include <QRegularExpressionValidator>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "nfmmod.h"
#include "nfmmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;

namespace Ui {
    class NFMModGUI;
}

class NFMModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static NFMModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::NFMModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    NFMModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    NFMMod* m_nfmMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    int m_audioSampleRate;
    int m_feedbackAudioSampleRate;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    NFMModSettings::NFMModInputAF m_modAFInput;
    MessageQueue m_inputMessageQueue;
    QRegularExpressionValidator m_dcsCodeValidator;

    explicit NFMModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~NFMModGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateWithStreamData();
    void updateWithStreamTime();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_channelSpacingApply_clicked();
    void on_rfBW_valueChanged(int value);
    void on_afBW_valueChanged(int value);
    void on_preEmphasis_toggled(bool checked);
    void on_fmDev_valueChanged(int value);
    void on_toneFrequency_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_tone_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);
    void on_mic_toggled(bool checked);
    void on_compressor_toggled(bool checked);
    void on_play_toggled(bool checked);

    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void on_ctcss_currentIndexChanged(int index);
    void on_ctcssOn_toggled(bool checked);
    void on_dcsOn_toggled(bool checked);
    void on_dcsCode_editingFinished();
    void on_dcsPositive_toggled(bool checked);
    void on_bpf_toggled(bool checked);

    void on_feedbackEnable_toggled(bool checked);
    void on_feedbackVolume_valueChanged(int value);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void configureFileName();
    void audioSelect();
    void audioFeedbackSelect();
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODNFM_NFMMODGUI_H_ */
