///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_ADSBDEMODTRACKER_H
#define INCLUDE_ADSBDEMODTRACKER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QList>
#include <QTimer>
#include <QRandomGenerator>
#include <QDateTime>
#include <QPointF>
#include <QRegularExpression>
#include <QPointer>

#include "util/message.h"
#include "util/messagequeue.h"
#include "util/azel.h"
#include "util/movingaverage.h"
#include "util/openaip.h"
#include "util/osndb.h"
#include "util/aircraft3dmodels.h"
#include "util/ourairportsdb.h"

#include "util/flightinformation.h"

#include "adsbaircraft.h"
#include "adsbdemodsettings.h"

namespace SWGSDRangel {
    class SWGMapAnimation;
}

class ADSBDemod;
class ADSBDemodWorker;
class QNetworkAccessManager;
class QNetworkReply;

// Rows of the statistics table, which the tracker fills in and the GUI displays
enum StatsRow {
    ADSB_FRAMES,
    MODE_S_FRAMES,
    TOTAL_FRAMES,
    ADSB_RATE,
    MODE_S_RATE,
    TOTAL_RATE,
    DATA_RATE,
    CORRELATOR_MATCHES,
    PERCENT_VALID,
    PREAMBLE_FAILS,
    CRC_FAILS,
    TYPE_FAILS,
    INVALID_FAILS,
    ICAO_FAILS,
    RANGE_FAILS,
    ALT_FAILS,
    AVERAGE_CORRELATION,
    TC_0,
    TC_1_4,
    TC_5_8,
    TC_9_18,
    TC_19,
    TC_20_22,
    TC_24,
    TC_28,
    TC_29,
    TC_31,
    TC_UNUSED,
    DF0,
    DF4,
    DF5,
    DF11,
    DF16,
    DF17,
    DF18,
    DF19,
    DF20_21,
    DF22,
    DF24,
    MAX_RANGE,
    MAX_ALTITUDE,
    MAX_RATE
};

// One aircraft's decoded state, on its way to the GUI. The state carries its own
// record of which table cells changed, so the GUI repaints only those
struct ADSBAircraftUpdate {
    ADSBAircraft m_state;
    bool m_isNew = false;
};

Q_DECLARE_METATYPE(ADSBAircraftUpdate)

// Decodes ADS-B and Mode-S frames and tracks the aircraft they come from.
//
// This runs on the channel's worker thread rather than in the GUI, so that a busy
// sky doesn't compete with drawing the user interface, and so that the channel still
// decodes, reports and feeds aggregators when SDRangel is run headless as a server.
//
// Frames arrive here directly from the demodulator; those that are to be exported in
// Beast format or logged are passed on to ADSBDemodWorker, which shares this thread.
class ADSBDemodTracker : public QObject {
    Q_OBJECT

public:
    class MsgConfigureADSBDemodTracker : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const ADSBDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureADSBDemodTracker* create(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureADSBDemodTracker(settings, settingsKeys, force);
        }

    private:
        ADSBDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureADSBDemodTracker(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    explicit ADSBDemodTracker(ADSBDemod *adsbDemod);
    ~ADSBDemodTracker();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    // The worker that exports frames in Beast format and writes the .csv log. It
    // shares this thread, so frames can be handed to it directly
    void setExportWorker(ADSBDemodWorker *worker) { m_exportWorker = worker; }
    void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }

    static QString getDataDir();

public slots:
    // Called from the GUI thread, so these are invoked through the event loop
    void setPosition(float latitude, float longitude, float altitude);
    void setTrackAircraft(quint32 icao);
    void setLoadingData(bool loading)
    {
        if (loading && !m_loadingData) {
            m_replayAircraftReported.clear();
        }
        m_loadingData = loading;
    }
    void deleteAllAircraft();
    void clearCoverage();
    void resetStats();
    void reloadDatabases();
    void requestFlightInformation(const QString& flight);

public:

signals:
    // Everything that changed since the last flush, about four times a second
    void aircraftUpdated(const QList<ADSBAircraftUpdate>& updates);
    void aircraftRemoved(const QList<quint32>& icaos);
    void statsUpdated(const QHash<int, QVariant>& stats);
    void coverageUpdated(int band, const QVector<QPointF>& polygon);
    void qnhEstimate(int qnh);
    // For the frames-per-second chart
    void rates(double adsbRate, double modesRate, int activeAircraft);
    // The GUI owns the voice, the selection and the photo panel
    void speechRequest(const QString& speech);
    void highlightRequest(quint32 icao);
    void targetRequest(quint32 icao);
    void photoRefresh(quint32 icao);

public slots:
    // Called when the worker thread starts. Anything with thread affinity - timers,
    // sockets, network access - must be created here rather than in the constructor
    void startWork();

private slots:
    void handleInputMessages();
    void flightInformationUpdated(const FlightInformation::Flight& flight);
    // Sends everything that changed to the GUI in one go
    void flush();
    // Ages out aircraft that have not been heard from, and reports to the web API
    void tick();

private:
    ADSBDemod *m_adsbDemod;
    ADSBDemodWorker *m_exportWorker;
    MessageQueue *m_messageQueueToGUI;
    MessageQueue m_inputMessageQueue;
    ADSBDemodSettings m_settings;

