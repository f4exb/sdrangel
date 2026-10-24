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

#ifndef INCLUDE_FEATURE_AIRCRAFTTRACKER_H_
#define INCLUDE_FEATURE_AIRCRAFTTRACKER_H_

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QHostAddress>
#include <QGeoCoordinate>
#include <QMutex>

#include "util/acarsmessage.h"
#include <QMetaType>

#include "util/message.h"
#include "util/messagequeue.h"
#include "util/aircraftreport.h"
#include "util/osndb.h"
// For the tables a filed route is resolved against - see resolveRoute()
#include "util/openaip.h"
#include "util/ourairportsdb.h"
#include "util/waypoints.h"

#include "aircraftsettings.h"

class Feature;
struct sqlite3;

namespace SWGSDRangel {
    class SWGMapItem;
    class SWGMapCoordinate;
    class SWGMapFlightPlan;
    class SWGMapDatalinkMessage;
}

// Snapshots of the tracker's state, sent to the GUI over queued signals so the
// GUI never touches the tracker thread's data
struct AircraftDisplay {
    quint64 m_id = 0;
    quint32 m_icao = 0;
    QString m_registration;
    QString m_flight;
    // The flight it is on now, under both of its names - see FlightDisplay and
    // AircraftTracker::flightNameForms(). The aliases belong to the FLIGHT, so an
    // airframe that is not on one has only the name it was last heard under.
    QString m_callsign;
    QString m_flightNumber;
    bool m_positionValid = false;
    // Distance from the antenna that heard the aircraft, in km - see TrackedAircraft
    bool m_distanceValid = false;
    float m_distanceKm = 0.0f;
    float m_latitude = 0.0f;
    float m_longitude = 0.0f;
    bool m_altitudeValid = false;
    float m_altitudeFt = 0.0f;
    bool m_headingValid = false;
    float m_heading = 0.0f;
    bool m_speedValid = false;
    float m_speedKts = 0.0f;
    QString m_protocols;
    int m_messages = 0;
    QDateTime m_lastSeen;
    //!< When the Map stops holding this aircraft, which is the same instant the CZML
    //!< availability interval ends. An expiry rather than a flag, so that a row the
    //!< tracker has not touched since still answers correctly as time passes - the
    //!< sweep that takes a stale aircraft off the Map does not refresh the tables.
    //!< Invalid when there is no position, i.e. it was never placed at all.
    QDateTime m_mapAvailableUntil;
    QString m_mapName;
    quint64 m_currentFlightId = 0;
    bool m_active = false;      // Data is arriving for this aircraft
    //!< Its SELCAL code: the one it was heard called on, otherwise what the SELCAL
    //!< database assigns it
    QString m_selcal;
};

struct FlightDisplay {
    quint64 m_id = 0;
    quint64 m_aircraftId = 0;
    QString m_flight;
    // The same flight under its two names. An airline has an ICAO designator of three
    // letters and an IATA one of two, and the aircraft is called by both: the transponder
    // broadcasts TOM7BD while ACARS carries BY07BD, which is the same flight. They are
    // held together as aliases; these say which of them is which, so the table can put
    // them side by side rather than leaving you to match them up.
    QString m_callsign;         // ICAO form, or the primary name where none is known
    QString m_flightNumber;     // IATA form, empty until a message gives one
    QString m_aliases;
    QString m_reg;
    QString m_departure;
    QString m_arrival;
    QString m_route;
    QDateTime m_firstSeen;
    QDateTime m_lastSeen;
    int m_documents = 0;
    int m_messages = 0;
    QString m_protocols;        // Protocols and frequencies THIS flight was heard on
    QDateTime m_out;
    QDateTime m_off;
    QDateTime m_on;
    QDateTime m_in;
    // Whether the flight has produced each of the documents worth knowing about at a
    // glance, so the table can say so without opening the flight
    bool m_hasLoadsheet = false;
    bool m_hasClearance = false;
    bool m_hasFlightPlan = false;
    bool m_active = false;      // The flight an active aircraft is currently flying
    // Where the airport pair came from, and what the callsign to route database makes of
    // it. The database is a guess - one route per callsign, flown or not - so a message
    // that disagrees with it is the message being right, as TOM3TW was.
    QString m_routeSource;      // "Database" or "Message", empty when neither end is known
    bool m_routeChecked = false;    // The database had an entry to compare against
    bool m_routeAgrees = false;     // ...and it says the same thing
    // Whether each end was stated by a received message rather than guessed from the
    // callsign. Per end, because they often arrive from different messages at different
    // times, and the table colours each cell on its own.
    bool m_departureFromMessage = false;
    bool m_arrivalFromMessage = false;
};

struct DocumentEvent {
    quint64 m_aircraftId = 0;
    quint64 m_flightId = 0;
    QDateTime m_received;
    QString m_flight;
    QString m_reg;
    QString m_kind;
    QString m_title;
    QString m_text;
    QString m_mapName;
};

struct AtcEvent {
    quint64 m_aircraftId = 0;
    quint64 m_flightId = 0;
    QDateTime m_received;
    QString m_protocol;
    //!< The link the message came in on and its frequency, as "VDL2 136.975", in the
    //!< same form as the Protocols column of the aircraft and flights tables. Empty for
    //!< messages read back from a database written before this was recorded
    QString m_link;
    bool m_uplink = false;
    QString m_from;
    QString m_to;
    QString m_message;
    QString m_tooltip;
    QString m_mapName;
    bool m_stored = false;      //!< Already in the database - only the rest are written
};

Q_DECLARE_METATYPE(AircraftDisplay)
Q_DECLARE_METATYPE(FlightDisplay)
Q_DECLARE_METATYPE(DocumentEvent)
// A weather report is about an AIRPORT, not about the aircraft that happened to ask
// for it - so it outlives that aircraft and is kept in its own log
struct WeatherEvent {
    QDateTime m_received;
    QString m_airport;          // ICAO code, empty if the report did not name one
    QString m_kind;             // METAR, TAF, ATIS, TWIP, NOTAM, PIREP, SIGMET
    QString m_summary;          // First line, for the table
    QString m_text;             // The report as sent
    QString m_from;             // Aircraft that carried it, for provenance
};

Q_DECLARE_METATYPE(AtcEvent)
Q_DECLARE_METATYPE(WeatherEvent)

