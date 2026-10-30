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

#include <QTextDocumentFragment>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
#include <cmath>

#include <QDebug>
#include <QDataStream>
#include <QJsonDocument>
#include <QHostInfo>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>

#include "SWGMapItem.h"
#include "SWGMapCoordinate.h"
#include "SWGMapAnimation.h"

#include "feature/feature.h"
#include "maincore.h"
#include "util/units.h"
#include "util/acarstextformat.h"

#include "util/airlines.h"
#include "util/sidstar.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "util/aircraft3dmodels.h"
#include "util/aircraftenginesound.h"
#include "util/selcaldb.h"

#include "aircrafttracker.h"

MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgConfigureTracker, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgSelect, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgReport, Message)

AircraftTracker::MsgReport::~MsgReport()
{
    delete m_report.m_mapItem;
}
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgDeleteAll, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgAcarsBlock, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgResetStatistics, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgResync, Message)
MESSAGE_CLASS_DEFINITION(AircraftTracker::MsgWatchFlight, Message)

AircraftTracker::AircraftTracker(Feature *feature) :
    m_feature(feature)
{
    qRegisterMetaType<AircraftDisplay>("AircraftDisplay");
    qRegisterMetaType<FlightDisplay>("FlightDisplay");
    qRegisterMetaType<DocumentEvent>("DocumentEvent");
    qRegisterMetaType<AtcEvent>("AtcEvent");
    qRegisterMetaType<QList<AircraftDisplay>>("QList<AircraftDisplay>");
    qRegisterMetaType<QList<FlightDisplay>>("QList<FlightDisplay>");
    qRegisterMetaType<QList<DocumentEvent>>("QList<DocumentEvent>");
    qRegisterMetaType<QList<AtcEvent>>("QList<AtcEvent>");
    qRegisterMetaType<WeatherEvent>("WeatherEvent");
    qRegisterMetaType<QList<WeatherEvent>>("QList<WeatherEvent>");
    qRegisterMetaType<QList<quint64>>("QList<quint64>");
    qRegisterMetaType<QList<qint64>>("QList<qint64>");
    qRegisterMetaType<QList<float>>("QList<float>");

    for (int i = 0; i < CHART_COUNTS; i++) {
        m_chartCounts[i] = 0;
    }

    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &AircraftTracker::handleInputMessages);
}

AircraftTracker::~AircraftTracker()
{
    saveDatabase();
    closeDatabase();
    qDeleteAll(m_aircraft);
}

// Runs on the tracker thread once it starts
void AircraftTracker::startWork()
{
    // As in the ADS-B tracker: get the shared 3D model tables built off the critical path
    Aircraft3DModels::instance();

    m_statistics.m_sessionStart = QDateTime::currentDateTime();
    m_statistics.m_session = AircraftStatistics::Scope();
    m_allTimeSince = m_statistics.m_sessionStart;
    m_sessionHeard.clear();
    m_sessionRecorded.clear();

    m_aircraftInfo = OsnDB::getAircraftInformationByReg();
    m_aircraftInfoByIcao = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    m_selcalCodes = SELCALDB::getCodesByRegistration();
    m_navAids = OpenAIP::getNavAids();
    m_airports = OurAirportsDB::getAirportsByIdent();
    // The ACARS parser resolves ICAO codes to airport names through a table of its own,
    // which the demodulator fills in when IT starts. Documents are re-rendered here, at
    // load, which happens before any demodulator has run and would happen even with none
    // present - so without this every re-rendered document would read EGLL where it once
    // read EGLL (London Heathrow Airport). Through the library's once-only initialiser,
    // never by assignment: ACARS demodulator workers read and set the same global from
    // their own threads, and QSharedPointer assignment is not atomic.
    acarsInitAirportInfo();
    m_waypoints = Waypoints::getWaypoints();

    // On this thread, not in the constructor: a socket belongs to the thread that
    // created it, and everything that touches these runs here
    m_feedSocket = new QUdpSocket(this);
    m_feedAirframesTcpSocket = new QTcpSocket(this);
    m_feedAvdelphiTcpSocket = new QTcpSocket(this);
    updateFeedConnections();

    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, &QTimer::timeout, this, &AircraftTracker::flush);
    m_flushTimer->start(300);

    m_removalTimer = new QTimer(this);
    connect(m_removalTimer, &QTimer::timeout, this, &AircraftTracker::removeOldAircraft);
    m_removalTimer->start(60*1000);

    m_chartTimer = new QTimer(this);
    connect(m_chartTimer, &QTimer::timeout, this, &AircraftTracker::updateChartCounts);
    m_chartTimer->start(10*1000);
    m_chartRateTime = QDateTime::currentDateTime();

    if (openDatabase())
    {
        // Brings the all time records and the airframe count with it - see loadDatabase()
        loadDatabase();
    }
}

void AircraftTracker::handleInputMessages()
{
    Message* message;
    while ((message = m_inputMessageQueue.pop()))
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool AircraftTracker::handleMessage(const Message& message)
{
    if (MsgReport::match(message))
    {
        MsgReport& report = (MsgReport&) message;
        processReport(report.getReport());
        return true;
    }
    else if (MsgConfigureTracker::match(message))
    {
        const MsgConfigureTracker& cfg = (const MsgConfigureTracker&) message;
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else if (MsgAcarsBlock::match(message))
    {
        const MsgAcarsBlock& block = (const MsgAcarsBlock&) message;
        acarsBlockReceived(block.getBlock(), block.getDateTime(), block.getFrequency(),
                           block.getLevel(), block.getProtocol());
        return true;
    }
    else if (MsgDeleteAll::match(message))
    {
        deleteAll();
        // Straight to disk rather than waiting for the periodic save. Nothing is left in
        // memory to lose, the user has just asked for everything to go, and until the
        // save runs the database still holds all of it - so the size the statistics
        // report would be unchanged, and closing SDRangel in between would bring it all
        // back. Not inside deleteAll(): that is also how changing database clears the
        // old session out of memory, and saving there would write the emptied state over
        // the database being left behind.
        if (m_db && !m_loadingDatabase) {
            saveDatabase();
        }
        return true;
    }
    else if (MsgSelect::match(message))
    {
        MsgSelect& msg = (MsgSelect&) message;
        // Straight through to the GUI: what is selected is a property of the tables,
        // which live there
        emit selectRequested((int) msg.getKind(), msg.getValue());
        return true;
    }
    else if (MsgResetStatistics::match(message))
    {
        MsgResetStatistics& msg = (MsgResetStatistics&) message;
        if (msg.getAllTime())
        {
            m_statistics.m_allTime = AircraftStatistics::Scope();
            m_statistics.m_firstStart = QDateTime::currentDateTime();
            m_allTimeSecondsBase = 0;
            m_allTimeSince = m_statistics.m_firstStart;
            // Without this the aircraft count would come straight back, since it is
            // counted from the table rather than held as a number
            resetSeen();
            // Anything heard since the reset still belongs to the new all time figures,
            // so let this session's airframes be counted again
            m_sessionRecorded.clear();
            // The records are rows now, so clearing them in memory is only half of it.
            // saveStatistics() writes only valid records, so the next save empties the
            // table; without the dirty flag that save might not happen at all.
            m_dbDirty = true;
            m_recordMapState = RecordUnreported;
        }
        else
        {
            m_statistics.m_session = AircraftStatistics::Scope();
            m_statistics.m_sessionStart = QDateTime::currentDateTime();
            m_sessionHeard.clear();
        }
        m_statisticsDirty = true;
        return true;
    }
    else if (MsgResync::match(message))
    {
        resync();
        return true;
    }
    else if (MsgWatchFlight::match(message))
    {
        const MsgWatchFlight& watch = (const MsgWatchFlight&) message;
        TrackedFlight *previous = flightById(m_watchedFlight);
        m_watchedFlight = watch.getFlightId();
        TrackedFlight *flight = flightById(m_watchedFlight);
        // The map draws the watched flight's track rather than the current one, so both
        // the aircraft that is no longer watched and the one now watched need theirs sent
        for (TrackedFlight *f : { previous, flight })
        {
            if (f && f->m_aircraft)
            {
                f->m_aircraft->m_mapTrackChanged = true;
                m_dirtyAircraft.insert(f->m_aircraft);
            }
        }
        // The routes follow the selection, so the flight being left has its taken off
        // the map and the one now selected has its put on
        if (previous != flight)
        {
            removeRoutes(previous);
            sendRoutes(flight);
        }
        if (flight) {
            emitProfile(flight);
        }
        return true;
    }
    return false;
}

void AircraftTracker::applySettings(const AircraftSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    bool atcLabelsChanged = settingsKeys.contains("atcLabels") && (settings.m_atcLabels != m_settings.m_atcLabels);
    // The callsign style is part of the label too - atcLabel() writes the flight through
    // flightLabel(), which is what turns BAW123 into SPEEDBIRD 123 - so switching it has
    // to refresh the Map for exactly the same reason toggling the ATC button does
    bool atcCallsignsChanged = settingsKeys.contains("atcCallsigns")
        && (settings.m_atcCallsigns != m_settings.m_atcCallsigns);
    bool liveryChanged = settingsKeys.contains("favourLivery")
        && (settings.m_favourLivery != m_settings.m_favourLivery);
    // The icon is part of the map item, so switching icon styles has to refresh the Map
    // just like a label change does
    bool liveryIconsChanged = settingsKeys.contains("useLiveryIcons")
        && (settings.m_useLiveryIcons != m_settings.m_useLiveryIcons);
    // A forced configuration replaces the settings wholesale and may carry no key list
    // at all, which is how a preset arrives - and the database is opened in startWork()
    // before that happens, on the default path. Comparing the values rather than trusting
    // the key list is what lets a preset's own database be opened.
    bool databaseChanged = (force || settingsKeys.contains("databaseFilename"))
        && (settings.m_databaseFilename != m_settings.m_databaseFilename);
    QString oldDatabasePath = databaseChanged ? databasePath(m_settings) : QString();

    const bool statisticsArrived = settingsKeys.contains("statistics") || force;
    // Which rule fired for an aircraft is remembered by its INDEX in the list, so
    // editing the list silently changes what those indices mean: a new rule that lands
    // on the index of one that has already fired would never fire itself. Compared by
    // content rather than by the key list, because a forced configuration carries no
    // keys at all, and re-firing every rule on an unchanged preset load would be its own
    // bug - the aircraft would all announce themselves again.
    const QString notificationsBefore = notificationSignature(m_settings);
    const bool coverageWasShown = m_settings.m_displayCoverage;
    const quint32 coverageWasHidden = m_settings.m_coverageHiddenProtocols;
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
    // Drawn or taken off the Map as the button is toggled. Sent from here rather than
    // waiting for the next flush, so the map responds to the click rather than to the
    // next aircraft heard - which on a quiet band can be minutes.
    if (m_settings.m_displayCoverage != coverageWasShown)
    {
        if (m_settings.m_displayCoverage) {
            sendCoverageToMap();
        } else {
            removeCoverageFromMap();
        }
    }
    else if (m_settings.m_displayCoverage && (m_settings.m_coverageHiddenProtocols != coverageWasHidden))
    {
        // Each protocol draws or takes off its own, so all of them is simplest
        sendCoverageToMap();
    }
    if (notificationSignature(m_settings) != notificationsBefore)
    {
        for (TrackedAircraft *item : m_aircraft) {
            item->m_notifiedRules.clear();
        }
    }

    // A changed host invalidates the cached DNS resolution, or the feed would keep
    // sending to the address it looked up for the previous one
    m_feedAirframesResolvedHost.clear();
    m_feedAvdelphiResolvedHost.clear();
    updateFeedConnections();

    // The records live in the database now. A preset written before that still carries
    // them, so they are imported once - only when the database had none, which is what
    // loadStatistics() reports through m_statisticsLoaded. After the next save they are
    // in the database and the blob is never read again.
    const bool resetRequested = settingsKeys.contains("statistics")
                             && settings.m_statistics.isEmpty();
    // Wait for a blob with something in it. The GUI's constructor forces a configuration
    // through with default settings BEFORE the framework calls deserialize(), so keying
    // this on "the first settings to arrive" consumed the one chance to load on an empty
    // blob, and the stored records were then never read - the all time figures started
    // again from nothing on every launch.
    const bool haveStored = !m_settings.m_statistics.isEmpty();
    if (statisticsArrived && ((!m_statisticsLoaded && haveStored) || resetRequested))
    {
        m_statisticsLoaded = haveStored;
        if (haveStored) {
            qDebug() << "AircraftTracker: importing the all time statistics from the preset"
                     << "into the session database, which is where they live now";
        }
        AircraftStatistics stored = deserializeStatistics(m_settings.m_statistics);
        // Only the all time side is stored, so keep everything this session has counted
        stored.m_session = m_statistics.m_session;
        stored.m_sessionStart = m_statistics.m_sessionStart;
        if (!stored.m_firstStart.isValid())
        {
            // Nothing stored: either this is the first run, or a reset has just set the
            // moment the all time figures start from, which must not be discarded here
            stored.m_firstStart = m_statistics.m_firstStart.isValid()
                                ? m_statistics.m_firstStart : m_statistics.m_sessionStart;
        }
        m_allTimeSecondsBase = stored.m_allTime.m_seconds;
        m_allTimeSince = (m_allTimeSecondsBase == 0) && m_statistics.m_firstStart.isValid()
                       ? m_statistics.m_firstStart : m_statistics.m_sessionStart;
        // The aircraft count is the one figure the blob cannot hold - see the note on
        // AircraftStatistics. It comes from the database, which may not be open yet on
        // the first pass, in which case the next flush picks it up
        stored.m_allTime.m_distinctAircraft = seenCount();
        m_statistics = stored;
        m_statisticsDirty = true;
        m_dbDirty = true;       // So the import is written out
    }

    if (databaseChanged) {
        changeDatabase(oldDatabasePath);
    }

    // Refresh the labels of aircraft already on the Map. Only active aircraft are on
    // it, so archived ones must not be sent here - that would put the whole history
    // back on the Map every time settings are applied.
    //
    // Pass-through aircraft are included, which flush() deliberately does NOT do. The
    // reasoning that holds there does not hold here. flush() runs on every report, and
    // a basic item built by this feature would keep replacing the richer one ADS-B is
    // sending, so while ADS-B is talking it stands back. This is a one-off refresh after
    // the user toggled a setting, and skipping pass-through aircraft on the assumption
    // that "their next ADS-B report will relabel them" is only true for aircraft ADS-B
    // is STILL hearing. One that went quiet a few seconds ago gets skipped here and
    // never receives another report, so its label stayed stale indefinitely - which is
    // exactly the reported symptom of only some labels changing.
    //
    // The cost is small and self-correcting: an aircraft ADS-B is actively sending gets
    // a plainer popup for one update, restored on its next report a fraction of a second
    // later. The icon, the heading and the 3D model are all preserved, because
    // sendToMap() re-sends the model remembered from the pass-through.
    // Choosing again is only meaningful for models we picked ourselves; a pass-through
    // model belongs to the demodulator, which will send its own next report anyway
    if (liveryChanged)
    {
        for (TrackedAircraft *item : m_aircraft)
        {
            if (item->m_modelIsOurs || !item->m_liveryIcon.isEmpty())
            {
                if (item->m_modelIsOurs) {
                    item->m_model3D = "";
                }
                item->m_modelIsOurs = false;
                item->m_liveryIcon = "";
                update3DModel(item);
            }
        }
    }

    if (atcLabelsChanged || atcCallsignsChanged || liveryChanged || liveryIconsChanged || force)
    {
        for (TrackedAircraft *item : m_aircraft)
        {
            if (item->m_active && item->m_positionValid) {
                sendToMap(item);
            }
        }
    }
}

QString AircraftTracker::regFor(const TrackedAircraft *item) const
{
    return !item->m_registration.isEmpty() ? item->m_registration
        : (item->m_icao ? QString("%1").arg(item->m_icao, 6, 16, QChar('0')).toUpper() : QString());
}

// Identity keys are namespaced so an ICAO hex string can never collide with a
// registration or a flight number
static QString icaoKey(quint32 icao) { return QString("I%1").arg(icao, 6, 16, QChar('0')); }
static QString regKey(const QString& reg) { return "R" + reg; }
static QString flightKey(const QString& flight) { return "F" + flight; }

// Give an identity to this aircraft, folding in any other record that already held it.
// A registration and an ICAO address each belong to exactly one airframe, so two records
// answering to the same one ARE the same airframe - an ACARS registration-only record and
// an ADS-B ICAO-only record, tied together the moment a message carries both. The key was
// simply re-pointed at the survivor before, leaving the other record in the list holding
// its flights, documents and messages, and unreachable by any key.
//
// Deliberately not used for a flight number, which is not an identity: it is reused day
// after day and by different airframes, and the caller has its own narrower rule for it.
void AircraftTracker::claimKey(TrackedAircraft *item, const QString& key)
{
    TrackedAircraft *other = m_byKey.value(key);
    if (other && (other != item)) {
        absorbAircraft(item, other);
    }
    m_byKey.insert(key, item);
    // The identity has just strengthened, so what it is recognised by in the all time
    // table may have changed with it - see recordSeen()
    m_sessionRecorded.remove(item->m_id);
}

// A flight key stays registered for as long as the aircraft does, so asking when the
// AIRCRAFT was last heard says nothing about the callsign: an airframe heard a minute ago
// under today's callsign would keep every historical one it has ever flown looking just
// as fresh, and claim a report belonging to whoever is flying that number now.
QDateTime AircraftTracker::flightLastSeen(const TrackedAircraft *item, const QString& flight)
{
    QDateTime when;
    for (const TrackedFlight *f : item->m_flights)
    {
        if (!flightMatches(f, flight) || !f->m_lastSeen.isValid()) {
            continue;
        }
        if (!when.isValid() || (f->m_lastSeen > when)) {
            when = f->m_lastSeen;
        }
    }
    return when;
}

// Two flights of one airframe are the same operation when their times overlap, or when
// the gap between them is short enough that the aircraft never went away. The archive
// timeout is that boundary at run time - a gap longer than it starts a new flight - and
// using it here as well is what keeps a reload from rejoining what a session split.
bool AircraftTracker::sameOperation(const TrackedFlight *a, const TrackedFlight *b) const
{
    if (!a->m_firstSeen.isValid() || !a->m_lastSeen.isValid()
        || !b->m_firstSeen.isValid() || !b->m_lastSeen.isValid())
    {
        return true;        // Nothing to separate them by, so behave as before
    }
    if ((a->m_firstSeen <= b->m_lastSeen) && (b->m_firstSeen <= a->m_lastSeen)) {
        return true;        // Overlapping
    }
    const qint64 gap = (a->m_lastSeen < b->m_firstSeen)
        ? a->m_lastSeen.secsTo(b->m_firstSeen) : b->m_lastSeen.secsTo(a->m_firstSeen);
    return gap <= (qint64) m_settings.m_removalMins * 60;
}

AircraftTracker::TrackedAircraft *AircraftTracker::findOrCreateAircraft(const AircraftReport& report)
{
    TrackedAircraft *item = nullptr;

    // Strongest identity first
    if (report.m_icao && m_byKey.contains(icaoKey(report.m_icao))) {
        item = m_byKey.value(icaoKey(report.m_icao));
    } else if (!report.m_registration.isEmpty() && m_byKey.contains(regKey(report.m_registration))) {
        item = m_byKey.value(regKey(report.m_registration));
    }

    // A flight number is NOT an identity - it is reused, day after day and by different
    // airframes. Matching on one is only safe when the report offers nothing better:
    // an unseen ICAO address arriving on a historical flight number would otherwise be
    // folded in to the aircraft that flew it last, and then ignored, because the code
    // below only adopts an identity the item does not already have.
    //
    // Even with nothing stronger it has to be recent. The key lives as long as the
    // aircraft does, so without this a flight number seen days ago still claims today's.
    if (!item && !report.m_flight.isEmpty()
        && (report.m_icao == 0) && report.m_registration.isEmpty()
        && m_byKey.contains(flightKey(report.m_flight)))
    {
        TrackedAircraft *candidate = m_byKey.value(flightKey(report.m_flight));
        const QDateTime when = report.m_received.isValid() ? report.m_received
                                                           : QDateTime::currentDateTime();
        // How recently this aircraft flew THIS callsign, not how recently it was heard.
        //
        // Bounded at both ends: secsTo() runs negative when the report is OLDER than what
        // was last heard, and a negative number is less than any window - so a delayed
        // report claiming a callsign since taken up by another airframe was matched to
        // that airframe rather than rejected.
        const QDateTime flownAt = flightLastSeen(candidate, report.m_flight);
        const qint64 age = flownAt.secsTo(when);
        if (flownAt.isValid() && (age >= 0) && (age < m_flightKeyValidSecs)) {
            item = candidate;
        }
    }

    if (!item)
    {
        item = new TrackedAircraft();
        item->m_id = m_nextId++;
        m_aircraft.append(item);
    }

    // Adopt any stronger identities the report carries and register their keys
    if (report.m_icao && (item->m_icao == 0))
    {
        item->m_icao = report.m_icao;
        claimKey(item, icaoKey(report.m_icao));
    }
    if (!report.m_registration.isEmpty() && item->m_registration.isEmpty())
    {
        item->m_registration = report.m_registration;
        claimKey(item, regKey(report.m_registration));
    }
    if (!report.m_flight.isEmpty())
    {
        // If the flight key belongs to a weaker entry created before this
        // airframe was identified, fold it in rather than orphaning it - but only when
        // that entry was flying this callsign at about the same time. A callsign is
        // flown again day after day, so an unbounded match folded a flight-only record
        // of somebody else's operation into this airframe.
        TrackedAircraft *other = m_byKey.value(flightKey(report.m_flight));
        if (other && (other != item) && (other->m_icao == 0) && other->m_registration.isEmpty())
        {
            const QDateTime flownAt = flightLastSeen(other, report.m_flight);
            const QDateTime when = report.m_received.isValid() ? report.m_received
                                                               : QDateTime::currentDateTime();
            const qint64 age = flownAt.isValid() ? flownAt.secsTo(when) : -1;
            if (flownAt.isValid() && (age >= 0) && (age < m_flightKeyValidSecs)) {
                absorbAircraft(item, other);
            }
        }
        item->m_flight = report.m_flight;
        m_byKey.insert(flightKey(report.m_flight), item);
    }

    // Cross reference registration and ICAO through the OpenSky database
    if (item->m_icao && item->m_registration.isEmpty()
        && m_aircraftInfoByIcao && m_aircraftInfoByIcao->contains((int) item->m_icao))
    {
        item->m_registration = m_aircraftInfoByIcao->value((int) item->m_icao)->m_registration;
        if (!item->m_registration.isEmpty()) {
            claimKey(item, regKey(item->m_registration));
        }
    }
    if ((item->m_icao == 0) && !item->m_registration.isEmpty()
        && m_aircraftInfo && m_aircraftInfo->contains(item->m_registration))
    {
        item->m_icao = (quint32) m_aircraftInfo->value(item->m_registration)->m_icao;
        if (item->m_icao) {
            claimKey(item, icaoKey(item->m_icao));
        }
    }

    return item;
}

// A location in a link management frame is a coarse handoff hint rather than a fix, so
// it is trusted least. Everything else that isn't ADS-B is a real position report
// A link management frame's altitude is both coarse and, on some aircraft, wrong - so
// it is only believed when nothing better has been heard lately, and never sets a record
int AircraftTracker::altitudeRank(const AircraftReport& report)
{
    if (report.m_protocol == AircraftReport::ADSB) {
        return AltitudeAdsb;
    }
    if (report.m_documentKind == AircraftReport::Logon) {
        return AltitudeCoarse;
    }
    return AltitudeReported;
}

int AircraftTracker::positionRank(const AircraftReport& report)
{
    if (report.m_protocol == AircraftReport::ADSB) {
        return PositionAdsb;
    }
    if (report.m_documentKind == AircraftReport::Logon) {
        return PositionCoarse;
    }

    return PositionReported;
}

void AircraftTracker::processReport(AircraftReport& report)
{
    // A SELCAL call names a code rather than an aircraft. Only when it can be pinned on
    // one aircraft does it go any further; otherwise it is logged and that is all.
    if ((report.m_protocol == AircraftReport::SELCAL) && !attributeSelcal(report)) {
        return;
    }

    TrackedAircraft *item = findOrCreateAircraft(report);

    // A CSV file is a new observation run, even when it contains the same airframe and
    // callsign as live traffic or a file replayed moments ago. Keeping the old current
    // flight made the first historical position rewind the icon while the in-order
    // track guard below quite correctly kept the old track. The Map then joined those
    // two unrelated endpoints with its provisional point.
    if (report.m_replayStart)
    {
        if (item->m_currentFlight) {
            m_dirtyFlights.insert(item->m_currentFlight);
        }
        item->m_currentFlight = nullptr;
        item->m_flight.clear();
        item->m_departure.clear();
        item->m_arrival.clear();
        item->m_route.clear();
        clearPositionAndMotion(item);
    }

    // Receiving data makes it active, which puts it in the active tables and on the map
    setActive(item, true);

    // The LATEST report, not the last one to arrive. A replayed or delayed report can
    // be hours old, and assigning it made a currently active aircraft look stale - at
    // which point the removal timer archived it and took it off the Map.
    if (report.m_received.isValid()
        && (!item->m_lastSeen.isValid() || (report.m_received > item->m_lastSeen))) {
        item->m_lastSeen = report.m_received;
    }
    item->m_messages++;
    item->m_sources[qMakePair((int) report.m_protocol, report.m_frequency)]++;

    if (!report.m_documentText.isEmpty()) {
        item->m_lastDocumentText = report.m_documentText;
    }

    // For the message rate chart: one series per protocol then a total
    if ((report.m_protocol >= 0) && (report.m_protocol < AircraftReport::ProtocolCount)) {
        m_chartCounts[report.m_protocol]++;
    }
    m_chartCounts[AircraftReport::ProtocolCount]++;

    // Track the flight this report belongs to (route facts are collated there),
    // and collect notable documents and ATC messages. The aircraft shows the
    // flight's primary name, so it doesn't flap between the ACARS flight number
    // and the ADS-B callsign forms.
    TrackedFlight *flight = updateFlight(item, report);
    // The same tally as the airframe's, but only for the flight it is on now
    if (flight) {
        flight->m_sources[qMakePair((int) report.m_protocol, report.m_frequency)]++;
    }
    if (flight) {
        flight->m_messages++;
    }
    // The airframe's name is its current flight's. A late message filed against the
    // flight before (see updateFlight()) is that flight's business, and copying its name
    // across flipped the aircraft back to the old callsign until the next report.
    if ((flight == item->m_currentFlight) && !flight->m_flight.isEmpty()) {
        item->m_flight = flight->m_flight;
    }
    addDocument(item, flight, report);
    addAtcMessage(item, flight, report);
    if (report.m_protocol == AircraftReport::SELCAL)
    {
        item->m_selcalHeard = report.m_selcalCode;
        logSelcalCall(item, flight, report, QStringList());
    }
    recordDcduMessage(item, report);
    addWeatherReport(item, report);

    checkNotifications(item, report);

    // Altitude is measured separately from position - on ADS-B by a different message
    // entirely - so it is taken in on its own account. It used to be updated only
    // alongside a position that had passed the ranking and plausibility checks, which
    // meant a distant aircraft whose position was rejected lost its altitude with it,
    // and a protocol that reports altitude without a position never set one at all.
    // Whether THIS report's altitude is one worth recording - see altitudeRank()
    bool reportAltitudeValid = false;
    float reportAltitudeFt = 0.0f;

    if (report.m_altitudeValid)
    {
        // Not if we already hold a later observation - a stored record from a log dump
        // arrives long after the altitude it describes
        const bool stale = report.m_altitudeDateTime.isValid()
            && item->m_altitudeDateTime.isValid()
            && (report.m_altitudeDateTime < item->m_altitudeDateTime);

        const int altRank = altitudeRank(report);
        // When each source was last heard, so it has to move forward only. A replayed or
        // delayed report carries an older reception time, and writing it here made live
        // ADS-B look as though it had gone quiet - at which point the coarser sources
        // below were let through to replace an altitude that was perfectly good.
        if (report.m_received.isValid()
            && (!item->m_altitudeRankTimes[altRank].isValid()
                || (report.m_received > item->m_altitudeRankTimes[altRank]))) {
            item->m_altitudeRankTimes[altRank] = report.m_received;
        }

        // Only fall back to a coarser source once the better ones have gone quiet, as
        // positions already do. Without this a single bad XID octet replaced a good
        // ADS-B altitude until the next ADS-B message arrived.
        bool altSuperseded = false;
        for (int better = 0; better < altRank; better++)
        {
            if (item->m_altitudeRankTimes[better].isValid()
                && (item->m_altitudeRankTimes[better].secsTo(report.m_received) < m_positionRankTimeoutSecs)) {
                altSuperseded = true;
            }
        }

        // The records take the altitude THIS report carried, and never a coarse one:
        // an aircraft heard only on link management frames is better left without a
        // highest-altitude record than given a wrong one, because a record is never
        // corrected afterwards
        reportAltitudeValid = (altRank != AltitudeCoarse);
        reportAltitudeFt = report.m_altitudeFt;

        if (!stale && !altSuperseded)
        {
            // The one before it is kept as well, so a position timestamped between the
            // two can have its altitude worked out rather than guessed - see altitudeAt()
            if (report.m_altitudeDateTime.isValid() && item->m_altitudeDateTime.isValid()
                && (report.m_altitudeDateTime > item->m_altitudeDateTime))
            {
                item->m_prevAltitudeFt = item->m_altitudeFt;
                item->m_prevAltitudeDateTime = item->m_altitudeDateTime;
            }
            item->m_altitudeFt = report.m_altitudeFt;
            item->m_altitudeValid = true;
            if (report.m_altitudeDateTime.isValid()) {
                item->m_altitudeDateTime = report.m_altitudeDateTime;
            }
        }
    }

    // How far away the position THIS report carried is, whether or not it was adopted
    bool reportRangeValid = false;
    float reportRangeKm = 0.0f;

    // Exactly 0N 0E is a zeroed-out field, not a position off the coast of Africa
    bool positionValid = report.m_positionValid
        && ((report.m_latitude != 0.0f) || (report.m_longitude != 0.0f));

    if (positionValid)
    {
        // A report timestamped before the position we are already holding is a report
        // of where the aircraft USED to be - a stored record from a log dump (seen with
        // HFDL performance data), or simply out of order. Adopting one rewound the
        // coordinates while the timestamp stayed where it was, so the two then described
        // different moments, and appended a point to the track out of time order.
        //
        // The one exception is a held position that has itself gone stale: it is on its
        // way off the Map, so an older fix is still an improvement on it. That exception
        // is needed because the protocols timestamp differently - ADS-B stamps when we
        // received the frame, an ACARS position report when the aircraft measured it, so
        // an ACARS fix routinely looks minutes old the moment it arrives. Without it, an
        // aircraft that had just left ADS-B coverage would refuse its own oceanic
        // positions until their clock caught up with the last ADS-B one.
        QDateTime posTime = report.m_positionDateTime.isValid() ? report.m_positionDateTime : report.m_received;
        bool current = !item->m_positionDateTime.isValid()
            || (item->m_positionDateTime.msecsTo(posTime) >= 0)
            // A live report from a slower bearer may legitimately replace an expired
            // ADS-B position. Historical replay has its own explicit boundary above;
            // comparing its old timestamp with today's wall clock would make every
            // position stale and silently allow the replay to run backwards.
            || (!report.m_replay && positionStale(item, report.m_received));

        const QGeoCoordinate reported(report.m_latitude, report.m_longitude);
        const int rank = positionRank(report);

        // Only fall back to a less accurate source once the better ones have gone quiet
        bool superseded = false;
        for (int better = 0; better < rank; better++)
        {
            if (item->m_positionRankTimes[better].isValid()
                && (item->m_positionRankTimes[better].secsTo(report.m_received) < m_positionRankTimeoutSecs)) {
                superseded = true;
            }
        }

        // A position the aircraft could not have flown to since the last one is a report
        // of where it used to be, or a bad decode - not where it is now. The tolerance
        // covers the resolution of the position and any slop in the times
        // A better source is always allowed to correct a worse one - otherwise a coarse
        // position 11 km out could reject the ADS-B fix that would have put it right
        bool plausible = true;
        if (item->m_positionValid && item->m_positionDateTime.isValid() && (rank >= item->m_positionRank))
        {
            const double km = QGeoCoordinate(item->m_latitude, item->m_longitude).distanceTo(reported) / 1000.0;
            const double hours = std::abs((double) item->m_positionDateTime.msecsTo(posTime)) / 3600000.0;
            plausible = km <= (m_maxGroundSpeedKn * 1.852 * hours) + m_positionToleranceKm;
        }

        // The range record for a protocol asks how far away the aircraft was when we heard
        // it on that protocol, so it uses the position THIS report carried - not the one we
        // end up holding, which may have come from a better source. Gating it on the
        // position being ADOPTED was wrong: an aircraft inside ADS-B coverage has every
        // ACARS, VDL-2 and Aero position superseded within the rank timeout, so those
        // protocols could never record a range at all however far away they heard it. A
        // report carrying no position still measures nothing, which is what stopped
        // ADS-B distances being credited to HFDL.
        //
        // Checked against the position we are holding whatever its rank - unlike the
        // tracked position, a range record is never corrected later, so a bad decode
        // would leave a wrong figure in it for ever.
        bool rangePlausible = true;
        if (item->m_positionValid && item->m_positionDateTime.isValid())
        {
            const double km = QGeoCoordinate(item->m_latitude, item->m_longitude).distanceTo(reported) / 1000.0;
            const double hours = std::abs((double) item->m_positionDateTime.msecsTo(posTime)) / 3600000.0;
            rangePlausible = km <= (m_maxGroundSpeedKn * 1.852 * hours) + m_positionToleranceKm;
        }
        if (rangePlausible)
        {
            // Distance from the antenna that heard this report, not from "here" - a
            // remote SDR is somewhere else entirely, and a range figure measured from
            // the wrong end is worse than none
            const QGeoCoordinate receiver = receiverPosition(report.m_deviceSetIndex);
            if (receiver.isValid())
            {
                reportRangeKm = (float) (receiver.distanceTo(reported) / 1000.0);
                reportRangeValid = true;
                // Which direction it was heard in, as well as how far, so the range
                // pattern can be drawn rather than just the one furthest figure
                updateCoverage(report.m_protocol, receiver,
                    (float) receiver.azimuthTo(reported), reportRangeKm);
            }
        }

        if (current && !superseded && plausible)
        {
            item->m_positionRankTimes[rank] = report.m_received;
            item->m_positionRank = rank;
            item->m_positionValid = true;
            item->m_latitude = report.m_latitude;
            item->m_longitude = report.m_longitude;
            if (reportRangeValid)
            {
                item->m_distanceKm = reportRangeKm;
                item->m_distanceValid = true;
            }
            // The position and the moment it describes always come from the same
            // report. Keeping the newer of the two timestamps while taking an older
            // report's coordinates is what made the pair disagree.
            item->m_positionDateTime = posTime;

            // Accumulate the full track - it survives gaps between protocols
            // The altitude that belongs with this fix is the altitude AT posTime, which
            // on ADS-B is not the newest one received - see altitudeAt()
            const float trackAltFt = report.m_altitudeValid
                ? altitudeAt(item, posTime, report.m_altitudeFt) : 0.0f;
            QGeoCoordinate coordinate(report.m_latitude, report.m_longitude,
                report.m_altitudeValid ? Units::feetToMetres(trackAltFt) : 0.0f);
            // Every reported position is kept. There is deliberately no distance
            // threshold: a transponder reports at its own rate whatever the aircraft is
            // doing, so a distance guard only ever discards points from SLOW aircraft -
            // taxiing, on approach - which is precisely where the track needs detail,
            // while doing nothing at all about a fast one. The only thing worth dropping
            // is a position identical to the one before it, which carries nothing.
            // Position only. An altitude report does not move the aircraft, and it
            // arrives carrying the position and timestamp of the LAST position report,
            // so appending on any change of altitude filled the track with points at a
            // repeated place and time. The altitude recorded against a track point is
            // the altitude when that position was reported, which is what it should be.
            // ... and in time order. An older fix is adopted only when the position
            // being held has gone stale, but appending it after a newer point would
            // leave the track's times running backwards - and the Map's smoother divides
            // by the difference between consecutive times, so a negative one gives inf
            // and then nan, exactly as two identical times do - which is why a fix at
            // the same time as the last point is not appended either. Minute-resolution
            // ACARS and HFDL positions a few kilometres apart produced exactly that.
            // Where the aircraft is now belongs to the flight it is flying now: a late
            // message for the flight before still carries today's position, and adding
            // it to the landed flight extended that leg past its landing.
            TrackedFlight *flown = item->m_currentFlight ? item->m_currentFlight : flight;
            // The stored points have to be in memory before anything is appended: until
            // they are, m_trackSaved counts rows the list does not hold, and the save
            // would append nothing at all
            ensureTrackLoaded(flown);
            const bool inOrder = flown && (flown->m_trackTimes.isEmpty()
                || (posTime > flown->m_trackTimes.last()));
            const bool moved = inOrder && (flown->m_track.isEmpty()
                || (flown->m_track.last().latitude() != coordinate.latitude())
                || (flown->m_track.last().longitude() != coordinate.longitude()));
            if (moved)
            {
                // Heading along the track when the report has none
                if (!flown->m_track.isEmpty())
                {
                    float trackHeading = (float) flown->m_track.last().azimuthTo(coordinate);
                    if (!report.m_headingValid)
                    {
                        item->m_heading = trackHeading;
                        item->m_headingValid = true;
                        item->m_headingDateTime = posTime;
                    }
                }
                flown->m_track.append(coordinate);
                flown->m_trackTimes.append(posTime.toUTC());   // UTC, so sending the track to the Map converts nothing
                item->m_mapTrackAppended = true;

            }
        }
    }
    // Both only move forward, as the position, the altitude and the last seen time do:
    // a delayed report describes what the aircraft was doing then, not now
    const QDateTime observed = report.m_received.isValid() ? report.m_received
                                                           : QDateTime::currentDateTime();
    if (report.m_headingValid
        && (!item->m_headingDateTime.isValid() || (observed >= item->m_headingDateTime)))
    {
        item->m_heading = report.m_heading;
        item->m_headingValid = true;
        item->m_headingDateTime = observed;
    }
    if (report.m_speedValid
        && (!item->m_speedDateTime.isValid() || (observed >= item->m_speedDateTime)))
    {
        item->m_speedKts = report.m_speedKts;
        item->m_speedValid = true;
        item->m_speedDateTime = observed;
    }

    // The GUI and basic map item are updated from the flush timer
    update3DModel(item);
    updateStatistics(report, item, reportRangeValid, reportRangeKm,
                     reportAltitudeValid, reportAltitudeFt);
    m_dbDirty = true;
    m_dirtyAircraft.insert(item);

    if (report.m_mapItem)
    {
        // ADS-B pass-through goes straight out - the item carries this
        // report's aircraft state for the Map's PFD
        forwardMapItem(item, report.m_mapItem);
        report.m_mapItem = nullptr;     // Ownership transferred
        // Wall clock, not the report's time: this says whether pass-through is still
        // FLOWING, which flush() tests against now. A CSV replay's reports are hours old,
        // so stamping their time made every replayed item look abandoned and flush()
        // replaced each one with a basic item 300 ms later.
        item->m_lastMapItemTime = QDateTime::currentDateTime();
    }
}

// Flush batched GUI notifications and basic map updates
void AircraftTracker::flush()
{
    if (!m_dirtyAircraft.isEmpty())
    {
        const QDateTime now = QDateTime::currentDateTime();
        QList<AircraftDisplay> displays;
        for (TrackedAircraft *item : m_dirtyAircraft)
        {
            displays.append(displayFor(item));

            // While pass-through items are flowing, a basic item built here
            // would replace the rich one, so only send when ADS-B is quiet.
            // Only aircraft we are currently hearing are drawn.
            bool passThroughActive = item->m_lastMapItemTime.isValid()
                && (item->m_lastMapItemTime.secsTo(now) < 60);
            if (item->m_active && item->m_positionValid && !passThroughActive)
            {
                sendToMap(item);
                item->m_mapResync = false;
                m_mapResyncPending.remove(item);
            }
        }
        m_dirtyAircraft.clear();
        emit aircraftUpdated(displays);
    }

    // Aircraft a Map that has just opened is still owed. They were not sendable when
    // they were last looked at - typically their ADS-B pass-through had been quiet for
    // less than a minute, so a basic item would have replaced a richer one - and the
    // dirty set they came from is emptied every pass, so without a queue of their own a
    // quiet aircraft would simply never reach that Map.
    if (!m_mapResyncPending.isEmpty())
    {
        const QDateTime now = QDateTime::currentDateTime();
        const QSet<TrackedAircraft *> owed = m_mapResyncPending;
        for (TrackedAircraft *item : owed)
        {
            const bool passThroughActive = item->m_lastMapItemTime.isValid()
                && (item->m_lastMapItemTime.secsTo(now) < 60);
            if (!item->m_active)
            {
                m_mapResyncPending.remove(item);    // Archived, so not on any Map
            }
            else if (item->m_positionValid && !passThroughActive)
            {
                sendToMap(item);
                item->m_mapResync = false;
                m_mapResyncPending.remove(item);
            }
        }
    }

    // After the aircraft loop above has emptied m_dirtyAircraft, so the aircraft it marks
    // dirty go out on the next flush rather than being cleared unsent. The routes and the
    // record marker below are re-sent within this one.
    checkMapConsumers();

    // All three are cheap unless something they depend on has changed
    updateWatchedTrack();
    updateRecordMapItem();
    retryRoutes();

    // The concurrent count has to be recomputed on a timer rather than only when a report
    // arrives: aircraft leaving the window lower it, and nothing signals that.
    const QDateTime stamp = QDateTime::currentDateTime();
    updateConcurrent(stamp);

    // Both elapsed figures advance whether or not anything is being received, so nudge
    // the display once a minute rather than leaving the clocks frozen on a quiet band
    if (m_statistics.m_sessionStart.isValid())
    {
        const qint64 minute = m_statistics.m_sessionStart.secsTo(stamp) / 60;
        if (minute != m_statisticsMinute)
        {
            m_statisticsMinute = minute;
            m_statisticsDirty = true;
            // On this tick rather than on every flush: it is a file stat, and the database
            // only grows when it is saved, which is far less often than a flush
            updateDatabaseSize();
        }
    }

    if (m_statisticsDirty)
    {
        m_statisticsDirty = false;
        if (m_statistics.m_sessionStart.isValid())
        {
            m_statistics.m_session.m_seconds = m_statistics.m_sessionStart.secsTo(stamp);
            m_statistics.m_allTime.m_seconds = m_allTimeSecondsBase
                + (m_allTimeSince.isValid() ? m_allTimeSince.secsTo(stamp)
                                            : m_statistics.m_session.m_seconds);
        }
        emit statisticsUpdated(m_statistics);
    }

    if (!m_dirtyFlights.isEmpty())
    {
        QList<FlightDisplay> displays;
        bool watchedDirty = false;
        for (TrackedFlight *flight : m_dirtyFlights)
        {
            displays.append(displayFor(flight));
            watchedDirty = watchedDirty || (flight->m_id == m_watchedFlight);
        }
        m_dirtyFlights.clear();
        emit flightsUpdated(displays);
        if (watchedDirty)
        {
            TrackedFlight *flight = flightById(m_watchedFlight);
            if (flight) {
                emitProfile(flight);
            }
        }
    }

    if (!m_pendingDocuments.isEmpty())
    {
        emit documentsAdded(m_pendingDocuments);
        m_pendingDocuments.clear();
    }
    if (!m_pendingAtc.isEmpty())
    {
        emit atcMessages(m_pendingAtc);
        m_pendingAtc.clear();
    }
    if (!m_pendingWeather.isEmpty())
    {
        emit weatherReports(m_pendingWeather);
        m_pendingWeather.clear();
    }

    // Redrawn only when a bin actually grew, which on a busy day is a few times a minute
    // rather than once per report
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        if (m_coverageDirty[i])
        {
            sendCoverageToMap(i);
            m_coverageDirty[i] = false;
            m_coverageChanged = true;
        }
    }

    // For the web API's report, which reads it from another thread
    updateTotals();
}

void AircraftTracker::updateChartCounts()
{
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 elapsedMS = m_chartRateTime.msecsTo(now);
    if (elapsedMS <= 0) {
        return;
    }
    QList<float> rates;
    for (int i = 0; i < CHART_COUNTS; i++)
    {
        rates.append(m_chartCounts[i] / (elapsedMS / 1000.0f));
        m_chartCounts[i] = 0;
    }
    m_chartRateTime = now;
    emit messageRates(rates);
}

AircraftDisplay AircraftTracker::displayFor(const TrackedAircraft *item) const
{
    AircraftDisplay d;
    d.m_id = item->m_id;
    d.m_icao = item->m_icao;
    d.m_registration = item->m_registration;
    d.m_flight = item->m_flight;
    // From the current flight where there is one, since that is what holds the other
    // name; from the airframe's own last flight name otherwise, which leaves the
    // callsign showing it and the number empty rather than showing nothing at all
    flightNameForms(item->m_flight,
                    item->m_currentFlight ? item->m_currentFlight->m_aliases : QStringList(),
                    d.m_callsign, d.m_flightNumber);
    d.m_positionValid = item->m_positionValid;
    d.m_distanceValid = item->m_distanceValid;
    d.m_distanceKm = item->m_distanceKm;
    d.m_latitude = item->m_latitude;
    d.m_longitude = item->m_longitude;
    d.m_altitudeValid = item->m_altitudeValid;
    d.m_altitudeFt = item->m_altitudeFt;
    d.m_headingValid = item->m_headingValid;
    d.m_heading = item->m_heading;
    d.m_speedValid = item->m_speedValid;
    d.m_speedKts = item->m_speedKts;
    d.m_protocols = sourcesText(item->m_sources);
    d.m_messages = item->m_messages;
    d.m_lastSeen = item->m_lastSeen;
    d.m_mapAvailableUntil = mapAvailableUntil(item).toUTC();
    d.m_positionDateTime = item->m_positionDateTime.toUTC();
    d.m_altitudeDateTime = item->m_altitudeDateTime.toUTC();
    d.m_headingDateTime = item->m_headingDateTime.toUTC();
    d.m_speedDateTime = item->m_speedDateTime.toUTC();
    d.m_mapName = item->m_mapName.isEmpty() ? mapItemName(item) : item->m_mapName;
    d.m_currentFlightId = item->m_currentFlight ? item->m_currentFlight->m_id : 0;
    d.m_active = item->m_active;
    if (!item->m_selcalHeard.isEmpty()) {
        d.m_selcal = item->m_selcalHeard;
    } else if (m_selcalCodes && !item->m_registration.isEmpty()) {
        d.m_selcal = m_selcalCodes->value(item->m_registration);
    }
    return d;
}

FlightDisplay AircraftTracker::displayFor(const TrackedFlight *flight) const
{
    FlightDisplay d;
    d.m_id = flight->m_id;
    d.m_aircraftId = flight->m_aircraft->m_id;
    d.m_flight = flight->m_flight;
    flightNameForms(flight->m_flight, flight->m_aliases, d.m_callsign, d.m_flightNumber);
    d.m_aliases = flight->m_aliases.join(", ");
    d.m_reg = regFor(flight->m_aircraft);
    d.m_departure = flight->m_departure;
    d.m_arrival = flight->m_arrival;
    d.m_route = flight->m_route;
    if (!flight->m_departure.isEmpty() || !flight->m_arrival.isEmpty())
    {
        // Whichever end we hold; where both are held and disagree about where they
        // came from, a message anywhere in the pair is what the column reports
        d.m_departureFromMessage =
            !flight->m_departure.isEmpty() && !flight->m_departureFromDatabase;
        d.m_arrivalFromMessage =
            !flight->m_arrival.isEmpty() && !flight->m_arrivalFromDatabase;
        d.m_routeSource = (d.m_departureFromMessage || d.m_arrivalFromMessage)
            ? "Message" : "Database";
    }
    // Only worth comparing when something told us: a pair taken FROM the database
    // trivially agrees with it and says nothing. Aliases are searched too, because the
    // database is keyed on callsign and a flight answers to more than one - the entry
    // may be filed under the ICAO form while the ACARS form is the primary here.
    if (m_routeInfo && (d.m_routeSource == "Message"))
    {
        QStringList names;
        names.append(flight->m_flight);
        names.append(flight->m_aliases);
        for (const QString& name : names)
        {
            if (name.isEmpty() || !m_routeInfo->contains(name)) {
                continue;
            }
            const AircraftRouteInformation *route = m_routeInfo->value(name);
            // Each end judged only where we hold one, so a flight known to be going to
            // KJFK with no departure yet is not called a disagreement for the half that
            // is simply not known
            const bool departureAgrees = flight->m_departure.isEmpty()
                || (route->m_dep == flight->m_departure);
            const bool arrivalAgrees = flight->m_arrival.isEmpty()
                || (route->m_arr == flight->m_arrival);
            d.m_routeChecked = true;
            d.m_routeAgrees = departureAgrees && arrivalAgrees;
            break;
        }
    }
    d.m_firstSeen = flight->m_firstSeen;
    d.m_lastSeen = flight->m_lastSeen;
    // Logons excluded: they are counted in the thousands where everything else is in
    // the tens, so a count including them said nothing about whether a flight had
    // anything worth opening. They have their own tab.
    d.m_documents = 0;
    for (const TrackedDocument *doc : flight->m_documents)
    {
        if (doc->m_kind != AircraftReport::Logon) {
            d.m_documents++;
        }
    }
    d.m_messages = flight->m_messages;
    d.m_protocols = sourcesText(flight->m_sources);
    d.m_out = flight->m_out;
    d.m_off = flight->m_off;
    d.m_on = flight->m_on;
    d.m_in = flight->m_in;
    for (const TrackedDocument *doc : flight->m_documents)
    {
        switch (doc->m_kind)
        {
        case AircraftReport::Loadsheet:  d.m_hasLoadsheet = true; break;
        case AircraftReport::Clearance:  d.m_hasClearance = true; break;
        case AircraftReport::DepartureClearance: d.m_hasClearance = true; break;
        // Asking for one counts: it says the flight is going oceanic, which is what the
        // column is read for
        case AircraftReport::ClearanceRequest: d.m_hasClearance = true; break;
        case AircraftReport::FlightPlan: d.m_hasFlightPlan = true; break;
        default: break;
        }
    }
    // Only the flight an active aircraft is currently flying counts as active
    d.m_active = flight->m_aircraft->m_active && (flight->m_aircraft->m_currentFlight == flight);
    return d;
}

DocumentEvent AircraftTracker::documentEvent(const TrackedAircraft *item, const TrackedFlight *flight, const TrackedDocument *doc) const
{
    DocumentEvent e;
    e.m_aircraftId = item->m_id;
    e.m_flightId = flight->m_id;
    e.m_received = doc->m_received;
    e.m_flight = flight->m_flight;
    e.m_reg = regFor(item);
    e.m_kind = documentKindName(doc->m_kind);
    e.m_title = doc->m_title;
    e.m_text = doc->m_text;
    e.m_mapName = item->m_mapName.isEmpty() ? mapItemName(item) : item->m_mapName;
    return e;
}

AircraftTracker::TrackedFlight *AircraftTracker::flightById(quint64 id) const
{
    if (id == 0) {
        return nullptr;
    }
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            if (flight->m_id == id) {
                return flight;
            }
        }
    }
    return nullptr;
}

