///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMODGUI_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMODGUI_H_

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "datvmod.h"
#include "datvmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class QMessageBox;

namespace Ui {
    class DATVModGUI;
}

class DATVModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DATVModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::DATVModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DATVModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    bool m_tickMsgOutstanding;

    DATVMod* m_datvMod;
    MovingAverageUtil<double, double, 20> m_channelPowerDbAvg;

    int m_channelSampleRate;
    int m_sampleRate;

    quint32 m_streamLength;     //!< TS file length in seconds
    float m_bitrate;            //!< TS file bitrate
    int m_frameCount;           //!< TS frame count (E.g. 188 byte TS frames, not video frames)
    std::size_t m_tickCount;
    bool m_enableNavTime;
    MessageQueue m_inputMessageQueue;

    explicit DATVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~DATVModGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateWithStreamData();
    void updateWithStreamTime();
    void setChannelMarkerBandwidth();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void updateFEC();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_channelMute_toggled(bool checked);
    void on_dvbStandard_currentIndexChanged(int index);
    void on_symbolRate_valueChanged(int value);
    void on_rfBW_valueChanged(int value);
    void on_fec_currentIndexChanged(int index);
    void on_modulation_currentIndexChanged(int index);
    void on_rollOff_currentIndexChanged(int index);

    void on_inputSelect_currentIndexChanged(int index);
    void on_tsFileDialog_clicked(bool checked);

    void on_playFile_toggled(bool checked);
    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);

    void on_udpAddress_editingFinished();
    void on_udpPort_valueChanged(int value);

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void configureTsFileName();
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODDATV_DATVMODGUI_H_ */
