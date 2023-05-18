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

#ifndef INCLUDE_DSCDEMODGUI_H
#define INCLUDE_DSCDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/aprsfi.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "dscdemod.h"
#include "dscdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class DSCDemod;
class DSCDemodGUI;

namespace Ui {
    class DSCDemodGUI;
}
class DSCDemodGUI;

class DSCDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DSCDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::DSCDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DSCDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    DSCDemod* m_dscDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    APRSFi *m_aprsFi;
    QMenu *m_menu;                        // Column select context menu
    QStringList m_mapItems;

    enum MessageCol {
        MESSAGE_COL_RX_DATE,
        MESSAGE_COL_RX_TIME,
        MESSAGE_COL_FORMAT,
        MESSAGE_COL_ADDRESS,
        MESSAGE_COL_ADDRESS_COUNTRY,
        MESSAGE_COL_ADDRESS_TYPE,
        MESSAGE_COL_ADDRESS_NAME,
        MESSAGE_COL_CATEGORY,
        MESSAGE_COL_SELF_ID,
        MESSAGE_COL_SELF_ID_COUNTRY,
        MESSAGE_COL_SELF_ID_TYPE,
        MESSAGE_COL_SELF_ID_NAME,
        MESSAGE_COL_SELF_ID_RANGE,
        MESSAGE_COL_TELECOMMAND_1,
        MESSAGE_COL_TELECOMMAND_2,
        MESSAGE_COL_RX,
        MESSAGE_COL_TX,
        MESSAGE_COL_POSITION,
        MESSAGE_COL_DISTRESS_ID,
        MESSAGE_COL_DISTRESS,
        MESSAGE_COL_NUMBER,
        MESSAGE_COL_TIME,
        MESSAGE_COL_COMMS,
        MESSAGE_COL_EOS,
        MESSAGE_COL_ECC,
        MESSAGE_COL_ERRORS,
        MESSAGE_COL_VALID,
        MESSAGE_COL_RSSI
    };

    explicit DSCDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~DSCDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void messageReceived(const DSCMessage& message, int errors, float rssi);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked);
    void createMenuOpenURLAction(QMenu* tableContextMenu, const QString& text, const QString& url, const QString& arg);
    void createMenuFindOnMapAction(QMenu* tableContextMenu, const QString& text, const QString& target);
    void sendAreaToMapFeature(const QString& name, const QString& address, const QString& text);
    void clearAreaFromMapFeature(const QString& name);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_filterInvalid_clicked(bool checked=false);
    void on_filterColumn_currentIndexChanged(int index);
    void on_filter_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_feed_clicked(bool checked=false);
    void on_feed_rightClicked(const QPoint &point);
    void filterRow(int row);
    void filter();
    void messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messages_sectionResized(int logicalIndex, int oldSize, int newSize);
    void columnSelectMenu(QPoint pos);
    void columnSelectMenuChecked(bool checked = false);
    void customContextMenuRequested(QPoint point);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void aprsFiDataUpdated(const QList<APRSFi::AISData>& data);
};

#endif // INCLUDE_DSCDEMODGUI_H

