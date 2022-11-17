///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODGUI_H_
#define PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODGUI_H_

#include <QIcon>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "freedvmod.h"
#include "freedvmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class FreeDVModGUI;
}

class FreeDVModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static FreeDVModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::FreeDVModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    FreeDVModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
	int m_spectrumRate;

    SpectrumVis* m_spectrumVis;
    FreeDVMod* m_freeDVMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    QString m_fileName;
    quint32 m_recordLength;
    int m_recordSampleRate;
    int m_samplesCount;
    int m_audioSampleRate;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    MessageQueue m_inputMessageQueue;

    explicit FreeDVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~FreeDVModGUI();

    bool blockApplySettings(bool block);
    void applySettings(bool force = false);
    void applyBandwidths(int spanLog2, bool force = false);
    void displayBandwidths(int spanLog2);
    void displaySettings();
    void updateWithStreamData();
    void updateWithStreamTime();
    void channelMarkerUpdate();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_spanLog2_valueChanged(int value);
    void on_gaugeInput_toggled(bool checked);
    void on_volume_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_freeDVMode_currentIndexChanged(int index);
    void on_tone_toggled(bool checked);
    void on_toneFrequency_valueChanged(int value);
    void on_mic_toggled(bool checked);
    void on_play_toggled(bool checked);
    void on_playLoop_toggled(bool checked);
    void on_morseKeyer_toggled(bool checked);

    void on_navTimeSlider_valueChanged(int value);
    void on_showFileDialog_clicked(bool checked);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void configureFileName();
    void audioSelect();
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODFREEDV_FREEDVMODGUI_H_ */
