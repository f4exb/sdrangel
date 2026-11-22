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

#ifndef INCLUDE_RADIOPAGINGMODGUI_H
#define INCLUDE_RADIOPAGINGMODGUI_H

#include <memory>
#include <vector>

#include <QDateTime>
#include <QTimer>

#include "audio/audiofifo.h"
#include "channel/channelgui.h"
#include "util/messagerecord.h"
#include "radiopagingwav.h"
#include "dsp/channelmarker.h"
#include "util/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "radiopagingmod.h"
#include "radiopagingmodsettings.h"
#include "gui/pagingcomposer.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class QAction;
class QModelIndex;
class QSortFilterProxyModel;
class PagingVoicePlayer;
class RadioPagingModMessageModel;

namespace Ui {
    class RadioPagingModGUI;
}

// GUI of the radio paging modulator, in sections: Settings (channel), Network (the defaults
// of the messages sent), Message (compose and send) and Queue (status and log)
class RadioPagingModGUI : public ChannelGUI {
    Q_OBJECT

public:
    static RadioPagingModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
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

private:
    Ui::RadioPagingModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RadioPagingModSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    RadioPagingMod* m_radioPagingMod;
    MovingAverageUtil<double, double, 2> m_channelPowerDbAvg;   // Short: transmissions are short
    // The messages, one row each from queued to their last event; the chips filter them
    RadioPagingModMessageModel *m_messageModel;
    QSortFilterProxyModel *m_messageFilter;
    PagingVoicePlayer *m_voicePlayer;
    QTimer m_columnsTimer;              //!< The columns are saved once they stop changing
    bool m_follow;                      //!< The table shows the message on air (until the user scrolls or selects)
    bool m_scrolling;                   //!< The table is scrolled by followOnAir, not by the user
    int m_onAirShown;                   //!< Row of the model shown on air, -1 if none
    quint64 m_localRefused;             //!< Requests refused in the window before they were posted to the channel
    QString m_lastLocalRefusal;
    void updateQueueStatus(const RadioPagingModSource::Status& status);
    void refused(const QString& error);
    void statusLinkActivated(const QString& link);
    void followOnAir();
    void messagesContextMenu(const QPoint& pos);
    void messagesHeaderMenu(const QPoint& pos);
    void messageClicked(const QModelIndex& index);
    void showMessage(const QModelIndex& index);
    void setupMessages();

    MessageQueue m_inputMessageQueue;

    explicit RadioPagingModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = nullptr);
    virtual ~RadioPagingModGUI();

    void blockApplySettings(bool block);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void displaySettings();
    void displayProtocolAvailability();
    void displayProtocol();
    void fillTimeZones();
    QDateTime flexTimeNow() const;
    void displayTime(bool flex);
    void displayNumbering(bool flex);
    //!< FLEX System Event Notification: the controls (FLEX network only) and, while it is sent, the frames left
    void displaySystemEvent(bool flex, bool network);
    void updateSystemEvent(const RadioPagingModSource::Status& status);
    //!< The event flags checked in the menu of the events, e10-e0
    unsigned systemEventFlags() const;
    //! The composer for the channel: its protocols, network mode, FLEX numbering, microphone
    void displayMessage();

    // Live voice page: the page goes out, the microphone is taken once the alert time is over and
    // its voice is passed to the transmitter as it comes, until End or the voice length
    AudioFifo m_micFifo;
    RadioPagingResampler m_micResampler;        //!< Audio input rate to the rate of the modulator
    int m_micRate = 0;
    bool m_micRecording = false;                //!< The microphone is taken
    quint64 m_micSent = 0;                      //!< Samples of the voice passed, at the rate of the modulator
    quint64 m_liveId = 0;                       //!< The live voice page being sent, 0 if none
    bool m_liveSeen = false;                    //!< The transmitter has started it
    bool m_liveWaiting = false;                 //!< Its request was posted, the id is not known yet
    void startMic();
    void stopMic();
    void readMic();
    void endLive();
    void updateLive(const RadioPagingModSource::Status& status);
    void showTx(const MessageRecord& record, const QString& status);
    int channelBandwidth() const;
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    void log(const QString& text);
    void send();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_channelMute_toggled(bool checked);
    void on_protocol_currentIndexChanged(int index);
    void on_mode_currentIndexChanged(int index);
    void on_deviation_valueChanged(int value);
    void on_gain_valueChanged(int value);
    void on_speed_currentIndexChanged(int index);
    void on_polarity_currentIndexChanged(int index);
    void on_collapse_valueChanged(int value);
    void on_roaming_toggled(bool checked);
    void on_ssidZone_valueChanged(int value);
    void on_ssidLocalId_valueChanged(int value);
    void on_ssidCountry_valueChanged(int value);
    void on_ssidFlags_valueChanged(int value);
    void on_flexTime_toggled(bool checked);
    void on_flexTimeReference_currentIndexChanged(int index);
    void on_flexTimeDaylightSaving_toggled(bool checked);
    void on_flexTimeSendZone_toggled(bool checked);
    void on_flexTimeEquivalentYear_toggled(bool checked);
    void on_flexTimeExtendedSeconds_valueChanged(int value);
    void on_flexNumbering_toggled(bool checked);
    void on_flexMaxMessageNumber_valueChanged(int value);
    void on_flexEventSend_clicked();
    void on_flexTimeZone_currentIndexChanged(int index);
    void on_flexTimeClock_toggled(bool checked);
    void on_flexTimeSet_editingFinished();
    void on_voiceSeconds_valueChanged(int value);
    void on_mixPocsag_toggled(bool checked);
    void on_mixShared_toggled(bool checked);
    void on_mixSpeed_currentIndexChanged(int index);
    void on_mixPolarity_currentIndexChanged(int index);
    void on_mixDeviation_valueChanged(int value);
    void on_mixFrames_valueChanged(int value);
    void on_mixSwitch512_valueChanged(int value);
    void on_mixSwitch1200_valueChanged(int value);
    void on_mixSwitch2400_valueChanged(int value);
    void on_voiceFiles_clicked();
    void micAudioSelect(const QPoint& p);
    void micDataReady();
    void on_clear_clicked();
    void on_abort_clicked();

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);

    void tick();
};

#endif // INCLUDE_RADIOPAGINGMODGUI_H