void AircraftTracker::emitProfile(const TrackedFlight *flight)
{
    // Fetched now if it never was. Const because everything else about emitting is, and
    // reading the flight's own stored history does not change what the flight IS.
    ensureProfileLoaded(const_cast<TrackedFlight *>(flight));
    emit profileUpdated(flight->m_id, flight->m_profileTimes, flight->m_profileAltFt, flight->m_profileSpeedKts);
}

// Resend the complete state - the GUI has just connected
void AircraftTracker::resync()
{
    emit allCleared();

    QList<AircraftDisplay> aircraft;
    QList<FlightDisplay> flights;
    QList<DocumentEvent> documents;
    for (TrackedAircraft *item : m_aircraft)
    {
        aircraft.append(displayFor(item));
        for (TrackedFlight *flight : item->m_flights)
        {
            flights.append(displayFor(flight));
            for (TrackedDocument *doc : flight->m_documents) {
                documents.append(documentEvent(item, flight, doc));
            }
        }
    }
    std::sort(documents.begin(), documents.end(),
        [](const DocumentEvent& a, const DocumentEvent& b) { return a.m_received < b.m_received; });

    if (!aircraft.isEmpty()) {
        emit aircraftUpdated(aircraft);
    }
    if (!flights.isEmpty()) {
        emit flightsUpdated(flights);
    }
    if (!documents.isEmpty()) {
        emit documentsAdded(documents);
    }
    if (!m_atcLog.isEmpty()) {
        emit atcMessages(m_atcLog);
    }
    if (!m_weatherLog.isEmpty()) {
        emit weatherReports(m_weatherLog);
    }
}

void AircraftTracker::deleteAll()
{
    for (TrackedAircraft *item : m_aircraft)
    {
        if (!item->m_mapName.isEmpty()) {
            removeFromMap(item->m_mapName);
        }
        // The routes as well as the aircraft. They are named after the flight rather than
        // the airframe, so removing the aircraft leaves the lines and their waypoint
        // markers behind - and a moment later the flights holding the only record of what
        // those items are called are deleted, so nothing can ever take them off.
        for (TrackedFlight *flight : item->m_flights) {
            removeRoutes(flight);
        }
    }
    qDeleteAll(m_aircraft);
    m_aircraft.clear();
    m_byKey.clear();
    m_atcLog.clear();
    m_dirtyAircraft.clear();
    m_mapResyncPending.clear();
    m_dirtyFlights.clear();
    m_pendingDocuments.clear();
    m_pendingAtc.clear();
    // The documents and ATC messages go with them, and the next save has to be told so:
    // it will not empty those tables on its own initiative - see saveDatabase()
    m_dbMessageWipe = true;
    // The weather log is keyed on airports rather than aircraft, so it is easy to
    // forget here - but the GUI clears its rows on allCleared(), and leaving the log
    // behind put every report straight back on the next resync. Worse, the dedupe index
    // survived too, so a report that arrived again after the clear was recognised as
    // one already held and never re-emitted at all.
    m_acarsBlocks.clear();
    m_pendingWeather.clear();
    m_weatherLog.clear();
    m_weatherIndex.clear();
    m_routesPending.clear();
    if (!m_watchedTrackName.isEmpty())
    {
        removeFromMap(m_watchedTrackName);
        m_watchedTrackName.clear();
        m_watchedTrackKey.clear();
    }
    m_watchedFlight = 0;
    forgetSavedHistory();
    m_dbDirty = true;
    emit allCleared();
}

// Aircraft we have stopped hearing from are not discarded - they move to the archive
// tables and come off the map, so the session's history stays available
void AircraftTracker::removeOldAircraft()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // The aircraft databases may have been downloaded since we last looked
    m_aircraftInfo = OsnDB::getAircraftInformationByReg();
    m_aircraftInfoByIcao = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();
    m_selcalCodes = SELCALDB::getCodesByRegistration();

    // The timer that drives this also drives the periodic session save
    if (m_db && !m_loadingDatabase) {
        saveDatabase();
    } else if (!m_db) {
        // Only a save empties them, and with no database there is never one
        forgetPendingChanges();
    }

    for (TrackedAircraft *item : m_aircraft)
    {
        if (item->m_active
            && item->m_lastSeen.isValid()
            && (item->m_lastSeen.secsTo(currentDateTime) > m_settings.m_removalMins * 60))
        {
            setActive(item, false);
            continue;
        }

        // A stale position comes off the Map while the aircraft stays in the tables.
        // The 3D map drops the entity on its own when the availability interval ends,
        // but nothing takes it off the 2D map, so it would sit at its last known
        // position until the aircraft was archived - three hours later by default.
        if (item->m_active && !item->m_mapReplay && !item->m_mapName.isEmpty()
            && positionStale(item, currentDateTime))
        {
            removeFromMap(item->m_mapName);
            item->m_mapName = "";       // Re-sent in full if a fresh position arrives
            item->m_mapTrackChanged = true;
        }
    }

    discardOldAircraft();
}

// Archived aircraft are kept for a while so their history is still there, but not for
// ever - the session database is rewritten in full on every save, so it has to be
// bounded. A retention of zero means the user wants to keep everything. Flights have a
// retention of their own, as they hold most of the history: the aircraft can be kept for
// ever, with only their recent flights.
// What the weather log's dedupe index is keyed on
static size_t weatherKey(const WeatherEvent& e)
{
    // (qHashMulti would do this, but it is Qt 6 only)
    return qHash(e.m_airport + QChar(0) + e.m_kind + QChar(0) + e.m_text);
}

void AircraftTracker::discardOldAircraft()
{
    const bool aircraftLimit = m_settings.m_retentionDays > 0;
    const bool flightLimit = m_settings.m_flightRetentionDays > 0;
    if (!aircraftLimit && !flightLimit) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime aircraftCutoff = now.addDays(-m_settings.m_retentionDays);
    const QDateTime flightCutoff = now.addDays(-m_settings.m_flightRetentionDays);
    // For what belongs to no aircraft or flight still kept, below
    const QDateTime cutoff = m_settings.messageCutoff();
    QList<quint64> discardedAircraft;
    QList<quint64> discardedFlights;
    QSet<quint64> discardedFlightIds;     // Of aircraft that are kept

    for (int i = m_aircraft.size() - 1; i >= 0; i--)
    {
        TrackedAircraft *item = m_aircraft[i];

        if (!aircraftLimit || item->m_active || !item->m_lastSeen.isValid() || (item->m_lastSeen >= aircraftCutoff))
        {
            if (flightLimit) {
                discardOldFlights(item, flightCutoff, discardedFlights, discardedFlightIds);
            }
            continue;
        }

        discardedAircraft.append(item->m_id);
        for (TrackedFlight *flight : item->m_flights)
        {
            discardedFlights.append(flight->m_id);
            m_flightsToDelete.insert(flight->m_id);
            m_dirtyFlights.remove(flight);
            // Before the flight is deleted, which is where the names its lines and
            // waypoint markers were drawn under are held - after it, nothing could ever
            // take them off the map again
            removeRoutes(flight);
            if (m_watchedFlight == flight->m_id) {
                m_watchedFlight = 0;
            }
        }
        for (const QString& key : m_byKey.keys(item)) {
            m_byKey.remove(key);
        }
        m_dirtyAircraft.remove(item);
        m_mapResyncPending.remove(item);
        m_aircraft.removeAt(i);
        delete item;
        m_dbDirty = true;
    }

    // The ATC and weather logs go by the same rules, or they grow for ever: a message goes
    // with the aircraft or flight it was from, and one with no aircraft once it is older
    // than the shorter retention period. That is an unattributed call (id 0), or a message
    // from an aircraft that went before its messages were taken with it - which otherwise
    // stayed for ever. A weather report is about an airport, so it goes by age alone.
    const QSet<quint64> discardedIds(discardedAircraft.begin(), discardedAircraft.end());
    QSet<quint64> liveIds;
    for (const TrackedAircraft *item : m_aircraft) {
        liveIds.insert(item->m_id);
    }
    QList<AtcEvent> keptAtc;
    bool atcPruned = false;
    for (const AtcEvent& e : m_atcLog)
    {
        if (discardedIds.contains(e.m_aircraftId))
        {
            atcPruned = true;
        }
        else if (discardedFlightIds.contains(e.m_flightId))
        {
            m_atcFlightsToDelete.insert(e.m_flightId);
            atcPruned = true;
        }
        else if (!liveIds.contains(e.m_aircraftId) && e.m_received.isValid() && (e.m_received < cutoff))
        {
            m_atcPrunedIds.insert(e.m_aircraftId);
            atcPruned = true;
        }
        else
        {
            keptAtc.append(e);
        }
    }
    if (atcPruned)
    {
        m_atcLog = keptAtc;
        m_atcAircraftToDelete.unite(discardedIds);
        m_atcPruneCutoff = cutoff;
        m_dbDirty = true;
    }
    const int weatherBefore = m_weatherLog.size();
    m_weatherLog.erase(std::remove_if(m_weatherLog.begin(), m_weatherLog.end(),
        [&](const WeatherEvent& e) { return e.m_received.isValid() && (e.m_received < cutoff); }),
        m_weatherLog.end());
    if (m_weatherLog.size() != weatherBefore)
    {
        // The index holds positions in the log, which have all moved
        m_weatherIndex.clear();
        for (int i = 0; i < m_weatherLog.size(); i++) {
            m_weatherIndex.insert(weatherKey(m_weatherLog[i]), i);
        }
    }

    if (!discardedFlights.isEmpty()) {
        emit flightsRemoved(discardedFlights);
    }
    if (!discardedFlightIds.isEmpty())
    {
        qDebug() << "AircraftTracker::discardOldAircraft: discarded" << discardedFlightIds.size()
                 << "flights not heard for" << m_settings.m_flightRetentionDays << "days";
    }
    if (!discardedAircraft.isEmpty())
    {
        qDebug() << "AircraftTracker::discardOldAircraft: discarded" << discardedAircraft.size()
                 << "aircraft not heard for" << m_settings.m_retentionDays << "days";
        emit aircraftRemoved(discardedAircraft);
    }
}

