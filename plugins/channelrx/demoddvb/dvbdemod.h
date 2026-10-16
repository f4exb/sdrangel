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

#ifndef INCLUDE_DVBDEMOD_H
#define INCLUDE_DVBDEMOD_H

#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include <QElapsedTimer>
#include <QMutex>
#include <QNetworkRequest>
#include <QTimer>
#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "dvbdemodbaseband.h"
#include "dvbdemodsettings.h"

namespace SWGSDRangel {
    class SWGDVBDemodReport;
    class SWGDVBDemodEPGEvent;
}

class DeviceAPI;
class QNetworkAccessManager;
class QNetworkReply;

class DVBDemod : public BasebandSampleSink, public ChannelAPI, public DTVRecordingSchedule {
public:
    class MsgConfigureDVBDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DVBDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureDVBDemod* create(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureDVBDemod(settings, settingsKeys, force);
        }

    private:
        DVBDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureDVBDemod(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    DVBDemod(DeviceAPI *deviceAPI);
    //! A search or scan under way is just stopped: the device isn't retuned back (its device set may be going too),
    //! nor the GUI (deleted first) told
    virtual ~DVBDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    int getChannelSampleRate() const override;
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    // The web API's calls are run in this object's thread (the main thread), waiting for it, as its timers change the
    // settings, the schedule and the search there: so they're only to be called from it, or a thread it never waits
    // for (as the web API's), else they'd deadlock
    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const DVBDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            DVBDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void getStatus(DVBDemodSink::Status& status) { m_basebandSink->getStatus(status); }
    DTVTSFifo *getTSFifo() { return m_basebandSink->getTSFifo(); }
    AudioFifo *getAudioFifo() { return m_basebandSink->getAudioFifo(); }
    void getAudioOutput(int& sampleRate, double& latency) const { m_basebandSink->getAudioOutput(sampleRate, latency); }
    //! The programme guide, from the transport stream, shared by every DVB demodulator through a cache file
    DVBDemodEPG *getEPG() { return m_basebandSink->getEPG(); }
    //! The services' interactive applications, and the carousels of the one played
    DTVApps *getApps() { return m_basebandSink->getApps(); }

    // Recording, to .ts files in the settings' directory. Manual recordings are of the multiplex or the service
    // selected, as the settings say. Scheduled recordings are of programmes in the guide
    bool startRecording(QString& error);
    void stopRecording();
    bool isRecording() const { return m_manualRecording >= 0; }
    std::vector<DVBDemodSink::Recording> getRecordings() { return m_basebandSink->getRecordings(); }
    //! A programme from the guide to record. False if it's already scheduled
    bool scheduleRecording(int networkId, int serviceId, int eventId) override;
    void cancelScheduledRecording(int networkId, int serviceId, int eventId) override;
    bool isScheduled(int networkId, int serviceId, int eventId) const override;
    QList<DTVScheduledRecording> getScheduledRecordings() const override { return m_settings.m_scheduledRecordings; }
    //! Record a programme's series: every programme of its service with its series CRID, each once (not repeats).
    //! False if it has no series CRID, or it's recorded already
    bool scheduleSeries(int networkId, int serviceId, int eventId) override;
    //! Stop recording a programme's series (the programmes scheduled from it stay scheduled)
    void cancelSeries(int networkId, int serviceId, int eventId) override;
    void cancelSeries(int networkId, int serviceId, const QString& seriesCrid) override;
    //! Scheduled recordings may retune to their service's multiplex
    bool recordRetune() const override { return m_settings.m_recordRetune; }
    //! The programme's series is being recorded
    bool isSeriesScheduled(int networkId, int serviceId, int eventId) const override;
    QList<DTVSeriesRecording> getSeriesRecordings() const override { return m_settings.m_seriesRecordings; }
    //! The service selected, or if automatic, the one the player's playing (it chose it), else (no player, as headless)
    //! the first TV service of the multiplex by channel number. 0 if none known. Recording and the reports take this,
    //! so they're of what's watched
    int currentServiceId() const;
    //! The service the GUI's player is playing, when the setting's automatic (0 when it stops). From the GUI's thread
    void setPlayingService(int serviceId) { m_playingServiceId = serviceId; }
    //! Play a service from the guide, retuning the device to its multiplex if it's on another. If the multiplex's
    //! frequency isn't known (the UK's NIT doesn't give DVB-T2 multiplexes'), the UHF channels are searched for it.
    //! False, with why, if it can't
    bool playService(int networkId, int serviceId, QString& error);
    //! A service on another satellite multiplex, whose polarisation (EN 300 468's: 0 linear horizontal, 1 linear
    //! vertical, 2 circular left, 3 circular right) isn't the one received: true, with both, if they're known and differ
    bool polarisationChange(int networkId, int serviceId, int& needed, int& current);
    //! The LNB's polarisation, as switched to (by the user, or their command), until a multiplex's received that says
    void setPolarisation(int polarisation) { m_polarisation = polarisation; }
    //! Searching the UHF channels for a service's multiplex
    bool isSearching() const { return m_search.m_active || m_scan.m_active; }
    //! What the search is doing, or why the last failed. Empty if neither
    QString getSearchText() const { return m_search.m_text; }
    //! Stop searching, returning to the multiplex searched from
    void cancelSearch();
    //! Scan for satellite transponders, the device stepped from from to to (downlink Hz, as the device's frequency),
    //! or just its band if they're 0: each signal tried, and those with a transport stream added to the guide. Then
    //! back to where it was. Progress and the outcome in getSearchText()
    bool startScan(qint64 from, qint64 to, QString& error);
    void cancelScan();
    bool isScanning() const { return m_scan.m_active; }
    int getScanTransponders() const { return (int) m_scan.m_found.size(); }
    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    // Timeouts, ms
    static constexpr qint64 m_retuneSettleMs = 1000;          //!< After a retune: SI decoded since is from the new frequency
    static constexpr qint64 m_retuneConfirmTimeoutMs = 5000;  //!< For the device to say it's at the frequency asked for
    static constexpr qint64 m_pendingRecordingTimeoutMs = 20000;  //!< For a recording's multiplex to be received
    static constexpr qint64 m_learnFrequencyMs = 3000;        //!< A multiplex received this long, locked, is at the frequency
    static constexpr qint64 m_searchNoSignalMs = 2500;        //!< A UHF channel with nothing to synchronise to, left after
    static constexpr qint64 m_searchMaxDwellMs = 10000;       //!< A UHF channel with no transport stream identified, left after
    static constexpr qint64 m_scanRetuneTimeoutMs = 5000;     //!< For the device to get to a sweep's step
    static constexpr qint64 m_scanSpectrumTimeoutMs = 3000;   //!< For a step's spectrum
    static constexpr qint64 m_scanBandTimeoutMs = 3000;       //!< For the sink to apply a signal's band
    static constexpr qint64 m_scanSettleMs = 2000;            //!< A signal tried: locked, its SI's taken as its from after this
    static constexpr qint64 m_scanNoRateMs = 4000;            //!< A signal tried: no symbol rate found in this
    static constexpr qint64 m_scanNoLockMs = 6000;            //!< A signal tried: no lock in this
    static constexpr qint64 m_recordRetuneGiveUpMs = 60000;   //!< Retuned for a recording, none started: back after this
    static constexpr qint64 m_scheduleJumpConfirmMs = 30000;  //!< A jump in the schedule's time, kept to this long, is believed
    // Others
    static constexpr qint64 m_retuneToleranceHz = 10000;      //!< The device this near the frequency asked for is there (rounded)
    static constexpr qint64 m_recordRetuneLeadSeconds = 120;  //!< Retuned for a recording due within this, it's stayed for
    static constexpr qint64 m_scheduleJumpSeconds = 120;      //!< The schedule's time this far from what's expected has jumped
    static constexpr int m_scheduleJumpChecks = 2;            //!< Checks a jump's kept to before it's believed (and the time above)
    //! Hz, a retune further than which, from the guide's, is to another transponder (they're 10 MHz or more apart)
    static constexpr qint64 m_guideRetuneTolerance = 1000000;
    // Hz: the part of a step's spectrum taken, either side of its centre, as a fraction of the sample rate (its edges are
    // filtered). More than half the step, so the steps overlap a little, and leave no gap that would split a signal in two
    static constexpr double m_sweepHalfWidth = 0.42;
    // Hz: the noise floor's measured over this either side, as its 10th percentile there: far enough to include the gaps
    // between transponders (which may fill the device's band), near enough to follow the LNB's response across its band
    static constexpr double m_sweepFloorSpan = 75e6;
    static constexpr double m_sweepFloorStep = 1e6;
    // A signal's this far over the floor (times, signal and noise over noise): S/N 6 dB. Well over the device's ripple across
    // its band (+-1.5 dB with a B210 at 34.5 MS/s), the floor's errors, and the noise, as each candidate costs a retune and a
    // search, a try of up to 10 s if it doesn't lock. Weaker, on Astra 2, were the other polarisation's transponders (S/N
    // 3.4 to 5.4 dB, rates measured all over, none locked), and 8PSK 2/3 needs 6.6 dB
    static constexpr double m_sweepThreshold = 4.98;  // 1 + 10^0.6

    //! Run f in object's thread, waiting for it. Only from a thread that thread never waits for (see above)
    static int inObjectThread(QObject *object, const std::function<int()>& f);
    //! Set a string in the SWG settings, reusing the QString if there is one
    static void setString(QString *existing, std::function<void(QString*)> setter, const QString& value);
    //! What service_type is, for the web API
    static QString serviceKind(int type);
    //! An event of the guide, for the web API: just its id, title and time, unless full
    static SWGSDRangel::SWGDVBDemodEPGEvent *webapiEPGEvent(const DVBDemodEPG::Event& event, int networkId, bool full, bool scheduled);

    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    DVBDemodBaseband* m_basebandSink;
    //! Changed only in this object's thread, so read there without the mutex. start(), in the device's DSP thread,
    //! reads them (and m_basebandSampleRate, m_centerFrequency) with it, as they're changed with it
    DVBDemodSettings m_settings;
    QMutex m_settingsMutex;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QTimer m_epgSaveTimer;
    QTimer m_scheduleTimer;
    QTimer m_tuneTimer;                     //!< Learns multiplexes' frequencies, and runs the search
    QElapsedTimer m_sinceRetune;            //!< Since the device or the channel was retuned
    bool m_retuneSettled;                   //!< The transport stream since is from the new frequency
    qint64 m_retuneTarget = 0;              //!< The device's frequency asked for by retuneDevice(), until it's there
    QElapsedTimer m_retuneRequested;        //!< Since it was asked for
    //! The satellite standard, symbol rate, code rate, roll-off and stream were set from the guide (the NIT) for a
    //! transponder it retuned to (the device's frequency for it): on a retune elsewhere, they're automatic again
    bool m_guideSettings = false;
    qint64 m_guideCenterFrequency = 0;
    int m_polarisation = -1;    //!< The LNB's, as switched to from the guide, -1 if not known

    // A search of the UHF channels for the multiplex of a service to play
    struct Search
    {
        bool m_active = false;
        int m_networkId = 0;
        int m_transportStreamId = 0;
        int m_serviceId = 0;
        QString m_name;
        std::vector<qint64> m_frequencies;  //!< Channel centres to try, Hz
        int m_index = 0;
        QElapsedTimer m_dwell;              //!< Since tuned to the channel being tried
        qint64 m_tuned = 0;                 //!< The channel being tried (the channel's frequency the device was put at)
        qint64 m_centerFrequency = 0;       //!< Device's, to return to if not found
        DVBDemodSettings m_settings;        //!< To return to if not found
        QString m_text;
    };
    Search m_search;

    //! A recording asked for with playing a service on another multiplex: started once that multiplex is received
    //! (its transport stream identified, after the retune or search), so none of the multiplex before is recorded
    struct PendingRecording
    {
        bool m_active = false;
        int m_networkId = 0;
        int m_transportStreamId = 0;
        int m_serviceId = 0;
        QElapsedTimer m_since;              //!< Since asked, or the search ended: given up after a while
    };
    PendingRecording m_pendingRecording;
    void checkPendingRecording();

    // A scan for satellite transponders
    struct ScanSignal
    {
        qint64 m_frequency;                 //!< Hz, downlink
        double m_width;                     //!< Hz, half power
        double m_levelDb;                   //!< Over the noise floor
    };
    struct ScanFound
    {
        qint64 m_frequency;
        int m_symbolRate;
        QString m_text;
    };
    struct Scan
    {
        bool m_active = false;
        qint64 m_from = 0;
        qint64 m_to = 0;
        std::vector<qint64> m_centres;      //!< The device's, to step to
        int m_centre = 0;
        qint64 m_tuned = 0;                 //!< Where the device was put: the step's centre, or the signal's
        bool m_surveying = false;           //!< Sweeping (the spectrum at this centre), else trying a signal
        // The sweep's spectrum, from m_from in bins of m_binHz: each bin's power summed over the steps that saw it
        std::vector<double> m_power;
        std::vector<int> m_seen;
        double m_binHz = 0.0;
        double m_minWidth = 0.0;            //!< Hz, of a signal
        bool m_bandSent = false;
        std::vector<ScanSignal> m_signals;
        int m_signal = 0;
        QElapsedTimer m_dwell;
        uint64_t m_bandGeneration = 0;      //!< Of the last band set: the sink's status gives it back once it's applied
        qint64 m_centerFrequency = 0;       //!< To return to
        DVBDemodSettings m_settings;
        std::vector<ScanFound> m_found;
        std::vector<ScanSignal> m_tried;    //!< Not found
    };
    Scan m_scan;
    uint64_t m_bandGeneration = 0;          //!< Of the last band set the sink

    int m_manualRecording;                  //!< Recording id, -1 if not recording
    std::atomic<int> m_playingServiceId{0}; //!< The player's service, 0 if none (cleared on a retune: it chooses again)
    std::map<qint64, int> m_scheduledRecordingIds;  //!< (network, service, event) of those recording -> recording id
    std::set<qint64> m_scheduledRecordingFailed;    //!< Those that couldn't be started, so it's only logged once

    void saveEPG();
    void checkSchedule();
    //! t, the time the schedule's run by, follows the time before (as measured here): a jump (from a multiplex with its
    //! clock wrong, or a bad TDT) isn't believed until it's been kept to for a while. Until then, false: nothing's
    //! started, ended or dropped by it
    bool scheduleTimeConfirmed(qint64 t);
    struct ScheduleClock
    {
        QElapsedTimer m_clock;
        qint64 m_time = 0;          //!< The time believed, s, when m_clock was at m_at, ms
        qint64 m_at = 0;
        bool m_jumped = false;      //!< To m_jumpTime at m_jumpAt, not yet believed
        qint64 m_jumpTime = 0;
        qint64 m_jumpAt = 0;
        int m_jumpChecks = 0;       //!< Since, that have kept to it
    };
    ScheduleClock m_scheduleClock;
    void retuned();
    //! The device to frequency (its centre), the retune taken to have settled once it's there (or not, after a while)
    bool retuneDevice(qint64 frequency);
    void resetReceivers();
    //! The multiplex being received is at this frequency (for the guide, when its NIT doesn't say)
    void learnFrequency(qint64 frequency);
    bool recordingInProgress() const { return (m_manualRecording >= 0) || !m_scheduledRecordingIds.empty(); }
    void checkTuning();
    void startSearch(const DVBDemodEPG::Service& target);
    void scanCentre();
    void addScanSpectrum(const DVBDemodSink::Status& status);
    void findScanSignals();
    void scanSignal();
    void checkScan();
    void endScan(const QString& text);
    void setRateSearchBand(double frequency, double width, bool spectrumOnly = false);
    //! The satellite standard, symbol rate, code rate, roll-off and stream automatic
    static void setSatelliteAutomatic(DVBDemodSettings& settings);
    //! The network of the service with the id: the one given (if >= 0, and it has the service), else the one received,
    //! else the first with it. -1 if none has it
    int serviceNetworkId(int serviceId, int networkId);
    void searchNext();
    void endSearch(bool found, const QString& text);
    void applyServiceSettings(const DVBDemodSettings& settings, const QStringList& settingsKeys);
    void setSchedule(const QList<DVBDemodSettings::ScheduledRecording>& schedule);
    void setSeries(const QList<DVBDemodSettings::SeriesRecording>& series);
    //! The series' new programmes, scheduled. Into schedule; true if any were (series then changed too)
    bool checkSeries(QList<DVBDemodSettings::ScheduledRecording>& schedule);
    quint64 m_seriesRevision = 0;           //!< The guide's revision series were last checked at
    //! A scheduled recording's service on another multiplex: retuned to (m_recordRetune), and back afterwards
    struct RecordingRetune
    {
        bool m_active = false;
        bool m_recorded = false;            //!< A recording started on the multiplex retuned to
        qint64 m_returnFrequency = 0;       //!< The device's, before
        DVBDemodSettings m_returnSettings;
        qint64 m_tunedFrequency = 0;        //!< The device's when recording: if it moves, the user's retuned it
        QElapsedTimer m_since;              //!< Since the retune
    };
    RecordingRetune m_recordingRetune;
    bool m_retuningForRecording = false;   //!< While playService() retunes for a recording (not the user)
    //! Retune to a scheduled recording's service: false if it can't now (recording, searching, scanning, ...)
    bool retuneForRecording(const DVBDemodSettings::ScheduledRecording& r);
    //! Back to the multiplex before a retune for recordings, once none's being recorded (or due)
    void checkRecordingReturn(const QList<DVBDemodSettings::ScheduledRecording>& schedule, qint64 t);
    QString recordingFileName(const QString& name, qint64 utc) const;
    QString serviceName(int serviceId) const;
    static qint64 scheduleKey(int networkId, int serviceId, int eventId) {
        return ((qint64) networkId << 32) | ((qint64) serviceId << 16) | (qint64) eventId;
    }

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiFormatT2Report(const DVBDemodSink::Status& status, SWGSDRangel::SWGDVBDemodReport *report);
    void webapiFormatSatelliteReport(const DVBDemodSink::Status& status, SWGSDRangel::SWGDVBDemodReport *report);
    void webapiFormatEPGReport(SWGSDRangel::SWGDVBDemodReport *report);
    void webapiFormatEPGLists(SWGSDRangel::SWGDVBDemodReport *report, std::vector<DVBDemodEPG::Service> services, int serviceId);
    void webapiFormatAppsReport(SWGSDRangel::SWGDVBDemodReport *report);
    void webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DVBDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DVBDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_DVBDEMOD_H
