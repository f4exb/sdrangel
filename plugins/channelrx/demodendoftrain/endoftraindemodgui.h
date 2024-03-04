///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
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

#ifndef INCLUDE_ENDOFTRAINDEMODGUI_H
#define INCLUDE_ENDOFTRAINDEMODGUI_H

#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "endoftraindemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class EndOfTrainDemod;
class EndOfTrainDemodGUI;

namespace Ui {
    class EndOfTrainDemodGUI;
}
class EndOfTrainDemodGUI;

class EndOfTrainDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static EndOfTrainDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::EndOfTrainDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    EndOfTrainDemodSettings m_settings;
    QStringList m_settingsKeys;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    EndOfTrainDemod* m_endoftrainDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *menu;                        // Column select context menu

    explicit EndOfTrainDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~EndOfTrainDemodGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    void packetReceived(const QByteArray& packet, const QDateTime& dateTime);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

    enum EndOfTrainCol {
        PACKETS_COL_DATE,
        PACKETS_COL_TIME,
        PACKETS_COL_CHAINING_BITS,
        PACKETS_COL_BATTERY_CONDITION,
        PACKETS_COL_TYPE,
        PACKETS_COL_ADDRESS,
        PACKETS_COL_PRESSURE,
        PACKETS_COL_BATTERY_CHARGE,
        PACKETS_COL_DISCRETIONARY,
        PACKETS_COL_VALVE_CIRCUIT_STATUS,
        PACKETS_COL_CONF_IND,
        PACKETS_COL_TURBINE,
        PACKETS_COL_MOTION,
        PACKETS_COL_MARKER_BATTERY,
        PACKETS_COL_MARKER_LIGHT,
        PACKETS_COL_ARM_STATUS,
        PACKETS_COL_CRC,
        PACKETS_COL_DATA_HEX
    };

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_filterFrom_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_useFileTime_toggled(bool checked=false);
    void filterRow(int row);
    void filter();
    void endoftrains_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void endoftrains_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_ENDOFTRAINDEMODGUI_H
