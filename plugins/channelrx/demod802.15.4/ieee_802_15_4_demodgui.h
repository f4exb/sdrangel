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

#ifndef INCLUDE_IEEE_802_15_4_DEMODGUI_H
#define INCLUDE_IEEE_802_15_4_DEMODGUI_H

#include <QMenu>
#include <QTableWidgetItem>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "ieee_802_15_4_demodsettings.h"
#include "ieee_802_15_4_demod.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;

namespace Ui {
    class IEEE_802_15_4_DemodGUI;
}

class IEEE_802_15_4_DemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static IEEE_802_15_4_DemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::IEEE_802_15_4_DemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    IEEE_802_15_4_DemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    IEEE_802_15_4_Demod* m_demod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;
    bool m_loadingData;

    QMenu *m_framesMenu;                        // Column select context menu

    enum FrameCol {
        FRAME_COL_DATE_TIME,
        FRAME_COL_TYPE,
        FRAME_COL_SEQ,
        FRAME_COL_DST_PAN,
        FRAME_COL_DST_ADDR,
        FRAME_COL_SRC_PAN,
        FRAME_COL_SRC_ADDR,
        FRAME_COL_INFO,
        FRAME_COL_LENGTH,
        FRAME_COL_PAYLOAD,
        FRAME_COL_PSDU,
        FRAME_COL_FCS,
        FRAME_COL_POWER,
        FRAME_COL_SNR,
        FRAME_COL_LQI,
        FRAME_COL_FREQUENCY_OFFSET
    };

    explicit IEEE_802_15_4_DemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~IEEE_802_15_4_DemodGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    void displayPHYDependent();
    void frameReceived(const QByteArray& psdu, const QDateTime& dateTime, bool fcsOK, float frequencyOffset,
                       float lqi, float powerDB, float snr);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void checkSampleRate();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_phy_currentIndexChanged(int index);
    void on_samplesPerChip_valueChanged(int value);
    void on_rfBW_valueChanged(int value);
    void on_maxFrequencyOffset_valueChanged(int value);
    void on_threshold_valueChanged(int value);
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_udpFormat_currentIndexChanged(int index);
    void on_filterFrameType_currentIndexChanged(int index);
    void on_filterPANID_editingFinished();
    void on_filterAddress_editingFinished();
    void on_showBadFCS_toggled(bool checked);
    void on_useFileTime_toggled(bool checked);
    void on_logEnable_clicked(bool checked = false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_clearTable_clicked();
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

#endif // INCLUDE_IEEE_802_15_4_DEMODGUI_H