// An aircraft that is kept, with its flights older than the cutoff. Its most recent flight
// stays, however old: it is the flight the aircraft tables show it as flying, and the one
// a report from the aircraft would carry on. Everything a flight holds - its track,
// profile and documents - goes with it in the database, through m_flightsToDelete.
void AircraftTracker::discardOldFlights(TrackedAircraft *item, const QDateTime& cutoff,
                                        QList<quint64>& discarded, QSet<quint64>& discardedIds)
{
    for (int j = item->m_flights.size() - 1; j >= 0; j--)
    {
        TrackedFlight *flight = item->m_flights[j];
        if ((flight == item->m_currentFlight) || !flight->m_lastSeen.isValid() || (flight->m_lastSeen >= cutoff)) {
            continue;
        }

        discarded.append(flight->m_id);
        discardedIds.insert(flight->m_id);
        m_flightsToDelete.insert(flight->m_id);
        m_dirtyFlights.remove(flight);
        m_routesPending.remove(flight->m_id);
        // Before the flight is deleted, as for a discarded aircraft
        removeRoutes(flight);
        if (m_watchedFlight == flight->m_id) {
            m_watchedFlight = 0;
        }
        item->m_flights.removeAt(j);
        delete flight;
        // Its flight count has changed
        m_dirtyAircraft.insert(item);
        m_dbDirty = true;
    }
}

// Moving an aircraft between the active and archive tables
void AircraftTracker::setActive(TrackedAircraft *item, bool active)
{
    if (item->m_active == active) {
        return;
    }

    item->m_active = active;

    if (!active && !item->m_mapName.isEmpty())
    {
        removeFromMap(item->m_mapName);
        item->m_mapTrackChanged = true;   // The whole track is resent if it returns
    }

    m_dirtyAircraft.insert(item);
    for (TrackedFlight *flight : item->m_flights) {
        m_dirtyFlights.insert(flight);
    }
    m_dbDirty = true;
}

// Fold a weaker duplicate into an identified aircraft. A flight-only sighting
// (e.g. ACARS with no registration) creates an aircraft keyed only by its
// flight; when a later report ties that flight to a real airframe, the weak
// entry's data moves across instead of being orphaned.
void AircraftTracker::absorbAircraft(TrackedAircraft *item, TrackedAircraft *weak)
{
    item->m_messages += weak->m_messages;
    QMapIterator<QPair<int, qint64>, int> sourceIt(weak->m_sources);
    while (sourceIt.hasNext())
    {
        sourceIt.next();
        item->m_sources[sourceIt.key()] += sourceIt.value();
    }
    // Which of the two is carrying the current state varies: the weak entry is usually
    // the flight-only ACARS record and the survivor the ICAO-backed one, but it is just
    // as often the other way round. Keeping the survivor's simply because it survives
    // threw away a position, altitude, speed or heading it did not have - so each is
    // merged on validity and then on time.
    const bool takeWeakPosition = weak->m_positionValid
        && (!item->m_positionValid
            || (weak->m_positionDateTime.isValid()
                && (!item->m_positionDateTime.isValid()
                    || (weak->m_positionDateTime > item->m_positionDateTime))));
    if (takeWeakPosition)
    {
        item->m_positionValid = true;
        item->m_latitude = weak->m_latitude;
        item->m_longitude = weak->m_longitude;
        item->m_positionDateTime = weak->m_positionDateTime;
        item->m_positionRank = weak->m_positionRank;
        item->m_distanceValid = weak->m_distanceValid;
        item->m_distanceKm = weak->m_distanceKm;
        // The track moves with the flights, below
        item->m_mapTrackChanged = true;
    }

    // When a position or altitude of each rank was last heard is the union of the two.
    // It is what decides whether a coarser source may overwrite a better one, so losing
    // half of it would let a stale ACARS fix supersede live ADS-B.
    for (int i = 0; i < PositionRanks; i++)
    {
        if (weak->m_positionRankTimes[i].isValid()
            && (!item->m_positionRankTimes[i].isValid()
                || (weak->m_positionRankTimes[i] > item->m_positionRankTimes[i]))) {
            item->m_positionRankTimes[i] = weak->m_positionRankTimes[i];
        }
    }
    for (int i = 0; i < AltitudeRanks; i++)
    {
        if (weak->m_altitudeRankTimes[i].isValid()
            && (!item->m_altitudeRankTimes[i].isValid()
                || (weak->m_altitudeRankTimes[i] > item->m_altitudeRankTimes[i]))) {
            item->m_altitudeRankTimes[i] = weak->m_altitudeRankTimes[i];
        }
    }

    // Altitude is measured separately from position on ADS-B - a Mode S altitude reply
    // carries no position - so it merges separately, and takes with it the pair of
    // observations altitudeAt() interpolates a track point's height between.
    // The two most recent samples of the four, rather than one record's pair. Taking a
    // pair whole discards a newer sample the other record held, and altitudeAt()
    // interpolates a track point's height BETWEEN these two - so the wrong pair does not
    // merely lose detail, it interpolates over the wrong interval.
    if (weak->m_altitudeValid)
    {
        QList<QPair<QDateTime, float>> samples;
        auto offer = [&samples](bool valid, const QDateTime& when, float value)
        {
            if (valid && when.isValid()) {
                samples.append(qMakePair(when, value));
            }
        };
        offer(item->m_altitudeValid, item->m_altitudeDateTime, item->m_altitudeFt);
        offer(item->m_altitudeValid, item->m_prevAltitudeDateTime, item->m_prevAltitudeFt);
        offer(true, weak->m_altitudeDateTime, weak->m_altitudeFt);
        offer(true, weak->m_prevAltitudeDateTime, weak->m_prevAltitudeFt);

        std::sort(samples.begin(), samples.end(),
                  [](const QPair<QDateTime, float>& a, const QPair<QDateTime, float>& b) {
                      return a.first > b.first;
                  });
        // Both records usually hold the same newest observation, having both just heard
        // it. Taking the top two of the sorted list would then take that one instant
        // twice, throw away the real second newest, and leave altitudeAt() with a zero
        // span to interpolate over - which it refuses, so the merge would cost exactly
        // the interpolation it is meant to preserve.
        for (int i = samples.size() - 1; i > 0; i--)
        {
            if (samples[i].first == samples[i - 1].first) {
                samples.removeAt(i);
            }
        }

        if (samples.isEmpty())
        {
            // Neither record timestamped its altitude, so there is nothing to order them
            // by and the survivor's is kept unless it has none at all
            if (!item->m_altitudeValid)
            {
                item->m_altitudeValid = true;
                item->m_altitudeFt = weak->m_altitudeFt;
            }
        }
        else
        {
            item->m_altitudeValid = true;
            item->m_altitudeDateTime = samples[0].first;
            item->m_altitudeFt = samples[0].second;
            if (samples.size() > 1)
            {
                item->m_prevAltitudeDateTime = samples[1].first;
                item->m_prevAltitudeFt = samples[1].second;
            }
        }
    }

    // Neither speed nor heading is timestamped, so "whichever one of the two has it" is
    // as far as this can honestly go
    if (weak->m_speedValid
        && (!item->m_speedValid
            || (weak->m_speedDateTime.isValid()
                && (!item->m_speedDateTime.isValid()
                    || (weak->m_speedDateTime > item->m_speedDateTime)))))
    {
        item->m_speedValid = true;
        item->m_speedKts = weak->m_speedKts;
        item->m_speedDateTime = weak->m_speedDateTime;
    }
    if (weak->m_headingValid
        && (!item->m_headingValid
            || (weak->m_headingDateTime.isValid()
                && (!item->m_headingDateTime.isValid()
                    || (weak->m_headingDateTime > item->m_headingDateTime)))))
    {
        item->m_headingValid = true;
        item->m_heading = weak->m_heading;
        item->m_headingDateTime = weak->m_headingDateTime;
    }
    if (weak->m_lastSeen.isValid()
        && (!item->m_lastSeen.isValid() || (weak->m_lastSeen > item->m_lastSeen))) {
        item->m_lastSeen = weak->m_lastSeen;
    }
    if (item->m_departure.isEmpty()) {
        item->m_departure = weak->m_departure;
    }
    if (item->m_arrival.isEmpty()) {
        item->m_arrival = weak->m_arrival;
    }
    if (item->m_route.isEmpty()) {
        item->m_route = weak->m_route;
    }
    if (item->m_lastDocumentText.isEmpty()) {
        item->m_lastDocumentText = weak->m_lastDocumentText;
    }

    // The weak entry's flight history moves across, oldest first
    for (TrackedFlight *flight : weak->m_flights) {
        flight->m_aircraft = item;
    }
    QList<TrackedFlight *> flights = weak->m_flights;
    flights.append(item->m_flights);
    item->m_flights = flights;
    // The more recently heard of the two. Keeping the survivor's because it survives
    // kept yesterday's flight current over the one the weak entry is hearing now - so the
    // next report started a third flight, splitting today's in two.
    if (weak->m_currentFlight
        && (!item->m_currentFlight
            || (weak->m_currentFlight->m_lastSeen > item->m_currentFlight->m_lastSeen))) {
        item->m_currentFlight = weak->m_currentFlight;
    }
    weak->m_flights.clear();
    weak->m_currentFlight = nullptr;

    // What would otherwise be done again, or lost: a rule that has already fired for the
    // weak entry fired for this airframe, and an ADS-B item it was receiving says the
    // pass-through is live, which stops a basic item being sent over the top of it
    item->m_notifiedRules.unite(weak->m_notifiedRules);
    if (weak->m_lastMapItemTime.isValid()
        && (!item->m_lastMapItemTime.isValid() || (weak->m_lastMapItemTime > item->m_lastMapItemTime))) {
        item->m_lastMapItemTime = weak->m_lastMapItemTime;
    }
    if (item->m_selcalHeard.isEmpty()) {
        item->m_selcalHeard = weak->m_selcalHeard;
    }
    if (weak->m_dcduMessage.m_received > item->m_dcduMessage.m_received)
    {
        // The sequence carries on from the survivor's, so the Map sees a new message
        const int seq = std::max(item->m_dcduMessage.m_seq, weak->m_dcduMessage.m_seq) + 1;
        item->m_dcduMessage = weak->m_dcduMessage;
        item->m_dcduMessage.m_seq = seq;
    }

    // ATC messages are held by id, so they are moved by hand - see mergeFlightInto()
    if (moveMessages(true, weak->m_id, item->m_id)) {
        m_atcAircraftMoves.append({weak->m_id, item->m_id});
    }

    for (const QString& key : m_byKey.keys(weak)) {
        m_byKey.insert(key, item);
    }
    if (!weak->m_mapName.isEmpty()) {
        removeFromMap(weak->m_mapName);
    }
    m_dirtyAircraft.remove(weak);
    m_mapResyncPending.remove(weak);
    // It was counted as an aircraft heard when it was created, and it has just turned
    // out to be one already counted - so take it back off both the session set and the
    // count, or identifying an airframe part way through inflates the figure by one
    if (m_sessionHeard.remove(weak->m_id)) {
        m_statistics.m_session.m_distinctAircraft = m_sessionHeard.size();
        m_statisticsDirty = true;
    }
    m_sessionRecorded.remove(weak->m_id);
    emit aircraftMerged(weak->m_id, item->m_id);
    emit aircraftRemoved(QList<quint64>{weak->m_id});
    m_aircraft.removeAll(weak);
    delete weak;

    // Merging histories can leave the same flight twice
    mergeLoadedFlights(item);
    m_dirtyAircraft.insert(item);
    for (TrackedFlight *flight : item->m_flights) {
        m_dirtyFlights.insert(flight);
    }
    m_dbDirty = true;
}

// Re-point the messages held against one aircraft or flight id to another, saying
// whether the ATC log had any (which the database then has to be told about). The
// pending ATC messages and documents too: they are on their way to the GUI, which has
// already been told of the merge by the time they arrive, so they would reach it under
// an id that no longer exists.
bool AircraftTracker::moveMessages(bool aircraftId, quint64 from, quint64 to)
{
    auto move = [aircraftId, from, to](auto& e) {
        quint64& id = aircraftId ? e.m_aircraftId : e.m_flightId;
        if (id == from)
        {
            id = to;
            return true;
        }
        return false;
    };

    bool moved = false;
    for (AtcEvent& e : m_atcLog) {
        moved |= move(e);
    }
    for (AtcEvent& e : m_pendingAtc) {
        move(e);
    }
    for (DocumentEvent& e : m_pendingDocuments) {
        move(e);
    }
    return moved;
}

// Merge any of an aircraft's flights that are the same flight under different
// names - after loading a session saved before names were aliased, or after
// absorbing a duplicate aircraft
void AircraftTracker::mergeLoadedFlights(TrackedAircraft *item)
{
    for (int i = 0; i < item->m_flights.size(); i++)
    {
        TrackedFlight *dst = item->m_flights[i];
        for (int j = i + 1; j < item->m_flights.size(); )
        {
            TrackedFlight *src = item->m_flights[j];
            bool matches = flightMatches(dst, src->m_flight);
            for (const QString& alias : src->m_aliases) {
                matches = matches || flightMatches(dst, alias);
            }
            // The name is not enough. One airframe flies the same callsign again and
            // again - a daily rotation is the same number every day - and merging on the
            // name alone put every one of those operations into a single flight holding
            // days of track, several sets of OOOI times and several routes. Run time
            // splits them on the archive timeout; this is the same rule, so a reload
            // stops undoing it.
            if (matches && sameOperation(dst, src)) {
                mergeFlightInto(item, dst, src);    // Removes src from the list
            } else {
                j++;
            }
        }
    }

    // One airframe flies one flight at a time, so flights whose time spans
    // overlap are the same flight under names that can't be matched textually
    // (some airlines' callsigns are unrelated to the flight number)
    for (int i = 0; i < item->m_flights.size(); i++)
    {
        TrackedFlight *dst = item->m_flights[i];
        for (int j = i + 1; j < item->m_flights.size(); )
        {
            TrackedFlight *src = item->m_flights[j];
            if (dst->m_firstSeen.isValid() && dst->m_lastSeen.isValid()
                && src->m_firstSeen.isValid() && src->m_lastSeen.isValid()
                && (dst->m_firstSeen <= src->m_lastSeen)
                && (src->m_firstSeen <= dst->m_lastSeen)) {
                mergeFlightInto(item, dst, src);
            } else {
                j++;
            }
        }
    }

    // Keep the current flight at the end of the history
    if (item->m_currentFlight)
    {
        item->m_flights.removeAll(item->m_currentFlight);
        item->m_flights.append(item->m_currentFlight);
    }
}

// Split a flight identity into the airline designator and the flight number:
// a 3-letter ICAO designator (BAW31) or a 2-character IATA one (BA0031), with
// leading zeros stripped from the number so the two forms compare equal
// Which of the names a flight is known by is the ICAO callsign and which the IATA
// flight number. The airline designator is what tells them apart - three letters for
// ICAO, two characters for IATA - which is the same split the aliasing itself is built
// on, so a pair that were matched as the same flight are classified consistently here.
//
// The callsign falls back to the primary name so the column is never empty: a flight
// heard only under its IATA number is still called something, and blanking it would
// lose from the table what the aircraft is actually being called.
void AircraftTracker::flightNameForms(const QString& primary, const QStringList& aliases,
                                      QString& callsign, QString& flightNumber)
{
    callsign.clear();
    flightNumber.clear();

    QStringList names;
    if (!primary.isEmpty()) {
        names.append(primary);
    }
    names.append(aliases);

    for (const QString& name : names)
    {
        QString airline, number;
        if (!splitFlightName(name, airline, number)) {
            continue;       // Neither form - a registration flown as a callsign, say
        }
        QString& into = (airline.size() == 3) ? callsign : flightNumber;
        if (into.isEmpty()) {
            into = name;
        }
    }
    if (callsign.isEmpty()) {
        callsign = primary;
    }
}

bool AircraftTracker::splitFlightName(const QString& flight, QString& airline, QString& number)
{
    int prefixLen;
    if ((flight.size() > 3) && flight[0].isLetter() && flight[1].isLetter() && flight[2].isLetter()) {
        prefixLen = 3;
    } else if ((flight.size() > 2) && !(flight[0].isDigit() && flight[1].isDigit())) {
        prefixLen = 2;
    } else {
        return false;
    }
    airline = flight.left(prefixLen);
    QString rest = flight.mid(prefixLen);
    int i = 0;
    while ((i < rest.size() - 1) && (rest[i] == '0')) {
        i++;
    }
    number = rest.mid(i);
    return !number.isEmpty();
}

// Whether two names are the same flight in its IATA and ICAO callsign forms,
// e.g. BA0031 and BAW31. Only used within one airframe, so matching on the
// flight number alone is safe.
bool AircraftTracker::sameFlightNumber(const QString& a, const QString& b)
{
    QString airlineA, numberA, airlineB, numberB;
    if (!splitFlightName(a, airlineA, numberA) || !splitFlightName(b, airlineB, numberB)) {
        return false;
    }
    return numberA == numberB;
}

bool AircraftTracker::flightMatches(const TrackedFlight *flight, const QString& name)
{
    if ((flight->m_flight == name) || flight->m_aliases.contains(name)) {
        return true;
    }
    if (sameFlightNumber(flight->m_flight, name)) {
        return true;
    }
    for (const QString& alias : flight->m_aliases)
    {
        if (sameFlightNumber(alias, name)) {
            return true;
        }
    }
    return false;
}

// Record another name for a flight. The ICAO callsign form (3-letter airline
// designator) is preferred as the primary name - it is what the route database
// is keyed on - with the other forms kept as aliases.
void AircraftTracker::addFlightAlias(TrackedAircraft *item, TrackedFlight *flight, const QString& name)
{
    // No name is not another name for it. Recorded, it showed as a trailing ", " in the
    // Aliases column and put "F" in the key map - after which every unnamed flight
    // matched this one by "name" and was merged into it.
    if (name.isEmpty() || (flight->m_flight == name) || flight->m_aliases.contains(name)) {
        return;
    }
    if (flight->m_flight.isEmpty())
    {
        flight->m_flight = name;
        m_byKey.insert("F" + name, item);
        return;
    }

    QString airline, number;
    bool nameIsCallsign = splitFlightName(name, airline, number) && (airline.size() == 3);
    bool primaryIsCallsign = splitFlightName(flight->m_flight, airline, number) && (airline.size() == 3);

    if (nameIsCallsign && !primaryIsCallsign)
    {
        flight->m_aliases.append(flight->m_flight);
        flight->m_flight = name;
    }
    else
    {
        flight->m_aliases.append(name);
    }
    m_byKey.insert("F" + name, item);
}

// Fold one flight into another - they are the same flight under different names
void AircraftTracker::mergeFlightInto(TrackedAircraft *item, TrackedFlight *dst, TrackedFlight *src)
{
    // Both histories have to be in memory BEFORE they are combined - see
    // ensureTrackLoaded(). Loading afterwards did nothing: the merge below resets the
    // saved counts to zero, which is what tells these there is nothing stored to read, so
    // a restored flight's stored track and profile were dropped and then rewritten away.
    ensureTrackLoaded(dst);
    ensureTrackLoaded(src);
    ensureProfileLoaded(dst);
    ensureProfileLoaded(src);

    addFlightAlias(item, dst, src->m_flight);
    for (const QString& alias : src->m_aliases) {
        addFlightAlias(item, dst, alias);
    }
    if (src->m_firstSeen.isValid() && (!dst->m_firstSeen.isValid() || (src->m_firstSeen < dst->m_firstSeen))) {
        dst->m_firstSeen = src->m_firstSeen;
    }
    if (src->m_lastSeen.isValid() && (src->m_lastSeen > dst->m_lastSeen)) {
        dst->m_lastSeen = src->m_lastSeen;
    }
    // Keeping whatever dst already had meant a database guess beat a fact, because the
    // two were indistinguishable once stored. These two flights are the same operation
    // under its ACARS and ADS-B identities, and only one of them tends to carry the
    // filed plan - so the side that was told wins over the side that guessed.
    if (dst->m_departure.isEmpty()
        || (dst->m_departureFromDatabase && !src->m_departureFromDatabase
            && !src->m_departure.isEmpty()))
    {
        dst->m_departure = src->m_departure;
        dst->m_departureFromDatabase = src->m_departureFromDatabase;
    }
    if (dst->m_arrival.isEmpty()
        || (dst->m_arrivalFromDatabase && !src->m_arrivalFromDatabase
            && !src->m_arrival.isEmpty()))
    {
        dst->m_arrival = src->m_arrival;
        dst->m_arrivalFromDatabase = src->m_arrivalFromDatabase;
    }
    if (dst->m_route.isEmpty())
    {
        dst->m_route = src->m_route;
        // With the route, not separately: left to itself the time would end up claiming
        // that whichever route dst kept was filed when src's was
        dst->m_routeFiled = src->m_routeFiled;
    }
    QMapIterator<QPair<int, qint64>, int> flightSourceIt(src->m_sources);
    while (flightSourceIt.hasNext())
    {
        flightSourceIt.next();
        dst->m_sources[flightSourceIt.key()] += flightSourceIt.value();
    }
    dst->m_documents.append(src->m_documents);
    src->m_documents.clear();

    // Interleave the profile samples in time order
    if (!src->m_profileTimes.isEmpty())
    {
        QList<qint64> times;
        QList<float> alts, speeds;
        int i = 0, j = 0;
        while ((i < dst->m_profileTimes.size()) || (j < src->m_profileTimes.size()))
        {
            bool takeDst = (j >= src->m_profileTimes.size())
                || ((i < dst->m_profileTimes.size()) && (dst->m_profileTimes[i] <= src->m_profileTimes[j]));
            if (takeDst)
            {
                times.append(dst->m_profileTimes[i]);
                alts.append(dst->m_profileAltFt[i]);
                speeds.append(dst->m_profileSpeedKts[i]);
                i++;
            }
            else
            {
                times.append(src->m_profileTimes[j]);
                alts.append(src->m_profileAltFt[j]);
                speeds.append(src->m_profileSpeedKts[j]);
                j++;
            }
        }
        dst->m_profileTimes = times;
        dst->m_profileAltFt = alts;
        dst->m_profileSpeedKts = speeds;
        // Rebuilt, not appended to, so what is stored for this flight is now wrong
        dst->m_profileSaved = 0;
        m_profilesToRewrite.insert(dst->m_id);
    }

    dst->m_messages += src->m_messages;
    if (!dst->m_out.isValid()) { dst->m_out = src->m_out; }
    if (!dst->m_off.isValid()) { dst->m_off = src->m_off; }
    if (!dst->m_on.isValid())  { dst->m_on  = src->m_on; }
    if (!dst->m_in.isValid())  { dst->m_in  = src->m_in; }

    if (!src->m_track.isEmpty())
    {
        QList<QGeoCoordinate> track;
        QList<QDateTime> times;
        int i = 0, j = 0;
        while ((i < dst->m_track.size()) || (j < src->m_track.size()))
        {
            const bool takeDst = (j >= src->m_track.size())
                || ((i < dst->m_track.size()) && (dst->m_trackTimes[i] <= src->m_trackTimes[j]));
            if (takeDst)
            {
                track.append(dst->m_track[i]);
                times.append(dst->m_trackTimes[i]);
                i++;
            }
            else
            {
                track.append(src->m_track[j]);
                times.append(src->m_trackTimes[j]);
                j++;
            }
        }
        dst->m_track = track;
        dst->m_trackTimes = times;
        dst->m_trackSaved = 0;
        m_tracksToRewrite.insert(dst->m_id);
        item->m_mapTrackChanged = true;
    }

    // The routes move too, or they are lost with src while what was drawn from them
    // stays on the Map with nothing left that knows how to take it off. Where dst has one
    // of its own, src's drawing is removed rather than kept: two flights have become one,
    // and only one route can be current.
    const QPair<TrackedFlight::FiledRoute *, TrackedFlight::FiledRoute *> routes[] = {
        { &src->m_flightPlan, &dst->m_flightPlan },
        { &src->m_clearance, &dst->m_clearance }
    };
    for (const auto& pair : routes)
    {
        TrackedFlight::FiledRoute *from = pair.first;
        TrackedFlight::FiledRoute *to = pair.second;
        // The newer route wins, not the destination's. Which flight is which here is
        // not the order they were flown: reconciling two names for one flight merges the
        // current flight INTO the alias it turns out to be, so keeping the destination's
        // route would systematically keep the older of the two.
        const bool takeFrom = !from->m_waypoints.isEmpty()
            && (to->m_waypoints.isEmpty()
                || (from->m_filed.isValid()
                    && (!to->m_filed.isValid() || (from->m_filed > to->m_filed))));
        if (takeFrom)
        {
            // What the destination had drawn comes off first: two flights have become
            // one, and only one route can be current
            removeDrawnRoute(*to);
            // Taken whole, drawn names included, so a later redraw knows what to replace
            *to = *from;
            if (to->m_pending) {
                m_routesPending.insert(dst->m_id);
            }
        }
        else {
            removeDrawnRoute(*from);
        }
    }
    m_routesPending.remove(src->m_id);

    m_flightsToDelete.insert(src->m_id);
    // The ATC log holds ids, not pointers, so its messages have to be moved by hand - or
    // after the next resync or restart they belong to a flight that no longer exists, and
    // selecting the survivor shows none of them
    if (moveMessages(false, src->m_id, dst->m_id)) {
        m_atcFlightMoves.append({src->m_id, dst->m_id});
    }
    if (m_watchedFlight == src->m_id) {
        m_watchedFlight = dst->m_id;
    }
    if (item->m_currentFlight == src) {
        item->m_currentFlight = dst;
    }
    m_dirtyFlights.remove(src);
    m_dirtyFlights.insert(dst);
    // The route that moved was taken off the map above, under the name it was drawn
    // under. If what survives is the flight being looked at, it goes back on under the
    // survivor's - and the merge may be what made it the selected one, just above.
    if (routeDrawn(dst)) {
        sendRoutes(dst);
    }
    m_dbDirty = true;
    // Logged because it is invisible otherwise, and it moves documents, ATC messages and
    // the drawn route from one flight to another under the user - which is exactly the
    // kind of thing to want in the log when a view goes empty on its own
    qDebug() << "AircraftTracker::mergeFlights: flight" << src->m_id << src->m_flight
             << "merged into" << dst->m_id << dst->m_flight
             << "-" << dst->m_documents.size() << "documents now on it";
    // Before flightsRemoved, so that anything holding the old id has moved to the new one
    // by the time the removal is announced - including the selection, which would
    // otherwise be cleared for a flight that has not gone anywhere
    emit flightsMerged(src->m_id, dst->m_id);
    emit flightsRemoved(QList<quint64>{src->m_id});
    item->m_flights.removeAll(src);
    delete src;
}

// Forget where the aircraft is and what it is doing, for when what we hold describes
// an earlier operation rather than this one. The next report starts afresh, with no
// held position for its own to be judged against.
void AircraftTracker::clearPositionAndMotion(TrackedAircraft *item)
{
    item->m_positionValid = false;
    item->m_positionDateTime = QDateTime();
    item->m_positionRank = PositionRanks;
    for (QDateTime& time : item->m_positionRankTimes) {
        time = QDateTime();
    }

    item->m_altitudeValid = false;
    item->m_altitudeDateTime = QDateTime();
    item->m_prevAltitudeDateTime = QDateTime();
    for (QDateTime& time : item->m_altitudeRankTimes) {
        time = QDateTime();
    }

    item->m_headingValid = false;
    item->m_headingDateTime = QDateTime();
    item->m_speedValid = false;
    item->m_speedDateTime = QDateTime();
    item->m_distanceValid = false;
    item->m_mapTrackChanged = true;     // A different operation, a different track
}