// Collates aircraft reports from the demodulators into aircraft, flights and
// documents, drives the Map and persists the session, all off the GUI thread.
// Runs on its own thread inside the Aircraft feature, in both GUI and server
// builds; the GUI, when there is one, is a view driven by this class's signals.
// Session and lifetime statistics shown on the Statistics tab.
//
// Every figure is kept twice: once for this session and once for all time. The two
// answer different questions - "is the aerial working as well as it did yesterday"
// wants the session, "how far have we ever heard" wants all time - and neither is
// recoverable from the other, so both are counted as reports arrive.
//
// The all time side persists. Most of it travels in the settings blob, as the records
// always have. The one exception is the aircraft count, which cannot be a running
// total: the same airframe heard on two days must count once, so it needs the set of
// who has been heard, not a number. That set is the database's "seen" table, which is
// also the only part of this that survives the retention period pruning the rest.
struct AircraftStatistics {
    // A record holder: the value, who set it, when, and where they were at the time.
    // The position is what lets the max range record be drawn on the Map at the point
    // it was actually achieved rather than wherever the aircraft is now - which for an
    // all time record is very probably nowhere, the aircraft having long since landed.
    struct Record {
        bool m_valid = false;
        float m_value = 0.0f;
        QString m_aircraft;         // Registration, else flight, else ICAO address
        QDateTime m_when;
        bool m_positionValid = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        bool m_altitudeValid = false;
        float m_altitudeFt = 0.0f;
        //!< The channel it was heard on, in Hz. Which protocol set the record is already
        //!< known - there is a row per protocol - but not which of its frequencies, and
        //!< on HFDL and Aero that is the interesting part of the answer.
        qint64 m_frequency = 0;

        // Returns true if this beat the record
        bool update(float value, const QString& aircraft, const QDateTime& when,
                    bool positionValid, float latitude, float longitude,
                    bool altitudeValid, float altitudeFt, qint64 frequency)
        {
            if (m_valid && (value <= m_value)) {
                return false;
            }
            m_valid = true;
            m_value = value;
            m_aircraft = aircraft;
            m_when = when;
            m_positionValid = positionValid;
            m_latitude = latitude;
            m_longitude = longitude;
            m_altitudeValid = altitudeValid;
            m_altitudeFt = altitudeFt;
            m_frequency = frequency;
            return true;
        }
    };

    // One complete set of figures, over whatever period the holder covers
    struct Scope {
        // Furthest heard on each protocol, indexed by AircraftReport::Protocol. Per
        // protocol rather than one overall figure because the whole point of comparing
        // them is that they are so different - line of sight VHF against HF skywave
        // against a satellite.
        Record m_maxRange[AircraftReport::ProtocolCount];
        Record m_fastest;           // Ground speed, knots
        Record m_highest;           // Altitude, feet

        quint64 m_totalMessages = 0;    // Reports of any kind
        quint64 m_messagesByProtocol[AircraftReport::ProtocolCount] = {};
        int m_distinctAircraft = 0;     // Different aircraft heard
        int m_maxConcurrent = 0;        // Most aircraft heard within any 15 minute window
        QDateTime m_maxConcurrentWhen;
        qint64 m_seconds = 0;           // Time spent listening, filled in as it is emitted

        // Applies a report to every record this scope keeps
        void update(int protocol, bool rangeValid, float rangeKm, bool speedValid,
                    float speedKts, bool altitudeValid, float altitudeFt,
                    const QString& who, const QDateTime& when,
                    bool positionValid, float latitude, float longitude,
                    qint64 frequency)
        {
            m_totalMessages++;
            if ((protocol >= 0) && (protocol < AircraftReport::ProtocolCount))
            {
                m_messagesByProtocol[protocol]++;
                if (rangeValid) {
                    m_maxRange[protocol].update(rangeKm, who, when, positionValid,
                                                latitude, longitude, altitudeValid, altitudeFt,
                                                frequency);
                }
            }
            if (speedValid) {
                m_fastest.update(speedKts, who, when, positionValid, latitude, longitude,
                                 altitudeValid, altitudeFt, frequency);
            }
            if (altitudeValid) {
                m_highest.update(altitudeFt, who, when, positionValid, latitude, longitude,
                                 altitudeValid, altitudeFt, frequency);
            }
        }
    };

    Scope m_session;
    Scope m_allTime;
    QDateTime m_sessionStart;
    QDateTime m_firstStart;         // When the all time figures began
    // How much disk the session database is taking, including its write ahead log.
    // Measured rather than stored, so it is not part of what is saved or restored.
    qint64 m_databaseBytes = 0;

    static const int ConcurrentWindowMins = 15;
};

Q_DECLARE_METATYPE(AircraftStatistics)

class AircraftTracker : public QObject
{
    Q_OBJECT

public:
    //!< Select an aircraft or a flight in the GUI. Sent by the web API, and so by the
    //!< Map when one of the links in an aircraft's popup is clicked.
    class MsgSelect : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        enum Kind { Icao, Registration, Flight };
        Kind getKind() const { return m_kind; }
        const QString& getValue() const { return m_value; }
        static MsgSelect* create(Kind kind, const QString& value) {
            return new MsgSelect(kind, value);
        }
    private:
        Kind m_kind;
        QString m_value;
        MsgSelect(Kind kind, const QString& value) :
            Message(), m_kind(kind), m_value(value)
        { }
    };

    class MsgConfigureTracker : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const AircraftSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }
        static MsgConfigureTracker* create(const AircraftSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureTracker(settings, settingsKeys, force);
        }
    private:
        AircraftSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;
        MsgConfigureTracker(const AircraftSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(), m_settings(settings), m_settingsKeys(settingsKeys), m_force(force)
        {}
    };

    class MsgReport : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AircraftReport& getReport() { return m_report; }
        static MsgReport* create(const AircraftReport& report) { return new MsgReport(report); }
        ~MsgReport();   // Owns m_report.m_mapItem until processing transfers it
    private:
        AircraftReport m_report;
        MsgReport(const AircraftReport& report) : Message(), m_report(report) {}
    };

    // One ACARS block as received, to be archived and fed on
    class MsgAcarsBlock : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        const QByteArray& getBlock() const { return m_block; }
        const QDateTime& getDateTime() const { return m_dateTime; }
        qint64 getFrequency() const { return m_frequency; }
        double getLevel() const { return m_level; }
        int getProtocol() const { return m_protocol; }
        static MsgAcarsBlock* create(const QByteArray& block, const QDateTime& dateTime,
                                     qint64 frequency, double level, int protocol)
        { return new MsgAcarsBlock(block, dateTime, frequency, level, protocol); }
    private:
        QByteArray m_block;
        QDateTime m_dateTime;
        qint64 m_frequency;
        double m_level;
        int m_protocol;
        MsgAcarsBlock(const QByteArray& block, const QDateTime& dateTime, qint64 frequency,
                      double level, int protocol) :
            Message(), m_block(block), m_dateTime(dateTime), m_frequency(frequency),
            m_level(level), m_protocol(protocol) {}
    };

    class MsgDeleteAll : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgDeleteAll* create() { return new MsgDeleteAll(); }
    private:
        MsgDeleteAll() : Message() {}
    };

    // Zero one half of the statistics. The all time half also clears the record of
    // which airframes have been heard, which is the only way its aircraft count can go
    // back to nothing
    class MsgResetStatistics : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        bool getAllTime() const { return m_allTime; }
        static MsgResetStatistics* create(bool allTime) { return new MsgResetStatistics(allTime); }
    private:
        bool m_allTime;
        MsgResetStatistics(bool allTime) : Message(), m_allTime(allTime) {}
    };

    // GUI (re)connected: resend the complete state
    class MsgResync : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        static MsgResync* create() { return new MsgResync(); }
    private:
        MsgResync() : Message() {}
    };

    // Which flight's profile the GUI is charting; 0 for none
    class MsgWatchFlight : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        quint64 getFlightId() const { return m_flightId; }
        static MsgWatchFlight* create(quint64 flightId) { return new MsgWatchFlight(flightId); }
    private:
        quint64 m_flightId;
        MsgWatchFlight(quint64 flightId) : Message(), m_flightId(flightId) {}
    };

    explicit AircraftTracker(Feature *feature);
    ~AircraftTracker();

    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public:
    // Used by the GUI, which owns persistence: it holds the blob ready for serialize()
    static QByteArray serializeStatistics(const AircraftStatistics& statistics);
    static AircraftStatistics deserializeStatistics(const QByteArray& data);

