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

#include <QDebug>
#include <QStandardPaths>
#include <QNetworkAccessManager>

#include "dsp/dspcommands.h"
#include "maincore.h"

#include "util/osndb.h"

#include "adsbdemod.h"
#include "adsbdemodreport.h"
#include "adsbdemodtracker.h"
#include "adsbdemodworker.h"

MESSAGE_CLASS_DEFINITION(ADSBDemodTracker::MsgConfigureADSBDemodTracker, Message)

ADSBDemodTracker::ADSBDemodTracker(ADSBDemod *adsbDemod) :
    m_adsbDemod(adsbDemod),
    m_exportWorker(nullptr),
    m_messageQueueToGUI(nullptr),
    m_centerFrequency(0),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(0),
    m_flushTimer(nullptr),
    m_tickTimer(nullptr),
    m_statsChanged(false),
    m_trackAircraft(nullptr),
    m_qnhReported(0),
    m_rangeFails(0),
    m_altFails(0),
    m_adsbFrameRateCount(0),
    m_modesFrameRateCount(0),
    m_totalBytes(0),
    m_maxRangeStat(0.0f),
    m_maxAltitudeStat(0.0f),
    m_maxRateState(0.0f),
    m_loadingData(false),
    m_importNetworkManager(nullptr),
    m_importTimer(nullptr),
    m_importRequestGeneration(0),
    m_importAuthenticationRetry(false),
    m_flightInformation(nullptr)
{
    qRegisterMetaType<ADSBAircraftUpdate>("ADSBAircraftUpdate");
    qRegisterMetaType<QList<ADSBAircraftUpdate>>("QList<ADSBAircraftUpdate>");
    qRegisterMetaType<QHash<int, QVariant>>("QHash<int,QVariant>");
    qRegisterMetaType<QList<quint32>>("QList<quint32>");
    qRegisterMetaType<QVector<QPointF>>("QVector<QPointF>");

    for (int i = 0; i < 32; i++)
    {
        m_dfStats[i] = 0;
        m_tcStats[i] = 0;
    }
    m_frameRateTime = QDateTime::currentDateTime();
    m_statsTime = m_frameRateTime;

    initCoverageMap();
}

ADSBDemodTracker::~ADSBDemodTracker()
{
    // Anything still on the map belongs to aircraft that are about to go
    for (ADSBAircraft *aircraft : m_aircraft) {
        clearFromMap(aircraft->m_icaoHex);
    }
    delete m_flushTimer;
    delete m_tickTimer;
    delete m_importTimer;
    delete m_importNetworkManager;
    delete m_flightInformation;
    for (auto animations : m_pendingMapUpdates)
    {
        if (animations)
        {
            qDeleteAll(*animations);
            delete animations;
        }
    }
    m_pendingMapUpdates.clear();
    qDeleteAll(m_aircraft);
    m_aircraft.clear();
    m_inputMessageQueue.clear();
}

QString ADSBDemodTracker::getDataDir()
{
    // Where the aircraft and airport databases and the 3D models live
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    return locations[0];
}

void ADSBDemodTracker::startWork()
{
    // Build the shared 3D model tables now rather than letting the first aircraft report
    // pay for it. Cheap when another plugin has already done it.
    Aircraft3DModels::instance();

    // Created here so they belong to the thread that will use them
    m_flushTimer = new QTimer();
    m_tickTimer = new QTimer();
    m_importTimer = new QTimer();
    m_importNetworkManager = new QNetworkAccessManager();
    connect(m_importTimer, &QTimer::timeout, this, &ADSBDemodTracker::import);
    connect(m_importNetworkManager, &QNetworkAccessManager::finished,
            this, &ADSBDemodTracker::handleImportNetworkReply);
    connect(m_flushTimer, &QTimer::timeout, this, &ADSBDemodTracker::flush);
    connect(m_tickTimer, &QTimer::timeout, this, &ADSBDemodTracker::tick);
    // Four times a second is well above the rate at which aircraft move on screen,
    // and far below the rate at which frames arrive
    m_flushTimer->start(250);
    m_tickTimer->start(1000);

    applyImportSettings();
    initFlightInformation();

    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &ADSBDemodTracker::handleInputMessages);
    // Settings and frames can arrive before the thread has started running
    handleInputMessages();
}

// Send everything that has changed since the last time to the GUI
void ADSBDemodTracker::flush()
{
    if (!m_removedAircraft.isEmpty())
    {
        emit aircraftRemoved(m_removedAircraft);
        m_removedAircraft.clear();
    }

    if (!m_dirtyAircraft.isEmpty())
    {
        QList<ADSBAircraftUpdate> updates;
        updates.reserve(m_dirtyAircraft.size());

        for (ADSBAircraft *aircraft : m_dirtyAircraft)
        {
            ADSBAircraftUpdate update;
            update.m_state = *aircraft;
            update.m_isNew = aircraft->m_isNew;
            updates.append(update);

            // The copy carries the flags; ours start again
            aircraft->clearDirtyCells();
            aircraft->m_isNew = false;
            aircraft->m_positionChanged = false;
            aircraft->m_trailCleared = false;
            aircraft->m_positionUpdates.clear();
        }
        m_dirtyAircraft.clear();
        emit aircraftUpdated(updates);
    }

    if (!m_pendingMapUpdates.isEmpty())
    {
        for (auto it = m_pendingMapUpdates.cbegin(); it != m_pendingMapUpdates.cend(); ++it) {
            sendToMap(it.key(), it.value());
        }
        m_pendingMapUpdates.clear();
    }

    if (m_statsChanged)
    {
        emit statsUpdated(m_stats);
        m_statsChanged = false;
    }
}

