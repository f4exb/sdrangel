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

#ifndef INCLUDE_DABDEMODGUI_H
#define INCLUDE_DABDEMODGUI_H

#include <QAbstractListModel>
#include <QTableWidgetItem>
#include <QMenu>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "dabdemodsettings.h"
#include "dabdemod.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class DABDemod;
class DABDemodGUI;

namespace Ui {
    class DABDemodGUI;
}
class DABDemodGUI;

class DABDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DABDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::DABDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DABDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;

    DABDemod* m_dabDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;
    double m_channelFreq;

    QMenu *menu;                        // Column select context menu

    explicit DABDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~DABDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateEnsembleName(const QString& ensemble);
    int findProgramRowById(int id);
    void addProgramName(const DABDemod::MsgDABProgramName& program);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);
    void resetService();
    void clearProgram();
    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);

private slots:
    void audioSelect(const QPoint& p);
    void on_deltaFrequency_changed(qint64 value);
    void on_audioMute_toggled(bool checked);
    void on_volume_valueChanged(int value);
    void on_rfBW_valueChanged(int index);
    void on_filter_editingFinished();
    void on_clearTable_clicked();
    void on_programs_cellDoubleClicked(int row, int column);
    void on_channel_currentIndexChanged(int index);
    void on_findOnMap_clicked();
    void filterRow(int row);
    void filter();
    void programs_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void programs_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_DABDEMODGUI_H
