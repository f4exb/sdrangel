///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB                              //
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

#ifndef PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_
#define PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_

#include <QElapsedTimer>

#include "dsp/channelmarker.h"
#include "channel/channelgui.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "../remotesource/remotesourcesettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class RemoteSource;

namespace Ui {
    class RemoteSourceGUI;
}

class RemoteSourceGUI : public ChannelGUI {
    Q_OBJECT

public:
    static RemoteSourceGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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
    Ui::RemoteSourceGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RemoteSourceSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_remoteSampleRate;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    double m_shiftFrequencyFactor; //!< Channel frequency shift factor

    RemoteSource* m_remoteSrc;
    MessageQueue m_inputMessageQueue;

    uint32_t m_countUnrecoverable;
    uint32_t m_countRecovered;
    uint32_t m_lastCountUnrecoverable;
    uint32_t m_lastCountRecovered;
    uint32_t m_lastSampleCount;
    uint64_t m_lastTimestampUs;
    bool m_resetCounts;
    QElapsedTimer m_time;
    uint32_t m_tickCount;

    explicit RemoteSourceGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~RemoteSourceGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayRateAndShift();
    void displayPosition();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void displayEventCounts();
    void displayEventStatus(int recoverableCount, int unrecoverableCount);
    void displayEventTimer();

    void applyInterpolation();
    void applyPosition();

private slots:
    void handleSourceMessages();
    void on_interpolationFactor_currentIndexChanged(int index);
    void on_position_valueChanged(int value);
    void on_dataAddress_returnPressed();
    void on_dataPort_returnPressed();
    void on_dataApplyButton_clicked(bool checked);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void on_eventCountsReset_clicked(bool checked);
    void tick();
};


#endif /* PLUGINS_CHANNELTX_REMOTESRC_REMOTESRCGUI_H_ */
