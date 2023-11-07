///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_RADIOSONDEDEMODGUI_H
#define INCLUDE_RADIOSONDEDEMODGUI_H

#include <QTableWidgetItem>
#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "radiosondedemodsettings.h"
#include "radiosondedemod.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class RadiosondeDemod;
class RadiosondeDemodGUI;
class RS41Frame;

namespace Ui {
    class RadiosondeDemodGUI;
}

class RadiosondeDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static RadiosondeDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::RadiosondeDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RadiosondeDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    RadiosondeDemod* m_radiosondeDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *framesMenu;                        // Column select context menu
    QMenu *copyMenu;

    QHash<QString, RS41Subframe *> m_subframes; // Hash of serial to subframes

    explicit RadiosondeDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~RadiosondeDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void frameReceived(const QByteArray& frame, const QDateTime& dateTime, int errorsCorrected, int threshold, bool loadCSV);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);

    enum MessageCol {
        FRAME_COL_DATE,
        FRAME_COL_TIME,
        FRAME_COL_SERIAL,
        FRAME_COL_FRAME_NUMBER,
        FRAME_COL_FLIGHT_PHASE,
        FRAME_COL_LATITUDE,
        FRAME_COL_LONGITUDE,
        FRAME_COL_ALTITUDE,
        FRAME_COL_SPEED,
        FRAME_COL_VERTICAL_RATE,
        FRAME_COL_HEADING,
        FRAME_COL_PRESSURE,
        FRAME_COL_TEMP,
        FRAME_COL_HUMIDITY,
        FRAME_COL_BATTERY_VOLTAGE,
        FRAME_COL_BATTERY_STATUS,
        FRAME_COL_PCB_TEMP,
        FRAME_COL_HUMIDITY_PWM,
        FRAME_COL_TX_POWER,
        FRAME_COL_MAX_SUBFRAME_NO,
        FRAME_COL_SUBFRAME_NO,
        FRAME_COL_SUBFRAME,
        FRAME_COL_GPS_TIME,
        FRAME_COL_GPS_SATS,
        FRAME_COL_ECC,
        FRAME_COL_CORR,
        FRAME_COL_RANGE,
        FRAME_COL_FREQUENCY
    };

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_filterSerial_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_channel1_currentIndexChanged(int index);
    void on_channel2_currentIndexChanged(int index);
    void on_frames_cellDoubleClicked(int row, int column);
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void filterRow(int row);
    void filter();
    void frames_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void frames_sectionResized(int logicalIndex, int oldSize, int newSize);
    void framesColumnSelectMenu(QPoint pos);
    void framesColumnSelectMenuChecked(bool checked = false);
    void customContextMenuRequested(QPoint point);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_RADIOSONDEDEMODGUI_H