public slots:
    void startWork();
    // Invoked from the GUI thread by the coverage button's right click, so it has
    // to be a slot to cross to the tracker's own thread
    void clearCoverage();

public:
    //!< A snapshot of what is being tracked, for the web API's report. In a public
    //!< section of its own: moc reads everything under "public slots:" as a slot
    //!< declaration and rejects a type there.
    struct Totals {
        int m_aircraft = 0;
        int m_activeAircraft = 0;
        int m_flights = 0;
        quint64 m_messages = 0;
        quint64 m_messagesByProtocol[AircraftReport::ProtocolCount] = {};
    };
    //!< Safe to call from any thread: it returns a copy of a snapshot the tracker's own
    //!< thread refreshes on every flush, rather than walking the aircraft list, which
    //!< that thread is free to change and delete from at any moment
    Totals totals() const;
    //!< Public because the web API's report names each protocol, and the names belong
    //!< with the protocol enum rather than being written out a second time there
    static QString protocolName(int protocol);
    //!< A protocol and the frequency it was heard on, as "VDL2 136.975". The
    //!< frequency is left off when it is unknown, and for ADS-B, where it is always
    //!< the same and says nothing
    static QString sourceText(int protocol, qint64 frequency);

signals:
    void aircraftUpdated(const QList<AircraftDisplay>& aircraft);
    //!< Asked to select an aircraft or flight from outside - see MsgSelect
    void selectRequested(int kind, const QString& value);
    void aircraftRemoved(const QList<quint64>& ids);
    void flightsUpdated(const QList<FlightDisplay>& flights);
    void flightsRemoved(const QList<quint64>& ids);
    //!< Two flights turned out to be one - the same flight under its ACARS and its ADS-B
    //!< name - and everything that belonged to "from" now belongs to "to". Distinct from
    //!< flightsRemoved, which means a flight is genuinely gone: anything the views hold
    //!< against the old id has to be re-pointed rather than dropped.
    void flightsMerged(quint64 from, quint64 to);
    //!< The same for an aircraft absorbed into another: emitted before the aircraftRemoved
    //!< for "from", whose flights now belong to "to" with their ids unchanged
    void aircraftMerged(quint64 from, quint64 to);
    void documentsAdded(const QList<DocumentEvent>& documents);
    void atcMessages(const QList<AtcEvent>& messages);
    void weatherReports(const QList<WeatherEvent>& reports);
    void profileUpdated(quint64 flightId, const QList<qint64>& times, const QList<float>& altitudeFt, const QList<float>& speedKts);
    void messageRates(const QList<float>& ratesPerProtocolThenTotal);
    void statisticsUpdated(const AircraftStatistics& statistics);
    void speechNotification(const QString& speech);
    void allCleared();
    //!< The database could not be changed, so the setting has been put back to the file
    //!< actually in use. The GUI holds the settings that get serialised, so it has to
    //!< hear about it or it would keep - and save - a path nothing is writing to.
    void databaseFilenameReverted(const QString& filename);

