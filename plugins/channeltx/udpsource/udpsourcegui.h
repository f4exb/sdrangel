///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_
#define PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_

#include <QObject>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "settings/rollupstate.h"

#include "udpsource.h"
#include "udpsourcesettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class SpectrumVis;

namespace Ui {
    class UDPSourceGUI;
}

class UDPSourceGUI : public ChannelGUI {
    Q_OBJECT

public:
    static UDPSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    virtual void resetToDefaults();
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
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
    Ui::UDPSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    SpectrumVis* m_spectrumVis;
    UDPSource* m_udpSource;
    MovingAverageUtil<double, double, 4> m_channelPowerAvg;
    MovingAverageUtil<double, double, 4> m_inPowerAvg;
    uint32_t m_tickCount;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;

    // settings
    UDPSourceSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_rfBandwidthChanged;
    bool m_doApplySettings;
    MessageQueue m_inputMessageQueue;

    explicit UDPSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = NULL);
    virtual ~UDPSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void setSampleFormat(int index);
    void setSampleFormatIndex(const UDPSourceSettings::SampleFormat& sampleFormat);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_sampleFormat_currentIndexChanged(int index);
    void on_localUDPAddress_editingFinished();
    void on_localUDPPort_editingFinished();
    void on_multicastAddress_editingFinished();
    void on_multicastJoin_toggled(bool checked);
    void on_sampleRate_textEdited(const QString& arg1);
    void on_rfBandwidth_textEdited(const QString& arg1);
    void on_fmDeviation_textEdited(const QString& arg1);
    void on_amModPercent_textEdited(const QString& arg1);
    void on_applyBtn_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void on_gainIn_valueChanged(int value);
    void on_gainOut_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
    void on_channelMute_toggled(bool checked);
    void on_resetUDPReadIndex_clicked();
    void on_autoRWBalance_toggled(bool checked);
    void on_stereoInput_toggled(bool checked);
    void tick();
};

#endif /* PLUGINS_CHANNELTX_UDPSINK_UDPSOURCEGUI_H_ */
