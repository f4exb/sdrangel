///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef INCLUDE_RADIOPAGINGDEMODGUI_H
#define INCLUDE_RADIOPAGINGDEMODGUI_H

#include <QElapsedTimer>
#include <QRegularExpressionMatch>
#include <QSet>
#ifdef QT_TEXTTOSPEECH_FOUND
#include <QTextToSpeech>
#endif

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/messagerecord.h"
#include "settings/rollupstate.h"

#include "radiopagingdemodsettings.h"
#include "radiopagingdemodsink.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class RadioPagingDemod;
class QLabel;
class QCheckBox;
class QTableWidget;
class ScopeVis;

namespace Ui {
    class RadioPagingDemodGUI;
}

// Lean GUI: protocol and speed checkboxes, decoder status and the latest messages. The full
// history, filters, rules and re-interpretation are in the message center
class RadioPagingDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static RadioPagingDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; }
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }
    virtual QString getTitle() const { return m_settings.m_title; }
    virtual QColor getTitleColor() const { return m_settings.m_rgbColor; }
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    static const int m_maxRows = 500;

    Ui::RadioPagingDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RadioPagingDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    RadioPagingDemod* m_radioPagingDemod;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;
    ScopeVis *m_scopeVis;
    bool m_loadingData = false;         //!< A log is being read: its messages are shown, not notified again
    QSet<QString> m_mapItems;           //!< Names of the items sent to the Map
#ifdef QT_TEXTTOSPEECH_FOUND
    QTextToSpeech *m_speech = nullptr;
