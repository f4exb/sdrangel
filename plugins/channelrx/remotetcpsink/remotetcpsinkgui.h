///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <QHostAddress>

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
    uint32_t m_tickCount;
    bool m_squelchOpen;
    MessageQueue m_inputMessageQueue;

    MovingAverageUtil<float, float, 10> m_bwAvg;
    MovingAverageUtil<float, float, 10> m_compressionAvg;
    MovingAverageUtil<float, float, 10> m_networkBWAvg;

    enum ConnectionsCol {
        CONNECTIONS_COL_ADDRESS,
        CONNECTIONS_COL_PORT,
        CONNECTIONS_COL_CONNECTED,
        CONNECTIONS_COL_DISCONNECTED,
        CONNECTIONS_COL_TIME
    };

    static const QString m_dateTimeFormat;

    explicit RemoteTCPSinkGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~RemoteTCPSinkGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    void displayIQOnly();
    void displaySquelch();
    void displayRateAndShift();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    QString displayScaledF(float value, char type, int precision, bool showMult);
    void updateAbsoluteCenterFrequency();
    void resizeTable();
    void addConnection(const QHostAddress& address, int port);
    void removeConnection(const QHostAddress& address, int port);

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();
    void on_deltaFrequency_changed(qint64 value);
    void on_channelSampleRate_changed(int value);
    void on_gain_valueChanged(int value);
    void on_sampleBits_currentIndexChanged(int index);
    void on_dataAddress_editingFinished();
    void on_dataAddress_currentIndexChanged(int index);
    void on_dataPort_valueChanged(int value);
    void on_protocol_currentIndexChanged(int index);
    void on_remoteControl_toggled(bool checked);
    void on_squelchEnabled_toggled(bool checked);
    void on_squelch_valueChanged(int value);
    void on_squelchGate_valueChanged(double value);
    void on_displaySettings_clicked();
    void on_sendMessage_clicked();
    void on_txMessage_returnPressed();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void tick();
};

#endif /* PLUGINS_CHANNELRX_REMOTETCPSINK_REMOTETCPSINKGUI_H_ */
