///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FREQSCANNERGUI_H
#define INCLUDE_FREQSCANNERGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "freqscanner.h"
#include "freqscannersettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class FreqScanner;
class FreqScannerGUI;
class QMenu;

namespace Ui {
    class FreqScannerGUI;
}
class FreqScannerGUI;

class FreqScannerGUI : public ChannelGUI {
    Q_OBJECT

public:
    static FreqScannerGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::FreqScannerGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    FreqScannerSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;

    FreqScanner* m_freqScanner;
    int m_basebandSampleRate;
    MessageQueue m_inputMessageQueue;

    QMenu *m_menu;

    explicit FreqScannerGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~FreqScannerGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void addRow(qint64 frequency, bool enabled, const QString& notes = "");
    void updateAnnotation(int row);
    void updateAnnotations();
    void updateChannelsList(const QList<FreqScannerSettings::AvailableChannel>& channels);
    void setAllEnabled(bool enable);

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction* createCheckableItem(QString& text, int idx, bool checked);

    enum Col {
        COL_FREQUENCY,
        COL_ANNOTATION,
        COL_ENABLE,
        COL_POWER,
        COL_ACTIVE_COUNT,
        COL_NOTES
    };

private slots:
    void on_channels_currentIndexChanged(int index);
    void on_deltaFrequency_changed(qint64 value);
    void on_channelBandwidth_changed(qint64 index);
    void on_scanTime_valueChanged(int value);
    void on_retransmitTime_valueChanged(int value);
    void on_tuneTime_valueChanged(int value);
    void on_thresh_valueChanged(int value);
    void on_priority_currentIndexChanged(int index);
    void on_measurement_currentIndexChanged(int index);
    void on_mode_currentIndexChanged(int index);
    void on_table_cellChanged(int row, int column);
    void table_customContextMenuRequested(QPoint pos);
    void table_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void table_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void on_startStop_clicked(bool checked = false);
    void on_addSingle_clicked();
    void on_addRange_clicked();
    void on_remove_clicked();
    void on_removeInactive_clicked();
    void on_up_clicked();
    void on_down_clicked();
    void on_clearActiveCount_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
};

#endif // INCLUDE_FREQSCANNERGUI_H
