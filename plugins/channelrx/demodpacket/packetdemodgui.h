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

#ifndef INCLUDE_PACKETDEMODGUI_H
#define INCLUDE_PACKETDEMODGUI_H

#include <QIcon>
#include <QAbstractListModel>
#include <QModelIndex>
#include <QProgressDialog>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "packetdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class PacketDemod;
class PacketDemodGUI;

namespace Ui {
    class PacketDemodGUI;
}
class PacketDemodGUI;

class PacketDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static PacketDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::PacketDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    PacketDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;

    PacketDemod* m_packetDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *menu;                        // Column select context menu

    explicit PacketDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~PacketDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void packetReceived(QByteArray packet);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_mode_currentIndexChanged(int value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_filterFrom_editingFinished();
    void on_filterTo_editingFinished();
    void on_filterPID_stateChanged(int state);
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void filterRow(int row);
    void filter();
    void packets_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void packets_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_PACKETDEMODGUI_H
