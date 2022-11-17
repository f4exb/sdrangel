///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPSINKGUI_H_
#define PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPSINKGUI_H_

#include <stdint.h>

#include <QObject>

#include "dsp/channelmarker.h"
#include "channel/channelgui.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "settings/rollupstate.h"

#include "remotetcpsinksettings.h"

class PluginAPI;
class DeviceUISet;
class RemoteTCPSink;
class BasebandSampleSink;

namespace Ui {
    class RemoteTCPSinkGUI;
}

class RemoteTCPSinkGUI : public ChannelGUI {
    Q_OBJECT
public:
    static RemoteTCPSinkGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; }
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }
    virtual QString getTitle() const { return m_settings.m_title; }
    virtual QColor getTitleColor() const { return m_settings.m_rgbColor; }
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::RemoteTCPSinkGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RemoteTCPSinkSettings m_settings;
    int m_basebandSampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor
    bool m_doApplySettings;

    RemoteTCPSink* m_remoteSink;
    MessageQueue m_inputMessageQueue;

    uint32_t m_tickCount;
    MovingAverageUtil<float, float, 10> m_bwAvg;

    explicit RemoteTCPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~RemoteTCPSinkGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayRateAndShift();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    QString displayScaledF(float value, char type, int precision, bool showMult);
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void applyDecimation();
    void applyPosition();

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(int index);
    void on_channelSampleRate_changed(int value);
    void on_gain_valueChanged(int value);
    void on_sampleBits_currentIndexChanged(int index);
    void on_dataAddress_editingFinished();
    void on_dataPort_editingFinished();
    void on_protocol_currentIndexChanged(int index);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPSINKGUI_H_ */