    qint64 m_centerFrequency;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;

    QTimer *m_flushTimer;
    QTimer *m_tickTimer;
    QSet<ADSBAircraft *> m_dirtyAircraft;
    // Aircraft waiting to be sent to the Map, with any animations that have built up for
    // them. We hear several frames a second per aircraft but only its position moves, so
    // sending on every one just makes the Map redraw the same thing repeatedly
    QHash<ADSBAircraft *, QList<SWGSDRangel::SWGMapAnimation *> *> m_pendingMapUpdates;
    QList<quint32> m_removedAircraft;
    QHash<int, QVariant> m_stats;
    bool m_statsChanged;
    QDateTime m_statsTime;

    void get3DModel(ADSBAircraft *aircraft);
    void get3DModelBasedOnCategory(ADSBAircraft *aircraft);
    QHash<int, ADSBAircraft *> m_aircraft;  // Hashed on ICAO
    QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInfo;
    QSharedPointer<const QHash<QString, AircraftRouteInformation *>> m_routeInfo; // Hashed on callsign
    AzEl m_azEl;                        // Position of station
    ADSBAircraft *m_trackAircraft;          // Aircraft we want to track in Channel Report
    MovingAverageUtil<float, double, 10> m_correlationAvg;
    MovingAverageUtil<float, double, 10> m_correlationOnesAvg;
    MovingAverageUtil<float, double, 100> m_qnhAvg;
    //!< The QNH last reported to the GUI, and 0 when none has been. Rounding a moving
    //!< average flickers across the boundary between two whole hectopascals, and every
    //!< flicker used to run a settings update through the GUI, the channel, the
    //!< baseband, the sink, the worker and back into this tracker - so only a change
    //!< that has moved clear of the last reported value is sent.
    int m_qnhReported;
    static const int m_maxRangeDeg = 5;
    QList<float> m_maxRange[2];
    Airspace m_coverageAirspace[2];
    qint64 m_rangeFails;
    qint64 m_altFails;
    QDateTime m_frameRateTime;
    qint64 m_adsbFrameRateCount;
    qint64 m_modesFrameRateCount;
    qint64 m_totalBytes;
    float m_maxRangeStat;
    float m_maxAltitudeStat;
    float m_maxRateState;
    qint64 m_dfStats[32];
    qint64 m_tcStats[32];
    QRandomGenerator m_random;
    // The model tables live in sdrbase now, shared with the Aircraft feature - see
    // Aircraft3DModels. Only the emitter category fallback below is still ours, because
    // no other protocol transmits a category.
    bool m_loadingData;
    // Aircraft for which this CSV replay has already emitted its downstream track
    // boundary (an Aircraft report marker or a direct Map item reset). Cleared at the
    // start of every file, and deliberately independent of m_aircraft: the demodulator
    // can retain live state while a replay starts a new downstream track.
    QSet<quint32> m_replayAircraftReported;
    static const char m_idMap[];
    static const QString m_categorySetA[];
    static const QString m_categorySetB[];
    static const QString m_categorySetC[];
    static const QString m_emergencyStatus[];
    static const QString m_flightStatuses[];
    static const QString m_hazardSeverity[];
    static const QString m_fomSources[];
    static const QString m_nacvStrings[];
    void clearFromMap(const QString& name);
    void sendToMap(ADSBAircraft *aircraft,
                   QList<SWGSDRangel::SWGMapAnimation *> *animations,
                   const QDateTime& aircraftStateDateTime = QDateTime());
    static QList<SWGSDRangel::SWGMapAnimation *> *copyAnimations(QList<SWGSDRangel::SWGMapAnimation *> *animations);
    void queueMapUpdate(ADSBAircraft *aircraft, QList<SWGSDRangel::SWGMapAnimation *> *animations);
    void discardMapUpdate(ADSBAircraft *aircraft);
    ADSBAircraft *getAircraft(int icao, bool &newAircraft);
    void atcCallsign(ADSBAircraft *aircraft);
    void callsignToFlight(ADSBAircraft *aircraft);
    bool calcAirTemp(ADSBAircraft *aircraft);
    void handleADSB(
        const QByteArray data,
        const QDateTime dateTime,
        float correlation,
        float correlationOnes,
        unsigned crc,
        bool updateModel);
    void decodeID(const QByteArray& data, QString& emitterCategory, QString& callsign);
    bool decodeGroundspeed(const QByteArray& data, float& v, float& h);
    bool decodeAirspeed(const QByteArray& data, bool& tas, int& as, bool& hdgValid, float& hdg);
    bool decodeVerticalRate(const QByteArray& data, int& verticalRate);
    void updateAircraftPosition(ADSBAircraft *aircraft, double latitude, double longitude, const QDateTime& dateTime);
    bool validateGlobalPosition(double latitude, double longitude, bool countFailure);
    bool validateLocalPosition(double latitude, double longitude, bool surfacePosition, bool countFailure);
    bool decodeGlobalPosition(int f, const double cprLat[2], const double cprLong[2], const QDateTime cprTime[2], double& latitude, double& longitude, bool countFailure);
    bool decodeLocalPosition(int f, double cprLat, double cprLong, bool onSurface, const QDateTime& dateTime, const ADSBAircraft *aircraft, double& latitude, double& longitude, bool countFailure);
    void decodeCpr(const QByteArray& data, int& f, double& latCpr, double& lonCpr) const;
    bool decodeAltitude(const QByteArray& data, int& altFt) const;
    void decodeModeSAltitude(const QByteArray& data, const QDateTime dateTime, ADSBAircraft *aircraft);
    void decodeModeS(const QByteArray data, const QDateTime dateTime, int df, ADSBAircraft *aircraft);
    void decodeCommB(const QByteArray data, const QDateTime dateTime, int df, ADSBAircraft *aircraft, bool &updatedCallsign);
    void decodeRABroadcast(const QByteArray data, const QDateTime dateTime, ADSBAircraft *aircraft);
    QList<SWGSDRangel::SWGMapAnimation *> *animate(QDateTime dateTime, ADSBAircraft *aircraft);
    SWGSDRangel::SWGMapAnimation *gearAnimation(QDateTime startDateTime, bool up);
    SWGSDRangel::SWGMapAnimation *gearAngle(QDateTime startDateTime, bool flat);
    SWGSDRangel::SWGMapAnimation *flapsAnimation(QDateTime startDateTime, float currentFlaps, float flaps);
    SWGSDRangel::SWGMapAnimation *slatsAnimation(QDateTime startDateTime, bool retract);
    SWGSDRangel::SWGMapAnimation *rotorAnimation(QDateTime startDateTime, bool stop);
    SWGSDRangel::SWGMapAnimation *engineAnimation(QDateTime startDateTime, int engine, bool stop);
    // Whether the model the map draws for this aircraft has a named animation, so it is
    // only asked to play what it can - see Aircraft3DModels::hasAnimation()
    bool modelHasAnimation(const ADSBAircraft *aircraft, const QString &animation) const;
    void checkStaticNotification(ADSBAircraft *aircraft);
    void checkDynamicNotification(ADSBAircraft *aircraft);
    void commandNotification(ADSBAircraft *aircraft, const QString &command);
    QString subAircraftString(ADSBAircraft *aircraft, const QString &string);
    void removeAircraft(QHash<int, ADSBAircraft *>::iterator& i, ADSBAircraft *aircraft);
    ADSBAircraft *findAircraftByFlight(const QString& flight);
    int roundTo50Feet(int alt);
    int squawkDecode(int code) const;
    int gillhamToFeet(int n) const;
    int grayToBinary(int gray, int bits) const;
    void sendAircraftReport();
    void clearOldHeading(ADSBAircraft *aircraft, const QDateTime& dateTime, float newTrack);
    void updateQNH(const ADSBAircraft *aircraft, float qnh);
    void setCallsign(ADSBAircraft *aircraft, const QString& callsign);
    void initCoverageMap();
    void clearCoverageMap();
    void updateCoverageMap(float azimuth, float elevation, float distance, float altitude);
    void updateDFStats(int df);
    bool updateTCStats(int tc, int row, int low, int high);
    int countActiveAircraft();

    // Aircraft imported from the OpenSky Network, rather than heard directly
    QNetworkAccessManager *m_importNetworkManager;
    QTimer *m_importTimer;
    QPointer<QNetworkReply> m_importTokenReply;
    QPointer<QNetworkReply> m_importStatesReply;
    QString m_importAccessToken;
    QDateTime m_importAccessTokenExpiry;
    int m_importRequestGeneration;
    bool m_importAuthenticationRetry;

    FlightInformation *m_flightInformation;

    void initFlightInformation();
    static QString dataTimeToShortString(QDateTime dt);
    void applyImportSettings();
    bool importCredentialsConfigured() const;
    bool importAccessTokenValid() const;
    void invalidateImportAuthentication();
    void requestImportAccessToken();
    void sendImportRequest();
    void handleImportTokenReply(QNetworkReply *reply);
    void handleImportNetworkReply(QNetworkReply *reply);
    void handleImportReply(QNetworkReply *reply);
    void import();

    bool handleMessage(const Message& message);
    void applySettings(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force = false);
    void markDirty(ADSBAircraft *aircraft) { m_dirtyAircraft.insert(aircraft); }
    void setStat(int row, const QVariant& value);
};

#endif // INCLUDE_ADSBDEMODTRACKER_H