// The flight a report belongs to
AircraftTracker::TrackedFlight *AircraftTracker::updateFlight(TrackedAircraft *item, const AircraftReport& report)
{
    TrackedFlight *flight = item->m_currentFlight;

    // An airframe that comes back later on the same callsign is flying it again, not
    // still flying it. Without this the second operation was appended to the first, so
    // one flight held two days of track, two sets of OOOI times and two routes. The
    // archive timeout is the boundary because it is already the answer to "has this
    // aircraft gone away": if it was away long enough to be archived, its return is a
    // new flight.
    if (flight && flight->m_lastSeen.isValid() && report.m_received.isValid())
    {
        const qint64 gap = flight->m_lastSeen.secsTo(report.m_received);
        if (gap > (qint64) m_settings.m_removalMins * 60)
        {
            item->m_departure.clear();
            item->m_arrival.clear();
            item->m_route.clear();
            m_dirtyFlights.insert(flight);
            item->m_currentFlight = nullptr;
            flight = nullptr;
            // Nor does where it was, or what it was doing. A position from the last
            // flight was shown in the Aircraft table as current - days old after a
            // restart - with that flight's altitude, heading and speed beside it.
            clearPositionAndMotion(item);
        }
    }
    // An aircraft restored with no current flight, its flights having been discarded
    // sooner than the aircraft, has only the position's own time to go by
    else if (!flight && item->m_positionDateTime.isValid() && report.m_received.isValid()
             && (item->m_positionDateTime.secsTo(report.m_received) > (qint64) m_settings.m_removalMins * 60))
    {
        clearPositionAndMotion(item);
    }

    if (!flight)
    {
        flight = new TrackedFlight();
        flight->m_id = m_nextId++;
        flight->m_aircraft = item;
        flight->m_flight = report.m_flight;
        flight->m_firstSeen = report.m_received;
        item->m_flights.append(flight);
        item->m_currentFlight = flight;
    }
    else if (!report.m_flight.isEmpty() && (flight->m_flight != report.m_flight)
             && !flight->m_aliases.contains(report.m_flight))
    {
        if (flight->m_flight.isEmpty())
        {
            // The identity of the flight already under way became known
            flight->m_flight = report.m_flight;
        }
        else if (flightMatches(flight, report.m_flight))
        {
            // The same flight number in the other form, e.g. BA0031 vs BAW31
            addFlightAlias(item, flight, report.m_flight);
        }
        else
        {
            // Some airlines' callsigns are unrelated to the flight number
            // (e.g. LS0232 vs EXS79HP), so the forms can't be matched by name
            // alone. If the name matches the previous flight and that flight
            // was seen moments ago, the two are the same flight alternating
            // between its ACARS and ADS-B identities - reunite them.
            TrackedFlight *previous = (item->m_flights.size() >= 2)
                ? item->m_flights[item->m_flights.size() - 2] : nullptr;
            // Bounded at both ends, as the flight key match is: a report older than the
            // previous flight's last message is not evidence that the two are the same
            // flight alternating between its identities
            const qint64 sincePrevious = previous && previous->m_lastSeen.isValid()
                ? previous->m_lastSeen.secsTo(report.m_received) : -1;
            if (previous && flightMatches(previous, report.m_flight)
                && previous->m_lastSeen.isValid()
                && (sincePrevious >= 0) && (sincePrevious < 10*60))
            {
                if (previous->m_on.isValid() || previous->m_in.isValid())
                {
                    // Except once the previous flight has landed: then the current one is
                    // the next operation, and a late message for the last one (the IN
                    // report, or a flight ID that lags the transponder) merged the two
                    // for good - both routes and both sets of OOOI times on one flight.
                    // The message is filed against the flight it names instead.
                    flight = previous;
                }
                else
                {
                    mergeFlightInto(item, previous, flight);
                    flight = previous;
                }
            }
            else
            {
                // A new flight by the same airframe: the old flight's route no
                // longer describes the aircraft
                item->m_departure.clear();
                item->m_arrival.clear();
                item->m_route.clear();
                flight = new TrackedFlight();
                flight->m_id = m_nextId++;
                flight->m_aircraft = item;
                flight->m_flight = report.m_flight;
                flight->m_firstSeen = report.m_received;
                item->m_flights.append(flight);
                // The flight it was on is no longer the active one
                if (item->m_currentFlight) {
                    m_dirtyFlights.insert(item->m_currentFlight);
                }
                item->m_currentFlight = flight;
                item->m_mapTrackChanged = true;   // A different flight, a different track
            }
        }
    }

    // Another name for this flight, from a message that gave one. An alias rather than
    // a rename: the two forms belong to the same operation and neither is more correct,
    // and replacing the name would read as a different flight and start a second one.
    if (!report.m_flightAlias.isEmpty() && (report.m_flightAlias != flight->m_flight)
        && !flight->m_aliases.contains(report.m_flightAlias))
    {
        addFlightAlias(item, flight, report.m_flightAlias);
    }

    // A late message for the flight before the current one, filed against it above. It
    // does not move that flight's last seen time past the start of the current one: the
    // two would then overlap, and mergeLoadedFlights() - at the next start, or the next
    // time this airframe absorbs a duplicate - merges overlapping flights as one.
    TrackedFlight *current = item->m_currentFlight;
    const bool lateForPrevious = (flight != current) && current
        && current->m_firstSeen.isValid() && report.m_received.isValid()
        && (report.m_received >= current->m_firstSeen);

    // The latest report, not the last to arrive - as on the aircraft above. A flight
    // whose last seen time went backwards sorts wrongly and reads as finished.
    if (!lateForPrevious && report.m_received.isValid()
        && (!flight->m_lastSeen.isValid() || (report.m_received > flight->m_lastSeen))) {
        flight->m_lastSeen = report.m_received;
    }

    // An event happens once, so the first report of it is the one to keep - the same
    // event is often re-sent, and a later copy is not a later event
    if ((report.m_oooiEvent != AircraftReport::OooiNone) && report.m_oooiTime.isValid())
    {
        switch (report.m_oooiEvent)
        {
        case AircraftReport::OooiOut: if (!flight->m_out.isValid()) { flight->m_out = report.m_oooiTime; } break;
        case AircraftReport::OooiOff: if (!flight->m_off.isValid()) { flight->m_off = report.m_oooiTime; } break;
        case AircraftReport::OooiOn:  if (!flight->m_on.isValid())  { flight->m_on  = report.m_oooiTime; } break;
        case AircraftReport::OooiIn:  if (!flight->m_in.isValid())  { flight->m_in  = report.m_oooiTime; } break;
        default: break;
        }
    }

    // Flight profile sample, at most one every 5 seconds. Like the position, the altitude
    // and speed are the aircraft's now, so they go to the flight it is flying now.
    TrackedFlight *flown = current ? current : flight;
    if ((report.m_altitudeValid || report.m_speedValid) && report.m_received.isValid())
    {
        qint64 ms = report.m_received.toMSecsSinceEpoch();
        // Loaded before the last sample is looked at - see ensureTrackLoaded()
        ensureProfileLoaded(flown);
        if (flown->m_profileTimes.isEmpty() || (ms - flown->m_profileTimes.last() >= 5000))
        {
            flown->m_profileTimes.append(ms);
            flown->m_profileAltFt.append(report.m_altitudeValid ? report.m_altitudeFt : std::numeric_limits<float>::quiet_NaN());
            flown->m_profileSpeedKts.append(report.m_speedValid ? report.m_speedKts : std::numeric_limits<float>::quiet_NaN());
        }
    }

    // Route facts a message revealed. They move forward in time only, for the same
    // reason the filed routes below do: a delayed message says where the flight was
    // going earlier, not where it is going now. Without this the Route column could
    // disagree with the route drawn on the Map, which does compare filing times.
    //
    // A fact we do not hold at all is still worth taking from an older message, since
    // the alternative is nothing - but it does not move the clock forward, or the next
    // genuinely newer message would be rejected by it.
    const bool routeIsNewer = !flight->m_routeFiled.isValid() || !report.m_received.isValid()
                           || (report.m_received >= flight->m_routeFiled);
    // What is held is only a guess from the callsign, so anything an aircraft actually
    // said replaces it whatever the clock says. TOM3TW was the case: the route database
    // had that callsign as East Midlands to Lanzarote, the aircraft's own FMC filed
    // Bristol to Corfu, and the flight was listed as the former while showing the
    // latter's flight plan.
    const bool guessHeld = flight->m_departureFromDatabase || flight->m_arrivalFromDatabase;

    // A database entry is one route per callsign, filed as a whole. Where a message
    // contradicts either end of it, the entry is describing a different operation and
    // the other end of it is no more trustworthy than the end just disproved - so the
    // whole guess goes, not just the half that was contradicted.
    //
    // That leaves a field empty where the message names only one end, which is the
    // honest outcome: half of a wrong answer is still a wrong answer, and shown beside
    // a corrected one it reads as though both had been confirmed.
    if (guessHeld)
    {
        const bool contradicted =
               (!report.m_departure.isEmpty() && !flight->m_departure.isEmpty()
                && (report.m_departure != flight->m_departure))
            || (!report.m_arrival.isEmpty() && !flight->m_arrival.isEmpty()
                && (report.m_arrival != flight->m_arrival));
        if (contradicted)
        {
            // Only the ends the database supplied - one already corrected by a message
            // is not part of the guess being thrown away
            if (flight->m_departureFromDatabase)
            {
                flight->m_departure.clear();
                flight->m_departureFromDatabase = false;
            }
            if (flight->m_arrivalFromDatabase)
            {
                flight->m_arrival.clear();
                flight->m_arrivalFromDatabase = false;
            }
        }
    }

    bool routeFactTaken = false;
    if (!report.m_departure.isEmpty()
        && (routeIsNewer || flight->m_departure.isEmpty() || guessHeld))
    {
        flight->m_departure = report.m_departure;
        flight->m_departureFromDatabase = false;
        routeFactTaken = true;
    }
    if (!report.m_arrival.isEmpty()
        && (routeIsNewer || flight->m_arrival.isEmpty() || guessHeld))
    {
        flight->m_arrival = report.m_arrival;
        flight->m_arrivalFromDatabase = false;
        routeFactTaken = true;
    }
    // A flight plan comes in parts: one route insert carries the en route path, the next
    // only the approach - "ILS 19L" - and an approach is not a route. Newer is not better
    // here, so a route that names nothing which can be placed does not replace one that
    // does. Without this the second insert overwrote the first, the Route column read
    // "ILS 19L", and the line already on the Map was erased as undrawable.
    // Worked out once: placing a waypoint searches the navaid list, and a long route
    // asked four times over would search it for every fix four times over
    const int reportPlaceable = report.m_route.isEmpty()
        ? 0 : routePlaceable(report.m_route, report.m_routeWaypoints);
    const bool routeIsBetter = report.m_route.isEmpty() || (reportPlaceable >= 2)
        || (routePlaceable(flight->m_route, QList<AircraftReport::RouteWaypoint>()) < 2);
    if (!report.m_route.isEmpty() && routeIsBetter && (routeIsNewer || flight->m_route.isEmpty()))
    {
        flight->m_route = report.m_route;
        routeFactTaken = true;
    }
    if (routeFactTaken && routeIsNewer && report.m_received.isValid()) {
        flight->m_routeFiled = report.m_received;
    }
    // A flight plan and an oceanic clearance both name a list of waypoints, and both
    // are drawn. Nothing else that fills in m_route does - the route database fills it
    // with the airport pair, which is not a route at all.
    //
    // ANY part of a plan, not only a path. The arrival end of a plan names no waypoints
    // at all - just the STAR and the approach - so testing the route alone dropped that
    // insert on the floor and the STAR never reached the plan being assembled. The
    // demodulator already had to make this exact correction on its side when it decides
    // what counts as a flight plan document; this is the same test, one layer up, and it
    // was still the old one.
    const bool carriesPlan = !report.m_route.isEmpty() || !report.m_routeRunway.isEmpty()
        || !report.m_routeSid.isEmpty() || !report.m_routeStar.isEmpty()
        || !report.m_routeApproach.isEmpty() || !report.m_routeAlternate.isEmpty();
    if (carriesPlan)
    {
        // A route filed before the one being held is a record of what the aircraft was
        // cleared onto earlier, not what it is on now. Replacing on arrival rather than
        // on filing time let a delayed message overwrite the current route, and redraw
        // the Map with it.
        // One plan, assembled from the inserts that carry it. An aircraft files the
        // departure end - runway, SID and the en route path - in one message and the
        // arrival end - STAR and approach - in another, so taking a message whole meant
        // the second half replaced the first and the plan was never whole. Each part is
        // taken from whichever message carried it instead.
        auto file = [this, &report, reportPlaceable](TrackedFlight::FiledRoute& route)
        {
            if (route.m_filed.isValid() && report.m_received.isValid()
                && (report.m_received < route.m_filed)) {
                return false;
            }

            // Assembled only where it IS the same plan. An insert naming a different
            // departure or arrival is a re-file, and then nothing of the old one is
            // worth keeping - including what was drawn from it, which has to come off
            // the Map here, while the names it was drawn under are still known.
            const bool samePlan =
                   (report.m_departure.isEmpty() || route.m_departure.isEmpty()
                    || (report.m_departure == route.m_departure))
                && (report.m_arrival.isEmpty() || route.m_arrival.isEmpty()
                    || (report.m_arrival == route.m_arrival));
            if (!samePlan)
            {
                removeDrawnRoute(route);
                route = TrackedFlight::FiledRoute();
            }

            auto take = [](QString& into, const QString& from)
            {
                if (!from.isEmpty()) {
                    into = from;
                }
            };
            take(route.m_departure, report.m_departure);
            take(route.m_arrival, report.m_arrival);
            take(route.m_runway, report.m_routeRunway);
            take(route.m_sid, report.m_routeSid);
            take(route.m_star, report.m_routeStar);
            take(route.m_approach, report.m_routeApproach);
            take(route.m_alternate, report.m_routeAlternate);
            if (report.m_routeFlightLevel > 0) {
                route.m_flightLevel = report.m_routeFlightLevel;
            }
            if (report.m_routeMach > 0.0f) {
                route.m_mach = report.m_routeMach;
            }

            // The path only from a message that has one, and only when it places at
            // least as much as what is held: the arrival end of a plan places nothing,
            // and taking it because it is newer erased the line that was drawn
            const bool takePath = !report.m_route.isEmpty()
                && ((reportPlaceable >= 2)
                    || (routePlaceable(route.m_waypoints, route.m_points) < 2));
            if (takePath)
            {
                route.m_waypoints = report.m_route;
                route.m_points = report.m_routeWaypoints;
            }

            // The message that carried the path, since that is the substantive half and
            // the two are otherwise near identical - both are position reports, so
            // keeping both would show the same position, altitude and waypoints twice
            if (takePath || route.m_text.isEmpty()) {
                route.m_text = report.m_documentText;
            }
            route.m_filed = report.m_received;
            return true;
        };

        // Stored either way; drawn only when this is the flight being looked at, which
        // sendRouteToMap() decides
        if ((report.m_documentKind == AircraftReport::FlightPlan)
            || (report.m_documentKind == AircraftReport::DepartureClearance))
        {
            if (file(flight->m_flightPlan))
            {
                sendRouteToMap(flight, flight->m_flightPlan, "Flight plan");
                // So the plan reaches the ND without waiting for the next report
                if (flight->m_aircraft) {
                    sendToMap(flight->m_aircraft);
                }
            }
        }
        else if (report.m_documentKind == AircraftReport::Clearance)
        {
            if (file(flight->m_clearance))
            {
                sendRouteToMap(flight, flight->m_clearance, "Oceanic clearance");
                if (flight->m_aircraft) {
                    sendToMap(flight->m_aircraft);
                }
            }
        }
    }

    // Enrich from the callsign to route database when no message has revealed the
    // route - the same database the ADS-B demodulator uses
    if (!flight->m_flight.isEmpty() && flight->m_departure.isEmpty() && flight->m_arrival.isEmpty()
        && m_routeInfo && m_routeInfo->contains(flight->m_flight))
    {
        const AircraftRouteInformation *route = m_routeInfo->value(flight->m_flight);
        flight->m_departure = route->m_dep;
        flight->m_arrival = route->m_arr;
        // Remembered, so that the first message to say otherwise can overturn it. A
        // callsign is flown day after day and the database holds one route for it, so
        // this is a reasonable guess and no more than that.
        flight->m_departureFromDatabase = true;
        flight->m_arrivalFromDatabase = true;
        if (!route->m_stops.isEmpty()) {
            flight->m_route = route->m_stops;
        }
    }

    // The aircraft-level route facts drive the Map label and popups, so they are the
    // current flight's. Only filling them when empty kept the first value for the whole
    // flight - a database guess that a flight plan then corrected stayed on the Map while
    // the Flights table showed the filed route.
    if (flight == item->m_currentFlight)
    {
        item->m_departure = flight->m_departure;
        item->m_arrival = flight->m_arrival;
        item->m_route = flight->m_route;
    }

    m_dirtyFlights.insert(flight);
    m_dirtyFlights.insert(flown);   // Its profile, when that went to the current flight

    return flight;
}

QString AircraftTracker::documentKindName(int kind) const
{
    switch (kind)
    {
    case AircraftReport::AcarsText: return "Text";
    case AircraftReport::PositionReport: return "Position report";
    case AircraftReport::OooiEvent: return "OOOI";
    case AircraftReport::Loadsheet: return "Loadsheet";
    case AircraftReport::FlightPlan: return "Flight plan";
    case AircraftReport::Clearance: return "Clearance";
    case AircraftReport::DepartureClearance: return "Departure clearance";
    case AircraftReport::ClearanceRequest: return "Clearance request";
    case AircraftReport::AtisRequest: return "ATIS request";
    case AircraftReport::WeatherRequest: return "Weather request";
    case AircraftReport::AirlineDownlink: return "Airline downlink";
    case AircraftReport::AircrewMessage: return "Aircrew message";
    case AircraftReport::EmergencyMessage: return "Emergency";
    case AircraftReport::Cpdlc: return "CPDLC";
    case AircraftReport::Logon: return "Logon";
    case AircraftReport::PerformanceReport: return "Performance";
    default: return "";
    }
}

// Collect notable documents - flight plans, clearances, loadsheets, logons,
// OOOI events - against the flight they belong to. Plain ACARS messages would
// flood the log so they are not collected, and CPDLC goes to the ATC log.
void AircraftTracker::addDocument(TrackedAircraft *item, TrackedFlight *flight, const AircraftReport& report)
{
    switch (report.m_documentKind)
    {
    case AircraftReport::Loadsheet:
    case AircraftReport::FlightPlan:
    case AircraftReport::Clearance:
    case AircraftReport::DepartureClearance:
    case AircraftReport::ClearanceRequest:
    case AircraftReport::AtisRequest:
    case AircraftReport::WeatherRequest:
    case AircraftReport::AirlineDownlink:
    case AircraftReport::AircrewMessage:
    case AircraftReport::EmergencyMessage:
    case AircraftReport::Logon:
    case AircraftReport::OooiEvent:
        break;
    default:
        return;
    }
    if (report.m_documentText.isEmpty()) {
        return;
    }
    // Retransmissions produce identical documents back to back
    if (!flight->m_documents.isEmpty()
        && (flight->m_documents.last()->m_kind == (int) report.m_documentKind)
        && (flight->m_documents.last()->m_text == report.m_documentText)) {
        return;
    }

    TrackedDocument *doc = new TrackedDocument();
    doc->m_kind = report.m_documentKind;
    doc->m_title = report.m_documentTitle.isEmpty() ? documentKindName(report.m_documentKind) : report.m_documentTitle;
    doc->m_text = report.m_documentText;
    doc->m_received = report.m_received;
    if (report.m_documentDerivable)
    {
        doc->m_raw = report.m_documentRaw;
        doc->m_source = report.m_documentSource;
        // Rendered here rather than taken from the report, so that what is shown live is
        // produced by exactly the same call that will produce it after a reload. Taking
        // the report's text would leave the two able to differ, and the difference would
        // only appear the next time the session was opened.
        const QString rendered = renderDocument(doc->m_raw, doc->m_source);
        if (!rendered.isEmpty()) {
            doc->m_text = rendered;
        }
    }
    flight->m_documents.append(doc);

    m_pendingDocuments.append(documentEvent(item, flight, doc));
    m_dirtyFlights.insert(flight);
    m_dbDirty = true;
}

// Log of controller-pilot data link messages across all channels. Only the
// concise ATC message text (e.g. "WILCO") is kept, not the full decode.
QString AircraftTracker::weatherKindName(int kind)
{
    switch (kind)
    {
    case AircraftReport::Metar:  return "METAR";
    case AircraftReport::Taf:    return "TAF";
    case AircraftReport::Atis:   return "D-ATIS";
    case AircraftReport::Twip:   return "TWIP";
    case AircraftReport::Notam:  return "NOTAM";
    case AircraftReport::Pirep:  return "PIREP";
    case AircraftReport::Sigmet: return "SIGMET";
    default:                     return "Weather";
    }
}

// Weather reaches us through whichever aircraft asked for it, but it describes an
// airport, so it is logged on its own rather than against that aircraft.
void AircraftTracker::addWeatherReport(TrackedAircraft *item, const AircraftReport& report)
{
    if ((report.m_weatherKind == AircraftReport::WeatherNone)
        || report.m_weatherText.isEmpty()) {
        return;
    }

    WeatherEvent e;
    e.m_received = report.m_received;
    e.m_airport = report.m_weatherAirport;
    e.m_kind = weatherKindName(report.m_weatherKind);
    e.m_text = report.m_weatherText;
    // The report is HTML, which a table cell shows as tags - the detail pane below
    // the table renders it. One line of plain text is what the Report column wants
    e.m_summary = QTextDocumentFragment::fromHtml(e.m_text).toPlainText()
                      .replace('\n', " ").simplified();
    e.m_from = !item->m_flight.isEmpty() ? item->m_flight : regFor(item);

    // The same METAR is sent to every aircraft that asks, so the identical report
    // arrives again and again. Only a genuinely new one is worth a row.
    //
    // This used to scan the whole log, comparing the full text of every entry against
    // every arrival - and since the log only grows, the cost per report grew with it, on
    // the tracker thread, for exactly the traffic that produces the most repeats.
    const size_t key = weatherKey(e);
    for (auto it = m_weatherIndex.constFind(key);
         (it != m_weatherIndex.constEnd()) && (it.key() == key); ++it)
    {
        // value() rather than at(): should the log ever be cleared without the index,
        // a stale position then simply fails to match and the report is logged again,
        // where at() would be undefined behaviour
        const WeatherEvent seen = m_weatherLog.value(it.value());
        if ((seen.m_airport == e.m_airport) && (seen.m_kind == e.m_kind)
            && (seen.m_text == e.m_text)) {
            return;
        }
    }

    m_weatherIndex.insert(key, m_weatherLog.size());
    m_weatherLog.append(e);
    m_pendingWeather.append(e);
}

void AircraftTracker::addAtcMessage(TrackedAircraft *item, TrackedFlight *flight, const AircraftReport& report)
{
    if ((report.m_documentKind != AircraftReport::Cpdlc) || report.m_atc.isEmpty()) {
        return;
    }

    QString flightName = !flight->m_flight.isEmpty() ? flight->m_flight : regFor(item);

    AtcEvent e;
    e.m_aircraftId = item->m_id;
    e.m_flightId = flight->m_id;
    e.m_received = report.m_received;
    e.m_protocol = documentKindName(report.m_documentKind);
    e.m_link = sourceText(report.m_protocol, report.m_frequency);
    e.m_uplink = report.m_uplink;
    e.m_from = report.m_uplink ? report.m_station : flightName;
    e.m_to = report.m_uplink ? flightName : report.m_station;
    e.m_message = QString(report.m_atc).replace('\n', "; ");
    e.m_tooltip = report.m_atc;
    e.m_mapName = item->m_mapName.isEmpty() ? mapItemName(item) : item->m_mapName;

    // Retransmissions produce identical messages back to back
    if (!m_atcLog.isEmpty()
        && (m_atcLog.last().m_message == e.m_message)
        && (m_atcLog.last().m_from == e.m_from)
        && (m_atcLog.last().m_to == e.m_to)) {
        return;
    }

    m_atcLog.append(e);
    m_pendingAtc.append(e);
    m_dbDirty = true;
}

// Work out which aircraft a SELCAL call was for, and give the report its identity so it
// is processed like any other. A code is assigned to several aircraft, so it is only
// pinned on one when nothing else could have been meant:
//
//  - exactly one of the aircraft holding the code is being heard now, or
//  - the database gives the code to one aircraft only, and that one is in the archive.
//    Being called on a voice channel means it is flying, so it is made active again.
//
// Anything else - several candidates in the air, or none being tracked - is logged
// against nobody. Guessing would make up an aircraft, and a made up aircraft is far
// harder to notice than an unattributed call. A call never creates an aircraft: with no
// identity and no position there would be nothing to show for it.
bool AircraftTracker::attributeSelcal(AircraftReport& report)
{
    const QString code = SELCALDB::normalizeCode(report.m_selcalCode);
    if (code.isEmpty()) {
        return false;
    }
    report.m_selcalCode = code;

    const QStringList registrations = SELCALDB::getRegistrations(code);
    QList<TrackedAircraft *> tracked;

    for (const QString& registration : registrations)
    {
        // By registration, and failing that by the ICAO address the aircraft database
        // gives it, since an airframe heard only on ADS-B may not have a registration yet
        TrackedAircraft *item = m_byKey.value(regKey(registration));
        if (!item && m_aircraftInfo && m_aircraftInfo->contains(registration))
        {
            const quint32 icao = (quint32) m_aircraftInfo->value(registration)->m_icao;
            if (icao) {
                item = m_byKey.value(icaoKey(icao));
            }
        }
        if (item && !tracked.contains(item)) {
            tracked.append(item);
        }
    }

    QList<TrackedAircraft *> active;
    for (TrackedAircraft *item : tracked)
    {
        if (item->m_active) {
            active.append(item);
        }
    }

    TrackedAircraft *chosen = nullptr;
    if (active.size() == 1) {
        chosen = active.first();
    } else if (active.isEmpty() && (registrations.size() == 1) && (tracked.size() == 1)) {
        chosen = tracked.first();
    }

    if (!chosen)
    {
        logSelcalCall(nullptr, nullptr, report, registrations);

        // Still a message heard on this protocol, so it counts towards the rates and
        // totals - just with no aircraft to set a record or be counted as heard
        m_chartCounts[AircraftReport::SELCAL]++;
        m_chartCounts[AircraftReport::ProtocolCount]++;
        const QDateTime when = report.m_received.isValid() ? report.m_received
                                                           : QDateTime::currentDateTime();
        for (AircraftStatistics::Scope *scope : { &m_statistics.m_session, &m_statistics.m_allTime }) {
            scope->update(report.m_protocol, false, 0.0f, false, 0.0f, false, 0.0f, QString(),
                          when, false, 0.0f, 0.0f, report.m_frequency);
        }
        m_statisticsDirty = true;
        return false;
    }

    report.m_icao = chosen->m_icao;
    report.m_registration = chosen->m_registration;
    return true;
}

// A SELCAL call in the ATC log, with the aircraft it was for where that is known. From
// is the frequency it was heard on: the call carries no station, and the frequency is
// what says which oceanic control or airline radio made it.
void AircraftTracker::logSelcalCall(TrackedAircraft *item, TrackedFlight *flight,
                                    const AircraftReport& report, const QStringList& candidates)
{
    AtcEvent e;
    e.m_aircraftId = item ? item->m_id : 0;
    e.m_flightId = flight ? flight->m_id : 0;
    e.m_received = report.m_received;
    e.m_protocol = protocolName(AircraftReport::SELCAL);
    e.m_uplink = true;
    e.m_from = report.m_frequency
        ? QString("%1 MHz").arg(report.m_frequency / 1e6, 0, 'f', 3) : QString();
    e.m_message = QString("SELCAL %1").arg(report.m_selcalCode);

    if (item)
    {
        e.m_to = (flight && !flight->m_flight.isEmpty()) ? flight->m_flight : regFor(item);
        e.m_tooltip = QString("SELCAL %1 called %2").arg(report.m_selcalCode, regFor(item));
        e.m_mapName = item->m_mapName.isEmpty() ? mapItemName(item) : item->m_mapName;
    }
    else if (candidates.isEmpty())
    {
        e.m_to = "Unknown";
        e.m_tooltip = QString("SELCAL %1 is not assigned to any aircraft in the SELCAL"
                              " database, or the database has not been downloaded").arg(report.m_selcalCode);
    }
    else
    {
        e.m_to = (candidates.size() == 1) ? candidates.first()
                                           : QString("%1 aircraft").arg(candidates.size());
        e.m_tooltip = QString("SELCAL %1 is assigned to %2. None could be picked out as the"
                              " one called: either none is being heard, or more than one is")
                          .arg(report.m_selcalCode, candidates.join(", "));
    }

    // A ground station calls more than once when the crew is slow to answer, and the
    // decoder can report one transmission twice. Calls close together are one event.
    if (!m_atcLog.isEmpty())
    {
        const AtcEvent& last = m_atcLog.last();
        if ((last.m_message == e.m_message) && (last.m_from == e.m_from) && (last.m_to == e.m_to)
            && last.m_received.isValid() && e.m_received.isValid()
            && (std::abs(last.m_received.secsTo(e.m_received)) < 30)) {
            return;
        }
    }

    m_atcLog.append(e);
    m_pendingAtc.append(e);
    m_dbDirty = true;
}

// Speech and command notifications for aircraft matching the user's rules.
// Each rule fires at most once per aircraft. Speech is passed to the GUI;
// commands run here, so they work in server mode too.
void AircraftTracker::checkNotifications(TrackedAircraft *item, const AircraftReport& report)
{
    for (int i = 0; i < m_settings.m_notificationSettings.size(); i++)
    {
        if (item->m_notifiedRules.contains(i)) {
            continue;
        }
        const AircraftSettings::NotificationSettings *rule = m_settings.m_notificationSettings[i].data();

        QString match;
        switch (rule->m_matchColumn)
        {
        case AircraftSettings::MATCH_ICAO:
            if (item->m_icao) {
                match = QString("%1").arg(item->m_icao, 6, 16, QChar('0')).toUpper();
            }
            break;
        case AircraftSettings::MATCH_REG:
            match = item->m_registration;
            break;
        case AircraftSettings::MATCH_FLIGHT:
            match = item->m_flight;
            break;
        case AircraftSettings::MATCH_TYPE:
            if (m_aircraftInfo && m_aircraftInfo->contains(item->m_registration)) {
                match = m_aircraftInfo->value(item->m_registration)->m_type;
            }
            break;
        default:
            break;
        }

        if (!match.isEmpty()
            && rule->m_regularExpression.isValid()
            && rule->m_regularExpression.match(match).hasMatch())
        {
            item->m_notifiedRules.insert(i);
            if (!rule->m_speech.isEmpty()) {
                emit speechNotification(subAircraftString(item, report, rule->m_speech));
            }
            if (!rule->m_command.isEmpty()) {
                commandNotification(item, report, rule->m_command);
            }
        }
    }
}

QString AircraftTracker::subAircraftString(TrackedAircraft *item, const AircraftReport& report, const QString& string) const
{
    QString type;
    if (m_aircraftInfo && m_aircraftInfo->contains(item->m_registration)) {
        type = m_aircraftInfo->value(item->m_registration)->m_type;
    }
    const QHash<QString, QString> values {
        {"icao", item->m_icao ? QString("%1").arg(item->m_icao, 6, 16, QChar('0')).toUpper() : QString()},
        {"reg", item->m_registration},
        {"flight", item->m_flight},
        {"type", type},
        {"protocol", protocolName(report.m_protocol)}
    };

    // In one pass over the template, so a value is never searched for variables itself.
    // Replacing them one after another did exactly that: a registration or flight number
    // comes off the air, and one reading "${type}" was expanded again by the next step.
    static const QRegularExpression variable(R"(\$\{(\w+)\})");
    QString s;
    int last = 0;
    QRegularExpressionMatchIterator it = variable.globalMatch(string);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        s += string.mid(last, match.capturedStart() - last);
        auto value = values.constFind(match.captured(1));
        s += (value != values.constEnd()) ? value.value() : match.captured(0);
        last = match.capturedEnd();
    }
    s += string.mid(last);
    return s;
}

// The template is split into arguments BEFORE the aircraft's details go in. They come off
// the air - anyone transmitting chooses them - so substituting first let quotes and spaces
// in a received registration or flight number become argument boundaries, adding arguments
// to the user's program. Substituted per argument, a value stays within the one it was put in.
void AircraftTracker::commandNotification(TrackedAircraft *item, const AircraftReport& report,
                                          const QString& command)
{
#if QT_CONFIG(process)
    QStringList allArgs = QProcess::splitCommand(command);
    if (allArgs.size() > 0)
    {
        for (QString& arg : allArgs) {
            arg = subAircraftString(item, report, arg);
        }
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }
#else
    (void) item;
    (void) report;
    qWarning() << "AircraftTracker::commandNotification: QProcess not supported. Can't run: " << command;
#endif
}


