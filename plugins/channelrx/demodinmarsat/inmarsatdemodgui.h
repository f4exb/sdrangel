///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef INCLUDE_INMARSATDEMODGUI_H
#define INCLUDE_INMARSATDEMODGUI_H

#include <QIcon>
#include <QAbstractListModel>
#include <QModelIndex>
#include <QProgressDialog>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QHash>
#include <QDateTime>
#include <QGeoCoordinate>

#include <inmarsatc_parser.h>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "inmarsatdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class InmarsatDemod;
class InmarsatDemodGUI;

namespace Ui {
    class InmarsatDemodGUI;
}
class InmarsatDemodGUI;

struct MessagePart {
    int m_part;
    int m_packet;
    bool m_continuation;
    QString m_text;
};

class MultipartMessage {
public:

    MultipartMessage(int id, std::map<std::string, std::string> params, const QDateTime& dateTime);
    void update(std::map<std::string, std::string> params, const QDateTime& dateTime);
    void addPart(const MessagePart& part);
    QString getMessage() const;
    int getParts() const { return m_parts.size(); }
    int getTotalParts() const { return m_parts[m_parts.size()-1].m_packet * 2; }
    QDateTime getDateTime() const { return m_dateTime; }
    bool getComplete() const { return getParts() == getTotalParts() && !m_parts[m_parts.size()-1].m_continuation; }
    int getId() const { return m_id; }
    QString getService() const { return m_service; }
    QString getPriority() const { return m_priority; }
    QString getAddress() const { return m_address; }
    QIcon *getAddressIcon() const { return m_icon; }
    float getLatitude() const { return m_addressCoordinates.size() > 0 ? m_latitude : m_messageCoordinates[0].latitude(); }
    float getLongitude() const { return m_addressCoordinates.size() > 0 ? m_longitude : m_messageCoordinates[0].longitude(); }
    QList<QGeoCoordinate>& getCoordinates() { return m_addressCoordinates.size() > 0 ? m_addressCoordinates : m_messageCoordinates; }

    static QString decodeAddress(QString messageType, QString addressHex, float *latitude = nullptr, float *longitude = nullptr, QList<QGeoCoordinate> *coordinates = nullptr, QIcon **icon = nullptr);

private:

    QDateTime m_dateTime;
    int m_id;
    QString m_service;
    QString m_priority;
    QString m_address;
    QIcon *m_icon;
    QList<MessagePart> m_parts;
    float m_latitude;
    float m_longitude;
    QList<QGeoCoordinate> m_addressCoordinates;
    QList<QGeoCoordinate> m_messageCoordinates;

    static QRegularExpression m_re;
    static const QStringList m_navAreas;
    static const QStringList m_navAreaFlags;

    void parseMessage();
};

class InmarsatDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static InmarsatDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    Ui::InmarsatDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    InmarsatDemodSettings m_settings;
    QList<QString> m_settingsKeys;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    InmarsatDemod* m_inmarsatDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *m_packetsMenu;                        // Column select context menu
    QMenu *m_messagesMenu;
    QStringList m_mapItems;
    bool m_loadingData;

    inmarsatc::frameParser::FrameParser m_parser;
    QHash<int, MultipartMessage *> m_messages;

    QRegularExpression m_typeRE;
    QRegularExpression m_messageRE;

    explicit InmarsatDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~InmarsatDemodGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    void packetReceived(const QByteArray& packet, QDateTime dateTime);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void updateMessageTable(MultipartMessage *message);
    void resizeTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, bool packets);

    void sendAreaToMapFeature(const QString& name, const QString& text, float latitude, float longitude, const QList<QGeoCoordinate>& coordinates);
    void clearAreaFromMapFeature(const QString& name);
    void clearAllFromMapFeature();

    void updateRegularExpressions();
    void filterRow(QTableWidget *table, int row, int typeCol, int messageCol);
    void filter();

    void decodeAppend(QString& decode, const QString& title, const std::string& variable, inmarsatc::frameParser::FrameParser::frameParser_result& frame);
    void decodeAppendFreqMHz(QString& decode, const QString& title, const std::string& variable, inmarsatc::frameParser::FrameParser::frameParser_result& frame);
    void decodeAppendHTML(QString& decode, const QString& title, const std::string& variable, inmarsatc::frameParser::FrameParser::frameParser_result& frame);
    QString toHTML(const std::string& string) const;
    QString toHTML(const QString& string) const;

    enum MessageCol {
        MESSAGE_COL_DATE,
        MESSAGE_COL_TIME,
        MESSAGE_COL_ID,
        MESSAGE_COL_SERVICE,
        MESSAGE_COL_PRIORITY,
        MESSAGE_COL_ADDRESS,
        MESSAGE_COL_PARTS,
        MESSAGE_COL_MESSAGE
    };

    enum PacketCol {
        PACKET_COL_DATE,
        PACKET_COL_TIME,
        PACKET_COL_SAT,
        PACKET_COL_MES,
        PACKET_COL_LES,
        PACKET_COL_TYPE,
        PACKET_COL_FRAME_NO,
        PACKET_COL_LCN,
        PACKET_COL_ULF,
        PACKET_COL_DLF,
        PACKET_COL_MSG_ID,
        PACKET_COL_PRIORITY,
        PACKET_COL_ADDRESS,
        PACKET_COL_MESSAGE,
        PACKET_COL_DECODE,
        PACKET_COL_DATA_HEX
    };

private slots:
    void on_packets_itemSelectionChanged();
    void on_messages_itemSelectionChanged();
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_equalizer_currentIndexChanged(int index);
    void on_rrcRolloff_valueChanged(int value);
    void on_pll_clicked(bool checked=false);
    void on_pllBW_valueChanged(int value);
    void on_ssBW_valueChanged(int value);
    void on_filterType_editingFinished();
    void on_filterMessage_editingFinished();
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_useFileTime_toggled(bool checked=false);
    void packets_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void packets_sectionResized(int logicalIndex, int oldSize, int newSize);
    void packetsColumnSelectMenu(QPoint pos);
    void packetsColumnSelectMenuChecked(bool checked = false);
    void packetsCustomContextMenuRequested(QPoint pos);
    void messages_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void messages_sectionResized(int logicalIndex, int oldSize, int newSize);
    void messagesColumnSelectMenu(QPoint pos);
    void messagesColumnSelectMenuChecked(bool checked = false);
    void messagesCustomContextMenuRequested(QPoint pos);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_INMARSATDEMODGUI_H