private:
    struct TrackedAircraft;

    struct TrackedDocument {
        int m_kind;
        QString m_title;
        QString m_text;         //!< Rendered from the two below when they are set
        QDateTime m_received;
        //!< What it was rendered FROM, so a later decoder renders it again rather than
        //!< the text being frozen as it was first written - see renderDocument(). Empty
        //!< for a document libacars or the ATN decoder produced, which cannot be re-run
        //!< from here, and whose m_text is therefore what is stored.
        QByteArray m_raw;
        QString m_source;
    };

    struct TrackedFlight {
        quint64 m_id = 0;
        QString m_flight;
        QStringList m_aliases;
        TrackedAircraft *m_aircraft = nullptr;
        QDateTime m_firstSeen;
        QDateTime m_lastSeen;
        QString m_departure;
        QString m_arrival;
        QString m_route;
        //!< When the message that revealed m_departure, m_arrival and m_route was
        //!< received, so a delayed one cannot put back what a newer one has replaced.
        //!< Invalid where the facts came from the callsign to route database, which is a
        //!< standing guess rather than something the aircraft said.
        QDateTime m_routeFiled;
        //!< Whether m_departure and m_arrival are a guess from the callsign to route
        //!< database rather than something an aircraft said. Without it the two are
        //!< indistinguishable once stored, and a guess planted first was then defended
        //!< against the truth - see updateFlight() and mergeFlightInto().
        //!< Deliberately not persisted: it matters while a flight is being built up, and
        //!< adding a column would mean recreating everyone's database to gain nothing on
        //!< reload, where the stored airports are simply taken as they are.
        //!< Per airport, not per flight: one end can be a database guess while the
        //!< other has been stated by a message, and the table colours each on its own
        bool m_departureFromDatabase = false;
        bool m_arrivalFromDatabase = false;
        // A route the aircraft has been given, drawn on the Map. Kept apart from
        // m_route because the callsign to route database also fills that in, with the
        // airport pair rather than a list of waypoints, and only a real waypoint list
        // can be drawn.
        struct FiledRoute {
            QString m_waypoints;    //!< The route as filed, names separated by spaces
            //!< Where the message put those waypoints, when it said. Same order and
            //!< length as m_waypoints, or empty when it named them without placing them.
            QList<AircraftReport::RouteWaypoint> m_points;
            //!< The rest of the plan, each part taken from whichever insert carried
            //!< it - see updateFlight(). A clearance leaves them empty.
            QString m_runway;
            QString m_sid;
            QString m_star;
            QString m_approach;
            QString m_alternate;
            //!< What an oceanic clearance cleared it to fly the route at
            int m_flightLevel = 0;
            float m_mach = 0.0f;
            //!< The plan's own airports rather than the flight's, which is what tells a
            //!< second insert of the same plan from a re-file to somewhere else
            QString m_departure;
            QString m_arrival;
            QString m_text;         //!< The message that filed it, for the popup
            QDateTime m_filed;      //!< When, so a merge can tell two routes apart
            QString m_drawn;        //!< What was last sent, so it is only sent once
            //!< Held back for want of a Map to send it to or a name to key it on -
            //!< neither of which the flight is told about when it changes
            bool m_pending = false;
            //!< The Map item names last emitted, so that anything which will not be
            //!< drawn again can be taken off. The Map keys on the name, so a route that
            //!< has been renamed would otherwise be drawn twice over.
            QString m_drawnLine;
            QStringList m_drawnWaypoints;
        };
        FiledRoute m_flightPlan;    //!< An FPN plan, or a position report's route insert
        FiledRoute m_clearance;     //!< An oceanic clearance or its readback
        //!< Cache for the flight plan attached to map items for the Map's ND.
        //!< Assembling a route is database lookups per fix, so it is rebuilt only when
        //!< the signature - what the assembly depends on - changes. Points and coords
        //!< are index aligned, resolveRoute() having dropped anything unplaceable.
        QString m_ndPlanSignature;
        QList<AircraftReport::RouteWaypoint> m_ndPlanPoints;
        QList<QGeoCoordinate> m_ndPlanCoords;
        //!< Protocols and frequencies THIS flight has been heard on, the same tally the
        //!< airframe keeps but bounded to one flight. The Map shows this one: an
        //!< airframe heard on HFDL over the Atlantic last month is not being heard on
        //!< HFDL now, so naming it beside the aircraft on the Map is simply wrong.
        QHash<QPair<int, qint64>, int> m_sources;
        QList<TrackedDocument *> m_documents;
        QList<qint64> m_profileTimes;
        QList<float> m_profileAltFt;
        QList<float> m_profileSpeedKts;
        // The ground track belongs to the FLIGHT, not to the airframe. An aircraft is
        // heard again days later on a different route, and drawing every track it has
        // ever flown puts unrelated lines across the map
        // How many of the points below are already in the database. The track and the
        // profile only ever grow, so a save writes the tail rather than rewriting the
        // whole history - see saveDatabase(). Reset to zero, and the flight added to
        // m_tracksToRewrite or m_profilesToRewrite, whenever that array is rebuilt rather
        // than appended to.
        int m_trackSaved = 0;
        int m_profileSaved = 0;
        //!< Likewise for the documents, which are only ever appended to. Reset to zero,
        //!< and the flight added to m_documentsToRewrite, when the list is cut down.
        int m_documentsSaved = 0;
        //!< Whether the stored track and profile have been read in. A flight created
        //!< while running has nothing stored, so both start true; a restored one starts
        //!< false. Between them they were nine million rows and eight of the ten seconds
        //!< a season's database took to open, for history that is looked at one flight at
        //!< a time if at all - see ensureTrackLoaded().
        bool m_trackLoaded = true;
        bool m_profileLoaded = true;
        QList<QGeoCoordinate> m_track;
        QList<QDateTime> m_trackTimes;
        int m_messages = 0;             // Reports of any kind attributed to this flight
        // The OOOI times, as the flight reported them. Out of gate, take off, landing,
        // on gate - the four instants that describe a flight from the ground's point of
        // view, and the ones an airline's own systems run on.
        QDateTime m_out;
        QDateTime m_off;
        QDateTime m_on;
        QDateTime m_in;
        // The ground track belongs to the FLIGHT, not to the airframe. An aircraft is
        // heard again days later on a different route, and drawing every track it has
        // ever flown puts unrelated lines across the map

        ~TrackedFlight() { qDeleteAll(m_documents); }
    };

    // How much a position can be trusted, best first. A VDL-2 XID link management frame
    // carries an aircraft location parameter with 0.1 degree resolution - about 11 km -
    // which is enough for a ground station to decide a handoff, but drags an ADS-B track
    // sideways every time one arrives
    enum PositionRank {
        PositionAdsb = 0,       // ADS-B: metres, about once a second
        PositionReported = 1,   // A real position report, whatever protocol carried it
        PositionCoarse = 2,     // A location parameter from a link management frame
        PositionRanks = 3
    };

    static int positionRank(const AircraftReport& report);

    // Altitude is ranked separately from position, and for a different reason. An XID
    // location parameter carries an altitude as a single octet of thousands of feet, and
    // a minority of aircraft - overwhelmingly Boeing 737NGs - fill it with nonsense:
    // 59000, 65000, 90000, 123000 ft have all been observed off air, from aircraft whose
    // ADS-B put them at a normal cruise at the same moment. The value is not merely
    // coarse, it is sometimes wrong, so it must not set a record and must not displace a
    // real altitude. See the note above AcarsVdl2Atn's altitude handling for the
    // measurements behind that.
    enum AltitudeRank {
        AltitudeAdsb = 0,       // ADS-B: 25 ft, about once a second
        AltitudeReported = 1,   // A real position or performance report
        AltitudeCoarse = 2,     // A link management frame's location parameter
        AltitudeRanks = 3
    };

    static int altitudeRank(const AircraftReport& report);

    // A less accurate source is only used once the better ones have gone quiet for this long
    static const int m_positionRankTimeoutSecs = 60;
    // How long a flight number may still stand in for an identity. Long enough to cover
    // a flight heard intermittently, far short of the same number returning tomorrow.
    static const int m_flightKeyValidSecs = 12*60*60;
    // Positions that would need a faster aircraft than this to reach are stale or wrong
    static const int m_maxGroundSpeedKn = 700;
    static constexpr double m_positionToleranceKm = 5.0;

    struct TrackedAircraft {
        quint64 m_id = 0;
        quint32 m_icao = 0;
        QString m_registration;
        QString m_flight;
        bool m_positionValid = false;
        float m_latitude = 0.0f;
        float m_longitude = 0.0f;
        float m_altitudeFt = 0.0f;
        bool m_altitudeValid = false;
        // The two most recent altitude observations, so an altitude can be worked out
        // for the moment a position was measured rather than assuming they coincide
        QDateTime m_altitudeDateTime;
        float m_prevAltitudeFt = 0.0f;
        QDateTime m_prevAltitudeDateTime;
        float m_heading = 0.0f;
        bool m_headingValid = false;
        float m_speedKts = 0.0f;
        bool m_speedValid = false;
        //!< When each was observed. Neither is timestamped in its own right - a velocity
        //!< message says what the aircraft is doing now, not when - so this is the
        //!< report's reception time, which is enough to stop a delayed report putting
        //!< back a heading or speed the aircraft has already left behind.
        QDateTime m_headingDateTime;
        QDateTime m_speedDateTime;
        // Great circle distance from the receiver to the aircraft, in km. Measured from
        // the position of the DEVICE the report arrived on where it has one - a remote
        // SDR reports where it is - and from My Position otherwise, so a distance always
        // means "from the antenna that heard it" rather than "from here".
        bool m_distanceValid = false;
        float m_distanceKm = 0.0f;
        QDateTime m_positionDateTime;
        QDateTime m_positionRankTimes[PositionRanks];   // When a position of each rank last arrived
        int m_positionRank = PositionRanks;             // Rank of the position we are holding
        QDateTime m_altitudeRankTimes[AltitudeRanks];   // When an altitude of each rank last arrived
        QDateTime m_lastSeen;
        QString m_departure;
        QString m_arrival;
        QString m_route;
        QString m_lastDocumentText;
        //!< The latest datalink ATC message (CPDLC/PDC/oceanic clearance), attached to
        //!< map items for the Map's DCDU. The sequence number is what tells the Map a
        //!< new message from a re-send of the last one.
        struct DcduMessage {
            int m_seq = 0;          // 0 = never had one
            QDateTime m_received;
            bool m_uplink = false;
            QString m_station;
            QString m_text;
        } m_dcduMessage;
        // Every protocol and frequency this airframe has EVER been heard on, kept for
        // as long as the airframe is. The flight has its own, which is what the Map
        // shows: an airframe heard on HFDL over the Atlantic last month is not being
        // heard on HFDL now, and saying so on the Map is simply wrong.
        QHash<QPair<int, qint64>, int> m_sources;
        int m_messages = 0;
        QString m_mapName;
        QDateTime m_lastMapItemTime;
        QString m_model3D;
        float m_modelAltitudeOffset = 0.0f;     // So the undercarriage is not underground
        float m_labelAltitudeOffset = 0.0f;
        // Whether we chose the model or the ADS-B pass-through brought one. Only ours may
        // be thrown away and chosen again when the livery preference changes - replacing
        // a pass-through model with our own would fight the demodulator for it.
        bool m_modelIsOurs = false;
        bool m_mapTrackChanged = true;
        //!< The track has only grown since it was last sent. The ADS-B pass-through sends
        //!< it at most every few seconds for that (see forwardMapItem()), where a change
        //!< to what was already sent goes at once.
        bool m_mapTrackAppended = false;
        qint64 m_mapTrackSentMs = 0;
        // A fully timestamped ADS-B map item came from CSV replay. Its position is
        // deliberately historical, so wall-clock stale-position expiry must not
        // remove it before the Map's historical clock can display the flight.
        bool m_mapReplay = false;
        //!< Owed to a Map that has just appeared, and still owed until something has
        //!< actually been sent - see checkMapConsumers()
        bool m_mapResync = false;
        QList<TrackedFlight *> m_flights;
        TrackedFlight *m_currentFlight = nullptr;
        // Set when a report arrives, cleared when the aircraft goes quiet. Aircraft
        // restored from the session database start inactive, so a new session begins
        // with empty active tables
        bool m_active = false;
        QSet<int> m_notifiedRules;
        //!< The SELCAL code it was last called on this session. Only ever one the database
        //!< assigns it - a call is not attributed otherwise - but where the database gives
        //!< it several, this says which is in use.
        QString m_selcalHeard;

        ~TrackedAircraft() { qDeleteAll(m_flights); }
    };

    Feature *m_feature;                 // Pipe source for the Map
    AircraftSettings m_settings;

    //!< What totals() hands out - see there
    mutable QMutex m_totalsMutex;
    Totals m_totals;
    void updateTotals();
    MessageQueue m_inputMessageQueue;

    QList<TrackedAircraft *> m_aircraft;
    QHash<QString, TrackedAircraft *> m_byKey;
    quint64 m_nextId = 1;

    QSharedPointer<const QHash<QString, AircraftInformation *>> m_aircraftInfo;
    QSharedPointer<const QHash<int, AircraftInformation *>> m_aircraftInfoByIcao;
    QSharedPointer<const QHash<QString, AircraftRouteInformation *>> m_routeInfo;
    // Used to turn the named waypoints of a filed route into coordinates. All three are
    // process wide caches shared with the Map and the ACARS demodulator, so holding them
    // here costs a pointer rather than another copy of the data.
    QSharedPointer<const QList<NavAid *>> m_navAids;
    QSharedPointer<const QHash<QString, AirportInformation *>> m_airports;
    QSharedPointer<const QMultiHash<QString, Waypoint *>> m_waypoints;

    QList<AtcEvent> m_atcLog;           // Self-contained, outlives its aircraft
    QList<WeatherEvent> m_weatherLog;   // Likewise, and keyed on airports not aircraft
    // Which reports are already in the log, so a repeat is recognised without comparing
    // it against every entry ever logged. Maps a hash of the report to its position; the
    // full comparison still happens on a hash match, so a collision costs one comparison
    // rather than silently dropping a report that was not really a duplicate. The log is
    // only ever appended to, so the positions never move.
    QMultiHash<size_t, int> m_weatherIndex;

    // Batched GUI notifications
    QSet<TrackedAircraft *> m_dirtyAircraft;
    QSet<TrackedFlight *> m_dirtyFlights;
    QList<DocumentEvent> m_pendingDocuments;
    QList<AtcEvent> m_pendingAtc;
    QList<WeatherEvent> m_pendingWeather;
    QTimer *m_flushTimer = nullptr;
    QTimer *m_removalTimer = nullptr;
    QTimer *m_chartTimer = nullptr;
    // One counter per protocol plus a total
    static const int CHART_COUNTS = AircraftReport::ProtocolCount + 1;
    int m_chartCounts[CHART_COUNTS];
    QDateTime m_chartRateTime;

    quint64 m_watchedFlight = 0;        // Flight whose profile the GUI charts
    bool m_watchedFlightChanged = false;

    sqlite3 *m_db = nullptr;
    // Flights whose stored track and profile no longer match what is in memory. Deleted
    // flights have to be named, because the save no longer empties those tables and so
    // would otherwise leave their rows behind for ever.
    QSet<quint64> m_flightsToDelete;
    // Separate, because rewriting one must not delete the other's stored rows when only
    // one of them was rebuilt - the other may not even be loaded
    QSet<quint64> m_tracksToRewrite;
    QSet<quint64> m_profilesToRewrite;
    QSet<quint64> m_documentsToRewrite;
    bool m_dbTrackWipe = false;     //!< Everything in track and profile is stale
    // The ATC log is written incrementally too, so what changes in it after a message is
    // stored has to be carried to the database: the aircraft and flights it belongs to
    // when they are merged (from, to), and the messages that go with discarded aircraft
    QList<QPair<quint64, quint64>> m_atcAircraftMoves;
    QList<QPair<quint64, quint64>> m_atcFlightMoves;
    QSet<quint64> m_atcAircraftToDelete;
    //!< Messages from before m_atcPruneCutoff whose aircraft no longer exists - or never
    //!< did, id 0 being an unattributed call - by aircraft id
    QSet<quint64> m_atcPrunedIds;
    QDateTime m_atcPruneCutoff;
    bool m_dbDirty = false;
    bool m_loadingDatabase = false;

    bool handleMessage(const Message& message);
    void processReport(AircraftReport& report);

    TrackedAircraft *findOrCreateAircraft(const AircraftReport& report);
    void absorbAircraft(TrackedAircraft *item, TrackedAircraft *weak);
    TrackedFlight *updateFlight(TrackedAircraft *item, const AircraftReport& report);
    void mergeFlightInto(TrackedAircraft *item, TrackedFlight *dst, TrackedFlight *src);
    void mergeLoadedFlights(TrackedAircraft *item);
    static bool splitFlightName(const QString& flight, QString& airline, QString& number);
    static void flightNameForms(const QString& primary, const QStringList& aliases,
                                QString& callsign, QString& flightNumber);
    static bool sameFlightNumber(const QString& a, const QString& b);
    static bool flightMatches(const TrackedFlight *flight, const QString& name);
    //!< When this aircraft was last heard flying this callsign, which is not when it was
    //!< last heard: it is very likely flying something else by now
    static QDateTime flightLastSeen(const TrackedAircraft *item, const QString& flight);
    //!< Whether two of one airframe's flights are one operation rather than the same
    //!< callsign flown again on another day
    bool sameOperation(const TrackedFlight *a, const TrackedFlight *b) const;
    void addFlightAlias(TrackedAircraft *item, TrackedFlight *flight, const QString& name);
    void addDocument(TrackedAircraft *item, TrackedFlight *flight, const AircraftReport& report);
    void addAtcMessage(TrackedAircraft *item, TrackedFlight *flight, const AircraftReport& report);
    //!< Returns false when the call could not be pinned on one aircraft, having logged it
    bool attributeSelcal(AircraftReport& report);
    //!< item and flight are null for a call that was not attributed; candidates are then
    //!< the registrations the database gives the code
    void logSelcalCall(TrackedAircraft *item, TrackedFlight *flight, const AircraftReport& report,
                       const QStringList& candidates);
    //!< The SELCAL codes assigned to each registration, for the table's SELCAL column.
    //!< Refreshed with the other databases rather than per row: looking it up checks the
    //!< file for a newer download.
    QSharedPointer<const QHash<QString, QString>> m_selcalCodes;
    void addWeatherReport(TrackedAircraft *item, const AircraftReport& report);
    static QString weatherKindName(int kind);
    QString documentKindName(int kind) const;
    static QString notificationSignature(const AircraftSettings& settings);
    void claimKey(TrackedAircraft *item, const QString& key);
    void updateRecordMapItem();
    static const char *RecordMapItemName;
    //!< The Map settings group a filed route is drawn under, instead of this feature's
    //!< own. A route wants the opposite of what an aircraft wants from a minimum zoom -
    //!< an icon has to be decluttered away when zoomed out, while a route drawn across
    //!< an ocean is exactly what should still be visible there.
    static const char *RouteMapGroup;
    static const char *CoverageMapGroup;
    //!< Aggregator feed. One set of connections for the whole station, however many
    //!< ACARS demodulators are running - which is why this lives here and not in them.
    QUdpSocket *m_feedSocket = nullptr;
    QTcpSocket *m_feedAirframesTcpSocket = nullptr;
    QTcpSocket *m_feedAvdelphiTcpSocket = nullptr;
    QHostAddress m_feedAirframesAddr;   //!< Cached DNS resolutions, for UDP
    QString m_feedAirframesResolvedHost;
    QHostAddress m_feedAvdelphiAddr;
    QString m_feedAvdelphiResolvedHost;
    QHash<QString, QDateTime> m_feedResolveRetry;   //!< When a host that would not resolve is tried again
    //!< ACARS blocks heard since the last save, exactly as received, waiting to be written
    //!< to the archive. Raw so that a later decoder can be run over the whole archive -
    //!< see MainCore::MsgAcarsBlock. Emptied by each save; the archive lives on disk only.
    struct AcarsBlockRecord {
        QByteArray m_block;
        QDateTime m_dateTime;
        qint64 m_frequency = 0;
        double m_level = 0.0;
        int m_protocol = 0;
    };
    QList<AcarsBlockRecord> m_acarsBlocks;
    static QGeoCoordinate routeAnchor(const QStringList& names);
    static QString altitudeConstraint(const AircraftReport::RouteWaypoint& point);
    static QList<AircraftReport::RouteWaypoint> procedurePoints(const QString& procedure,
                                                                const QString& airport,
                                                                const QString& procedureType);
    void resolveRoute(QList<AircraftReport::RouteWaypoint>& points,
                      QList<QGeoCoordinate>& coords) const;
    QList<AircraftReport::RouteWaypoint> assembleRoutePoints(const TrackedFlight *flight,
                                                             const TrackedFlight::FiledRoute& route,
                                                             QList<QGeoCoordinate>& coords) const;
    SWGSDRangel::SWGMapFlightPlan *buildFlightPlan(TrackedAircraft *item) const;
    void recordDcduMessage(TrackedAircraft *item, const AircraftReport& report);
    SWGSDRangel::SWGMapDatalinkMessage *buildDatalinkMessage(const TrackedAircraft *item) const;
    void sendRouteToMap(TrackedFlight *flight, TrackedFlight::FiledRoute& route,
                        const QString& kind);
    void removeDrawnRoute(TrackedFlight::FiledRoute& route);
    bool routeDrawn(const TrackedFlight *flight) const;
    int routePlaceable(const QString& waypoints,
                       const QList<AircraftReport::RouteWaypoint>& given) const;
    void sendRoutes(TrackedFlight *flight);
    void removeRoutes(TrackedFlight *flight);

    // What updateRecordMapItem() last put on the Map, so an unchanged record is not
    // re-sent 3 times a second, and so it can be taken off again when the option is
    // turned off or the record is reset
    //!< The Map item name and contents of the watched flight's track, drawn on its own
    //!< when the aircraft itself is not on the Map - see updateWatchedTrack()
    QString m_watchedTrackName;
    QString m_watchedTrackKey;
    void updateWatchedTrack();

    QString m_recordMapKey;
    bool m_recordOnMap = false;
    //!< Which of the reasons the record marker is or is not on the Map has been
    //!< reported. Said once each rather than several times a second, and reset when the
    //!< situation changes, so the log explains a missing marker without filling up.
    enum RecordMapState { RecordUnreported, RecordDrawn, RecordNone, RecordUnplaceable };
    int m_recordMapState = RecordUnreported;

    // Flights with a route that could not be delivered. Held by id rather than by
    // pointer, so a flight deleted or merged away in the meantime simply fails to
    // resolve rather than leaving this dangling.
    QSet<quint64> m_routesPending;
    void retryRoutes();

    // WHICH Map features were listening last time anything was sent, by pipe id rather
    // than by count: closing one Map and opening another between flushes leaves the count
    // where it was, and the replacement would never be caught up. Delivery is remembered
    // per item rather than per Map - what has been drawn, what track was last sent - so a
    // Map opened after the fact is given later positions but none of what went out before
    // it existed. Saying it all again to everyone is the cheap answer; per consumer state
    // would be the thorough one.
    QSet<unsigned int> m_mapConsumers;
    // Aircraft owed to a Map that has just opened but not sendable yet: a basic item
    // would replace a richer pass-through one for a minute after the last of those
    // arrived. Kept here rather than in m_dirtyAircraft, which flush() empties every
    // pass and which drives a GUI update for everything in it.
    QSet<TrackedAircraft *> m_mapResyncPending;
    void checkMapConsumers();

    // Statistics
    void updateStatistics(const AircraftReport& report, TrackedAircraft *item,
                          bool rangeValid, float rangeKm,
                          bool altitudeValid, float altitudeFt);

    void updateConcurrent(const QDateTime& now);
    void updateDatabaseSize();
    static QString renderDocument(const QByteArray& raw, const QString& source);
    void acarsBlockReceived(const QByteArray& block, const QDateTime& dateTime,
                            qint64 frequency, double level, int protocol);
    void feedMessage(const AcarsMessage& message, const QDateTime& dateTime,
                     qint64 frequency, double level);
    void updateFeedConnections();
    QGeoCoordinate receiverPosition(int deviceSetIndex);
    void update3DModel(TrackedAircraft *item);

    AircraftStatistics m_statistics;
    bool m_statisticsDirty = false;
    bool m_statisticsLoaded = false;
    // Seconds carried in from previous sessions, so the all time clock can be reported
    // as a live total rather than only advancing when the app is restarted
    qint64 m_allTimeSecondsBase = 0;
    // What m_allTimeSecondsBase is measured from. Normally the start of the session, but
    // an all time reset moves it to the moment of the reset - otherwise the all time
    // clock would restart at however long this session had already been running
    QDateTime m_allTimeSince;
    // Aircraft heard this session. The tracked list is restored from the database at
    // startup, so "a TrackedAircraft was allocated" counts only aircraft that fell out
    // of the retention period - which under-reported the session badly
    QSet<quint64> m_sessionHeard;
    // Those of them already written to the all time table. Separate from the above
    // because an aircraft can be heard before it is identified - an ACARS report can
    // arrive with only a flight number and gain its registration later - and it must be
    // counted under the identity it will keep, not the first one it happened to show
    QSet<quint64> m_sessionRecorded;
    // Drives a statistics update once a minute so both elapsed clocks tick even when
    // nothing is being received
    qint64 m_statisticsMinute = -1;
    // Receiver positions, cached by device set index: getDevicePosition() builds a whole
    // device report and parses it as JSON, which is far too heavy to do per message
    QHash<int, QGeoCoordinate> m_devicePositions;
    QDateTime m_devicePositionsRefreshed;
    // Taking the tally rather than its owner, because both the airframe and the
    // flight it is on keep one
    QStringList sourcesList(const QHash<QPair<int, qint64>, int>& sources) const;
    QString sourcesText(const QHash<QPair<int, qint64>, int>& sources) const;
    QString mapItemName(const TrackedAircraft *item) const;
    QString flightLabel(const QString& flight) const;
    QString atcLabel(const TrackedAircraft *item) const;
    QString aircraftImage(const TrackedAircraft *item) const;
    //!< The same choice for an aircraft that is no longer tracked - all the record
    //!< marker has is the name and, where the airframe is still known, its 3D model
    QString aircraftImage(const QString& registration, const QString& model3D) const;
    QList<SWGSDRangel::SWGMapCoordinate *> *buildTrack(const TrackedFlight *flight) const;
    QDateTime mapAvailableUntil(const TrackedAircraft *item) const;
    int positionTimeoutSecs(const TrackedAircraft *item) const;
    bool positionStale(const TrackedAircraft *item, const QDateTime& now) const;
    static float altitudeAt(const TrackedAircraft *item, const QDateTime& when, float fallbackFt);
    // Which of an aircraft's flights the map should draw the track of - the one it is
    // flying now, unless the GUI is looking at one of its past flights
    const TrackedFlight *trackFlight(const TrackedAircraft *item) const;
    //!< trackFlight() and then ensureTrackLoaded() on it, for the two places that draw
    TrackedFlight *trackFlightLoaded(TrackedAircraft *item);
    void sendToMap(TrackedAircraft *item);
    void forwardMapItem(TrackedAircraft *item, SWGSDRangel::SWGMapItem *swgMapItem);

    void removeFromMap(const QString& name);
    void checkNotifications(TrackedAircraft *item, const AircraftReport& report);
    QString subAircraftString(TrackedAircraft *item, const AircraftReport& report, const QString& string) const;
    void commandNotification(TrackedAircraft *item, const AircraftReport& report, const QString& command);
    void deleteAll();
    void resync();
    void applySettings(const AircraftSettings& settings, const QList<QString>& settingsKeys, bool force);
    AircraftDisplay displayFor(const TrackedAircraft *item) const;
    FlightDisplay displayFor(const TrackedFlight *flight) const;
    DocumentEvent documentEvent(const TrackedAircraft *item, const TrackedFlight *flight, const TrackedDocument *doc) const;
    void emitProfile(const TrackedFlight *flight);
    TrackedFlight *flightById(quint64 id) const;
    QString regFor(const TrackedAircraft *item) const;

    // Implemented in aircraftdb.cpp
    bool openDatabase();
    // Remembers that this airframe has been heard at all, for the all time aircraft
    // count. Returns 1 if it had not been heard before, 0 if it had, and -1 if it cannot
    // be identified yet - in which case the caller should try again on the next report
    //!< Writes this airframe into the all time "seen" table, folding away a row left
    //!< under an identity it has since outgrown. Returns 1 when the table was written,
    //!< -1 when it could not be and the caller should ask again, and sets countDelta to
    //!< the change in the number of distinct airframes the table holds: +1 for one not
    //!< seen before, -1 where two rows turned out to be one airframe, 0 otherwise.
    int recordSeen(TrackedAircraft *item, int& countDelta);
    int seenCount();
    void resetSeen();
    void closeDatabase();
    //!< Returns false when nothing was written - the transaction rolled back, or there
    //!< is no database. The session is then still only in memory, which is what makes it
    //!< unsafe to switch databases: see changeDatabase().
    bool saveDatabase();
    //!< The all time records and counters, which live with the session they were measured
    //!< from rather than in the preset. saveStatistics() runs inside saveDatabase()'s
    //!< transaction; loadStatistics() returns false when there are none stored, which is
    //!< what makes the one time import from an older preset happen.
    bool saveStatistics();
    bool loadStatistics();
    //!< The coverage polygons, kept in their own table so that adding them did not mean
    //!< recreating everyone's database - see loadCoverage()
    bool saveCoverage();
    bool loadCoverage();

    //!< How far out each protocol has ever heard an aircraft, in each direction. The
    //!< ADS-B demodulator draws the same thing for itself; here there is one per
    //!< protocol, in its own colour, so their ranges can be compared - HFDL and Aero
    //!< reach thousands of miles where VHF is line of sight, and seeing that on one map
    //!< is the point.
    //!<
    //!< Bins of 5 degrees, as the ADS-B one uses. The range is kept rather than the
    //!< polygon: the polygon is derived, and a bin is a scalar that survives a change of
    //!< receiver position.
    static const int m_coverageBinDeg = 5;
    static const int m_coverageBins = 360 / m_coverageBinDeg;
    QVector<float> m_coverageRangeKm[AircraftReport::ProtocolCount];
    //!< Where the receiver was when the bins were filled. Per protocol, because they can
    //!< be different receivers - an HFDL dongle and an ADS-B one are not always in the
    //!< same place, and a polygon drawn from the wrong centre is worse than none.
    QGeoCoordinate m_coverageOrigin[AircraftReport::ProtocolCount];
    bool m_coverageDirty[AircraftReport::ProtocolCount] = {};
    //!< What is currently on the Map for each protocol. A list, because coverage is
    //!< drawn as one shape per run of adjacent measured bearings and the number of
    //!< them changes as the gaps fill in - so what to remove has to be remembered
    //!< rather than derived.
    QStringList m_coverageDrawnNames[AircraftReport::ProtocolCount];
    bool m_coverageLoaded = false;
    //!< Set when a bin grows or the user clears, so the next database save writes
    //!< the table and an idle session does not
    bool m_coverageChanged = false;
    void updateCoverage(int protocol, const QGeoCoordinate& receiver, float azimuthDeg,
                        float rangeKm);
    void sendCoverageToMap(int protocol);
    void sendCoverageToMap();
    void removeCoverageFromMap();
    static QRgb coverageColor(int protocol);
    //!< Read a restored flight's track or profile in, if that has not happened already.
    //!< Idempotent, so callers need not know which flights are which. MUST be called
    //!< before appending to either: until the stored points are in memory, m_trackSaved
    //!< counts rows the list does not yet hold, and the save would append nothing.
    void ensureTrackLoaded(TrackedFlight *flight);
    void ensureProfileLoaded(TrackedFlight *flight);
    void ensureAllHistoryLoaded();
    void revertDatabaseFilename(const QString& path);
    // The stored track and profile no longer relate to what is in memory at all
    void forgetSavedHistory();
    bool saveMessageChanges();
    void forgetPendingChanges();
    void forgetAllTimeFigures();
    bool moveMessages(bool aircraftId, quint64 from, quint64 to);
    void loadDatabase();

public:
    // Where the session database lives, given the settings - which is not simply the
    // filename, since an empty one means the default under AppData. Public because the
    // GUI names the file beside the database size it reports.
    static QString databasePath(const AircraftSettings& settings);
    int tableCount(const char *table);
    // Set by deleteAll(), so that emptying the documents and ATC tables is something the
    // user asked for rather than something saveDatabase() has to infer from memory being
    // empty - which is exactly the state it must not act on. Cleared after the commit.
    bool m_dbMessageWipe = false;
    // Set when what is stored no longer relates to what is held, so the documents and ATC
    // tables are emptied and everything in memory written again. Unlike m_dbMessageWipe,
    // the ACARS archive is left alone: it is not held in memory to be written back.
    bool m_dbMessageRewrite = false;

private slots:
    void handleInputMessages();
    void flush();
    void removeOldAircraft();
    void setActive(TrackedAircraft *item, bool active);
    void discardOldAircraft();
    void changeDatabase(const QString& oldPath);
    void updateChartCounts();
};

#endif // INCLUDE_FEATURE_AIRCRAFTTRACKER_H_