// The all time figures outlive the session that set them, so they travel in the settings
// as an opaque blob - a versioned stream rather than a settings key each, so that adding
// a statistic later does not need a new key and cannot collide with one.
//
// The session side is deliberately not written: it is defined by the run it describes.
// Nor is the all time aircraft count, which the database holds - see AircraftStatistics.
//
// Version 1 held the records alone. It still reads, and its records become the all time
// records, which is what they always were. Version 3 added where each record was set;
// records read from an older blob simply have no position, and are not drawn on the Map.
// Version 4 added the frequency it was heard on, which an older record likewise lacks.
QByteArray AircraftTracker::serializeStatistics(const AircraftStatistics& statistics)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);

    stream << (quint32) 4;      // format version
    stream << (quint32) AircraftReport::ProtocolCount;

    auto put = [&stream](const AircraftStatistics::Record& r)
    {
        stream << r.m_valid << r.m_value << r.m_aircraft << r.m_when;
        // Added in version 3
        stream << r.m_positionValid << r.m_latitude << r.m_longitude
               << r.m_altitudeValid << r.m_altitudeFt;
        stream << r.m_frequency;    // Added in version 4
    };

    const AircraftStatistics::Scope& all = statistics.m_allTime;
    for (int i = 0; i < AircraftReport::ProtocolCount; i++) {
        put(all.m_maxRange[i]);
    }
    put(all.m_fastest);
    put(all.m_highest);
    stream << all.m_maxConcurrent << all.m_maxConcurrentWhen;

    // Added in version 2
    stream << all.m_totalMessages;
    for (int i = 0; i < AircraftReport::ProtocolCount; i++) {
        stream << all.m_messagesByProtocol[i];
    }
    stream << all.m_seconds << statistics.m_firstStart;
    return data;
}

AircraftStatistics AircraftTracker::deserializeStatistics(const QByteArray& data)
{
    AircraftStatistics statistics;
    if (data.isEmpty()) {
        return statistics;
    }

    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_15);

    quint32 version = 0;
    quint32 protocols = 0;
    stream >> version >> protocols;
    // The protocol count is bounded too: a corrupt blob could otherwise ask for four
    // billion records from a stream that has already run dry. Far more than there will
    // ever be protocols, but small enough that a nonsense count is caught.
    if ((version < 1) || (version > 4) || (protocols > 256) || (stream.status() != QDataStream::Ok)) {
        return statistics;      // Written by something else, or truncated: start clean
    }

    auto get = [&stream, version](AircraftStatistics::Record& r)
    {
        stream >> r.m_valid >> r.m_value >> r.m_aircraft >> r.m_when;
        if (version >= 3)
        {
            stream >> r.m_positionValid >> r.m_latitude >> r.m_longitude
                   >> r.m_altitudeValid >> r.m_altitudeFt;
        }
        if (version >= 4) {
            stream >> r.m_frequency;
        }
    };

    AircraftStatistics::Scope& all = statistics.m_allTime;

    // Read what was written even if the protocol list has grown since; records for
    // protocols that did not exist then simply stay unset
    for (quint32 i = 0; i < protocols; i++)
    {
        AircraftStatistics::Record r;
        get(r);
        if (i < (quint32) AircraftReport::ProtocolCount) {
            all.m_maxRange[i] = r;
        }
    }
    get(all.m_fastest);
    get(all.m_highest);
    stream >> all.m_maxConcurrent >> all.m_maxConcurrentWhen;

    if (version >= 2)
    {
        stream >> all.m_totalMessages;
        for (quint32 i = 0; i < protocols; i++)
        {
            quint64 n = 0;
            stream >> n;
            if (i < (quint32) AircraftReport::ProtocolCount) {
                all.m_messagesByProtocol[i] = n;
            }
        }
        stream >> all.m_seconds >> statistics.m_firstStart;
    }

    if (stream.status() != QDataStream::Ok) {
        return AircraftStatistics();
    }
    return statistics;
}


// A 3D model for the map, for aircraft the ADS-B demodulator is not providing one for.
//
// This is the same matcher ADS-B uses - it moved to sdrbase so both can share it - and it
// works from the model name, manufacturer and operator in the OpenSky database, which is
// reached through the registration. So an aircraft heard only on ACARS, HFDL or Aero now
// gets a model too, which is most of the oceanic traffic ADS-B cannot see at all.
//
// Chosen ONCE and kept. The livery is picked at random where there is a choice, so asking
// again on every report would change the aircraft's appearance as it flew.
//
// With no model pack installed nothing matches, but the 2D livery icons are built in and
// named after the models, so the same matching is done over those - for the icon alone,
// as there is no model to send the map. It is kept the same way.
void AircraftTracker::update3DModel(TrackedAircraft *item)
{
    const bool iconsOnly = Aircraft3DModels::instance().isEmpty();
    if (!(iconsOnly ? item->m_liveryIcon : item->m_model3D).isEmpty()) {
        return;
    }
    const Aircraft3DModels &models = iconsOnly ? Aircraft3DModels::liveryIcons() : Aircraft3DModels::instance();

    const AircraftInformation *info = nullptr;
    if (m_aircraftInfo && !item->m_registration.isEmpty())
    {
        auto it = m_aircraftInfo->find(item->m_registration);
        if (it != m_aircraftInfo->end()) {
            info = it.value();
        }
    }

    QString model;
    float altitudeOffset = 0.0f;
    float labelOffset = 0.0f;
    bool matched = info && models.modelFor(
        info, m_settings.m_favourLivery, false, model, altitudeOffset, labelOffset);

    if (!matched)
    {
        // Only a fraction of types have a model of their own, and an aircraft we know
        // nothing about has none either. Rather than leave it as a flat billboard among
        // modelled aircraft, stand in something of about the right shape and size
        matched = models.defaultModel(
            info ? info->m_type : QString(), model, altitudeOffset, labelOffset);
    }

    if (matched && iconsOnly)
    {
        item->m_liveryIcon = model;
    }
    else if (matched)
    {
        item->m_model3D = model;
        item->m_modelAltitudeOffset = altitudeOffset;
        item->m_labelAltitudeOffset = labelOffset;
        item->m_modelIsOurs = true;
    }
}

// Where the receiver was for a report that arrived on this device set.
//
// A device can know where it is - a remote SDR reports the server's position - and that
// is the antenna that actually heard the aircraft, so it is preferred over My Position.
// getDevicePosition() leaves its arguments alone and returns false when the device has
// none, which makes the fallback a single call.
//
// It is cached because that helper builds an entire device report and parses it as JSON:
// far too expensive per message, but nothing here moves quickly, so a periodic refresh
// picks up a device that has just learned where it is.
QGeoCoordinate AircraftTracker::receiverPosition(int deviceSetIndex)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!m_devicePositionsRefreshed.isValid()
        || (m_devicePositionsRefreshed.secsTo(now) > 60))
    {
        m_devicePositions.clear();
        m_devicePositionsRefreshed = now;
    }

    auto cached = m_devicePositions.find(deviceSetIndex);
    if (cached != m_devicePositions.end()) {
        return cached.value();
    }

    float latitude = MainCore::instance()->getSettings().getLatitude();
    float longitude = MainCore::instance()->getSettings().getLongitude();
    float altitude = MainCore::instance()->getSettings().getAltitude();

    if (deviceSetIndex >= 0) {
        ChannelWebAPIUtils::getDevicePosition((unsigned int) deviceSetIndex, latitude, longitude, altitude);
    }

    QGeoCoordinate position(latitude, longitude, altitude);
    m_devicePositions.insert(deviceSetIndex, position);
    return position;
}

// What the web API's report says. Counted on demand rather than kept, because it is
// asked for rarely and would otherwise be another thing to keep in step.
AircraftTracker::Totals AircraftTracker::totals() const
{
    QMutexLocker locker(&m_totalsMutex);
    return m_totals;
}

// On the tracker's thread, which is the only one that may walk the aircraft
void AircraftTracker::updateTotals()
{
    Totals totals;
    totals.m_aircraft = m_aircraft.size();
    for (const TrackedAircraft *item : m_aircraft)
    {
        if (item->m_active) {
            totals.m_activeAircraft++;
        }
        totals.m_flights += item->m_flights.size();
    }
    totals.m_messages = m_statistics.m_session.m_totalMessages;
    for (int i = 0; i < AircraftReport::ProtocolCount; i++) {
        totals.m_messagesByProtocol[i] = m_statistics.m_session.m_messagesByProtocol[i];
    }
    QMutexLocker locker(&m_totalsMutex);
    m_totals = totals;
}

// Records, counters and the concurrent aircraft window
void AircraftTracker::updateStatistics(const AircraftReport& report, TrackedAircraft *item,
                                      bool rangeValid, float rangeKm,
                                      bool altitudeValid, float altitudeFt)
{
    m_statisticsDirty = true;

    const QString who = !item->m_registration.isEmpty() ? item->m_registration
                      : (!item->m_flight.isEmpty() ? item->m_flight
                         : QString("%1").arg(item->m_icao, 6, 16, QChar('0')).toUpper());
    const QDateTime when = report.m_received.isValid() ? report.m_received
                                                       : QDateTime::currentDateTime();

    // rangeValid/rangeKm describe the position THIS report carried - see the note where
    // they are worked out. A report that carried none measures nothing, which is what
    // keeps ADS-B distances from being credited to HFDL's text-only reports.
    //
    // The position stored with a record is the one THIS report carried, for the same
    // reason: it is the fix the range was measured from, so it is where the record was
    // set. Exactly 0N 0E is a zeroed field rather than a place, as it is on ingest.
    const bool positionValid = report.m_positionValid
        && ((report.m_latitude != 0.0f) || (report.m_longitude != 0.0f));

    m_statistics.m_session.update(report.m_protocol, rangeValid, rangeKm,
        item->m_speedValid, item->m_speedKts, altitudeValid, altitudeFt, who, when,
        positionValid, report.m_latitude, report.m_longitude, report.m_frequency);
    m_statistics.m_allTime.update(report.m_protocol, rangeValid, rangeKm,
        item->m_speedValid, item->m_speedKts, altitudeValid, altitudeFt, who, when,
        positionValid, report.m_latitude, report.m_longitude, report.m_frequency);

    // First time this airframe has been heard in this session - which is not the same as
    // the first time it was tracked, because the tracked list is restored from the
    // database at startup
    if (!m_sessionHeard.contains(item->m_id))
    {
        m_sessionHeard.insert(item->m_id);
        m_statistics.m_session.m_distinctAircraft = m_sessionHeard.size();
    }

    // Counted into the all time figure separately, and only once it can be identified
    // well enough to recognise again on another day
    if (!m_sessionRecorded.contains(item->m_id))
    {
        int countDelta = 0;
        if (recordSeen(item, countDelta) >= 0)
        {
            m_sessionRecorded.insert(item->m_id);
            if (countDelta != 0)
            {
                m_statistics.m_allTime.m_distinctAircraft =
                    std::max(0, m_statistics.m_allTime.m_distinctAircraft + countDelta);
                m_statisticsDirty = true;
            }
        }
    }
}

// The most aircraft heard within any 15 minute window. Counted over what has actually
// been heard recently rather than over the tracked list, which holds aircraft for far
// longer than that and would only ever report the retention period's total.
void AircraftTracker::updateConcurrent(const QDateTime& now)
{
    int concurrent = 0;
    for (const TrackedAircraft *item : m_aircraft)
    {
        if (item->m_lastSeen.isValid()
            && (item->m_lastSeen.secsTo(now) <= AircraftStatistics::ConcurrentWindowMins * 60)) {
            concurrent++;
        }
    }
    for (AircraftStatistics::Scope *scope : { &m_statistics.m_session, &m_statistics.m_allTime })
    {
        if (concurrent > scope->m_maxConcurrent)
        {
            scope->m_maxConcurrent = concurrent;
            scope->m_maxConcurrentWhen = now;
            m_statisticsDirty = true;
        }
    }
}

// Everything a rule does, in the order the rules are held. Two lists with the same
// signature address the same aircraft in the same way, whatever else has changed in the
// settings, so the record of which have already fired still means what it says.
QString AircraftTracker::notificationSignature(const AircraftSettings& settings)
{
    QStringList parts;
    for (const auto& rule : settings.m_notificationSettings)
    {
        if (rule) {
            parts.append(QString("%1\x1f%2\x1f%3\x1f%4").arg(rule->m_matchColumn)
                .arg(rule->m_regExp).arg(rule->m_speech).arg(rule->m_command));
        }
    }
    return parts.join(QChar(0x1e));
}

QString AircraftTracker::protocolName(int protocol)
{
    switch (protocol)
    {
    case AircraftReport::ADSB: return "ADS-B";
    case AircraftReport::ACARS: return "ACARS";
    case AircraftReport::VDL2: return "VDL2";
    case AircraftReport::HFDL: return "HFDL";
    case AircraftReport::AERO: return "Aero";
    case AircraftReport::SELCAL: return "SELCAL";
    case AircraftReport::IRIDIUM: return "Iridium";
    default: return "?";
    }
}

// E.g: {"ADS-B", "ACARS 131.725", "HFDL 5.720"}. ADS-B has no frequency - it is
// always 1090 MHz.
QString AircraftTracker::sourceText(int protocol, qint64 frequency)
{
    QString text = protocolName(protocol);

    if ((frequency != 0) && (protocol != (int) AircraftReport::ADSB)) {
        text += QString(" %1").arg(frequency / 1e6, 0, 'f', 3);
    }

    return text;
}

QStringList AircraftTracker::sourcesList(const SourceCounts& sources) const
{
    QStringList parts;
    // In key order already - SourceCounts is a QMap
    const QList<QPair<int, qint64>> keys = sources.keys();
    for (const auto& key : keys)
    {
        QString part = sourceText(key.first, key.second);
        if (!parts.contains(part)) {
            parts.append(part);
        }
    }
    return parts;
}

QString AircraftTracker::sourcesText(const SourceCounts& sources) const
{
    return sourcesList(sources).join("; ");
}

// Name on the Map: registration when known, otherwise flight, otherwise the ICAO
// address in hex, so successive reports update one item
// How a flight is written on the Map: either as flown (BAW123) or as ATC says it
// (SPEEDBIRD 123), which is what is heard on the radio
QString AircraftTracker::flightLabel(const QString& flight) const
{
    if (!m_settings.m_atcCallsigns || (flight.size() < 3)) {
        return flight;
    }

    const Airline *airline = Airline::getByICAO(flight.left(3));

    if (airline && !airline->m_callsign.isEmpty()) {
        return QString("%1 %2").arg(airline->m_callsign, flight.mid(3));
    }
    return flight;
}

QString AircraftTracker::mapItemName(const TrackedAircraft *item) const
{
    return aircraftMapItemName(item->m_registration, item->m_flight, item->m_icao);
}

// ATC style Map label, like the ADS-B demodulator's: identity, route, flight
// level, then speed and type
QString AircraftTracker::atcLabel(const TrackedAircraft *item) const
{
    QStringList lines;
    lines.append(!item->m_flight.isEmpty() ? flightLabel(item->m_flight) : mapItemName(item));
    if (!item->m_departure.isEmpty() && !item->m_arrival.isEmpty()) {
        lines.append(QString("%1-%2").arg(item->m_departure).arg(item->m_arrival));
    }
    if (item->m_altitudeValid && item->m_positionValid)
    {
        QChar c = item->m_altitudeFt >= 6000.0f ? 'F' : 'A';
        lines.append(QString("%1%2").arg(c).arg((int) (item->m_altitudeFt / 100.0f)));
    }
    QStringList row;
    if (item->m_speedValid) {
        row.append(QString("G%1").arg((int) item->m_speedKts));
    }
    if (m_aircraftInfo && m_aircraftInfo->contains(item->m_registration))
    {
        const QString& type = m_aircraftInfo->value(item->m_registration)->m_type;
        if (!type.isEmpty()) {
            row.append(type);
        }
    }
    if (!row.isEmpty()) {
        lines.append(row.join(" "));
    }
    return lines.join("<br>");
}

// Map icon by aircraft type, reusing the ADS-B demodulator's icon set (its qrc
// resources are process wide)
QString AircraftTracker::aircraftImage(const TrackedAircraft *item) const
{
    return aircraftImage(item->m_registration, item->m_model3D.isEmpty() ? item->m_liveryIcon : item->m_model3D);
}

QString AircraftTracker::aircraftImage(const QString& registration, const QString& model3D) const
{
    static const QStringList fourEngineTypes = {
        "A388", "B741", "B742", "B743", "B744", "B748", "B74R", "B74S", "BLCF",
        "A342", "A343", "A345", "A346", "C17", "K35R"};
    static const QStringList heavyTwinTypes = {
        "B772", "B773", "B77L", "B77W", "B788", "B789", "B78X",
        "A332", "A333", "A338", "A339", "A359", "A35K",
        "B762", "B763", "B764", "MD11", "DC10"};
    static const QStringList smallTypes = {
        "GLF4", "GLF5", "GLF6", "GLEX", "GL5T", "CL60", "C25A", "C25B", "C25C",
        "C525", "C550", "C560", "C56X", "C680", "C68A", "C750", "E50P", "E55P",
        "E545", "E550", "F2TH", "F900", "FA7X", "FA8X", "LJ35", "LJ45", "LJ60",
        "PC24", "PRM1", "HDJT"};

    // The livery icons are named after the 3D models they were rendered from
    // (e.g. BB_Airbus_png/A320/A320_BAW.gltf -> A320_BAW.png), so an aircraft's icon
    // follows its matched model's basename. Like the plain icons, they are the ADS-B
    // demodulator's resources, so may not exist in every build
    if (m_settings.m_useLiveryIcons && !model3D.isEmpty())
    {
        QString image = QString("liveries/%1.png").arg(Aircraft3DModels::liveryIconName(model3D));
        if (QFile::exists(":/map/" + image)) {
            return image;
        }
    }

    QString name = "aircraft_large.png";
    if (m_aircraftInfo && m_aircraftInfo->contains(registration))
    {
        const QString& type = m_aircraftInfo->value(registration)->m_type;
        if (fourEngineTypes.contains(type)) {
            name = "aircraft_heavy_4engine.png";
        } else if (heavyTwinTypes.contains(type)) {
            name = "aircraft_heavy_2engine.png";
        } else if (smallTypes.contains(type)) {
            name = "aircraft_small.png";
        }
    }
    // No model match - use a white version of the plain icon, so the style stays
    // consistent with the livery icons rather than dropping back to black
    if (m_settings.m_useLiveryIcons && QFile::exists(":/map/liveries/" + name)) {
        return "liveries/" + name;
    }
    return name;
}

// A position is timestamped when it was measured, an altitude when IT was measured,
// and on ADS-B those are different messages - a Mode S altitude reply carries no
// position. Pairing a fix with whatever altitude arrived most recently therefore states
// a height the aircraft had at some other moment. With the two most recent altitude
// observations bracketing the fix, the right value is a linear interpolation between
// them; outside that, the nearest is the best available and is used unchanged.
float AircraftTracker::altitudeAt(const TrackedAircraft *item, const QDateTime& when, float fallbackFt)
{
    if (!when.isValid() || !item->m_altitudeDateTime.isValid()
        || !item->m_prevAltitudeDateTime.isValid())
    {
        return fallbackFt;      // Nothing timestamped to work from
    }
    const qint64 span = item->m_prevAltitudeDateTime.msecsTo(item->m_altitudeDateTime);
    const qint64 offset = item->m_prevAltitudeDateTime.msecsTo(when);
    if ((span <= 0) || (offset < 0) || (offset > span)) {
        return fallbackFt;      // Not between the two, so nothing to interpolate
    }
    const float t = (float) offset / (float) span;
    return item->m_prevAltitudeFt + t * (item->m_altitudeFt - item->m_prevAltitudeFt);
}

// How long a position stays believable, as a CZML availability interval. Without one an
// entity is available for ever, and on the 3D map that means its model and every texture
// the model carries stay resident until something explicitly deletes it - which was
// enough to exhaust GPU texture memory and lose the WebGL context on a busy sky.
//
// The window has to match how often the protocol that gave us the position actually
// reports. ADS-B is about once a second, so the demodulator's 61 seconds is right for
// it; a position that came from ACARS, VDL-2 or HFDL can be the only one for many
// minutes, and expiring it that fast would make the aircraft flicker.
// How long the position we hold stays believable. ADS-B reports about once a second,
// so a position a minute old means we have stopped hearing the aircraft; a position
// from ACARS, VDL-2 or HFDL may be the only one for many minutes and expiring it as
// fast would leave the aircraft off the map almost always.
int AircraftTracker::positionTimeoutSecs(const TrackedAircraft *item) const
{
    const int mins = (item->m_positionRank == PositionAdsb) ? m_settings.m_adsbPositionMins
                                                            : m_settings.m_acarsPositionMins;
    // The extra second is the demodulator's, for CesiumGS/cesium#12426
    return (mins * 60) + 1;
}

bool AircraftTracker::positionStale(const TrackedAircraft *item, const QDateTime& now) const
{
    return item->m_positionDateTime.isValid()
        && (item->m_positionDateTime.secsTo(now) > positionTimeoutSecs(item));
}

QDateTime AircraftTracker::mapAvailableUntil(const TrackedAircraft *item) const
{
    if (!item->m_positionDateTime.isValid()) {
        return QDateTime();
    }
    return item->m_positionDateTime.addSecs(positionTimeoutSecs(item));
}

// The whole collated track, so it survives gaps between protocols. Each
// coordinate must carry its time - the Map dereferences it unconditionally.
// Selecting one of an aircraft's past flights draws that flight's track instead of the
// one it is on now. The icon is left where the aircraft actually is - the track is
// history, the aircraft is not
// The flight whose track is drawn, with its stored points fetched. Everything that draws
// goes through here rather than trackFlight() directly, or a restored flight would be
// drawn as however much of it has been heard since - usually nothing.
AircraftTracker::TrackedFlight *AircraftTracker::trackFlightLoaded(TrackedAircraft *item)
{
    TrackedFlight *flight = const_cast<TrackedFlight *>(trackFlight(item));
    ensureTrackLoaded(flight);
    return flight;
}

const AircraftTracker::TrackedFlight *AircraftTracker::trackFlight(const TrackedAircraft *item) const
{
    if (m_watchedFlight != 0)
    {
        for (const TrackedFlight *flight : item->m_flights)
        {
            if (flight->m_id == m_watchedFlight) {
                return flight;
            }
        }
    }
    return item->m_currentFlight;
}

// Times for the Map are sent in UTC. The Map keeps them in UTC, and a string with no
// offset is local time, which it then has to convert - a call into the C runtime's time
// zone code, under a lock Qt shares between all threads, for every item on its GUI
// thread. Free for a time that is already UTC, as track times are
static QString mapDateTime(const QDateTime& dateTime)
{
    return dateTime.toUTC().toString(Qt::ISODateWithMs);
}

QList<SWGSDRangel::SWGMapCoordinate *> *AircraftTracker::buildTrack(const TrackedFlight *flight) const
{
    QList<SWGSDRangel::SWGMapCoordinate *> *track = new QList<SWGSDRangel::SWGMapCoordinate *>();
    if (!flight) {
        return track;   // An empty list, not null: null means "unchanged" to the Map
    }
    for (int i = 0; i < flight->m_track.size(); i++)
    {
        SWGSDRangel::SWGMapCoordinate *coord = new SWGSDRangel::SWGMapCoordinate();
        coord->setLatitude(flight->m_track[i].latitude());
        coord->setLongitude(flight->m_track[i].longitude());
        coord->setAltitude(flight->m_track[i].altitude());
        // WithMs, not Qt::ISODate: that is whole seconds, and two positions in the
        // same second then reach the Map with identical times - which the Map's
        // Whittaker-Eilers smoother divides by, giving inf and then nan
        coord->setDateTime(new QString(mapDateTime(flight->m_trackTimes[i])));
        track->append(coord);
    }
    return track;
}

void AircraftTracker::sendToMap(TrackedAircraft *item)
{
    // The Map shows what we are currently hearing. Archived aircraft are taken off it
    // by setActive(), and must not be put back by any other path - nor may an aircraft
    // whose position has gone stale, which keeps sending messages that carry no new one.
    // Nor one with no position at all: positionStale() says no to that, having no time to
    // go on, and an ACARS-only aircraft given a clearance at the gate was drawn at 0N 0E.
    if (!item->m_active || !item->m_positionValid
        || (!item->m_mapReplay && positionStale(item, QDateTime::currentDateTime()))) {
        return;
    }
    // Nor while the ADS-B pass-through is supplying the item. The basic item built here
    // replaced it whole - zeroing the PFD and dropping the altitude handling, so the model
    // sank or floated - whenever a clearance or CPDLC message arrived. forwardMapItem()
    // attaches the plan and the message to the next ADS-B item instead.
    if (item->m_lastMapItemTime.isValid()
        && (item->m_lastMapItemTime.secsTo(QDateTime::currentDateTime()) < 60)) {
        return;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);

    if (mapPipes.size() == 0) {
        return;
    }

    // When the name upgrades (a logon revealed the airframe), the old item is
    // removed and the full track continues under the new name
    QString name = mapItemName(item);
    if (name.isEmpty()) {
        return;
    }
    if (!item->m_mapName.isEmpty() && (item->m_mapName != name)) {
        removeFromMap(item->m_mapName);
    }
    // Rebuilding the whole track for every report is expensive, so it is only
    // sent when it has changed; the Map keeps its stored track otherwise
    bool sendTrack = item->m_mapTrackChanged || item->m_mapTrackAppended
        || (item->m_mapName != name);
    item->m_mapName = name;

    const AircraftInformation *info =
        (m_aircraftInfo && m_aircraftInfo->contains(item->m_registration))
            ? m_aircraftInfo->value(item->m_registration) : nullptr;

    QStringList text;
    // The same picture header the ADS-B demodulator puts on its popups - a side view of
    // the type in the operator's livery, or failing that the airline's logo, beside the
    // country flag. An aircraft heard only on ACARS then reads the same as one heard on
    // ADS-B, which is the point: the popup should not say which protocol found it.
    if (info)
    {
        const QString sideview = AircraftInformation::resourcePathToURL(
            AircraftInformation::getSideviewIconPath(info->m_registration,
                                                     info->m_operatorICAO, info->m_type));
        const QString airline = AircraftInformation::resourcePathToURL(
            AircraftInformation::getAirlineIconPath(info->m_operatorICAO));
        const QString country = info->getFlag();
        const QString flag = country.isEmpty()
            ? QString() : AircraftInformation::getFlagIconURL(country);

        if (!flag.isEmpty() && !sideview.isEmpty()) {
            text.append(QString("<table width=100%><tr><td><img src=%1 width=85 height=20>"
                                "<td><img src=%2 align=right></table>").arg(sideview).arg(flag));
        } else if (!flag.isEmpty() && !airline.isEmpty()) {
            text.append(QString("<table width=100%><tr><td><img src=%1>"
                                "<td><img src=%2 align=right></table>").arg(airline).arg(flag));
        } else if (!flag.isEmpty()) {
            text.append(QString("<img src=%1>").arg(flag));
        } else if (!sideview.isEmpty()) {
            text.append(QString("<img src=%1>").arg(sideview));
        } else if (!airline.isEmpty()) {
            text.append(QString("<img src=%1>").arg(airline));
        }
    }
    // The identities are links: clicking one selects that aircraft or flight in this
    // feature, which is the question someone looking at the Map usually has next
    if (item->m_icao)
    {
        text.append(QString("ICAO: %1").arg(aircraftMapLink("icao",
            QString("%1").arg(item->m_icao, 6, 16, QChar('0')).toUpper())));
    }
    if (!item->m_registration.isEmpty()) {
        text.append(QString("Reg: %1").arg(aircraftMapLink("registration", item->m_registration)));
    }
    if (!item->m_flight.isEmpty()) {
        text.append(QString("Flight: %1").arg(aircraftMapLink("flight", item->m_flight)));
    }
    if (info)
    {
        QString type = info->m_model.isEmpty() ? info->m_type
            : (info->m_type.isEmpty() ? info->m_model
                : QString("%1 (%2)").arg(info->m_model).arg(info->m_type));
        if (!type.isEmpty()) {
            text.append(QString("Type: %1").arg(type));
        }
    }
    if (!item->m_departure.isEmpty()) {
        text.append(QString("From: %1").arg(item->m_departure));
    }
    if (!item->m_arrival.isEmpty()) {
        text.append(QString("To: %1").arg(item->m_arrival));
    }
    if (!item->m_route.isEmpty()) {
        text.append(QString("Route: %1").arg(item->m_route));
    }
    // The flight's list, not the airframe's: the Map is about what is being heard
    // now, and an airframe's all time list names protocols and frequencies it was heard
    // on weeks ago and is not being heard on at all
    QStringList sources = item->m_currentFlight
        ? sourcesList(item->m_currentFlight->m_sources) : QStringList();
    if (sources.size() == 1)
    {
        text.append(QString("Heard on: %1").arg(sources[0]));
    }
    else if (!sources.isEmpty())
    {
        text.append("Heard on:");
        text.append(sources);
    }
    // The most recent decoded message used to go on the end. An ACARS message is often
    // twenty lines of free text, which made the popup unreadable and pushed everything
    // above it off screen. It is in the feature's own Documents tab, which is where a
    // whole message belongs.

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(item->m_latitude);
        swgMapItem->setLongitude(item->m_longitude);
        swgMapItem->setAltitude(item->m_altitudeValid ? Units::feetToMetres(item->m_altitudeFt) : 0.0f);
        swgMapItem->setPositionDateTime(new QString(mapDateTime(item->m_positionDateTime)));
        const QDateTime until = mapAvailableUntil(item);
        if (until.isValid()) {
            swgMapItem->setAvailableUntil(new QString(mapDateTime(until)));
        }
        if (item->m_altitudeDateTime.isValid()) {
            swgMapItem->setAltitudeDateTime(new QString(mapDateTime(item->m_altitudeDateTime)));
        }
        swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(aircraftImage(item))));
        if (item->m_headingValid) {
            swgMapItem->setImageRotation((int) item->m_heading);
        }
        swgMapItem->setText(new QString(text.join("\n")));
        if (m_settings.m_atcLabels) {
            swgMapItem->setLabel(new QString(atcLabel(item)));
        } else {
            swgMapItem->setLabel(new QString(item->m_flight.isEmpty() ? name : item->m_flight));
        }
        if (sendTrack) {
            swgMapItem->setTrack(buildTrack(trackFlightLoaded(item)));
        }

        // Engine sound, looped by the 3D map, spatially located at the aircraft
        const QString engineSound = AircraftEngineSound::engineSound(
            info ? info->m_type : QString(), QString(), info ? info->m_model : QString());
        if (!engineSound.isEmpty()) {
            swgMapItem->setLoopSound(new QString(engineSound));
        }

        // Keep using the 3D model a pass-through provided, so the 3D map shows
        // the model rather than falling back to a flat billboard of the icon
        if (!item->m_model3D.isEmpty())
        {
            swgMapItem->setModel(new QString(item->m_model3D));
            // Without these the model sits half underground: 0,0,0 is the middle of the
            // model rather than the bottom of its undercarriage. They were never sent
            // from here, so even a pass-through model was at the wrong height whenever
            // this feature re-sent it.
            swgMapItem->setModelAltitudeOffset(item->m_modelAltitudeOffset);
            swgMapItem->setLabelAltitudeOffset(item->m_labelAltitudeOffset);
            if (item->m_headingValid)
            {
                swgMapItem->setOrientation(1);
                swgMapItem->setHeading(item->m_heading);
            }
        }

        // A minimal aircraft state to carry the filed route for the Map's ND and the
        // latest datalink message for its DCDU, built only when there is something to
        // send - this path is for aircraft heard on protocols that do not build the
        // full state the ADS-B pass-through has
        SWGSDRangel::SWGMapFlightPlan *plan = buildFlightPlan(item);
        SWGSDRangel::SWGMapDatalinkMessage *datalinkMessage = buildDatalinkMessage(item);
        if (plan || datalinkMessage)
        {
            SWGSDRangel::SWGMapAircraftState *aircraftState = new SWGSDRangel::SWGMapAircraftState();
            aircraftState->setCallsign(new QString(item->m_flight.isEmpty() ? name : item->m_flight));
            if (info && !info->m_type.isEmpty()) {
                aircraftState->setAircraftType(new QString(info->m_type));
            }
            aircraftState->setOnSurface(-1);
            if (item->m_altitudeValid)
            {
                aircraftState->setAltitude(item->m_altitudeFt);
                if (item->m_altitudeDateTime.isValid()) {
                    aircraftState->setAltitudeDateTime(new QString(mapDateTime(item->m_altitudeDateTime)));
                }
            }
            if (item->m_headingValid) {
                aircraftState->setTrack(item->m_heading);
            }
            if (item->m_speedValid) {
                aircraftState->setGroundspeed(item->m_speedKts);
            }
            if (plan) {
                aircraftState->setFlightPlan(plan);
            }
            if (datalinkMessage) {
                aircraftState->setDatalinkMessage(datalinkMessage);
            }
            swgMapItem->setAircraftState(aircraftState);
        }

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
        messageQueue->push(msg);
    }
    if (sendTrack)
    {
        item->m_mapTrackChanged = false;
        item->m_mapTrackAppended = false;
        item->m_mapTrackSentMs = QDateTime::currentMSecsSinceEpoch();
    }
}