void ADSBDemodTracker::setStat(int row, const QVariant& value)
{
    if (m_stats.value(row) != value)
    {
        m_stats[row] = value;
        m_statsChanged = true;
    }
}

// Once a second: age out aircraft we have stopped hearing, work out frame rates and
// report the current picture for the web API
void ADSBDemodTracker::tick()
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 nowSecs = now.toSecsSinceEpoch();

    QHash<int, ADSBAircraft *>::iterator i = m_aircraft.begin();
    while (i != m_aircraft.end())
    {
        ADSBAircraft *aircraft = i.value();

        if (nowSecs - aircraft->m_updateTime.toSecsSinceEpoch() >= m_settings.m_removeTimeout) {
            removeAircraft(i, aircraft);
        } else {
            // A resolution advisory is re-broadcast every second or so while it is
            // active, so one that stops arriving is over - and the clear-of-conflict
            // indication only lingers briefly after it was raised. Queued to the map
            // on the transition: no frame will arrive to carry the change otherwise
            if (   (aircraft->m_raActive || aircraft->m_raTerminated)
                && aircraft->m_raDateTime.isValid()
                && (aircraft->m_raDateTime.secsTo(now) >= 10))
            {
                aircraft->m_raActive = false;
                aircraft->m_raTerminated = false;
                aircraft->m_tcasAlert = false;
                queueMapUpdate(aircraft, nullptr);
            }
            ++i;
        }
    }

    sendAircraftReport();

    qint64 ms = m_frameRateTime.msecsTo(now);
    if (ms > 0)
    {
        double s = ms / 1000.0;
        double adsbRate = m_adsbFrameRateCount / s;
        double modesRate = m_modesFrameRateCount / s;
        double totalRate = adsbRate + modesRate;

        setStat(ADSB_RATE, adsbRate);
        setStat(MODE_S_RATE, modesRate);
        setStat(TOTAL_RATE, totalRate);
        if (totalRate > m_maxRateState)
        {
            m_maxRateState = (float) totalRate;
            setStat(MAX_RATE, m_maxRateState);
        }
        setStat(DATA_RATE, (m_totalBytes * 8 / 1000) / s);

        m_frameRateTime = now;
        m_adsbFrameRateCount = 0;
        m_modesFrameRateCount = 0;
        m_totalBytes = 0;

        emit rates(adsbRate, modesRate, countActiveAircraft());
    }
}

// The databases are loaded by the GUI and shared read-only
void ADSBDemodTracker::reloadDatabases()
{
    m_aircraftInfo = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    qDebug() << "ADSBDemodTracker::reloadDatabases:" << (m_aircraftInfo ? m_aircraftInfo->size() : 0) << "aircraft";

    // Aircraft already being tracked may now have details, so look them up again
    for (ADSBAircraft *aircraft : m_aircraft)
    {
        if (m_aircraftInfo->contains(aircraft->m_icao))
        {
            aircraft->m_aircraftInfo = m_aircraftInfo->value(aircraft->m_icao);
            markDirty(aircraft);
        }
    }
}

void ADSBDemodTracker::requestFlightInformation(const QString& flight)
{
    if (m_flightInformation) {
        m_flightInformation->getFlightInformation(flight);
    } else {
        qDebug() << "ADSBDemodTracker::requestFlightInformation: no flight information service - is the API key set?";
    }
}

void ADSBDemodTracker::setPosition(float latitude, float longitude, float altitude)
{
    const bool moved = (m_azEl.getLocationSpherical().m_latitude != latitude)
                    || (m_azEl.getLocationSpherical().m_longitude != longitude);

    m_azEl.setLocation(latitude, longitude, altitude);

    if (moved)
    {
        // The coverage outline is centred on the station, so start it again from here
        initCoverageMap();
        for (int i = 0; i < 2; i++) {
            emit coverageUpdated(i, m_coverageAirspace[i].m_polygon);
        }
    }
}

void ADSBDemodTracker::setTrackAircraft(quint32 icao)
{
    m_trackAircraft = m_aircraft.value((int) icao, nullptr);
}

void ADSBDemodTracker::deleteAllAircraft()
{
    qDebug() << "ADSBDemodTracker::deleteAllAircraft:" << m_aircraft.size() << "aircraft";

    QHash<int, ADSBAircraft *>::iterator i = m_aircraft.begin();

    while (i != m_aircraft.end())
    {
        ADSBAircraft *aircraft = i.value();
        removeAircraft(i, aircraft);
    }
    // Tell the GUI now rather than waiting for the next flush
    flush();
}

void ADSBDemodTracker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool ADSBDemodTracker::handleMessage(const Message& message)
{
    if (MsgConfigureADSBDemodTracker::match(message))
    {
        MsgConfigureADSBDemodTracker& cfg = (MsgConfigureADSBDemodTracker&) message;

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;

        // Beast export and .csv logging
        if (m_exportWorker) {
            m_exportWorker->getInputMessageQueue()->push(new ADSBDemodReport::MsgReportADSB(report));
        }
        handleADSB(report.getData(), report.getDateTime(), report.getPreambleCorrelation(),
                   report.getCorrelationOnes(), report.getCRC(), true);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;

        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        m_deviceCenterFrequency = notif.getCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemodTracker::applySettings(const ADSBDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    const bool wasManualQNH = m_settings.m_manualQNH;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    // Coming off manual QNH, the spin box holds whatever was typed, so the next estimate
    // has to be reported even if it is within the hysteresis of the last one we sent
    if (wasManualQNH && !m_settings.m_manualQNH) {
        m_qnhReported = 0;
    }

    if (m_importTimer) {
        applyImportSettings();
    }
}
