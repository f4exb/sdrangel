///////////////////////////////////////////////////////////////////////////////////
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

#ifndef INCLUDE_DRONEIDDEMODGUI_H
#define INCLUDE_DRONEIDDEMODGUI_H

#include <QTableWidgetItem>
#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "droneiddemodsettings.h"
#include "droneiddemod.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class DroneIDDemod;
class DroneIDDemodGUI;

namespace Ui {
    class DroneIDDemodGUI;
}

class DroneIDDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DroneIDDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    void channelMarkerHighlightedByCursor();

private:
    Ui::DroneIDDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DroneIDDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    DroneIDDemod* m_droneIDDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;
    int m_burstsDetected;
    int m_burstsDecoded;

    QMenu *m_messagesMenu;                        // Column select context menu

    static constexpr int m_occupiedBandwidth = 9000000;
    static constexpr int m_maxRows = 10000;    //!< Maximum number of messages in table

    explicit DroneIDDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~DroneIDDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    void messageReceived(const QByteArray& message, const QDateTime& dateTime, float snr, float cfo, qint64 frequency, bool live);
    void burstResult(const DroneIDDemod::MsgBurstResult& result);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void updateChannelSelection();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);

    enum MessageCol {
        COL_DATE_TIME,
        COL_SERIAL,
        COL_MODEL,
        COL_TYPE,
        COL_SEQUENCE,
        COL_LATITUDE,
        COL_LONGITUDE,
        COL_ALTITUDE,
        COL_HEIGHT,
        COL_SPEED,
        COL_VERTICAL_SPEED,
        COL_COURSE,
        COL_YAW,
        COL_PILOT_LATITUDE,
        COL_PILOT_LONGITUDE,
        COL_HOME_LATITUDE,
        COL_HOME_LONGITUDE,
        COL_UUID,
        COL_STATE,
        COL_GPS_TIME,
        COL_SNR,
        COL_CFO,
        COL_FREQUENCY,
        COL_CRC
    };

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_channel_currentIndexChanged(int index);
    void on_scan_toggled(bool checked);
    void on_scan_rightClicked(const QPoint& point);
    void on_scanBand_currentIndexChanged(int index);
    void on_threshold_valueChanged(int value);
    void on_turboIterations_valueChanged(int value);
    void on_mapTimeout_valueChanged(int value);
    void on_mapAltitude_currentIndexChanged(int index);
    void on_filterSerial_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_messages_cellDoubleClicked(int row, int column);
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_useFileTime_toggled(bool checked=false);
    void filterRow(int row);
    void filter();
    void messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messages_sectionResized(int logicalIndex, int oldSize, int newSize);
    void messagesColumnSelectMenu(QPoint pos);
    void messagesColumnSelectMenuChecked(bool checked = false);
    void customContextMenuRequested(QPoint point);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_DRONEIDDEMODGUI_H