// Forward a fully-built map item from a demodulator (the ADS-B pass-through, which
// carries the 3D model and full aircraft state for the Map's PFD) to the Map,
// renamed to this feature's naming and enriched with the collated track and route
// facts. Takes ownership of the item.
template <typename T>
static QList<T *> *cloneSWGList(QList<T *> *list)
{
    if (!list) {
        return nullptr;
    }
    QList<T *> *copy = new QList<T *>();
    for (T *element : *list)
    {
        T *elementCopy = new T();
        QJsonObject *json = element->asJsonObject();
        elementCopy->fromJsonObject(*json);
        delete json;
        copy->append(elementCopy);
    }
    return copy;
}

// A copy for each additional Map feature. The lists are not left to the JSON round trip:
// the default constructor leaves them null, which fromJsonObject dereferences as soon as
// the JSON has an array for them, and asJsonObject drops an empty list - which to the Map
// means "clear the track", where null means "unchanged".
static SWGSDRangel::SWGMapItem *cloneMapItem(SWGSDRangel::SWGMapItem *item)
{
    SWGSDRangel::SWGMapItem *copy = new SWGSDRangel::SWGMapItem();
    QJsonObject *json = item->asJsonObject();
    json->remove("track");
    json->remove("predictedTrack");
    json->remove("animations");
    json->remove("coordinates");
    copy->fromJsonObject(*json);
    delete json;
    copy->setTrack(cloneSWGList(item->getTrack()));
    copy->setPredictedTrack(cloneSWGList(item->getPredictedTrack()));
    copy->setAnimations(cloneSWGList(item->getAnimations()));
    copy->setCoordinates(cloneSWGList(item->getCoordinates()));
    return copy;
}

void AircraftTracker::forwardMapItem(TrackedAircraft *item, SWGSDRangel::SWGMapItem *swgMapItem)
{
    // ADS-B marks every CSV replay state with its frame time. The tracked position is
    // historical too, so comparing it with wall-clock time would discard the item here
    // (and again in removeOldAircraft()) before either its track or PFD history reached
    // the 3D Map. A later live pass-through has no state date and restores normal expiry.
    SWGSDRangel::SWGMapAircraftState *aircraftState = swgMapItem->getAircraftState();
    const bool replay = aircraftState && aircraftState->getDateTime()
        && !aircraftState->getDateTime()->isEmpty();
    item->m_mapReplay = replay;

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);

    QString name = mapItemName(item);
    if ((mapPipes.size() == 0) || name.isEmpty()
        || (!replay && positionStale(item, QDateTime::currentDateTime())))
    {
        delete swgMapItem;
        return;
    }
    if (!item->m_mapName.isEmpty() && (item->m_mapName != name)) {
        removeFromMap(item->m_mapName);
    }
    // Rebuilding the whole track for every report is expensive, so it is only
    // sent when it has changed; the Map keeps its stored track otherwise. Here, at ADS-B
    // rates, a track that has merely grown is only sent every few seconds: building it
    // for every position was O(n) per report and O(n^2) per flight. In between, the Map
    // draws a provisional point from the last one sent to the live position, so the line
    // still reaches the aircraft.
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    bool sendTrack = item->m_mapTrackChanged || (item->m_mapName != name)
        || (item->m_mapTrackAppended && ((nowMs - item->m_mapTrackSentMs) >= 5000));

    // A model from the pass-through is remembered for the items built here. It used to be
    // removed and re-added when it replaced a billboard, since a CZML packet only adds to
    // an entity and both would show - but the Map now hides the one it is not drawing,
    // and the removal threw away the Map's history for the aircraft.
    QString *model = swgMapItem->getModel();
    bool hasModel = (model != nullptr) && !model->isEmpty();
    if (hasModel)
    {
        item->m_model3D = *model;
        // ... and what goes with it. Without these the item built here on the next quiet
        // period, and the row written to the database, carry a zero offset or the offsets
        // of whichever model this one replaced - so the aircraft sits at the wrong height
        // as soon as the pass-through stops.
        item->m_modelAltitudeOffset = swgMapItem->getModelAltitudeOffset();
        item->m_labelAltitudeOffset = swgMapItem->getLabelAltitudeOffset();
        item->m_modelIsOurs = false;
    }
    else if (swgMapItem->getImage())
    {
        // With no model pack, ADS-B picks a livery icon in place of a model, and that is
        // remembered the same way - otherwise the icon would switch to the one picked
        // here whenever the pass-through went quiet. Not the plain icons in white, which
        // are what it sends when it found nothing
        static const QString liveries = QStringLiteral("qrc:///map/liveries/");
        const QString &image = *swgMapItem->getImage();
        if (image.startsWith(liveries))
        {
            const QString icon = QFileInfo(image).completeBaseName();
            if (!icon.startsWith("aircraft_")) {
                item->m_liveryIcon = icon;
            }
        }
    }
    item->m_mapName = name;
    item->m_mapResync = false;
    m_mapResyncPending.remove(item);

    if (swgMapItem->getName()) {
        *swgMapItem->getName() = name;
    } else {
        swgMapItem->setName(new QString(name));
    }

    // Both paths describe the same entity, so they must agree on how long it lives
    const QDateTime until = mapAvailableUntil(item);
    if (until.isValid())
    {
        if (swgMapItem->getAvailableUntil()) {
            *swgMapItem->getAvailableUntil() = mapDateTime(until);
        } else {
            swgMapItem->setAvailableUntil(new QString(mapDateTime(until)));
        }
    }

    // Add what other protocols have revealed to the popup
    QStringList extra;
    QString text = swgMapItem->getText() ? *swgMapItem->getText() : QString();
    if (!item->m_departure.isEmpty() && !text.contains(item->m_departure)) {
        extra.append(QString("From: %1").arg(item->m_departure));
    }
    if (!item->m_arrival.isEmpty() && !text.contains(item->m_arrival)) {
        extra.append(QString("To: %1").arg(item->m_arrival));
    }
    if (!item->m_route.isEmpty() && !text.contains(item->m_route)) {
        extra.append(QString("Route: %1").arg(item->m_route));
    }
    // The flight's list, for the same reason as in sendToMap()
    QStringList sources = item->m_currentFlight
        ? sourcesList(item->m_currentFlight->m_sources) : QStringList();
    if (sources.size() == 1)
    {
        extra.append(QString("Heard on: %1").arg(sources[0]));
    }
    else if (!sources.isEmpty())
    {
        extra.append("Heard on:");
        extra.append(sources);
    }
    if (swgMapItem->getText()) {
        swgMapItem->getText()->append("\n" + extra.join("\n"));
    } else {
        swgMapItem->setText(new QString(extra.join("\n")));
    }

    // The label is always ours, not the demodulator's: its own ATC labels
    // setting would otherwise override this feature's on every update
    QString label = m_settings.m_atcLabels ? atcLabel(item)
        : (item->m_flight.isEmpty() ? name : item->m_flight);
    if (swgMapItem->getLabel()) {
        *swgMapItem->getLabel() = label;
    } else {
        swgMapItem->setLabel(new QString(label));
    }

    // The filed route and latest datalink message, for the Map's ND and DCDU, which
    // the demodulator's state does not have - both come by ACARS and are collated here
    if (swgMapItem->getAircraftState())
    {
        SWGSDRangel::SWGMapFlightPlan *plan = buildFlightPlan(item);
        if (plan) {
            swgMapItem->getAircraftState()->setFlightPlan(plan);
        }
        SWGSDRangel::SWGMapDatalinkMessage *message = buildDatalinkMessage(item);
        if (message) {
            swgMapItem->getAircraftState()->setDatalinkMessage(message);
        }
    }

    if (sendTrack)
    {
        swgMapItem->setTrack(buildTrack(trackFlightLoaded(item)));
        item->m_mapTrackChanged = false;
        item->m_mapTrackAppended = false;
        item->m_mapTrackSentMs = nowMs;
    }

    // Each message takes ownership of its item, so additional Map features get a
    // copy
    QList<SWGSDRangel::SWGMapItem *> items;
    items.append(swgMapItem);
    for (int i = 1; i < mapPipes.size(); i++) {
        items.append(cloneMapItem(swgMapItem));
    }
    for (int i = 0; i < mapPipes.size(); i++)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(mapPipes[i]->m_element);
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, items[i]);
        messageQueue->push(msg);
    }
}

// The furthest an aircraft has ever been heard, drawn at the position it was in when
// it set the record. Unlike everything else this feature puts on the Map this is not an
// aircraft that is flying now - it marks a place - so it is a fixed position with no
// track and no expiry, and it stays there until the record is beaten, the records are
// reset, or the option is turned off.
//
// Records set before version 3 of the statistics blob have no position stored, so the
// best record that CAN be drawn is used rather than the best overall - otherwise
// upgrading would leave the option doing nothing until the old record was beaten.
const char *AircraftTracker::RecordMapItemName = "Max range record";
// Has to match a group the Map knows, or it falls back to this feature's own - see
// MapSettings::MapSettings() and MapItem.group in the API
const char *AircraftTracker::RouteMapGroup = "Aircraft Routes";

const char *AircraftTracker::CoverageMapGroup = "Aircraft Coverage";

// One colour per protocol, chosen to stay apart from each other and from the orange the
// aircraft themselves are drawn in. Alpha is low because the polygons overlap - VHF
// coverage sits inside HFDL's - and an opaque fill would hide whatever is underneath.
QRgb AircraftTracker::coverageColor(int protocol)
{
    switch (protocol)
    {
    case AircraftReport::ADSB: return qRgba(244, 151,  57, 80);     // Orange
    case AircraftReport::ACARS: return qRgba( 80, 200,  80, 80);    // Green
    case AircraftReport::VDL2: return qRgba( 70, 140, 255, 80);     // Blue
    case AircraftReport::HFDL: return qRgba(200,  80, 200, 80);     // Magenta
    case AircraftReport::AERO: return qRgba( 80, 220, 220, 80);     // Cyan
    case AircraftReport::IRIDIUM: return qRgba(220,  60,  60, 80);  // Red
    default: return qRgba(200, 200, 200, 80);
    }
}

// The furthest this protocol has heard an aircraft on this bearing. Called with the
// range that was just measured, from the receiver that measured it.
void AircraftTracker::updateCoverage(int protocol, const QGeoCoordinate& receiver,
                                     float azimuthDeg, float rangeKm)
{
    if ((protocol < 0) || (protocol >= AircraftReport::ProtocolCount)
        || !receiver.isValid() || (rangeKm <= 0.0f)) {
        return;
    }

    if (m_coverageRangeKm[protocol].isEmpty()) {
        m_coverageRangeKm[protocol].fill(0.0f, m_coverageBins);
    }
    // The receiver is remembered from the first report that measured anything, and then
    // left alone: moving the centre would move every bin's endpoint with it, so a
    // relocated receiver invalidates the pattern rather than shifting it. Clearing is
    // the user's call - the button offers it.
    if (!m_coverageOrigin[protocol].isValid()) {
        m_coverageOrigin[protocol] = receiver;
    }

    // azimuthTo() gives 0 to 360; 360 itself would index off the end
    int bin = (int) (azimuthDeg / m_coverageBinDeg);
    if (bin < 0) {
        bin = 0;
    } else if (bin >= m_coverageBins) {
        bin = m_coverageBins - 1;
    }

    if (rangeKm > m_coverageRangeKm[protocol][bin])
    {
        m_coverageRangeKm[protocol][bin] = rangeKm;
        m_coverageDirty[protocol] = true;
    }
}

// A polygon per protocol, two points per bin so each is a flat step at its own range
// rather than a line to the next bin's - the same shape the ADS-B demodulator draws.
// One shape per RUN of adjacent bearings that were actually measured, rather than one
// outline around the lot.
//
// Joining across bearings nothing was heard on cannot be done honestly. Taking the gap
// to zero draws a star of thin spikes back to the receiver; interpolating across it
// draws a smooth arc at roughly constant range through directions that were never
// listened to at all - which on HFDL, measured on a handful of bearings, claimed a
// clean 180 degree arc across the whole western sky. Both are inventions. A wedge per
// run says exactly what was heard and nothing about what was not: where every bearing
// has been measured, as ADS-B's have, the runs join up and it is a single filled shape
// again.
void AircraftTracker::sendCoverageToMap(int protocol)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);
    if (mapPipes.isEmpty()) {
        return;
    }

    const QVector<float>& ranges = m_coverageRangeKm[protocol];

    // Which bearings were measured, as runs of adjacent bins. Rotated so a run never
    // straddles the start of the array, or the one covering north would come out as two.
    QList<QPair<int, int>> runs;        // First bin, and how many
    const int bins = ranges.size();
    int measured = 0;
    for (int i = 0; i < bins; i++)
    {
        if (ranges[i] > 0.0f) {
            measured++;
        }
    }

    if (measured && m_settings.m_displayCoverage && m_settings.coverageShown(protocol)
        && m_coverageOrigin[protocol].isValid())
    {
        int first = 0;
        if (measured < bins)
        {
            // Start at a bin whose predecessor is empty, so runs are whole
            while (!((ranges[first] > 0.0f)
                     && (ranges[(first + bins - 1) % bins] <= 0.0f))) {
                first++;
            }
        }
        int i = 0;
        while (i < bins)
        {
            const int bin = (first + i) % bins;
            if (ranges[bin] <= 0.0f)
            {
                i++;
                continue;
            }
            int length = 0;
            while ((i + length < bins) && (ranges[(first + i + length) % bins] > 0.0f)) {
                length++;
            }
            runs.append(qMakePair(bin, length));
            i += length;
        }
    }

    // Whatever was drawn last time and is not being drawn now has to come off - the
    // number of runs changes as bearings fill in
    QStringList drawn;
    float furthest = 0.0f;
    for (int i = 0; i < bins; i++) {
        furthest = std::max(furthest, ranges[i]);
    }

    for (int r = 0; r < runs.size(); r++)
    {
        const int firstBin = runs[r].first;
        const int length = runs[r].second;
        // Numbered, because one protocol can need many. The name is what the Map keys
        // on, so it has to be stable for a given run index.
        const QString name = QString("Coverage %1 %2").arg(protocolName(protocol)).arg(r);
        drawn.append(name);

        QStringList text;
        text.append(QString("%1 coverage").arg(protocolName(protocol)));
        text.append(QString("Bearings: %1 to %2 degrees")
            .arg(firstBin * m_coverageBinDeg)
            .arg(((firstBin + length) % bins) * m_coverageBinDeg));
        float runFurthest = 0.0f;
        for (int i = 0; i < length; i++) {
            runFurthest = std::max(runFurthest, ranges[(firstBin + i) % bins]);
        }
        text.append(QString("Furthest here: %1 km").arg(runFurthest, 0, 'f', 1));
        text.append(QString("Furthest overall: %1 km").arg(furthest, 0, 'f', 1));
        text.append(QString("Measured from: %1, %2")
            .arg(m_coverageOrigin[protocol].latitude(), 0, 'f', 4)
            .arg(m_coverageOrigin[protocol].longitude(), 0, 'f', 4));

        for (const auto& pipe : mapPipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

            SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
            swgMapItem->setName(new QString(name));
            swgMapItem->setGroup(new QString(CoverageMapGroup));
            swgMapItem->setLabel(new QString(""));
            swgMapItem->setText(new QString(text.join("\n")));
            swgMapItem->setLatitude(m_coverageOrigin[protocol].latitude());
            swgMapItem->setLongitude(m_coverageOrigin[protocol].longitude());
            swgMapItem->setAltitude(0.0);
            swgMapItem->setImage(new QString("none"));
            swgMapItem->setImageRotation(0);
            swgMapItem->setFixedPosition(true);
            swgMapItem->setAltitudeReference(1);     // Clamp to ground
            swgMapItem->setColorValid(1);
            swgMapItem->setColor(coverageColor(protocol));

            QList<SWGSDRangel::SWGMapCoordinate *> *polygon = new QList<SWGSDRangel::SWGMapCoordinate *>();
            auto at = [&](double bearing, double km)
            {
                const QGeoCoordinate c =
                    m_coverageOrigin[protocol].atDistanceAndAzimuth(km * 1000.0, bearing);
                SWGSDRangel::SWGMapCoordinate *p = new SWGSDRangel::SWGMapCoordinate();
                p->setLatitude(c.latitude());
                p->setLongitude(c.longitude());
                p->setAltitude(0.0);
                polygon->append(p);
            };

            // A run short of the full circle is a wedge, so it starts and ends at the
            // receiver; one that goes all the way round is a ring and must not, or the
            // shape is slit open along one bearing.
            //
            // Every edge is stepped, at most 250 km a step: the radials, where one bin's
            // range steps to the next, as well as each bin's arc. The 2D map draws a
            // straight line between consecutive vertices, and a constant bearing over
            // thousands of miles is a curve on it - HFDL's step from 2,000 km to 9,500 km
            // on one bearing came out as a straight band across Asia where the bearing
            // runs up over the Arctic. Only the two ends of a wedge used to be stepped,
            // which left out every radial inside a run - and once every bearing has been
            // heard, the whole pattern is one run.
            const bool wedge = (length < bins);
            const float stepKm = 250.0f;
            // From the point already added, along the bearing, to toKm
            auto radial = [&](double bearing, float fromKm, float toKm, bool includeEnd)
            {
                if (fromKm == toKm) {
                    return;
                }
                const int steps = std::max(1, (int) std::ceil(std::abs(toKm - fromKm) / stepKm));
                const int last = includeEnd ? steps : (steps - 1);
                for (int k = 1; k <= last; k++) {
                    at(bearing, fromKm + (toKm - fromKm) * k / (float) steps);
                }
            };
            // From the point already added, round one bin at constant range
            auto arc = [&](double fromBearing, float km)
            {
                const double arcKm = km * m_coverageBinDeg * M_PI / 180.0;
                const int steps = std::max(1, (int) std::ceil(arcKm / stepKm));
                for (int k = 1; k <= steps; k++) {
                    at(fromBearing + m_coverageBinDeg * k / (double) steps, km);
                }
            };

            const double firstBearing = firstBin * m_coverageBinDeg;
            if (wedge)
            {
                at(firstBearing, 0.0);
                radial(firstBearing, 0.0f, ranges[firstBin], true);
            }
            else
            {
                at(firstBearing, ranges[firstBin]);
            }
            for (int i = 0; i < length; i++)
            {
                const int bin = (firstBin + i) % bins;
                // A flat step at the bin's own range, so each bearing shows its own figure
                if (i > 0) {
                    radial(bin * m_coverageBinDeg, ranges[(bin + bins - 1) % bins], ranges[bin], true);
                }
                arc(bin * m_coverageBinDeg, ranges[bin]);
            }
            // Back to where it started, which the polygon closes onto itself
            const int lastBin = (firstBin + length - 1) % bins;
            if (wedge) {
                radial((lastBin + 1) * m_coverageBinDeg, ranges[lastBin], 0.0f, false);
            } else {
                radial(firstBearing, ranges[lastBin], ranges[firstBin], false);
            }

            swgMapItem->setCoordinates(polygon);
            swgMapItem->setType(2);                  // Polygon

            MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
            messageQueue->push(msg);
        }
    }

    for (const QString& previous : m_coverageDrawnNames[protocol])
    {
        if (!drawn.contains(previous)) {
            removeFromMap(previous);
        }
    }
    m_coverageDrawnNames[protocol] = drawn;
}

void AircraftTracker::sendCoverageToMap()
{
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        sendCoverageToMap(i);
        m_coverageDirty[i] = false;
    }
}

void AircraftTracker::removeCoverageFromMap()
{
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        for (const QString& name : m_coverageDrawnNames[i]) {
            removeFromMap(name);
        }
        m_coverageDrawnNames[i].clear();
    }
}

// Start again, which is what the button's right click offers - after moving the antenna,
// or when a stray position has thrown the outline out to somewhere it never reached
void AircraftTracker::clearCoverage()
{
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        m_coverageRangeKm[i].fill(0.0f, m_coverageBins);
        m_coverageOrigin[i] = QGeoCoordinate();
        m_coverageDirty[i] = true;
    }
    removeCoverageFromMap();
    m_coverageChanged = true;
}

// A newly downloaded aircraft database. removeOldAircraft() would pick it up within the
// minute, but the aircraft already tracked keep the model or icon they were given, and
// one matched without the database - no type, no operator - was only ever a stand-in.
// So those we chose are matched again now, from what the database says
void AircraftTracker::reloadAircraftDatabase()
{
    m_aircraftInfo = OsnDB::getAircraftInformationByReg();
    m_aircraftInfoByIcao = OsnDB::getAircraftInformation();
    m_routeInfo = OsnDB::getAircraftRouteInformation();

    for (TrackedAircraft *item : m_aircraft)
    {
        if (item->m_modelIsOurs || !item->m_liveryIcon.isEmpty())
        {
            if (item->m_modelIsOurs) {
                item->m_model3D = "";
            }
            item->m_modelIsOurs = false;
            item->m_liveryIcon = "";
            update3DModel(item);
        }
        m_dirtyAircraft.insert(item);
    }
}




// Selecting one of an aircraft's past flights draws that flight's track. Where the
// aircraft is still being heard that comes for free - trackFlight() hands the watched
// flight to the aircraft's own map item - but an ARCHIVED aircraft is not on the Map at
// all, so there was nothing to hang the track on and selecting one of its flights showed
// a profile and no track.
//
// So the track goes out on its own, as a line of its own, exactly as a filed route does.
// Not by relaxing the guard in sendToMap(): that guard is what keeps archived aircraft
// off the Map, and an icon sitting at a position from hours ago would say the aircraft is
// there, which a historic track does not.
void AircraftTracker::updateWatchedTrack()
{
    TrackedFlight *flight = m_watchedFlight ? flightById(m_watchedFlight) : nullptr;
    TrackedAircraft *item = flight ? flight->m_aircraft : nullptr;

    // Only when the aircraft is not drawn itself, or its own item would carry the same
    // track and the two would be drawn one on top of the other
    const bool aircraftDrawn = item && item->m_active && item->m_positionValid
        && (item->m_mapReplay || !positionStale(item, QDateTime::currentDateTime()));

    if (!flight || !item || aircraftDrawn)
    {
        if (!m_watchedTrackName.isEmpty())
        {
            removeFromMap(m_watchedTrackName);
            m_watchedTrackName.clear();
            m_watchedTrackKey.clear();
        }
        return;
    }

    ensureTrackLoaded(flight);
    if (flight->m_track.size() < 2)
    {
        // One point is not a line, and drawing it draws nothing
        if (!m_watchedTrackName.isEmpty())
        {
            removeFromMap(m_watchedTrackName);
            m_watchedTrackName.clear();
            m_watchedTrackKey.clear();
        }
        return;
    }

    const QString name = QString("%1 Track").arg(
        flight->m_flight.isEmpty() ? regFor(item) : flight->m_flight);
    // The flight and how much of it there is: the track only grows, so its length is
    // enough to notice a change without comparing every point three times a second
    const QString key = QString("%1|%2|%3").arg(name).arg(flight->m_id).arg(flight->m_track.size());
    if ((key == m_watchedTrackKey) && !m_watchedTrackName.isEmpty()) {
        return;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);
    if (mapPipes.isEmpty()) {
        return;             // No Map open: try again when there is one
    }

    // A rename means the old line is a different item, and the Map keys on the name
    if (!m_watchedTrackName.isEmpty() && (m_watchedTrackName != name)) {
        removeFromMap(m_watchedTrackName);
    }
    m_watchedTrackName = name;
    m_watchedTrackKey = key;

    QStringList text;
    text.append("Flight track");
    text.append(QString("Flight: %1").arg(flight->m_flight.isEmpty() ? regFor(item) : flight->m_flight));
    if (!flight->m_departure.isEmpty() && !flight->m_arrival.isEmpty()) {
        text.append(QString("Route: %1 to %2").arg(flight->m_departure).arg(flight->m_arrival));
    }
    if (flight->m_firstSeen.isValid() && flight->m_lastSeen.isValid())
    {
        text.append(QString("Heard: %1 to %2")
            .arg(flight->m_firstSeen.toString("yyyy-MM-dd hh:mm"))
            .arg(flight->m_lastSeen.toString("hh:mm")));
    }
    text.append(QString("Points: %1").arg(flight->m_track.size()));

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLabel(new QString(name));
        swgMapItem->setText(new QString(text.join("\n")));
        swgMapItem->setLatitude(flight->m_track[0].latitude());
        swgMapItem->setLongitude(flight->m_track[0].longitude());
        swgMapItem->setAltitude(flight->m_track[0].altitude());
        swgMapItem->setImage(new QString("none"));
        swgMapItem->setImageRotation(0);
        swgMapItem->setFixedPosition(true);
        // Absolute rather than clamped: unlike a filed route, a flown track has real
        // altitudes and on the 3D map they are most of what makes it worth looking at
        swgMapItem->setAltitudeReference(0);

        QList<SWGSDRangel::SWGMapCoordinate *> *line = new QList<SWGSDRangel::SWGMapCoordinate *>();
        for (const auto& coord : flight->m_track)
        {
            SWGSDRangel::SWGMapCoordinate *c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(coord.latitude());
            c->setLongitude(coord.longitude());
            // Every coordinate must carry a finite altitude: Qt writes a non-finite
            // double into the Map's CZML as null, which Cesium rejects outright
            c->setAltitude(std::isfinite(coord.altitude()) ? coord.altitude() : 0.0);
            line->append(c);
        }
        swgMapItem->setCoordinates(line);
        // A track rather than a plain line, so the Map colours it by altitude out of the
        // group's palette, the way it colours the track under a live aircraft. A filed
        // route stays a line: its waypoints carry no altitude to colour it by.
        swgMapItem->setType(4);

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
        messageQueue->push(msg);
    }
}

void AircraftTracker::updateRecordMapItem()
{
    const AircraftStatistics::Record *best = nullptr;
    int bestProtocol = -1;

    if (m_settings.m_displayMaxRangeOnMap)
    {
        // The record, not the furthest record that happens to be placeable. Skipping the
        // unplaceable ones put a 301 km ACARS marker on the Map while the statistics
        // table said the record was 7281 km on HFDL - two different answers to the same
        // question, which is worse than no marker at all.
        for (int i = 0; i < AircraftReport::ProtocolCount; i++)
        {
            const AircraftStatistics::Record& r = m_statistics.m_allTime.m_maxRange[i];
            if (r.m_valid && (!best || (r.m_value > best->m_value)))
            {
                best = &r;
                bestProtocol = i;
            }
        }
    }

    // A record set before the position was stored alongside it has nowhere to be drawn.
    // Nothing is drawn until it is beaten, rather than something that contradicts the
    // table - said once, since this is checked several times a second.
    if (best && !best->m_positionValid)
    {
        if (m_recordMapState != RecordUnplaceable)
        {
            qDebug() << "AircraftTracker: the maximum range record -" << best->m_value
                     << "km, set by" << best->m_aircraft
                     << "on" << protocolName(bestProtocol)
                     << "- was set before positions were kept with records, so there is"
                     << "nowhere to draw it. It will appear once the record is beaten,"
                     << "or reset the all time statistics to start again.";
            m_recordMapState = RecordUnplaceable;
        }
        best = nullptr;
    }
    else if (!best && m_settings.m_displayMaxRangeOnMap && (m_recordMapState != RecordNone))
    {
        qDebug() << "AircraftTracker: no maximum range record to draw on the Map yet";
        m_recordMapState = RecordNone;
    }

    if (!best)
    {
        if (m_recordOnMap)
        {
            removeFromMap(RecordMapItemName);
            m_recordOnMap = false;
            m_recordMapKey.clear();
        }
        return;
    }

    // flush() runs three times a second, and the record changes perhaps twice a day
    const QString key = QString("%1|%2|%3|%4|%5")
        .arg(best->m_aircraft)
        .arg(best->m_value, 0, 'f', 3)
        .arg(best->m_latitude, 0, 'f', 5)
        .arg(best->m_longitude, 0, 'f', 5)
        .arg(bestProtocol);
    if (m_recordOnMap && (key == m_recordMapKey)) {
        return;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);
    if (mapPipes.isEmpty()) {
        return;         // No Map open: try again when there is one
    }
    m_recordMapKey = key;
    m_recordOnMap = true;
    if (m_recordMapState != RecordDrawn)
    {
        qDebug() << "AircraftTracker: drawing the maximum range record on the Map -"
                 << best->m_value << "km, set by" << best->m_aircraft
                 << "on" << protocolName(bestProtocol)
                 << "at" << best->m_latitude << best->m_longitude;
        m_recordMapState = RecordDrawn;
    }

    // The record names the aircraft as its registration where it had one, otherwise its
    // flight number - both of which the key map is built on, so the airframe can be found
    // again where it is still tracked and its own icon used, livery and all. Where it is
    // not, the type still comes from the registration, and the icon is the plain one in
    // white rather than the black it used to be - which is the same fallback a tracked
    // aircraft with no model match gets.
    const TrackedAircraft *holder = m_byKey.value(regKey(best->m_aircraft), nullptr);
    if (!holder) {
        holder = m_byKey.value(flightKey(best->m_aircraft), nullptr);
    }
    const QString recordImage = holder ? aircraftImage(holder)
                                       : aircraftImage(best->m_aircraft, QString());

    QStringList text;
    text.append(QString("<b>Maximum range record</b>"));
    text.append(QString("Aircraft: %1").arg(best->m_aircraft));
    text.append(QString("Range: %1 km").arg(best->m_value, 0, 'f', 1));
    text.append(QString("Protocol: %1").arg(protocolName(bestProtocol)));
    if (best->m_frequency) {
        text.append(QString("Frequency: %1 MHz").arg(best->m_frequency / 1e6, 0, 'f', 3));
    }
    if (best->m_altitudeValid) {
        text.append(QString("Altitude: %1 ft").arg((int) std::round(best->m_altitudeFt)));
    }
    if (best->m_when.isValid()) {
        text.append(QString("When: %1").arg(best->m_when.toString("yyyy-MM-dd hh:mm:ss")));
    }

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(RecordMapItemName));
        swgMapItem->setLatitude(best->m_latitude);
        swgMapItem->setLongitude(best->m_longitude);
        swgMapItem->setAltitude(best->m_altitudeValid
                                ? Units::feetToMetres(best->m_altitudeFt) : 0.0f);
        swgMapItem->setFixedPosition(1);
        swgMapItem->setImage(new QString(QString("qrc:///map/%1").arg(recordImage)));
        swgMapItem->setLabel(new QString(QString("%1 %2 km")
            .arg(best->m_aircraft).arg(best->m_value, 0, 'f', 0)));
        swgMapItem->setText(new QString(text.join("\n")));
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
        messageQueue->push(msg);
    }
}