#endif

    explicit RadioPagingDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~RadioPagingDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    //!< The message is shown and given to the notifications (not when loading a log)
    void showRecord(const MessageRecord& record);
    void checkNotifications(const MessageRecord& record);
    //!< The field of the record that a notification matches
    //!< The texts of the field of the record a notification is tried on: the address and the members of a group message
    static QStringList matchTexts(const MessageRecord& record, int field);
    //!< ${address}, ${message}, ${protocol}, ${type}, ${channel} and ${0}, ${1}... of the match, replaced in the string
    static QString subStrings(const MessageRecord& record, const QRegularExpressionMatch& match, const QString& string);
    void enableSpeechIfNeeded();
    void speechNotification(const QString& speech);
    void commandNotification(const QString& command);
    void sendToMap(const MessageRecord& record, float latitude, float longitude);
    void clearFromMap();
    //!< Hides the rows whose address does not match the filter; a row shown is hidden or not at once
    void applyAddressFilter();
    bool addressMatchesFilter(const QStringList& addresses) const;
    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);
    void updateStatus();
    //!< The protocols: their status rows in the settings, their settings and their details
    void layoutProtocols();
    //!< The polarity of the network being received by a protocol (in sync, else received last)
    int currentPolarity(const RadioPagingDemodSink::Status& status, int protocol);
    //!< The group slot bars of FLEX, one for each polarity, with the state of each slot
    void updateGroupBars(const RadioPagingDemodSink::Status& status);
    static QString groupSlotToolTip(const RadioPagingFlexDecoder::GroupSlot& slot, int pol, const RadioPagingDemodSink::Status& status);

    // FLEX: between the frames of a transmission the decoder hunts for the next sync 1. The
    // display keeps SYNC until that sync is due: a frame (1.875 s, STD-43A 3.2) after the start
    // of the last one, plus one status period (the display sees the start that much late)
    static const qint64 m_flexFrameMs = 1875;
    static const qint64 m_statusPeriodMs = 200;
    QElapsedTimer m_statusClock;
    qint64 m_flexFrameStart = -1;       //!< m_statusClock when the last frame was first seen, -1 none
    bool m_flexWasSync = false;
    int m_currentPol[3] = {0, 0, 0};    //!< The polarity of the live line of each protocol
    QLabel *m_codepageLabel[3];
    //!< The editor of the custom codepage of a protocol (0 POCSAG, 1 FLEX, 2 GSC): a character for each code of the
    //!< default table, the direction, export and import
    void editCustomCodepage(int protocol);
    //!< The codepage lists with the Custom entry of each protocol
    void fillCodepageLists();
    QWidget *m_flexGroupBar[2] = {nullptr, nullptr};    //!< The 16 group slots of each polarity
    QLabel *m_flexGroupCell[2][16];

    // The status of each protocol in the settings: what is received (speeds or mode) and its polarity; FLEX also the
    // cycle and frame and the network time indicator
    QLabel *m_receivingLabel[3] = {nullptr, nullptr, nullptr};
    QLabel *m_polarityLabel[3] = {nullptr, nullptr, nullptr};
    QLabel *m_berLabel[3] = {nullptr, nullptr, nullptr};
    QLabel *m_offsetLabel[3] = {nullptr, nullptr, nullptr};     //!< The carrier offset of each protocol
    QLabel *m_flexFrameLabel = nullptr;
    QLabel *m_flexClockLabel = nullptr;
    int m_switchState[3] = {-1, -1, -1};    //!< The colour state of each protocol switch (to restyle on a change only)
    // The details: a table for each protocol, a row for each value, the normal and inverted networks in columns
    QTableWidget *m_detailsTable[3] = {nullptr, nullptr, nullptr};
    //!< FLEX: the bit error rate of each phase, a box each; for each polarity, a row for phases A and B, one for C and D
    QLabel *m_flexPhaseLabel[2][2] = {{nullptr, nullptr}, {nullptr, nullptr}};
    //!< FLEX: the network words (time, zone, local ID, country) of each polarity, a chip while fresh
    QLabel *m_flexWordLabel[2][4] = {{nullptr, nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr, nullptr}};

    // FLEX: when (the time of the signal processed, from its samples) and in which frame each kind of network word
    // was last received, for each polarity, -1 never. Its age, received or not since: in the last frame, in this cycle
    // (128 frames, 4 minutes), in this hour (the least a network sends the time, STD-43A 3.7.2), older
    enum FlexWord { WordDateTime, WordZone, WordSsid1, WordSsid2, WordCount };
    enum FlexWordAge { AgeNever, AgeFrame, AgeCycle, AgeHour, AgeOld };
    qint64 m_flexWordMs[2][WordCount] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}};
    int m_flexWordFrame[2][WordCount] = {{-1, -1, -1, -1}, {-1, -1, -1, -1}};
    int m_flexLastFrame[2] = {-1, -1};      //!< The last frame (cycle * 128 + frame) of each polarity whose words were taken
    qint64 m_flexLastFrameMs[2] = {-1, -1}; //!< When it was taken
    void stampFlexWords(const RadioPagingDemodSink::Status& status);
    FlexWordAge flexWordAge(int pol, FlexWord word, qint64 signalMs) const;
    //!< A network word in a cell of the details, by its age: a green chip in the last frame, a light green chip in this
    //!< cycle, the text colour in this hour, grey after
    //!< assumed: the time mode is assumed (not confirmed by the time words of a cycle), a light yellow chip
    void setNetworkDetail(FlexWord word, int pol, const QString& text, FlexWordAge age, bool assumed = false);
    //!< The kind of the network time, its precision and when it was received, for its tooltips
    static QString flexTimeKind(const RadioPagingFlexDecoder::NetworkTime& t, const RadioPagingFlexDecoder::FrameReport& frame);
    void setDetail(int protocol, int row, int pol, const QString& text, bool live, const QColor& colour = QColor());
    void updateDetails(const RadioPagingDemodSink::Status& status, int flexPol, bool flexHeld);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int value);
    void on_pocsagDeviation_valueChanged(int value);
    void on_flexDeviation_valueChanged(int value);
    void on_flexMaxMessageNumber_valueChanged(int value);
    void on_pocsagEnabled_clicked(bool checked);
    void on_flexEnabled_clicked(bool checked);
    void on_gscEnabled_clicked(bool checked);
    void on_gscVoiceTimeout_valueChanged(int value);
    void on_gscDeviation_valueChanged(int value);
    void on_gscAudio_toggled(bool checked);
    void on_gscVoiceRecord_toggled(bool checked);
    void on_gscVoiceFiles_clicked();
    void voiceDirectorySelect(const QPoint& p);
    void displayVoiceRecordToolTip();
    void on_volume_valueChanged(int value);
    void audioSelect(const QPoint& p);
    void on_clearTable_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_valueChanged(int value);
    void on_udpFormat_currentIndexChanged(int index);
    void on_notifications_clicked();
    void on_logEnable_clicked(bool checked);
    void on_logFilename_clicked();
    void on_logOpen_clicked();
    void on_filterAddress_editingFinished();
    void on_channel1_currentIndexChanged(int index);
    void on_channel2_currentIndexChanged(int index);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_RADIOPAGINGDEMODGUI_H
