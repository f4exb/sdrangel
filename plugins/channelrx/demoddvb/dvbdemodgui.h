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

#ifndef INCLUDE_DVBDEMODGUI_H
#define INCLUDE_DVBDEMODGUI_H

#include <atomic>
#include <deque>
#include <functional>
#include <vector>

#include <QElapsedTimer>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "dvbdemodsettings.h"
#include "dvbdemodsink.h"
#include "dtv/dtvvideoplayer.h"
#include "dtv/dtvappsdialog.h"
#include "dtv/dtvepgdialog.h"
#include "dtv/dtvguideoverlay.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class DVBDemod;
class GLVideoWidget;
class DVBDemodHbbTV;
class DTVStreamPlayback;

namespace Ui {
    class DVBDemodGUI;
}

class DVBDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static DVBDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    static const int m_bandwidths[6];

    //! EN 300 468's polarisations
    static QString polarisationName(int polarisation);
    //! The name of a language from its ISO 639-2 code
    static QString languageName(const QString& code);
    //! Narrowband interferers notched out, for the CFO measurement
    static QString notchText(const std::vector<double>& notches);
    //! A codec's name, short enough for the table
    static QString shortCodecName(const QString& name);

    Ui::DVBDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    DVBDemodSettings m_settings;
    QStringList m_settingsKeys;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;

    DVBDemod* m_dvbDemod;
    DTVVideoPlayer *m_player;
    uint32_t m_tickCount;
    bool m_videoFullScreen;
    GLVideoWidget *m_fullScreenTV;               //!< Window for full screen video, made when first needed
    QPointer<QProcess> m_polarisationProcess;    //!< The user's command switching the LNB's polarisation, while it runs
    //! The polarisation command the user's typed or agreed to run, this session: one that came with a preset or
    //! configuration (perhaps someone else's) is shown and confirmed before it's first run
    QString m_polarisationCommandConfirmed;
    DTVGuideOverlay *m_guideOverlay;        //!< Programme guide over full screen video
    QTimer m_pointerTimer;                       //!< Hides the mouse pointer over full screen video when it stops
    QTimer m_splitterTimer;                      //!< Saves the splitter's sizes once it stops moving
    std::atomic<GLVideoWidget*> m_videoScreen;   //!< Where the player shows video
    // A message is shown in place of the video when there is none
    QString m_videoMessage;
    bool m_serviceHasVideo;
    QList<DTVVideoPlayer::Service> m_services;
    int m_playingService;                   //!< Id of the service selected by the player, 0 for none yet
    QString m_overlayText;                  //!< Last shown, to show it again if the service's name or number changes
    QPointer<DTVEPGDialog> m_epgDialog;
    QPointer<DTVAppsDialog> m_appsDialog;
    DVBDemodHbbTV *m_hbbtv = nullptr;       //!< Runs the service's HbbTV applications, when they're enabled
    QRectF m_hbbtvVideoArea;                //!< Where the application wants the video, of its page, null for all
    int m_hbbtvChannelRequest = 0;          //!< A service the application's asked to play, 0 if none
    QString m_hbbtvServiceState;            //!< What it was last told of the service and multiplex
    QString m_hbbtvStatus;                  //!< Shown over the video
    DTVStreamPlayback *m_streamPlayback = nullptr;  //!< Plays an application's media (as iPlayer's), from the internet
    int m_mediaId = 0;                      //!< The application's media being played, 0 if none
    bool m_streamPlaying = false;           //!< Its stream's playing, in place of the broadcast (whose player's stopped)
    QString m_nowTitle;                     //!< On the service playing, to show the overlay when it changes
    bool m_serviceHasAudio;
    uint64_t m_lastFrames;                  //!< Video or audio frames, to see if any are arriving
    uint64_t m_lastAudioFrames;             //!< To show when audio is playing
    QElapsedTimer m_audioFramesTimer;       //!< Since audio frames last arrived
    bool m_audioPlaying;
    //! The player's counts at a moment, for its A/V problems in the last 10 s
    struct AVCounts
    {
        qint64 m_time;                      //!< ms, of m_avTimer
        uint64_t m_videoFrames;
        uint64_t m_lateFrames;
        uint64_t m_videoErrors;
        uint64_t m_audioMuted;
        uint64_t m_audioGaps;
        uint64_t m_audioUnderruns;
        uint64_t m_audioOverflows;
        uint64_t m_clockRestarts;
        uint64_t m_tsOverflows;
    };
    std::deque<AVCounts> m_avHistory;       //!< Oldest first, from the latest 10 s or more ago
    std::vector<uint64_t> m_avToolTipInputs; //!< The totals the A/V tooltips were made with, empty if none
    QElapsedTimer m_avTimer;
    int m_avServiceId = -1;
    QElapsedTimer m_framesTimer;            //!< Since the last arrived
    QList<int> m_plpIds;            //!< In the PLP combo box, after Auto
    QList<int> m_isiIds;            //!< The same, for DVB-S2's input streams, which it lists then
    //! Another multiplex, or standard: the PLPs and input streams listed were the last's
    void clearStreamList();
    bool m_listingISIs = false;     //!< The PLP combo box lists input streams
    uint64_t m_lastErrorCount = 0;  //!< RS or BCH failures, for those in each point of the scope's history
    int m_lastErrorStandard = -1;
    QString m_searchText;           //!< Last shown, of the demodulator's search for a multiplex
    QElapsedTimer m_statusMessageTimer; //!< Since a message (as the search's) replaced now and next in the status
    MessageQueue m_inputMessageQueue;

    explicit DVBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
    virtual ~DVBDemodGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void setTrackPreferences();
    void clearTracks();
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    // Rows of the measurements table. Those from StatFEC1 to StatFEC4 depend on the standard
    enum Stat { StatMode, StatModulation, StatMER, StatSNR, StatFEC1, StatFEC2, StatFEC3, StatFEC4, StatCFO, StatClock, StatTSRate,
        StatVideo, StatResolution, StatAudio, StatFrameRate, StatVideoDrops, StatAVSync, StatAudioDrops, StatAudioBuffer, StatTSDropped,
        NbStats };

    void setupStats();
    void setStat(Stat row, const QString& name, const QString& value, const QString& toolTip);
    void setStatValue(Stat row, const QString& name, const QString& value);
    void setStatToolTip(Stat row, const QString& toolTip);
    void setStatWarning(Stat row, bool warning);
    void displayStatus();
    void displayPlayerInfo(uint64_t tsOverflows);
    void displayAVStats(const DTVVideoPlayer::Info& info, uint64_t tsOverflows);
    void displayT2Status(const DVBDemodSink::Status& status);
    void displayScopeExtras(const DVBDemodSink::Status& status, double mer, double iterations, uint64_t errors);
    void displayPLPs(const dvbt2::Receiver::Status& status);
    void displaySatelliteStatus(const DVBDemodSink::Status& status);
    void displayISIs(const dvbs2::Receiver::Status& status);
    void updateStandardWidgets();
    void fillStreamList();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void displayNowNext();
    void displayRecordings();
    void defaultScanRange(double& from, double& to);
    void selectService(int networkId, int serviceId, QWidget *parent, const std::function<void(const QString&)>& failed);
    void switchPolarisation(int polarisation, const std::function<void(const QString&)>& done);
    void displayRecordButton(bool recording);
    void displaySimulcast();
    void displaySearch();
    void updateAbsoluteCenterFrequency();
    void startPlayer();
    void stopPlayer();
    void setVideoFullScreen(bool fullScreen);
    void showVideoMessage(const QString& message);
    QString serviceText(int id) const;
    void selectServiceItem();
    bool fromPlayer() const;
    void showServiceOverlay();
    void checkVideoReceived();
    bool eventFilter(QObject *obj, QEvent *event) override;

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_bandwidth_currentIndexChanged(int index);
    void on_standard_currentIndexChanged(int index);
    void on_plp_currentIndexChanged(int index);
    void on_priority_currentIndexChanged(int index);
    void on_symbolRate_editingFinished();
    void on_rollOff_currentIndexChanged(int index);
    void on_codeRate_currentIndexChanged(int index);
    void on_plScrambling_valueChanged(int value);
    void on_equaliser_toggled(bool checked);
    void on_udpEnabled_toggled(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_videoEnabled_toggled(bool checked);
    void on_service_currentIndexChanged(int index);
    void on_deinterlace_toggled(bool checked);
    void on_epg_clicked();
    void on_apps_clicked();
    void updateAppsDialog();
    void appsMenu(const QPoint& position);
    void updateHbbTV();                     //!< Running HbbTV applications or not, as the settings and player are
    void updateHbbTVService();
    void updateHbbTVGeometry();
    QRect hbbtvPageRect(const QWidget *screen) const;
    void hbbtvVideoArea(const QRectF& area);
    void hbbtvChannelChange(int networkId, int transportStreamId, int serviceId);
    void hbbtvRed();                        //!< The red button clicked
    void hbbtvKey(int key);                 //!< A coloured button clicked: its key (G, Y or B) given to the application
    void on_hbbtv_toggled(bool checked);
    void hbbtvMedia(const QString& command, int id, const QString& url, const QMap<QString, QString>& options, double position);
    void stopStreamPlayback();              //!< The application's media stopped, and the broadcast played again
    void displayRedButton();
    void on_scan_clicked();
    void on_simulcast_clicked();
    void on_record_toggled(bool checked);
    void recordMenu(const QPoint& position);
    void epgMenu(const QPoint& position);
    void epgServiceSelected(int networkId, int serviceId);
    void guideServiceSelected(int networkId, int serviceId);
    void on_hardwareDecoding_toggled(bool checked);
    void on_audioTrack_currentIndexChanged(int index);
    void on_subtitleTrack_currentIndexChanged(int index);
    void tracksChanged(const DTVVideoPlayer::Tracks& tracks);
    void on_avOffset_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_audioVolume_valueChanged(int value);
    void audioSelect(const QPoint& p);
    void servicesChanged(const QList<DTVVideoPlayer::Service>& services);
    void serviceSelected(int id, bool hasVideo, bool hasAudio);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onSplitterMoved(int pos, int index);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_DVBDEMODGUI_H