// Place the points of a filed route, dropping any that cannot be placed.
//
// A position the message itself gave is used in preference to anything else: a route
// insert places most of its fixes, which settles which of two identically named fixes
// was meant and covers fixes no database here holds. Failing that the name is looked up
// - airports are four characters, waypoints five, VOR and NDB idents three, and an
// oceanic position seven (5530N, i.e. 55 North 030 West), the same rules the ACARS
// demodulator applied to an oceanic clearance, since a route is written the same way.
//
// EVERY coordinate is given an altitude. QGeoCoordinate's two argument constructor
// leaves altitude as qQNaN() while still reporting isValid(), and Qt writes a non-finite
// double into the Map's CZML as null, which Cesium rejects outright - taking down the
// whole 3D map rather than misplacing one route.
// Roughly where a run of route names is, taken from the first of them the waypoint
// database can place. Used to choose between navaids sharing an ident, which have no
// equivalent of findWayPoint()'s disambiguation - and one neighbour is enough, since the
// copies of an ident are hundreds of miles apart.
//
// The nearest placeable neighbour rather than an average of them: averaging longitudes
// puts a route either side of the antimeridian in the Gulf of Guinea. The names arrive
// nearest first, so the first hit is the closest one that could be placed.
//
// Waypoints only. An airway name is in no database, and looking a three letter ident up
// here would be the very ambiguity this exists to resolve.
// "58N030W" is 58 North 030 West, "5730N020W" is 57 degrees 30 minutes North. Written
// out once because the position is now read both to place the fix and, before that, to
// give the named fixes around it something to be judged against.
static QGeoCoordinate oceanicPosition(const QRegularExpressionMatch& match)
{
    float latitude = match.captured(1).toInt() + match.captured(2).toInt() / 60.0f;
    if (match.captured(3) == "S") {
        latitude = -latitude;
    }
    float longitude = match.captured(4).toInt() + match.captured(5).toInt() / 60.0f;
    if (match.captured(6) == "W") {
        longitude = -longitude;
    }
    return QGeoCoordinate(latitude, longitude, 0.0);
}

QGeoCoordinate AircraftTracker::routeAnchor(const QStringList& names)
{
    for (const QString& name : names)
    {
        if (name.size() != 5) {
            continue;
        }
        const Waypoint *waypoint = Waypoints::findWayPoint(name, names);
        if (waypoint) {
            return QGeoCoordinate(waypoint->m_latitude, waypoint->m_longitude);
        }
    }
    return QGeoCoordinate();        // Nothing to choose by
}

// The fixes a named departure or arrival procedure flies through, ready to be dropped
// into a route.
//
// A flight plan names its SID and STAR rather than listing them - "runway 26L, LAM1Z
// departure" - so the drawn line used to leave the departure airport straight for the
// first en route fix, which is not where the aircraft goes. A SID out of Gatwick turns
// through half a dozen fixes before it joins the airway.
//
// The database gives every point a position, so they are marked placed and never looked
// up by name. That matters as much as the shape: procedure fixes are named for their
// procedure - KKE02, KKN43 - and are in no en route waypoint database, so by name alone
// every one of them would be dropped.
//
// Empty when the procedure is not known, which includes the whole database being absent
// until it has been downloaded. Nothing else changes in that case: the route is drawn
// exactly as it was before.
// An altitude constraint as a procedure plate would read it, or empty where the fix
// carries none. Both bounds equal is a crossing altitude, one bound is a floor or a
// ceiling, and two different ones are a window.
QString AircraftTracker::altitudeConstraint(const AircraftReport::RouteWaypoint& point)
{
    if (point.m_hasMinimumAltitude && point.m_hasMaximumAltitude)
    {
        if (point.m_minimumAltitudeFt == point.m_maximumAltitudeFt) {
            return QString("%1 ft").arg(point.m_minimumAltitudeFt);
        }
        return QString("%1 to %2 ft").arg(point.m_minimumAltitudeFt)
                                     .arg(point.m_maximumAltitudeFt);
    }
    if (point.m_hasMinimumAltitude) {
        return QString("%1 ft or above").arg(point.m_minimumAltitudeFt);
    }
    if (point.m_hasMaximumAltitude) {
        return QString("%1 ft or below").arg(point.m_maximumAltitudeFt);
    }
    return QString();
}

QList<AircraftReport::RouteWaypoint> AircraftTracker::procedurePoints(
    const QString& procedure, const QString& airport, const QString& procedureType)
{
    QList<AircraftReport::RouteWaypoint> points;

    // Four characters because the lookup is by ICAO code, and a plan can carry an IATA
    // one or nothing at all
    if (procedure.isEmpty() || (airport.size() != 4)) {
        return points;
    }

    // A plan often qualifies the procedure with the runway it starts on - LAM1Z/26L -
    // and the database holds the procedure alone
    QString name = procedure.trimmed().toUpper();
    const int slash = name.indexOf('/');
    if (slash > 0) {
        name = name.left(slash);
    }

    const QString icao = airport.trimmed().toUpper();
    QList<SidStarPoint> procedureFixes = SidStar::getWaypoints(name, icao, procedureType);

    // The American form, which is most of the world's procedures: a base and a transition,
    // written BASE.TRANSITION, held in the database as two entries. DSNEE6.HIMDU is the
    // twelve fixes from HIMDU down to DSNEE, and DSNEE6 on its own is the three from DSNEE
    // to the runway - the part every transition joins. Flown, it is one and then the
    // other, so that is how it is drawn. They meet on the fix they share, which the
    // duplicate collapse in sendRouteToMap() folds back into one.
    //
    // A SID is the same idea the other way round: the common part first and the transition
    // off the end of it.
    if (procedureFixes.isEmpty() && name.contains('.'))
    {
        const int dot = name.indexOf('.');
        const QString base = name.left(dot);
        QString transition = name.mid(dot + 1);

        // Shortened a character at a time because the message can leave its own checksum
        // stuck on the end of the last field it wrote - DSNEE6.HIMDU83CF is the HIMDU
        // transition with 83CF after it. Longest first, so a transition that is a prefix
        // of a longer one cannot win over the longer one, and it stops at the first that
        // the database actually holds rather than guessing where the name ends.
        while (transition.size() >= 2)
        {
            procedureFixes = SidStar::getWaypoints(base + "." + transition, icao, procedureType);
            if (!procedureFixes.isEmpty()) {
                break;
            }
            transition.chop(1);
        }

        if (!procedureFixes.isEmpty())
        {
            const QList<SidStarPoint> common = SidStar::getWaypoints(base, icao, procedureType);
            // Which end the transition attaches to. A runway transition - named RW05,
            // RW13B - is the bit off the runway itself, so it comes before the common
            // part whichever kind of procedure it belongs to. Anything else is an en
            // route transition, which leads INTO an arrival and away from a departure.
            //
            //   departure, runway transition : off the runway, then the common part
            //   departure, en route          : the common part, then away to the airway
            //   arrival,   en route          : in off the airway, then the common part
            //   arrival,   runway transition : the common part, then on to the runway
            //
            // which is transition first exactly when the two agree.
            const bool runwayTransition = transition.startsWith("RW");
            const bool departure = procedureType.compare("SID", Qt::CaseInsensitive) == 0;
            if (departure == runwayTransition) {
                procedureFixes = procedureFixes + common;
            } else {
                procedureFixes = common + procedureFixes;
            }
        }
    }

    for (const SidStarPoint& fix : procedureFixes)
    {
        AircraftReport::RouteWaypoint point;
        point.m_name = fix.m_waypoint;
        point.m_positionValid = true;
        point.m_latitude = fix.m_latitude;
        point.m_longitude = fix.m_longitude;
        // Which procedure put it there, so the fix can say so on the Map - a procedure
        // fix is named for its procedure and means nothing on its own
        point.m_procedure = name;
        point.m_procedureType = procedureType;
        point.m_hasMinimumAltitude = fix.m_hasMinimumAltitude;
        point.m_minimumAltitudeFt = fix.m_minimumAltitudeFt;
        point.m_hasMaximumAltitude = fix.m_hasMaximumAltitude;
        point.m_maximumAltitudeFt = fix.m_maximumAltitudeFt;
        points.append(point);
    }

    return points;
}

void AircraftTracker::resolveRoute(QList<AircraftReport::RouteWaypoint>& points,
                                   QList<QGeoCoordinate>& coords) const
{
    // A route names the airways between its fixes as well as the fixes: "BPK Q295 PAAVO
    // M604 GIVPO". An airway is a letter or two and then digits, which no fix ident is,
    // so this is what keeps Q295 from being looked up as an airport and drawn as one.
    static const QRegularExpression airwayRe("^[A-Z]{1,2}[0-9]{1,4}$");
    // An oceanic position names itself: 58N030W is 58 North 030 West. The minutes are
    // optional on either half, so the same fix appears as 58N030W, 5730N020W (57 degrees
    // 30 minutes North, 020 West) or 5730N02030W - and a NAT track is routinely given on
    // the half degree, so the longer forms are not unusual. Only the 7 character form
    // used to be recognised, which dropped exactly those waypoints from the line.
    static const QRegularExpression oceanicRe(
        "^([0-9]{2})([0-9]{2})?([NS])([0-9]{3})([0-9]{2})?([EW])$");

    // A fix ident is not unique. There is more than one BALAN, and asked for the name
    // alone the databases can only hand back the first they hold - which is how one fix
    // of a route across France came out on the far side of the world. What tells them
    // apart is the company they keep, so every lookup is given the names around it.
    //
    // A WINDOW rather than the whole route: a candidate is scored on its mean distance
    // to the names it is given, so on a long route the far end would pull the choice
    // towards the middle of the span instead of towards the fixes either side of it.
    // Three either way, which survives the airway names in between - they are in no
    // database, so they contribute nothing and only take up room.
    QStringList allNames;
    allNames.reserve(points.size());
    for (const AircraftReport::RouteWaypoint& point : points) {
        allNames.append(point.m_name);
    }
    // Where each name is, for every one that needs no disambiguating: an oceanic
    // position states its own latitude and longitude, an ICAO airport ident is unique,
    // and a fix that arrived carrying a position already has one. These are what the
    // ambiguous names are then judged against, and they are worked out FIRST because a
    // name cannot be judged against a neighbour that has not been placed yet.
    //
    // Without this an oceanic clearance had nothing to judge by at all. ETARI exists
    // twice - once on the North Atlantic, once in Japan - and every fix around it on a
    // Shanwick clearance is a bare latitude and longitude that no waypoint database
    // holds. So the scoring found no neighbours it could look up, scored nothing, and
    // fell through to the first ETARI in the file, which is the Japanese one. The
    // 2000 km plausibility check could not catch it either, because it was anchored the
    // same way and so had no anchor.
    QList<QGeoCoordinate> placed;
    placed.reserve(points.size());
    for (const AircraftReport::RouteWaypoint& point : points)
    {
        const QRegularExpressionMatch oceanic = oceanicRe.match(point.m_name);
        if (point.m_positionValid) {
            placed.append(QGeoCoordinate(point.m_latitude, point.m_longitude));
        } else if (oceanic.hasMatch()) {
            placed.append(oceanicPosition(oceanic));
        } else if ((point.m_name.size() == 4) && m_airports
                   && m_airports->contains(point.m_name)) {
            const AirportInformation *airport = m_airports->value(point.m_name);
            placed.append(QGeoCoordinate(airport->m_latitude, airport->m_longitude));
        } else {
            placed.append(QGeoCoordinate());        // Needs a name lookup to be placed
        }
    }

    // The placed positions around a name, nearest first. The same window as the names
    // below and for the same reason: a candidate is scored on its mean distance to
    // these, so the far end of a long route would drag the choice towards the middle.
    auto placedNear = [&placed](int index)
    {
        QList<QGeoCoordinate> nearby;
        for (int offset = 1; offset <= 3; offset++)
        {
            if ((index - offset >= 0) && placed[index - offset].isValid()) {
                nearby.append(placed[index - offset]);
            }
            if ((index + offset < placed.size()) && placed[index + offset].isValid()) {
                nearby.append(placed[index + offset]);
            }
        }
        return nearby;
    };

    // The plausibility check needs only ONE position to measure from, so unlike the
    // scoring window it looks as far along the route as it must to find one
    auto placedAnchor = [&placed](int index)
    {
        for (int offset = 1; offset < placed.size(); offset++)
        {
            if ((index - offset >= 0) && placed[index - offset].isValid()) {
                return placed[index - offset];
            }
            if ((index + offset < placed.size()) && placed[index + offset].isValid()) {
                return placed[index + offset];
            }
        }
        return QGeoCoordinate();
    };

    // Nearest first, which is what lets routeAnchor() take the closest it can place
    auto neighbours = [&allNames](int index)
    {
        QStringList nearby;
        for (int offset = 1; offset <= 3; offset++)
        {
            if (index - offset >= 0) {
                nearby.append(allNames[index - offset]);
            }
            if (index + offset < allNames.size()) {
                nearby.append(allNames[index + offset]);
            }
        }
        return nearby;
    };

    // Its own, because the iterator's position moves as unresolvable names are dropped
    // while the window has to index the route as it was named
    int index = -1;

    QMutableListIterator<AircraftReport::RouteWaypoint> i(points);
    while (i.hasNext())
    {
        const AircraftReport::RouteWaypoint& point = i.next();
        const QString name = point.m_name;
        index++;

        if (point.m_positionValid)
        {
            // A procedure fix is flown at the altitude the procedure requires, so draw it
            // there rather than on the ground. The floor where there is one - an aircraft
            // is at least that high - otherwise the ceiling.
            double altitude = 0.0;
            if (point.m_hasAltitude) {
                altitude = Units::feetToMetres((float) point.m_altitudeFt);
            } else if (point.m_hasMinimumAltitude) {
                altitude = Units::feetToMetres((float) point.m_minimumAltitudeFt);
            } else if (point.m_hasMaximumAltitude) {
                altitude = Units::feetToMetres((float) point.m_maximumAltitudeFt);
            }
            coords.append(QGeoCoordinate(point.m_latitude, point.m_longitude, altitude));
            continue;
        }

        // Where a placed fix would have to be, to be believed. Not disambiguation - the
        // databases can hold exactly ONE fix of a name and still have the wrong one.
        //
        // ALR01 is the case that showed it up: a Stansted to Algiers route named it, and
        // the only ALR01 any database here holds is at 2S 44W, off the coast of Brazil.
        // The real one is a fix the aircraft's own FMS computed near the Alger VOR, which
        // is in no database at all, so choosing better among candidates cannot help -
        // there is only one and it is wrong. Drawn, it took the route across the Atlantic
        // and back.
        //
        // 2000 km, measured from the nearest fix around it that could be placed. Legs are
        // long on an oceanic route - a NAT track steps about 700 km between waypoints -
        // so this has to be generous to avoid throwing away good fixes, and it still
        // rejects a placement on the wrong continent by a wide margin. With nothing around
        // it placeable there is nothing to judge against, and the fix is taken as it comes.
        auto plausible = [&](const QGeoCoordinate& at)
        {
            QGeoCoordinate anchor = placedAnchor(index);
            if (!anchor.isValid()) {
                anchor = routeAnchor(neighbours(index));
            }
            if (!anchor.isValid() || !at.isValid()) {
                return true;
            }
            return anchor.distanceTo(at) <= 2000000.0;
        };

        bool found = false;
        if (!airwayRe.match(name).hasMatch())
        {
            if ((name.size() == 4) && m_airports)
            {
                const AirportInformation *airport = m_airports->value(name, nullptr);
                if (airport)
                {
                    found = true;
                    coords.append(QGeoCoordinate(airport->m_latitude, airport->m_longitude, 0.0));
                }
            }
            else if (name.size() == 5)
            {
                // Positions where the fixes around it could be placed without a
                // lookup, names where they could not - an ordinary airway route names
                // fixes that are themselves only in the database, and there the name
                // form is all there is
                const QList<QGeoCoordinate> nearby = placedNear(index);
                const Waypoint *waypoint = nearby.isEmpty()
                    ? Waypoints::findWayPoint(name, neighbours(index))
                    : Waypoints::findWayPoint(name, nearby);
                if (waypoint)
                {
                    const QGeoCoordinate at(waypoint->m_latitude, waypoint->m_longitude, 0.0);
                    if (plausible(at))
                    {
                        found = true;
                        placed[index] = at;     // Now an anchor for the names after it
                        coords.append(at);
                    }
                }
            }
            else if ((name.size() == 3) && m_navAids)
            {
                // Idents are no more unique than waypoint names, so the same rule
                // applies: of the navaids carrying this one, take the one nearest the
                // fixes around it. Where none of those can be placed there is nothing to
                // choose by, and the first is as good as any - which is all this did.
                QGeoCoordinate anchor = placedAnchor(index);
                if (!anchor.isValid()) {
                    anchor = routeAnchor(neighbours(index));
                }
                const NavAid *best = nullptr;
                double bestDistance = 0.0;

                for (int j = 0; j < m_navAids->size(); j++)
                {
                    const NavAid *navAid = m_navAids->at(j);
                    if (navAid->m_ident != name) {
                        continue;
                    }
                    if (!anchor.isValid())
                    {
                        best = navAid;
                        break;
                    }
                    const double d = anchor.distanceTo(
                        QGeoCoordinate(navAid->m_latitude, navAid->m_longitude));
                    if (!best || (d < bestDistance))
                    {
                        best = navAid;
                        bestDistance = d;
                    }
                }
                if (best)
                {
                    const QGeoCoordinate at(best->m_latitude, best->m_longitude, 0.0);
                    if (plausible(at))
                    {
                        found = true;
                        placed[index] = at;     // Now an anchor for the names after it
                        coords.append(at);
                    }
                }
            }
            else
            {
                // Whatever length it is - the regex is what decides, since the minutes
                // are optional on both halves
                const QRegularExpressionMatch match = oceanicRe.match(name);
                if (match.hasMatch())
                {
                    found = true;
                    coords.append(oceanicPosition(match));
                }
            }
        }

        if (!found) {
            i.remove();
        }
    }
}

// A route the aircraft has been given - a filed flight plan or an oceanic clearance -
// drawn on the Map as a line through the waypoints with a marker at each one. This used
// to live in the ACARS demodulator, for clearances only; it is here now so that both
// kinds are drawn the same way, and so that a route revealed by any protocol is drawn once
// however many demodulators heard it.
//
// The kind is part of every item name, so a flight with both a plan and a clearance
// shows both: the Map keys its items on the name, and identical names would have one
// quietly replacing the other.
//
// Not removed when the flight is archived, which is how clearances have always behaved:
// the route is a record of where the aircraft was going, and that stays interesting
// after it has landed.
// Whose filed routes are on the map. Only the selected one's: every oceanic flight in
// earshot files a clearance, so drawing them all covered the map in lines that had
// nothing to do with what the user was looking at - and since the routes are stored, a
// reload put every one of them up at once. Selecting an aircraft watches its current
// flight, so this one test covers a selected aircraft and a selected flight alike.
// How many of a route's waypoints can actually be placed, which is what decides whether
// a new filing is worth anything against the one already held - see updateFlight()
int AircraftTracker::routePlaceable(const QString& waypoints,
                                    const QList<AircraftReport::RouteWaypoint>& given) const
{
    QList<AircraftReport::RouteWaypoint> points = given;

    if (points.isEmpty())
    {
        static const QRegularExpression separatorRe("[ .]+");
        for (const QString& name : waypoints.split(separatorRe, Qt::SkipEmptyParts))
        {
            AircraftReport::RouteWaypoint point;
            point.m_name = name;
            points.append(point);
        }
    }

    QList<QGeoCoordinate> coords;
    resolveRoute(points, coords);       // Drops what it cannot place
    return coords.size();
}

bool AircraftTracker::routeDrawn(const TrackedFlight *flight) const
{
    return flight && (flight->m_id == m_watchedFlight);
}

// Both of a flight's routes at once, which is always how they are drawn and undrawn -
// the selection moves to a flight, not to one of its two routes
// One ACARS block as received: archived, and fed on to the aggregators.
//
// The block is kept exactly as it came off the air. That is the point of doing it here:
// the parser lives in the library now, so a later decoder can be run over the whole
// archive, and everything already stored improves with it. A rendered document cannot -
// one written before the three character sublabel fix keeps its stray characters for
// good.
// Render a document from what it was decoded from, with whatever the parser does today.
//
// This is what makes a decoder improvement reach everything already stored. Documents
// used to be written as finished text, so a fix reached only messages received after it -
// a three character sublabel read as two put a stray character through the middle of long
// messages, and every document written before that was found kept it for good, as did a
// wind direction rendered with an HTML entity in a field that is shown as plain text.
//
// The block is decoded for its header - the registration, the label, which way it was
// going - and then decoded again with the text, which for a message that arrived in
// several blocks is the assembled whole rather than any one of them. That is exactly what
// the demodulator does on reception.
QString AircraftTracker::renderDocument(const QByteArray& raw, const QString& source)
{
    if (raw.isEmpty()) {
        return QString();
    }

    // Decoded once, on the whole: decoding the block and then the whole into the same
    // object doubles every list a decoder appends to
    QScopedPointer<AcarsMessage> message(source.isEmpty() ? AcarsMessage::decode(raw) : AcarsMessage::decode(raw, source));
    if (!message) {
        return QString();
    }
    return acarsDecodeToPlain(message->toString());
}

void AircraftTracker::acarsBlockReceived(const QByteArray& block, const QDateTime& dateTime,
                                         qint64 frequency, double level, int protocol)
{
    AcarsBlockRecord record;
    record.m_block = block;
    record.m_dateTime = dateTime;
    record.m_frequency = frequency;
    record.m_level = level;
    record.m_protocol = protocol;
    // Only queued for a database to take them: the queue is emptied by a save, and with
    // no database (it would not open - locked, or read only) it grew for the whole session
    if (m_db)
    {
        m_acarsBlocks.append(record);
        m_dbDirty = true;
    }

    if (!m_settings.m_feedEnabled) {
        return;
    }

    // Decoded only for the feed, and only when there is one - the archive does not need
    // it, and an aggregator wants the fields rather than the block
    QScopedPointer<AcarsMessage> message(AcarsMessage::decode(block));
    if (message) {
        feedMessage(*message, dateTime, frequency, level);
    }
}

// Send one ACARS message on to the aggregators that are enabled, as one acarsdec style
// JSON object per UDP datagram - the format both airframes.io (port 5550) and
// avdelphi.com (vdlm2dec JSON, port 5556) ingest.
//
// Built here rather than in the demodulator so that the whole feed - the station identity,
// the connections and the format - is one thing in one place, set up once for the station
// however many demodulators are running. Messages from all three ACARS bearers arrive
// here over one set of connections.
void AircraftTracker::feedMessage(const AcarsMessage& message, const QDateTime& dateTime,
                                  qint64 frequency, double level)
{
    if (!m_feedSocket) {
        return;
    }

    QJsonObject json;
    json.insert("timestamp", dateTime.toMSecsSinceEpoch() / 1000.0);
    if (!m_settings.m_feedStationId.isEmpty()) {
        json.insert("station_id", m_settings.m_feedStationId);
    }
    json.insert("channel", 0);
    json.insert("freq", frequency / 1e6);
    json.insert("level", level);
    json.insert("error", 0);
    json.insert("mode", message.m_mode);
    json.insert("label", message.m_label);
    json.insert("block_id", QString(message.m_blockId));
    if ((message.m_ack == QChar(ASCII_NAK)) || message.m_ack.isNull()) {
        json.insert("ack", false);
    } else {
        json.insert("ack", QString(message.m_ack));
    }
    json.insert("tail", message.m_address);
    if (!message.m_flight.isEmpty()) {
        json.insert("flight", message.m_flight);
    }
    const QString msgNo = message.m_originator + message.m_messageNumber + message.m_blockSequence;
    if (!msgNo.isEmpty()) {
        json.insert("msgno", msgNo);
    }
    json.insert("text", message.m_text);
    json.insert("end", !message.m_multiBlock);
    // Identify the client, as the standalone decoders do - aggregators use this to
    // recognise the source
    QJsonObject app;
    app.insert("name", "SDRangel");
    app.insert("ver", QCoreApplication::applicationVersion());
    json.insert("app", app);

    QByteArray datagram = QJsonDocument(json).toJson(QJsonDocument::Compact);
    datagram.append('\n');

    // UDP: host names are resolved once and cached; the cache invalidates when the
    // setting changes, so a blocking DNS lookup happens at most once per host. A host
    // that would not resolve is not tried again for a minute: the lookup blocks this
    // thread, and with the network down it timed out again on every block received.
    auto sendUdp = [this, &datagram](const QString& host, int port, QHostAddress& cache,
                                     QString& cachedHost)
    {
        if (host.isEmpty()) {
            return;
        }
        if (host != cachedHost)
        {
            const QDateTime now = QDateTime::currentDateTimeUtc();
            const QDateTime retry = m_feedResolveRetry.value(host);
            if (retry.isValid() && (now < retry)) {
                return;
            }
            const QHostInfo info = QHostInfo::fromName(host);
            if (info.addresses().isEmpty())
            {
                qWarning() << "AircraftTracker: feed cannot resolve host" << host;
                m_feedResolveRetry.insert(host, now.addSecs(60));
                return;
            }
            m_feedResolveRetry.remove(host);
            cache = info.addresses().first();
            cachedHost = host;
            qDebug() << "AircraftTracker: feed sending UDP to" << host << "(" << cache << ") :" << port;
        }
        m_feedSocket->writeDatagram(datagram, cache, port);
    };

    // TCP: reconnect if the link has dropped, and drop this message rather than queue it -
    // a feed that has been down is not helped by a burst of stale reports, which matches
    // how the standalone feeder clients behave.
    auto sendTcp = [this, &datagram](QTcpSocket& socket, const QString& host, int port,
                                     const char *service)
    {
        if (host.isEmpty()) {
            return;
        }
        if (socket.state() == QAbstractSocket::ConnectedState)
        {
            const qint64 written = socket.write(datagram);
            socket.flush();
            qDebug() << "AircraftTracker: feed sent" << written << "bytes to" << service;
        }
        else if (socket.state() == QAbstractSocket::UnconnectedState)
        {
            qDebug() << "AircraftTracker: feed not connected to" << service
                     << "- reconnecting, message dropped";
            socket.abort();
            socket.connectToHost(host, port);
        }
        else
        {
            qDebug() << "AircraftTracker: feed still connecting to" << service
                     << "- message dropped";
        }
    };

    if (m_settings.m_feedAirframes)
    {
        if (m_settings.m_feedAirframesTcp) {
            sendTcp(*m_feedAirframesTcpSocket, m_settings.m_feedAirframesHost,
                m_settings.m_feedAirframesPort, "airframes.io");
        } else {
            sendUdp(m_settings.m_feedAirframesHost, m_settings.m_feedAirframesPort,
                m_feedAirframesAddr, m_feedAirframesResolvedHost);
        }
    }
    if (m_settings.m_feedAvdelphi)
    {
        if (m_settings.m_feedAvdelphiTcp) {
            sendTcp(*m_feedAvdelphiTcpSocket, m_settings.m_feedAvdelphiHost,
                m_settings.m_feedAvdelphiPort, "avdelphi.com");
        } else {
            sendUdp(m_settings.m_feedAvdelphiHost, m_settings.m_feedAvdelphiPort,
                m_feedAvdelphiAddr, m_feedAvdelphiResolvedHost);
        }
    }
}

// Open (or close) the aggregator connections to match the settings. Connections are also
// re-made on demand when a message needs sending, but connecting eagerly here means
// enabling the feed gives immediate feedback in the log rather than silently waiting for
// the first ACARS message.
void AircraftTracker::updateFeedConnections()
{
    if (!m_feedAirframesTcpSocket) {
        return;         // Not started yet; startWork() calls back in here
    }

    auto update = [this](QTcpSocket& socket, bool enabled, bool tcp, const QString& host, int port)
    {
        bool want = m_settings.m_feedEnabled && enabled && tcp && !host.isEmpty();
        bool wrongPeer = (socket.peerName() != host) || (socket.peerPort() != port);
        if (want)
        {
            if ((socket.state() == QAbstractSocket::UnconnectedState) || wrongPeer)
            {
                qDebug() << "AircraftTracker: feed connecting to" << host << ":" << port;
                socket.abort();
                socket.connectToHost(host, port);
            }
        }
        else if (socket.state() != QAbstractSocket::UnconnectedState)
        {
            socket.abort();
        }
    };
    update(*m_feedAirframesTcpSocket, m_settings.m_feedAirframes, m_settings.m_feedAirframesTcp,
        m_settings.m_feedAirframesHost, m_settings.m_feedAirframesPort);
    update(*m_feedAvdelphiTcpSocket, m_settings.m_feedAvdelphi, m_settings.m_feedAvdelphiTcp,
        m_settings.m_feedAvdelphiHost, m_settings.m_feedAvdelphiPort);
}

void AircraftTracker::sendRoutes(TrackedFlight *flight)
{
    if (flight)
    {
        sendRouteToMap(flight, flight->m_flightPlan, "Flight plan");
        sendRouteToMap(flight, flight->m_clearance, "Oceanic clearance");
    }
}

void AircraftTracker::removeRoutes(TrackedFlight *flight)
{
    if (flight)
    {
        removeDrawnRoute(flight->m_flightPlan);
        removeDrawnRoute(flight->m_clearance);
    }
}

// Assemble a filed route into placed fixes: the airports it runs between, the departure
// and arrival procedures expanded, the path itself, altitudes stamped and duplicates
// collapsed, then everything resolved against the databases. Points and coords come out
// index aligned, resolveRoute() having dropped anything it could not place. Shared by
// the drawn route (sendRouteToMap()) and the ND flight plan (buildFlightPlan()).
QList<AircraftReport::RouteWaypoint> AircraftTracker::assembleRoutePoints(
    const TrackedFlight *flight, const TrackedFlight::FiledRoute& route,
    QList<QGeoCoordinate>& coords) const
{
    // The route field is the waypoints alone, so the airports it runs between are added
    // here - which is what makes the drawn line start and finish somewhere recognisable
    QList<AircraftReport::RouteWaypoint> points;
    auto named = [](const QString& waypoint)
    {
        AircraftReport::RouteWaypoint point;
        point.m_name = waypoint;
        return point;
    };
    // The plan's own airports in preference to the flight's: a procedure belongs to the
    // aerodrome the plan was filed against, and a flight that has been re-filed still
    // carries the arrival it was first heard with.
    const QString departure = route.m_departure.size() == 4 ? route.m_departure
                                                            : flight->m_departure;
    const QString arrival = route.m_arrival.size() == 4 ? route.m_arrival
                                                        : flight->m_arrival;

    // The same airports as the procedures, so the line runs down the STAR to the airport
    // it serves. Bookending with the flight's own let a re-file the flight had not taken
    // on draw down the new STAR and then jump to the old arrival.
    if (departure.size() == 4) {
        points.append(named(departure));
    }
    // The departure procedure, between the aerodrome and the route it joins
    points.append(procedurePoints(route.m_sid, departure, "SID"));
    // The message's own positions where it gave them, its names alone where it did not
    if (route.m_points.isEmpty())
    {
        // Both separators, because both forms reach here. A clearance names its waypoints
        // with spaces - SUNOT 58N030W JANJO - while a flight plan is written as an ARINC
        // 702A path, DVR.UL9.KONAN.UL607.KOK..KOMOB.., where a single dot joins a fix to
        // the airway leaving it and a double dot means direct to the next fix. Split on
        // spaces alone, the whole of a filed plan was one unresolvable name, and all that
        // was left to draw was the line between the two airports.
        static const QRegularExpression separatorRe("[ .]+");
        for (const QString& waypoint : route.m_waypoints.split(separatorRe, Qt::SkipEmptyParts)) {
            points.append(named(waypoint));
        }
    }
    else
    {
        points.append(route.m_points);
    }
    // And the arrival procedure, between the route and the aerodrome
    points.append(procedurePoints(route.m_star, arrival, "STAR"));
    if (arrival.size() == 4) {
        points.append(named(arrival));
    }

    // The level the route is flown at, which is what a clearance clears the aircraft to
    // and what a filed plan cruises at. Applied to the FIXES, never to the airports it
    // runs between, and never over an altitude something more specific already gave - a
    // procedure fix has its own, and a predicted route gives one per fix.
    //
    // Identified by name rather than by position in the list: an airport is four
    // characters where a waypoint is five, a navaid three and an oceanic position seven
    // or more. Skipping the first and last entries instead looks equivalent and is not -
    // the list does not always start or end with an airport, and doing it that way left
    // the last fix of every route on the ground.
    if (route.m_flightLevel > 0)
    {
        for (AircraftReport::RouteWaypoint& point : points)
        {
            // ANY altitude of its own, which is three separate fields: what a predicted
            // route expects at the fix, and the floor and ceiling a procedure requires
            // there. Testing only the first of them is a trap, because a procedure fix
            // carries the other two and looks unconstrained - a SID that crosses BPK at
            // 6000 ft had it stamped with the cruise level instead and drawn at FL390,
            // which is where the aircraft is an hour later rather than on departure.
            const bool hasOwn = point.m_hasAltitude
                             || point.m_hasMinimumAltitude
                             || point.m_hasMaximumAltitude;
            if (!hasOwn && (point.m_name.size() != 4))
            {
                point.m_hasAltitude = true;
                point.m_altitudeFt = route.m_flightLevel * 100;
            }
        }
    }

    // A fix named twice in a row is one fix. It happens: a clearance gives its entry
    // point and then repeats it as the first waypoint of the route, and the departure
    // airport prepended above can be the first waypoint too. Drawn as they come, they
    // make a segment of no length and two markers under one name - and the Map keys its
    // items on the name, so the second would quietly replace the first.
    //
    // Which of the two to keep is not arbitrary. A procedure and the route it joins name
    // the same fix at the join, and only the procedure's copy knows what altitude to
    // cross it at - so the one carrying something is kept and the bare one dropped,
    // whichever came first. Dropping the later one always would have worked for a SID,
    // which is spliced in BEFORE the route, and quietly discarded every STAR constraint,
    // because a STAR is spliced in after it.
    auto informative = [](const AircraftReport::RouteWaypoint& point)
    {
        return point.m_positionValid || point.m_hasAltitude
            || point.m_hasMinimumAltitude || point.m_hasMaximumAltitude;
    };
    for (int i = points.size() - 1; i > 0; i--)
    {
        if (points[i].m_name != points[i - 1].m_name) {
            continue;
        }
        if (informative(points[i]) && !informative(points[i - 1])) {
            points.removeAt(i - 1);
        } else {
            points.removeAt(i);
        }
    }

    resolveRoute(points, coords);
    return points;
}

// Build the flight plan attached to the aircraft's own map item, which is what the 3D
// map's ND (Navigation Display) draws. Unlike the drawn route this goes out for every
// aircraft with a filed route, not just the watched flight - the ND shows it only for
// the selected aircraft. Returns nullptr when there is nothing to send; otherwise the
// caller owns the object (typically handing it to SWGMapAircraftState::setFlightPlan()).
SWGSDRangel::SWGMapFlightPlan *AircraftTracker::buildFlightPlan(TrackedAircraft *item) const
{
    TrackedFlight *flight = item->m_currentFlight;
    if (!flight) {
        return nullptr;
    }

    // Prefer the clearance when it has waypoints and is at least as recently filed as
    // the plan - a clearance is what the aircraft is actually cleared to fly
    const TrackedFlight::FiledRoute& plan = flight->m_flightPlan;
    const TrackedFlight::FiledRoute& clearance = flight->m_clearance;
    const bool planUsable = !plan.m_waypoints.isEmpty()
                         || !plan.m_sid.isEmpty() || !plan.m_star.isEmpty();
    const bool clearanceUsable = !clearance.m_waypoints.isEmpty();
    const TrackedFlight::FiledRoute *route;
    if (clearanceUsable && (!planUsable || (clearance.m_filed >= plan.m_filed))) {
        route = &clearance;
    } else if (planUsable) {
        route = &plan;
    } else {
        return nullptr;
    }

    const QString departure = route->m_departure.size() == 4 ? route->m_departure
                                                             : flight->m_departure;
    const QString arrival = route->m_arrival.size() == 4 ? route->m_arrival
                                                         : flight->m_arrival;

    // Everything assembleRoutePoints() depends on, so the assembly - database lookups
    // per fix - runs only when the route actually changes, not per position report
    const QString signature = QStringList({
        departure, arrival, route->m_waypoints, route->m_sid, route->m_star,
        QString::number(route->m_flightLevel),
        route->m_filed.toString(Qt::ISODateWithMs)
    }).join(QChar(0x1f));
    if (signature != flight->m_ndPlanSignature)
    {
        flight->m_ndPlanCoords.clear();
        flight->m_ndPlanPoints = assembleRoutePoints(flight, *route, flight->m_ndPlanCoords);
        flight->m_ndPlanSignature = signature;
    }
    const QList<AircraftReport::RouteWaypoint>& points = flight->m_ndPlanPoints;
    const QList<QGeoCoordinate>& coords = flight->m_ndPlanCoords;
    if (coords.size() < 2) {
        return nullptr;
    }

    SWGSDRangel::SWGMapFlightPlan *swgPlan = new SWGSDRangel::SWGMapFlightPlan();
    if (!departure.isEmpty()) {
        swgPlan->setDeparture(new QString(departure));
    }
    if (!arrival.isEmpty()) {
        swgPlan->setArrival(new QString(arrival));
    }
    if (!route->m_approach.isEmpty()) {
        swgPlan->setApproach(new QString(route->m_approach));
    }
    // The arrival runway, for the ND's approach annunciator. An FPN writes the
    // departure runway with the arrival's in brackets - 27L(01R) - and the approach
    // procedure name usually ends in the runway it serves, e.g. ILSY01R. The bare
    // runway field is NOT used: on its own it is the departure runway.
    static const QRegularExpression bracketedRunwayRe(R"(\(([0-9]{2}[LRC]?)\))");
    static const QRegularExpression approachRunwayRe(R"(([0-9]{2}[LRC]?)$)");
    QString arrivalRunway;
    QRegularExpressionMatch runwayMatch = bracketedRunwayRe.match(route->m_runway);
    if (runwayMatch.hasMatch()) {
        arrivalRunway = runwayMatch.captured(1);
    }
    else
    {
        runwayMatch = approachRunwayRe.match(route->m_approach);
        if (runwayMatch.hasMatch()) {
            arrivalRunway = runwayMatch.captured(1);
        }
    }
    if (!arrivalRunway.isEmpty()) {
        swgPlan->setArrivalRunway(new QString(arrivalRunway));
    }

    // The waypoint being flown to: the one after the nearest, since past waypoints are
    // behind the aircraft. Rough - an aircraft mid-leg far from both ends still picks
    // its neighbours - but only leg colouring on the ND depends on it.
    int active = -1;
    if (item->m_positionValid)
    {
        const QGeoCoordinate position(item->m_latitude, item->m_longitude);
        int nearest = -1;
        double nearestDistance = 0.0;
        for (int i = 0; i < coords.size(); i++)
        {
            const double distance = position.distanceTo(coords[i]);
            if ((nearest == -1) || (distance < nearestDistance))
            {
                nearest = i;
                nearestDistance = distance;
            }
        }
        active = std::min(nearest + 1, (int) (coords.size() - 1));
    }
    swgPlan->setActiveWaypoint(active);

    QList<SWGSDRangel::SWGMapRouteWaypoint *> *waypoints = new QList<SWGSDRangel::SWGMapRouteWaypoint *>();
    for (int i = 0; i < points.size(); i++)
    {
        SWGSDRangel::SWGMapRouteWaypoint *waypoint = new SWGSDRangel::SWGMapRouteWaypoint();
        waypoint->setName(new QString(points[i].m_name));
        waypoint->setLatitude(coords[i].latitude());
        waypoint->setLongitude(coords[i].longitude());
        if (points[i].m_hasAltitude) {
            waypoint->setAltitude(points[i].m_altitudeFt);
        } else if (points[i].m_hasMinimumAltitude) {
            waypoint->setAltitude(points[i].m_minimumAltitudeFt);
        } else if (points[i].m_hasMaximumAltitude) {
            waypoint->setAltitude(points[i].m_maximumAltitudeFt);
        }
        if (points[i].m_hasEta) {
            waypoint->setEta(new QString(points[i].m_eta.toString("hh:mm:ss")));
        }
        waypoints->append(waypoint);
    }
    swgPlan->setWaypoints(waypoints);

    return swgPlan;
}

// Note the latest datalink ATC message, which the 3D map's DCDU pops up for the
// selected aircraft, and push it to the Map at once - these are events, and waiting
// for the next position report could sit on one for minutes
void AircraftTracker::recordDcduMessage(TrackedAircraft *item, const AircraftReport& report)
{
    TrackedAircraft::DcduMessage message;

    switch (report.m_documentKind)
    {
    case AircraftReport::Cpdlc:
        // The concise expanded text rather than the full decode tree. The station is
        // the ARINC 622 ground address (an ATC facility) on the ACARS path, and the
        // VDL-2 ground station on the ATN path.
        if (report.m_atc.isEmpty()) {
            return;
        }
        message.m_text = report.m_atc;
        message.m_station = report.m_station;
        break;
    case AircraftReport::DepartureClearance:
        // The message as transmitted, which is what the real unit shows. A PDC never
        // carries a station; the issuing facility is the departure airport.
        message.m_text = report.m_documentSource.isEmpty() ? report.m_documentText
                                                           : report.m_documentSource;
        message.m_station = report.m_departure;
        break;
    case AircraftReport::Clearance:
    case AircraftReport::ClearanceRequest:
        message.m_text = report.m_documentSource.isEmpty() ? report.m_documentText
                                                           : report.m_documentSource;
        message.m_station = report.m_station;
        break;
    default:
        return;
    }
    if (message.m_text.isEmpty()) {
        return;
    }

    message.m_seq = item->m_dcduMessage.m_seq + 1;
    message.m_received = report.m_received;
    message.m_uplink = report.m_uplink;
    item->m_dcduMessage = message;

    sendToMap(item);

    // The DCDU only shows the messages of the aircraft the 3D map has selected or is
    // following, so to watch them arrive the Map is asked to find each aircraft one is
    // for. The order the find and the message reach the map in doesn't matter: the DCDU
    // shows a message that is still recent when it first looks at an aircraft. The name
    // is only set while the aircraft is on the Map, which is the only time it can be found.
    // Through the main thread, which is where the GUI's own Find calls this from
    if (m_settings.m_findDatalinkAircraft && !item->m_mapName.isEmpty()
        && m_settings.datalinkFinds(message.m_text))
    {
        const QString name = item->m_mapName;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [name]() {
            FeatureWebAPIUtils::mapFind(name);
        }, Qt::QueuedConnection);
    }
}

// Build the datalink message attached to the aircraft's own map item, which the 3D
// map's DCDU displays. nullptr when the aircraft has never had one; otherwise the
// caller owns the object.
SWGSDRangel::SWGMapDatalinkMessage *AircraftTracker::buildDatalinkMessage(const TrackedAircraft *item) const
{
    const TrackedAircraft::DcduMessage& message = item->m_dcduMessage;
    if (message.m_seq == 0) {
        return nullptr;
    }

    SWGSDRangel::SWGMapDatalinkMessage *swgMessage = new SWGSDRangel::SWGMapDatalinkMessage();
    swgMessage->setId(message.m_seq);
    if (message.m_received.isValid()) {
        swgMessage->setReceived(new QString(message.m_received.toUTC().toString(Qt::ISODateWithMs)));
    }
    swgMessage->setUplink(message.m_uplink ? 1 : 0);
    if (!message.m_station.isEmpty()) {
        swgMessage->setStation(new QString(message.m_station));
    }
    swgMessage->setText(new QString(message.m_text));
    return swgMessage;
}

void AircraftTracker::sendRouteToMap(TrackedFlight *flight, TrackedFlight::FiledRoute& route,
                                     const QString& kind)
{
    if (!routeDrawn(flight))
    {
        // Off the map, and not waiting to go on it: selecting the flight is what sends a
        // route now, so nothing has to remember that this one wanted to be drawn. Cheap
        // for the flight that was never drawn - removeDrawnRoute() returns straight away.
        route.m_pending = false;
        removeDrawnRoute(route);
        return;
    }
    // Nothing to draw only when the plan names no path AND no procedure. A plan whose
    // arrival end is all that has been heard has no waypoints of its own, but its STAR
    // expands into a run of fixes, and the line from the departure airport through those
    // to the arrival airport is worth drawing.
    if (route.m_waypoints.isEmpty() && route.m_sid.isEmpty() && route.m_star.isEmpty()) {
        route.m_pending = false;
        return;
    }

    // Nothing can be drawn until the flight has a name, since that is what the Map keys
    // its items on - and on most protocols the route arrives before the identity does,
    // an oceanic clearance naming the flight in a message the registration follows.
    // This used to be tested AFTER the route was marked drawn, so a route filed before
    // the identity was known was recorded as drawn and then never sent at all.
    const QString name = flight->m_flight.isEmpty()
        ? (flight->m_aircraft ? flight->m_aircraft->m_registration : QString()) : flight->m_flight;
    // Both of the reasons a route cannot go out yet are things nothing tells the flight
    // about when they change - the Map being opened, the identity arriving - so the
    // route is remembered and retried from the flush timer. Without that, opening the
    // Map after a clearance had been received left the route undrawn until another
    // route-bearing message happened along, which on an oceanic flight can be an hour.
    if (name.isEmpty())
    {
        route.m_pending = true;
        m_routesPending.insert(flight->m_id);
        return;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);
    if (mapPipes.isEmpty())
    {
        route.m_pending = true;
        m_routesPending.insert(flight->m_id);
        return;
    }
    route.m_pending = false;

    QList<QGeoCoordinate> coords;
    QList<AircraftReport::RouteWaypoint> points = assembleRoutePoints(flight, route, coords);
    QStringList names;
    for (const AircraftReport::RouteWaypoint& point : points) {
        names.append(point.m_name);
    }
    // One resolvable point is not a route, and drawing a line through it draws nothing.
    // Not pending: only a fuller message can change this, and that calls back in here.
    if (coords.size() < 2)
    {
        // A route that has been re-filed onto waypoints we cannot place is still a
        // different route from the one on the Map, so what is drawn there is now wrong.
        // Returning without saying so left the previous line and its markers up.
        removeDrawnRoute(route);
        return;
    }

    // What would actually be drawn, rather than the waypoint list it came from. Keying
    // on the waypoints alone meant a route was drawn once and then never corrected,
    // where everything else about it improves after the first filing: the departure and
    // arrival airports that bookend the line arrive in a later message, a waypoint that
    // could not be placed at first is placed once the aircraft names it with its
    // position, and the text is fuller once the whole message has been reassembled.
    QStringList signature;
    signature.append(name);
    signature.append(flight->m_departure);
    signature.append(flight->m_arrival);
    signature.append(route.m_text);
    // Or an insert that adds only the STAR would change the popup without redrawing it
    signature.append(route.m_runway);
    signature.append(route.m_sid);
    signature.append(route.m_star);
    signature.append(route.m_approach);
    signature.append(route.m_alternate);
    // The level the route is flown at, and the ALTITUDE of every fix with it.
    //
    // Without these a route already on the map is never redrawn when only its heights
    // change, because nothing else about it has moved: the cruise level arrives in a
    // later message than the path it applies to, so the fixes stayed on the ground while
    // a SID or STAR - which changes m_sid or m_star, and so the signature - came out at
    // its proper height. That asymmetry is exactly what it looked like from outside.
    signature.append(QString::number(route.m_flightLevel));
    signature.append(QString::number(route.m_mach, 'f', 2));
    for (int i = 0; i < names.size(); i++)
    {
        signature.append(QString("%1@%2,%3,%4").arg(names[i])
            .arg(coords[i].latitude(), 0, 'f', 5).arg(coords[i].longitude(), 0, 'f', 5)
            .arg(coords[i].altitude(), 0, 'f', 0));
    }
    const QString drawn = signature.join(QChar(0x1f));
    if (drawn == route.m_drawn) {
        return;
    }

    // The Map keys its items on the name, and both the flight's name and the list of
    // waypoints can change between drawings - a registration gives way to the callsign
    // once it is known, and a re-filed route can be shorter than the one before it. What
    // was drawn last time and will not be drawn this time has to be taken off by name,
    // or the old line stays beside the new one and dropped waypoints stay on the map for
    // the rest of the session.
    const QString lineName = QString("%1 %2").arg(name).arg(kind);
    QStringList waypointNames;
    for (const QString& waypoint : names) {
        waypointNames.append(QString("%1 %2 %3").arg(name).arg(kind).arg(waypoint));
    }
    if (!route.m_drawnLine.isEmpty() && (route.m_drawnLine != lineName)) {
        removeFromMap(route.m_drawnLine);
    }
    for (const QString& previous : route.m_drawnWaypoints)
    {
        if (!waypointNames.contains(previous)) {
            removeFromMap(previous);
        }
    }

    QStringList text;
    text.append(kind);
    text.append(QString("Flight: %1").arg(name));
    if (!flight->m_departure.isEmpty() && !flight->m_arrival.isEmpty()) {
        text.append(QString("Route: %1 to %2").arg(flight->m_departure).arg(flight->m_arrival));
    }
    // In the order the flight is flown, and only the parts that were filed. A
    // clearance has none of them; a plan assembled from its inserts has most.
    if (!route.m_runway.isEmpty()) {
        text.append(QString("Runway: %1").arg(route.m_runway));
    }
    if (!route.m_sid.isEmpty()) {
        text.append(QString("SID: %1").arg(route.m_sid));
    }
    text.append("Waypoints: " + names.join(" "));
    if (!route.m_star.isEmpty()) {
        text.append(QString("STAR: %1").arg(route.m_star));
    }
    if (!route.m_approach.isEmpty()) {
        text.append(QString("Approach: %1").arg(route.m_approach));
    }
    if (!route.m_alternate.isEmpty()) {
        text.append(QString("Alternate: %1").arg(route.m_alternate));
    }
    if (route.m_flightLevel > 0) {
        text.append(QString("Flight level: FL%1").arg(route.m_flightLevel));
    }
    if (route.m_mach > 0.0f) {
        text.append(QString("Mach: %1").arg(route.m_mach, 0, 'f', 2));
    }
    // The message itself, which is where the clearance number, entry time, flight level
    // and Mach are. The demodulator used to repeat those on every waypoint; one copy on
    // the route says the same thing without covering the map in it.
    if (!route.m_text.isEmpty())
    {
        text.append(QString());
        text.append(route.m_text);
    }

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(lineName));
        swgMapItem->setGroup(new QString(RouteMapGroup));
        swgMapItem->setLabel(new QString(name));
        swgMapItem->setText(new QString(text.join("\n")));
        swgMapItem->setLatitude(coords[0].latitude());
        swgMapItem->setLongitude(coords[0].longitude());
        swgMapItem->setAltitude(coords[0].altitude());
        swgMapItem->setImage(new QString("none"));
        swgMapItem->setImageRotation(0);
        swgMapItem->setFixedPosition(true);
        swgMapItem->setAltitudeReference(3);    // 1 - CLAMP_TO_GROUND, 3 - CLIP_TO_GROUND

        QList<SWGSDRangel::SWGMapCoordinate *> *line = new QList<SWGSDRangel::SWGMapCoordinate *>();
        for (const auto& coord : coords)
        {
            SWGSDRangel::SWGMapCoordinate *c = new SWGSDRangel::SWGMapCoordinate();
            c->setLatitude(coord.latitude());
            c->setLongitude(coord.longitude());
            c->setAltitude(coord.altitude());
            line->append(c);
        }
        swgMapItem->setCoordinates(line);
        swgMapItem->setType(3);                 // Line

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
        messageQueue->push(msg);

        // The waypoints themselves. The Map does not draw its own waypoint layer across
        // the whole globe, and shows it only when zoomed well in, so the points of a
        // route have to be sent explicitly to be visible with the line.
        for (int i = 0; i < names.size(); i++)
        {
            swgMapItem = new SWGSDRangel::SWGMapItem();
            // Not the name the Map uses for the waypoint itself, which would replace it
            swgMapItem->setName(new QString(waypointNames[i]));
            swgMapItem->setGroup(new QString(RouteMapGroup));
            swgMapItem->setLabel(new QString(names[i]));

            QStringList waypointText;
            waypointText.append(QString("%1 waypoint").arg(kind));
            waypointText.append(QString("Flight: %1").arg(name));
            waypointText.append(QString("Waypoint: %1").arg(names[i]));
            // Which procedure it came out of. Without this a procedure fix is an
            // unexplained marker in the middle of nowhere - it is named for its
            // procedure, so KKN43 says nothing unless the SID is named with it.
            const AircraftReport::RouteWaypoint& point = points[i];
            if (!point.m_procedure.isEmpty())
            {
                waypointText.append(QString("%1: %2")
                    .arg(point.m_procedureType.isEmpty() ? QString("Procedure")
                                                         : point.m_procedureType)
                    .arg(point.m_procedure));
            }
            // A predicted fix says what the aircraft's own FMS expects there, which is
            // a plain figure rather than the procedure's constraint
            if (point.m_hasAltitude) {
                waypointText.append(QString("Altitude: %1 ft").arg(point.m_altitudeFt));
            }
            else
            {
                const QString constraint = altitudeConstraint(point);
                if (!constraint.isEmpty()) {
                    waypointText.append(QString("Altitude: %1").arg(constraint));
                }
            }
            if (point.m_hasEta) {
                waypointText.append(QString("ETA: %1").arg(point.m_eta.toString("hh:mm:ss")));
            }
            // What the clearance said to fly the route at, on every fix of it
            if (route.m_mach > 0.0f) {
                waypointText.append(QString("Mach: %1").arg(route.m_mach, 0, 'f', 2));
            }
            swgMapItem->setText(new QString(waypointText.join("\n")));

            swgMapItem->setLatitude(coords[i].latitude());
            swgMapItem->setLongitude(coords[i].longitude());
            swgMapItem->setAltitude(coords[i].altitude());
            swgMapItem->setImage(new QString("waypoint.png"));
            swgMapItem->setImageRotation(0);
            swgMapItem->setFixedPosition(true);
            // A fix with a real altitude is drawn AT it - clipping it to the ground would
            // throw away the constraint that was the point of using it
            swgMapItem->setAltitudeReference(coords[i].altitude() > 0.0 ? 0 : 3);

            msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
            messageQueue->push(msg);
        }
    }

    // Only once it has gone out. Marking it before anything had been sent is what let a
    // route be recorded as drawn and then dropped.
    route.m_drawn = drawn;
    route.m_drawnLine = lineName;
    route.m_drawnWaypoints = waypointNames;
    route.m_pending = false;
    if (!flight->m_flightPlan.m_pending && !flight->m_clearance.m_pending) {
        m_routesPending.remove(flight->m_id);
    }
}

// Take a route off the Map, by the names it was drawn under. Only what was actually
// emitted is removed, so this cannot disturb another route that happens to share a name.
void AircraftTracker::removeDrawnRoute(TrackedFlight::FiledRoute& route)
{
    if (route.m_drawnLine.isEmpty() && route.m_drawnWaypoints.isEmpty()) {
        return;
    }
    if (!route.m_drawnLine.isEmpty()) {
        removeFromMap(route.m_drawnLine);
    }
    for (const QString& waypoint : route.m_drawnWaypoints) {
        removeFromMap(waypoint);
    }
    route.m_drawnLine.clear();
    route.m_drawnWaypoints.clear();
    route.m_drawn.clear();
}

// Everything this feature draws is sent once and then only when it changes, which is
// what keeps a busy sky affordable - but it means a Map opened later, or closed and
// opened again, starts from nothing and is only ever told about changes. So when the
// number of Maps listening goes up, everything is offered again: the tracks, the routes
// and the record marker. Aircraft positions need no help, being re-sent as they arrive.
void AircraftTracker::checkMapConsumers()
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);

    QSet<unsigned int> consumers;
    bool arrived = false;
    for (const ObjectPipe *pipe : mapPipes)
    {
        consumers.insert(pipe->m_pipeId);
        if (!m_mapConsumers.contains(pipe->m_pipeId)) {
            arrived = true;
        }
    }
    m_mapConsumers = consumers;
    if (!arrived) {
        return;                         // The same Maps, which have what they need
    }

    for (TrackedAircraft *item : m_aircraft)
    {
        if (item->m_active)
        {
            // The whole track goes again. The name is kept: the Maps that already had
            // the item need it to be told when it is renamed or taken off, and with it
            // cleared, an item whose resend was then skipped (its position gone stale in
            // the meantime) stayed on them as a ghost for the rest of the session.
            item->m_mapTrackChanged = true;
            // Held until it has actually been sent. A pass-through aircraft whose ADS-B
            // has gone quiet is skipped by flush() for a minute after its last item, and
            // the dirty set is cleared either way - so without this the aircraft would
            // simply never reach the new Map.
            item->m_mapResync = true;
            m_mapResyncPending.insert(item);
            m_dirtyAircraft.insert(item);
        }

    }

    // Only the selected flight's routes are on a map at all, so only they have to be put
    // on a new one. It is offered whether or not its aircraft is still being heard: a
    // route is a record of where the aircraft was going, and is deliberately left up.
    if (TrackedFlight *watched = flightById(m_watchedFlight))
    {
        for (TrackedFlight::FiledRoute *route : { &watched->m_flightPlan, &watched->m_clearance })
        {
            // The same test as sendRouteToMap(): a plan with only a SID or a STAR is
            // drawn too, so it has to reach a new Map as well
            if (!route->m_waypoints.isEmpty() || !route->m_sid.isEmpty() || !route->m_star.isEmpty())
            {
                // Not removeDrawnRoute(): the items are still on the Maps that already
                // had them, and re-sending replaces them by name
                route->m_drawn.clear();
                route->m_pending = true;
                m_routesPending.insert(watched->m_id);
            }
        }
    }

    // Sent again for the same reason, and by the same means: forget what was last drawn
    m_recordMapKey.clear();
    m_recordOnMap = false;
    m_watchedTrackKey.clear();

    // The coverage outline is otherwise only sent when a bin grows, which on HFDL can be
    // hours away - and the one restored from the database at startup, before any Map
    // existed, was never sent at all
    if (m_settings.m_displayCoverage) {
        sendCoverageToMap();
    }
}

// A route held back for want of a Map to draw it on, or a name to draw it under. Neither
// is signalled, so this is polled - but only past the two tests that are cheap, so the
// expensive part, resolving each waypoint against the databases, is reached only when the
// route could actually be delivered.
void AircraftTracker::retryRoutes()
{
    if (m_routesPending.isEmpty()) {
        return;
    }

    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);
    if (mapPipes.isEmpty()) {
        return;
    }

    const QSet<quint64> pending = m_routesPending;
    for (quint64 id : pending)
    {
        TrackedFlight *flight = flightById(id);
        if (!flight)
        {
            m_routesPending.remove(id);     // Deleted, or merged into another flight
            continue;
        }
        sendRouteToMap(flight, flight->m_flightPlan, "Flight plan");
        sendRouteToMap(flight, flight->m_clearance, "Oceanic clearance");
        if (!flight->m_flightPlan.m_pending && !flight->m_clearance.m_pending) {
            m_routesPending.remove(id);
        }
    }
}

void AircraftTracker::removeFromMap(const QString& name)
{
    QList<ObjectPipe*> mapPipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(m_feature, "mapitems", mapPipes);

    for (const auto& pipe : mapPipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setImage(new QString(""));  // An empty image removes the item
        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_feature, swgMapItem);
        messageQueue->push(msg);
    }
}
