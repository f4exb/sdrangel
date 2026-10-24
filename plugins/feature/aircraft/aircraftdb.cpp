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

// Session persistence for the Aircraft feature: the collated aircraft, flights,
// documents and ATC log are kept in a SQLite database (vendored amalgamation -
// public domain, no Qt SQL dependency), written every minute and on exit, and
// restored on startup. Runs on the tracker thread, so the GUI never blocks on a
// save. Aircraft older than the removal timeout are pruned on load; the
// self-contained ATC log is restored in full.

#include <cmath>
#include <limits>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "sqlite/sqlite3.h"
#include "util/aircraft3dmodels.h"

#include "aircrafttracker.h"

// Current schema version. Only this version is read: an older database is started
// over and a newer one is left alone.
#define AIRCRAFT_DB_VERSION 5

// Where the session is kept. The setting wins; otherwise the default location, whose
// directory may not exist yet
QString AircraftTracker::databasePath(const AircraftSettings& settings)
{
    if (!settings.m_databaseFilename.isEmpty())
    {
        QDir().mkpath(QFileInfo(settings.m_databaseFilename).absolutePath());
        return settings.m_databaseFilename;
    }

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir().mkpath(locations[0]);
    return locations[0] + "/aircraft.db";
}

// What the session database is costing on disk. SQLite is in WAL mode, so what has been
// written most recently is in the -wal file rather than in the database itself, and a
// figure that ignored it would sit still for a whole session and then jump on a checkpoint.
void AircraftTracker::updateDatabaseSize()
{
    const QString path = databasePath(m_settings);
    qint64 bytes = 0;

    for (const QString& suffix : { QString(), QString("-wal"), QString("-shm") })
    {
        const QFileInfo info(path + suffix);
        if (info.exists()) {
            bytes += info.size();
        }
    }
    if (bytes != m_statistics.m_databaseBytes)
    {
        m_statistics.m_databaseBytes = bytes;
        m_statisticsDirty = true;
    }
}

static bool execSQL(sqlite3 *db, const char *sql)
{
    char *errMsg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker: SQL failed:" << sql << ":" << (errMsg ? errMsg : "");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// Run a DELETE taking one flight id for each of the given flights, saying whether all
// of them went through
static bool deleteByFlight(sqlite3 *db, const char *sql, const QSet<quint64>& flightIds)
{
    if (flightIds.isEmpty()) {
        return true;
    }
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::saveDatabase: could not prepare" << sql << ":" << sqlite3_errmsg(db);
        return false;
    }
    bool ok = true;
    for (quint64 id : flightIds)
    {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, (qint64) id);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            qWarning() << "AircraftTracker::saveDatabase:" << sql << "failed:" << sqlite3_errmsg(db);
            ok = false;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

// Run a prepared INSERT and say whether the row went in.
//
// sqlite3_step returns SQLITE_DONE for a successful INSERT; anything else - a full disk,
// an I/O error, a constraint - means the row is not there. Ignoring it let a failed save
// look successful, and saveDatabase() then cleared the dirty flag, so nothing retried and
// the database silently stayed as it was.
static bool stepInsert(sqlite3 *db, sqlite3_stmt *stmt, const char *what, bool& failed)
{
    if (failed) {
        return false;       // Already given up on this transaction
    }
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        qWarning() << "AircraftTracker::saveDatabase: writing" << what << "failed:"
                   << sqlite3_errmsg(db);
        failed = true;
        return false;
    }
    return true;
}

static void bindText(sqlite3_stmt *stmt, int idx, const QString& text)
{
    QByteArray utf8 = text.toUtf8();
    sqlite3_bind_text(stmt, idx, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
}

static QString columnText(sqlite3_stmt *stmt, int idx)
{
    const char *text = (const char *) sqlite3_column_text(stmt, idx);
    return text ? QString::fromUtf8(text) : QString();
}

static QDateTime columnDateTime(sqlite3_stmt *stmt, int idx)
{
    qint64 ms = sqlite3_column_int64(stmt, idx);
    return ms ? QDateTime::fromMSecsSinceEpoch(ms) : QDateTime();
}

static qint64 dateTimeValue(const QDateTime& dateTime)
{
    return dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : 0;
}

// Add a column if the table does not already have it.
//
// Migrating on the version number alone is not enough: a column added to CREATE TABLE
// and to the migration for the version BELOW the current one never reaches a database
// that is already at the current version, and the next INSERT then binds more values
// than the table has columns and fails to prepare - taking the whole save with it.
// Asking the table what it actually has is immune to that.
static bool tableExists(sqlite3 *db, const char *table)
{
    sqlite3_stmt *stmt = nullptr;
    bool exists = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
        exists = (sqlite3_step(stmt) == SQLITE_ROW);
    }
    sqlite3_finalize(stmt);
    return exists;
}

// The waypoint positions a message carried, as one string: "NAME@lat,lon" per waypoint
// where it was placed, the bare name where it was not, separated by spaces. Waypoint
// names hold neither a space nor an @, so nothing has to be escaped.
//
// They are stored rather than re-resolved from the names, because an oceanic clearance
// names positions like 5730N020W that no database holds - the coordinates the message
// itself gave are the only way to place them, and dropping them would lose exactly the
// waypoints that make an oceanic route worth drawing.
static QString encodeRoutePoints(const QList<AircraftReport::RouteWaypoint>& points)
{
    QStringList out;
    for (const AircraftReport::RouteWaypoint& point : points)
    {
        if (point.m_positionValid)
        {
            // Six decimal places: enough for a tenth of a metre, where the default six
            // SIGNIFICANT digits would round a three digit longitude to about 100 m
            out.append(QString("%1@%2,%3").arg(point.m_name)
                .arg(point.m_latitude, 0, 'f', 6).arg(point.m_longitude, 0, 'f', 6));
        }
        else
        {
            out.append(point.m_name);
        }
    }
    return out.join(" ");
}

// Several short fields as one. The unit separator, which no ARINC field contains, and a
// fixed order - so a part added later appends rather than shifting the ones before it,
// and a database written before it simply has fewer.
static const QChar RoutePartSeparator(0x1f);

static QString encodeRouteParts(const QStringList& parts)
{
    for (const QString& part : parts)
    {
        if (!part.isEmpty()) {
            return parts.join(RoutePartSeparator);
        }
    }
    return QString();       // Nothing filed, so nothing to store
}

static void decodeRouteParts(const QString& text, const QList<QString *>& into)
{
    if (text.isEmpty()) {
        return;
    }
    const QStringList parts = text.split(RoutePartSeparator);
    for (int i = 0; (i < parts.size()) && (i < into.size()); i++) {
        *into[i] = parts[i];
    }
}

// Recover a plan's parts from the message that filed it, for rows written before those
// parts were stored.
//
// The runway, SID, STAR, approach and alternate were only added to the flights table
// part way through a session, so every plan filed before that has its document text and
// its waypoints but none of the parts - the STAR is sitting in the message, in plain
// sight, and the route drawn for that flight cannot use it. The text is this
// application's own rendering of the plan, one "Label: value" a line, so reading the
// parts back out of it is reliable rather than a guess at someone else's format.
//
// Takes the parts in the same order decodeRouteParts() does, and only ever fills one
// that is empty - so a part an insert actually filed always wins over a parsed one, and
// running this twice does nothing the first run did not. Once the flight is saved again
// the parts are stored properly and this stops finding anything to do.
static void recoverRoutePartsFromText(const QString& text, const QList<QString *>& into)
{
    if (text.isEmpty()) {
        return;
    }

    // An airport is rendered with its name after it - "SBGR (Sao Paulo/Guarulhos...)" -
    // and sometimes with a runway stuck to it, as "ESSA(19L)". The code is the leading
    // run of letters and digits either way.
    auto airport = [](const QString& value)
    {
        int end = 0;
        while ((end < value.size()) && value[end].isLetterOrNumber()) {
            end++;
        }
        return value.left(end);
    };

    struct Field {
        const char *m_label;
        int m_index;
        bool m_isAirport;
    };
    static const Field fields[] {
        { "Runway",    0, false },
        { "SID",       1, false },
        { "STAR",      2, false },
        { "Approach",  3, false },
        { "Alternate", 4, true  },
        { "Departure", 5, true  },
        { "Arrival",   6, true  },
    };

    const QStringList lines = text.split('\n');

    for (const QString& line : lines)
    {
        const int colon = line.indexOf(':');

        if (colon < 0) {
            continue;
        }

        const QString label = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();

        if (value.isEmpty()) {
            continue;
        }

        for (const Field& field : fields)
        {
            if ((label != QLatin1String(field.m_label)) || (field.m_index >= into.size())) {
                continue;
            }
            // Only what is missing, so a filed part always wins over a parsed one
            if (into[field.m_index]->isEmpty()) {
                *into[field.m_index] = field.m_isAirport ? airport(value) : value;
            }
            break;
        }
    }
}

static QList<AircraftReport::RouteWaypoint> decodeRoutePoints(const QString& text)
{
    QList<AircraftReport::RouteWaypoint> points;

    for (const QString& field : text.split(" ", Qt::SkipEmptyParts))
    {
        AircraftReport::RouteWaypoint point;
        const int at = field.indexOf('@');
        if (at < 0)
        {
            point.m_name = field;
        }
        else
        {
            point.m_name = field.left(at);
            const QStringList coords = field.mid(at + 1).split(',');
            if (coords.size() == 2)
            {
                bool latOk = false, lonOk = false;
                const float latitude = coords[0].toFloat(&latOk);
                const float longitude = coords[1].toFloat(&lonOk);
                if (latOk && lonOk)
                {
                    point.m_positionValid = true;
                    point.m_latitude = latitude;
                    point.m_longitude = longitude;
                }
            }
        }
        points.append(point);
    }
    return points;
}

static void ensureColumn(sqlite3 *db, const char *table, const char *column, const char *decl)
{
    sqlite3_stmt *stmt = nullptr;
    const QString query = QString("PRAGMA table_info(%1)").arg(table);
    bool found = false;
    if (sqlite3_prepare_v2(db, query.toUtf8().constData(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            if (name && (qstrcmp((const char *) name, column) == 0))
            {
                found = true;
                break;
            }
        }
    }
    sqlite3_finalize(stmt);
    if (!found)
    {
        qDebug() << "AircraftTracker::openDatabase: adding" << table << "." << column;
        execSQL(db, QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table).arg(column).arg(decl).toUtf8().constData());
    }
}

// Whether a table has a column. Used to tell a database that merely SAYS it is the
// current version from one that actually has the current version's shape.
static bool columnExists(sqlite3 *db, const char *table, const char *column)
{
    sqlite3_stmt *stmt = nullptr;
    bool found = false;
    const QByteArray sql = QByteArray("PRAGMA table_info(") + table + ")";

    if (sqlite3_prepare_v2(db, sql.constData(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            if (name && (qstrcmp((const char *) name, column) == 0))
            {
                found = true;
                break;
            }
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

bool AircraftTracker::openDatabase()
{
    QString path = databasePath(m_settings);
    if (sqlite3_open(path.toUtf8().constData(), &m_db) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::openDatabase: Failed to open" << path << ":" << sqlite3_errmsg(m_db);
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Write-ahead logging is considerably faster for the periodic full rewrite
    execSQL(m_db, "PRAGMA journal_mode=WAL");
    execSQL(m_db, "PRAGMA synchronous=NORMAL");

    // Whether the all time table is about to be created, which says this database has
    // never had one and so its existing aircraft have never been counted into it
    const bool seenIsNew = !tableExists(m_db, "seen");

    // A database written by an older version is DROPPED here, before the tables below
    // are created, rather than merely emptied.
    //
    // Emptying is not enough, because a version change can move the schema and not only
    // the data: CREATE TABLE IF NOT EXISTS leaves an existing table exactly as it was, so
    // a table that has gained a column is still the old shape and DELETE does nothing
    // about that. That is not hypothetical - when documents gained its raw and source
    // columns the insert could no longer be prepared, so every save failed and rolled
    // back, and the next start found an empty database. Silently, because a save that
    // rolls back looks like a save that had nothing to do.
    //
    // seen is deliberately left alone: it is the all time airframe count, and it is meant
    // to survive everything else being thrown away.
    if (tableExists(m_db, "meta"))
    {
        int existing = 0;
        sqlite3_stmt *versionStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT version FROM meta", -1, &versionStmt, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(versionStmt) == SQLITE_ROW) {
                existing = sqlite3_column_int(versionStmt, 0);
            }
        }
        sqlite3_finalize(versionStmt);

        // What this build's inserts require, and therefore what makes a database the
        // right SHAPE rather than merely the right version number.
        //
        // The version alone is not enough to trust. A database written by the code that
        // emptied tables on a version change instead of recreating them ends up claiming
        // the new version while still having the old columns - and then every save fails
        // to prepare and rolls back, silently, for good. One did. Add a line here
        // whenever a table gains a column, and such a database repairs itself on the next
        // start rather than needing the version bumped to shift it.
        const bool shapeIsCurrent = columnExists(m_db, "documents", "raw")
                                 && columnExists(m_db, "documents", "source")
                                 && tableExists(m_db, "acars_blocks");

        // A newer build's database is left exactly as it is, rather than thrown away by
        // a build that merely cannot read it
        if (existing > AIRCRAFT_DB_VERSION)
        {
            qWarning() << "AircraftTracker::openDatabase:" << path << "is database version" << existing
                       << "- this build only reads version" << AIRCRAFT_DB_VERSION << "- not using it";
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }

        // 0 is a database with no version row yet. Only this version is read - older
        // ones were only ever development databases - so anything else is started over.
        if (((existing != 0) && (existing != AIRCRAFT_DB_VERSION)) || !shapeIsCurrent)
        {
            qDebug() << "AircraftTracker::openDatabase: Database version" << existing
                     << (shapeIsCurrent ? "cannot be migrated" : "does not have this version's tables")
                     << "- recreating its tables";
            execSQL(m_db,
                "DROP TABLE IF EXISTS aircraft; DROP TABLE IF EXISTS sources;"
                "DROP TABLE IF EXISTS track; DROP TABLE IF EXISTS flight_sources;"
                "DROP TABLE IF EXISTS stat_records; DROP TABLE IF EXISTS stat_counters;"
                "DROP TABLE IF EXISTS flights; DROP TABLE IF EXISTS documents;"
                "DROP TABLE IF EXISTS acars_blocks; DROP TABLE IF EXISTS profile;"
                "DROP TABLE IF EXISTS atc; DROP TABLE IF EXISTS meta;");
        }
    }

    bool ok = execSQL(m_db,
        "CREATE TABLE IF NOT EXISTS meta (version INTEGER);"
        "CREATE TABLE IF NOT EXISTS aircraft ("
            "id INTEGER PRIMARY KEY, icao INTEGER, registration TEXT, flight TEXT,"
            "position_valid INTEGER, latitude REAL, longitude REAL,"
            "altitude_valid INTEGER, altitude REAL,"
            "heading_valid INTEGER, heading REAL,"
            "speed_valid INTEGER, speed REAL,"
            "position_time INTEGER, last_seen INTEGER,"
            "departure TEXT, arrival TEXT, route TEXT,"
            "last_document TEXT, messages INTEGER, model3d TEXT);"
        "CREATE TABLE IF NOT EXISTS sources ("
            "aircraft_id INTEGER, protocol INTEGER, frequency INTEGER, count INTEGER);"
        // The same tally per flight rather than per airframe - what a given flight was
        // heard on, which is what the Map shows. Added without a version bump because
        // IF NOT EXISTS creates it on the next open of an older database, where bumping
        // would send every existing database down the "starting afresh" branch and
        // delete everything in it.
        // The all time statistics. They used to live in the preset, on the grounds that
        // they describe the receiving setup rather than the traffic - but the database is
        // the per setup file, and keeping the records in one place and the airframe count
        // that belongs with them ("seen", below) in another meant switching databases
        // gave one database's count with another's records. Like "seen", these are never
        // touched by clearing the aircraft or by the retention period: they outlive the
        // traffic they were measured from.
        //
        // One row per record: kind 0 is maximum range, and then protocol says which;
        // kind 1 is fastest and kind 2 highest, neither of which is per protocol.
        "CREATE TABLE IF NOT EXISTS stat_records ("
            "kind INTEGER, protocol INTEGER, value REAL, aircraft TEXT, recorded INTEGER,"
            "position_valid INTEGER, latitude REAL, longitude REAL,"
            "altitude_valid INTEGER, altitude_ft REAL, frequency INTEGER,"
            "PRIMARY KEY (kind, protocol));"
        "CREATE TABLE IF NOT EXISTS stat_counters (name TEXT PRIMARY KEY, value INTEGER);"
        "CREATE TABLE IF NOT EXISTS coverage ("
        "  protocol INTEGER, bin INTEGER, range REAL, latitude REAL, longitude REAL,"
        "  PRIMARY KEY (protocol, bin));"
        "CREATE TABLE IF NOT EXISTS flight_sources ("
            "flight_id INTEGER, protocol INTEGER, frequency INTEGER, count INTEGER);"
        "CREATE TABLE IF NOT EXISTS track ("
            "flight_id INTEGER, seq INTEGER, latitude REAL, longitude REAL, altitude REAL, time INTEGER);"
        "CREATE TABLE IF NOT EXISTS flights ("
            "id INTEGER PRIMARY KEY, aircraft_id INTEGER, flight TEXT,"
            "first_seen INTEGER, last_seen INTEGER,"
            "departure TEXT, arrival TEXT, route TEXT, is_current INTEGER,"
            "aliases TEXT DEFAULT '', messages INTEGER DEFAULT 0,"
            "out_time INTEGER DEFAULT 0, off_time INTEGER DEFAULT 0,"
            "on_time INTEGER DEFAULT 0, in_time INTEGER DEFAULT 0);"
        // text is what a document was LAST rendered as, and raw/source are what it was
        // rendered from. A document our own parser produced is rendered again on load, so
        // that a decoder fix reaches everything already stored rather than only what
        // arrives after it; one that libacars or the ATN decoder produced has no raw and
        // keeps its text, because neither of those is reachable from this feature.
        "CREATE TABLE IF NOT EXISTS documents ("
            "flight_id INTEGER, kind INTEGER, title TEXT, text TEXT, received INTEGER,"
            "raw BLOB, source TEXT);"
        // Every ACARS block heard, exactly as it came off the air. Raw, and one row per
        // BLOCK rather than per reassembled message, so that a later decoder can be run
        // over the whole archive - see MainCore::MsgAcarsBlock. None of the metadata is
        // in the block itself, so it is stored beside it.
        "CREATE TABLE IF NOT EXISTS acars_blocks ("
            "received INTEGER, frequency INTEGER, level REAL, protocol INTEGER, block BLOB);"
        "CREATE TABLE IF NOT EXISTS profile ("
            "flight_id INTEGER, time INTEGER, altitude REAL, speed REAL);"
        "CREATE TABLE IF NOT EXISTS atc ("
            "time INTEGER, protocol TEXT, uplink INTEGER,"
            "from_station TEXT, to_station TEXT, message TEXT, tooltip TEXT,"
            "aircraft_id INTEGER, flight_id INTEGER, map_name TEXT, link TEXT DEFAULT '');"
        // Every airframe ever heard, for the all time aircraft count. One row each, no
        // matter how often it comes back, which is the whole reason a count cannot do
        // the job. Kept out of every DELETE below: this is the one table that has to
        // outlive both the retention period and a schema reset, and losing it would
        // silently reset a figure the user cannot recover. Added without a version bump
        // because IF NOT EXISTS creates it on the next open of an older database - and
        // bumping would send every existing v3 database down the "starting afresh"
        // branch, which deletes everything
        "CREATE TABLE IF NOT EXISTS seen ("
            "key TEXT PRIMARY KEY, first_seen INTEGER, last_seen INTEGER);");
    if (!ok)
    {
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // An existing database already knows about aircraft heard before the all time table
    // existed, so seed it from them rather than starting the count at nothing on an
    // installation that has been running for months. Only what is still within the
    // retention period can be recovered, so the figure starts low and is honest about it
    // via the "since" date. Done once, on creation: repeating it every open would undo a
    // reset, since the aircraft table would put the same rows straight back.
    if (seenIsNew)
    {
        execSQL(m_db,
            "INSERT OR IGNORE INTO seen (key, first_seen, last_seen) "
            "SELECT CASE WHEN icao IS NOT NULL AND icao != 0 THEN printf('%06X', icao) "
                       "ELSE 'R:' || registration END, last_seen, last_seen "
            "FROM aircraft "
            "WHERE (icao IS NOT NULL AND icao != 0) "
               "OR (registration IS NOT NULL AND registration != '')");
    }

    // Columns added after a version bump, which the version chain below would miss on a
    // database already at the current version
    // Messages stored before the link was recorded keep an empty one, which the table
    // shows as a blank cell rather than guessing
    ensureColumn(m_db, "atc", "link", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "messages", "INTEGER DEFAULT 0");
    ensureColumn(m_db, "flights", "out_time", "INTEGER DEFAULT 0");
    ensureColumn(m_db, "flights", "off_time", "INTEGER DEFAULT 0");
    ensureColumn(m_db, "flights", "on_time", "INTEGER DEFAULT 0");
    ensureColumn(m_db, "flights", "in_time", "INTEGER DEFAULT 0");
    // The departure, arrival and route are stored, so what decides whether a report may
    // replace them has to be stored with them - otherwise the first message after a
    // restart wins however delayed it is, which is the case the timestamp exists for
    ensureColumn(m_db, "flights", "route_filed", "INTEGER DEFAULT 0");
    // The filed routes themselves. m_route above is only the route STRING shown in the
    // tables; what the Map draws is the flight plan and the oceanic clearance, each with
    // its own waypoints, the positions the message gave for them, the message text for
    // the popup and when it was filed. None of it was stored, so a restored flight had
    // nothing to draw and selecting an archived flight showed its track but no route.
    // Appended after route_filed, and in this order, because the flights INSERT binds by
    // position - so a database that predates them ends up with the same column order as
    // one created now.
    ensureColumn(m_db, "flights", "fp_waypoints", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "fp_points", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "fp_text", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "fp_filed", "INTEGER DEFAULT 0");
    ensureColumn(m_db, "flights", "oc_waypoints", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "oc_points", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "oc_text", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "oc_filed", "INTEGER DEFAULT 0");
    // What an insert carries besides the path - runway, SID, STAR, approach, alternate
    // and the plan's own airports - as one field each. They are only ever read and
    // written together, and a column apiece would be fourteen more on a table that
    // already has two dozen.
    ensureColumn(m_db, "flights", "fp_parts", "TEXT DEFAULT ''");
    ensureColumn(m_db, "flights", "oc_parts", "TEXT DEFAULT ''");

    // Without these, everything keyed on a flight is a full table scan - and track runs
    // to millions of rows. It was tolerable while the whole table was read once at
    // startup and never queried again, but a per flight DELETE on a merge or a prune
    // scanned the lot, and fetching one flight's track on demand would too.
    execSQL(m_db, "CREATE INDEX IF NOT EXISTS track_flight ON track (flight_id);");
    execSQL(m_db, "CREATE INDEX IF NOT EXISTS profile_flight ON profile (flight_id);");
    // And for the same reason the documents and ATC messages, now that a save deletes
    // and moves them by flight and aircraft rather than replacing the tables
    execSQL(m_db, "CREATE INDEX IF NOT EXISTS documents_flight ON documents (flight_id);");
    execSQL(m_db, "CREATE INDEX IF NOT EXISTS atc_aircraft ON atc (aircraft_id);");
    execSQL(m_db, "CREATE INDEX IF NOT EXISTS atc_flight ON atc (flight_id);");
    // A model is no use on its own: without the offsets it sits half underground, and
    // without knowing whether we chose it, the livery setting may not replace it
    ensureColumn(m_db, "aircraft", "model_altitude_offset", "REAL DEFAULT 0");
    ensureColumn(m_db, "aircraft", "label_altitude_offset", "REAL DEFAULT 0");
    // Defaulted to -1, which no row written since says: that is what distinguishes a
    // row from before these columns existed, whose model has no offsets to go with it,
    // from one that legitimately holds a zero offset
    ensureColumn(m_db, "aircraft", "model_is_ours", "INTEGER DEFAULT -1");

    // Schema version. Anything other than this version had its tables dropped above,
    // which leaves no version row, so the only case left is a new database.
    sqlite3_stmt *stmt = nullptr;
    int version = 0;
    if (sqlite3_prepare_v2(m_db, "SELECT version FROM meta", -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    if (version == 0) {
        execSQL(m_db, "INSERT INTO meta (version) VALUES (" QT_STRINGIFY(AIRCRAFT_DB_VERSION) ")");
    }

    return true;
}

// The user has chosen a different file for the session. If nothing is there yet the
// current session moves with them, so the aircraft, flights and messages they have
// collected are not left behind; if a database is already there it is theirs and is
// opened as it stands, replacing what is in memory.
// Put the setting back to the file actually in use, and tell the GUI - it owns the
// settings that are serialised, so leaving it holding the path that failed would save a
// pointer to a database nothing is writing to.
void AircraftTracker::revertDatabaseFilename(const QString& path)
{
    m_settings.m_databaseFilename =
        (path == AircraftSettings::defaultDatabaseFilename()) ? QString() : path;
    emit databaseFilenameReverted(m_settings.m_databaseFilename);
}

// A consistent copy of a database, -wal and all, while anything else may have it open
static bool copyDatabase(const QString& from, const QString& to)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open_v2(from.toUtf8().constData(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
    {
        sqlite3_close(db);
        return false;
    }
    sqlite3_stmt *stmt = nullptr;
    bool ok = false;
    if (sqlite3_prepare_v2(db, "VACUUM INTO ?", -1, &stmt, nullptr) == SQLITE_OK)
    {
        bindText(stmt, 1, to);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        if (!ok) {
            qWarning() << "AircraftTracker::changeDatabase: VACUUM INTO failed:" << sqlite3_errmsg(db);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

void AircraftTracker::changeDatabase(const QString& oldPath)
{
    const QString newPath = databasePath(m_settings);

    if (newPath == oldPath) {
        return;
    }

    // Make sure everything collected so far is on disk before the file moves. If it
    // could not be, the switch does not happen: adopting an existing database calls
    // deleteAll() below, which would throw away the very session that failed to save.
    // The setting goes back to the file actually in use, so the two do not disagree.
    if (m_db && !m_loadingDatabase && !saveDatabase())
    {
        qWarning() << "AircraftTracker::changeDatabase: could not save the session to"
                   << oldPath << "- keeping it rather than switching to" << newPath;
        revertDatabaseFilename(oldPath);
        return;
    }
    // Everything into the main file before it moves. Only a checkpoint that completed
    // says so: one that could not (another connection reading) leaves committed pages in
    // the -wal file, which a rename would leave behind.
    bool walFlushed = false;
    if (m_db) {
        walFlushed = sqlite3_wal_checkpoint_v2(m_db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr) == SQLITE_OK;
    }
    closeDatabase();

    const bool haveExisting = QFileInfo::exists(newPath) && (QFileInfo(newPath).size() > 0);

    // The session moves as the file it is, never as what memory can write back. Memory
    // does not hold the ACARS archive or the all time airframe count, nor the track and
    // profile of flights not yet looked at, which are read from the database on demand -
    // and once the database was closed they were read from the new, empty one.
    bool moved = false;
    bool copied = false;
    if (!haveExisting && QFileInfo::exists(oldPath))
    {
        // An empty file at the target holds nothing, and on Windows a rename onto an
        // existing file fails
        if (QFileInfo::exists(newPath)) {
            QFile::remove(newPath);
        }
        // QFile::rename copies and removes when the paths are on different volumes
        moved = walFlushed && QFile::rename(oldPath, newPath);
        if (moved)
        {
            qDebug() << "AircraftTracker::changeDatabase: moved the session from" << oldPath << "to" << newPath;
        }
        else
        {
            // A rename fails while anything else has the file open - a database browser,
            // say. SQLite can still copy it, consistently and with the -wal included,
            // and the original is left where it was.
            copied = copyDatabase(oldPath, newPath);
            if (copied)
            {
                qDebug() << "AircraftTracker::changeDatabase: could not move" << oldPath
                         << "so copied it to" << newPath;
            }
            else
            {
                qWarning() << "AircraftTracker::changeDatabase: could neither move nor copy" << oldPath
                           << "to" << newPath << "- staying with it";
                revertDatabaseFilename(oldPath);
                if (!openDatabase()) {
                    qWarning() << "AircraftTracker::changeDatabase: could not reopen" << oldPath
                               << "- the session is now in memory only";
                }
                return;
            }
        }
    }

    if (!openDatabase())
    {
        // Without this there is no database at all: the old one is closed and the new one
        // would not open, so everything from here on would be kept in memory and lost on
        // exit, silently. Go back to the file that was working.
        qWarning() << "AircraftTracker::changeDatabase: could not open" << newPath
                   << "- going back to" << oldPath;
        // The file has to go back before it can be reopened. Reverting the setting alone
        // would point at a path the history is no longer at, and opening it would create
        // an empty database there - leaving every restored aircraft to be written out
        // again over nothing, with the real history stranded under the new name.
        if (moved && !QFile::rename(newPath, oldPath))
        {
            qWarning() << "AircraftTracker::changeDatabase: could not move" << newPath
                       << "back to" << oldPath << "- staying with it rather than opening"
                       << "an empty database over the session";
            if (!openDatabase()) {
                qWarning() << "AircraftTracker::changeDatabase: and could not reopen it"
                           << "- the session is now in memory only";
            }
            return;
        }
        revertDatabaseFilename(oldPath);
        if (!openDatabase()) {
            qWarning() << "AircraftTracker::changeDatabase: could not reopen" << oldPath
                       << "either - the session is now in memory only";
        }
        return;
    }

    if (haveExisting)
    {
        // Their database, their contents: start again from what it holds - the all time
        // figures included, which are this database's and not the last one's
        deleteAll();
        forgetAllTimeFigures();
        loadDatabase();
    }
    else if (!moved && !copied)
    {
        // There was no file to bring: write what is held to the new one
        forgetSavedHistory();
        m_dbDirty = true;
        saveDatabase();
    }
    // Otherwise the file is the session as last saved, so what memory says is stored is
    // still true, and nothing has to be read or written again
}

// The all time statistics and coverage belong to the database they were measured into,
// so switching to another one starts from what that one holds. deleteAll() leaves them
// alone - they are meant to outlive the traffic - and the loaders only fill in what the
// new database has, so without this the last database's figures were kept wherever the
// new one had none, and then saved into it.
void AircraftTracker::forgetAllTimeFigures()
{
    m_statistics.m_allTime = AircraftStatistics::Scope();
    m_allTimeSecondsBase = 0;
    m_allTimeSince = QDateTime::currentDateTime();
    m_statisticsLoaded = false;
    m_statisticsDirty = true;

    removeCoverageFromMap();
    for (int i = 0; i < AircraftReport::ProtocolCount; i++)
    {
        m_coverageRangeKm[i].fill(0.0f, m_coverageBins);
        m_coverageOrigin[i] = QGeoCoordinate();
        m_coverageDirty[i] = true;
    }
    m_coverageLoaded = false;
    m_coverageChanged = false;
}

void AircraftTracker::closeDatabase()
{
    if (m_db)
    {
        // Every statement is finalized where it is used, so this should not fail. If it
        // does, the handle is left for SQLite to close when the last statement goes
        // rather than leaked, and the reason is in the log.
        if (sqlite3_close(m_db) != SQLITE_OK)
        {
            qWarning() << "AircraftTracker::closeDatabase: close failed:" << sqlite3_errmsg(m_db);
            sqlite3_close_v2(m_db);
        }
        m_db = nullptr;
    }
}

// Write the whole session state in one transaction, replacing the previous save
// The small tables are still rewritten whole - all of them together came to about
// 21000 rows against 1.06 MILLION in track and profile, and rewriting those two every
// minute was blocking the tracker thread for the best part of a second. Measured on a
// real 72 MB session database: 870506 track and 194527 profile rows, 0.93 s to delete
// and re-insert, once a minute and growing for as long as the session ran. That stall
// was visible as a sawtooth on the message rate chart, because reports queued behind it
// and were then all counted into whichever sample followed.
//
// Track and profile only ever grow, so they are written incrementally: each flight
// remembers how many of its points are already stored and the save appends the rest.
// The three ways that can stop being true are all accounted for - a flight merged into
// another has its track or profile rebuilt (m_tracksToRewrite, m_profilesToRewrite), a
// flight discarded by the retention period has to take its rows with it
// (m_flightsToDelete), and clearing everything invalidates the lot (m_dbTrackWipe).
bool AircraftTracker::saveDatabase()
{
    // Nothing to do if nothing changed since the last save - which counts as saved.
    // No database at all does not: nothing has been written and nothing will be.
    if (!m_db) {
        return false;
    }
    if (!m_dbDirty) {
        return true;
    }

    int documentsHeld = 0;
    for (const TrackedAircraft *item : m_aircraft)
    {
        for (const TrackedFlight *flight : item->m_flights) {
            documentsHeld += flight->m_documents.size();
        }
    }

    // The small tables are replaced whole, so a DELETE that fails after a BEGIN that
    // succeeded would leave the old rows in place and the new ones appended to them
    if (!execSQL(m_db, "BEGIN"))
    {
        qWarning() << "AircraftTracker::saveDatabase: could not start a transaction";
        return false;       // m_dbDirty stays set, so the next tick tries again
    }
    if (!execSQL(m_db,
            "DELETE FROM aircraft; DELETE FROM sources; DELETE FROM flight_sources;"
            "DELETE FROM flights;"))
    {
        execSQL(m_db, "ROLLBACK");
        return false;
    }

    // Documents and the ATC log are written incrementally, like the track. They used to
    // be replaced whole, which was most of the time and most of the write ahead log of
    // every save once a session had a few days in it - and it meant any moment memory
    // was short of them lost the stored ones, with nothing said.
    if (m_dbMessageWipe || m_dbMessageRewrite)
    {
        if (!execSQL(m_db, "DELETE FROM documents; DELETE FROM atc;"))
        {
            execSQL(m_db, "ROLLBACK");
            return false;
        }
    }
    else if (!saveMessageChanges())
    {
        execSQL(m_db, "ROLLBACK");
        return false;
    }

    // What is stored for these flights is stale, so it goes before anything is written
    if (m_dbTrackWipe)
    {
        if (!execSQL(m_db, "DELETE FROM track; DELETE FROM profile;"))
        {
            execSQL(m_db, "ROLLBACK");
            return false;
        }
        // The flag is cleared after the COMMIT, not here. forgetSavedHistory() has already
        // set every flight's saved count to zero, so a save that rolls back after this
        // point leaves the old rows in the database and the wipe forgotten - and the next
        // save appends the whole of every track on top of them.
    }
    else if (!m_flightsToDelete.isEmpty() || !m_tracksToRewrite.isEmpty()
             || !m_profilesToRewrite.isEmpty())
    {
        // Checked, because the flights being rewritten have their saved counts at zero:
        // a delete that failed while the save went on appended a second copy of the whole
        // track on top of the first
        if (!deleteByFlight(m_db, "DELETE FROM track WHERE flight_id = ?", m_flightsToDelete + m_tracksToRewrite)
            || !deleteByFlight(m_db, "DELETE FROM profile WHERE flight_id = ?", m_flightsToDelete + m_profilesToRewrite))
        {
            execSQL(m_db, "ROLLBACK");
            return false;
        }
    }

    bool failed = false;

    // The ACARS archive only ever grows, so only what has arrived since the last save is
    // written - rewriting every block every save would make the save time grow with the
    // session, and this is the one table with no upper bound on its size. Nor is it held
    // in memory: m_acarsBlocks is just what is waiting to be written.
    if (m_dbMessageWipe) {
        if (!execSQL(m_db, "DELETE FROM acars_blocks"))
        {
            execSQL(m_db, "ROLLBACK");
            return false;
        }
    }
    if (!m_acarsBlocks.isEmpty())
    {
        sqlite3_stmt *blockStmt = nullptr;
        if (sqlite3_prepare_v2(m_db,
            "INSERT INTO acars_blocks (received, frequency, level, protocol, block)"
            " VALUES (?, ?, ?, ?, ?)", -1, &blockStmt, nullptr) == SQLITE_OK)
        {
            for (int i = 0; i < m_acarsBlocks.size(); i++)
            {
                const AcarsBlockRecord& record = m_acarsBlocks[i];
                sqlite3_reset(blockStmt);
                sqlite3_bind_int64(blockStmt, 1, dateTimeValue(record.m_dateTime));
                sqlite3_bind_int64(blockStmt, 2, record.m_frequency);
                sqlite3_bind_double(blockStmt, 3, record.m_level);
                sqlite3_bind_int(blockStmt, 4, record.m_protocol);
                sqlite3_bind_blob(blockStmt, 5, record.m_block.constData(),
                                  record.m_block.size(), SQLITE_TRANSIENT);
                stepInsert(m_db, blockStmt, "acars_blocks", failed);
            }
            sqlite3_finalize(blockStmt);
        }
        else
        {
            // Failed, not skipped: the commit clears the queue, and these blocks are held
            // nowhere else
            qWarning() << "AircraftTracker::saveDatabase: could not prepare the acars_blocks insert:"
                       << sqlite3_errmsg(m_db);
            failed = true;
        }
    }


    sqlite3_stmt *aircraftStmt = nullptr;
    sqlite3_stmt *sourceStmt = nullptr;
    sqlite3_stmt *trackStmt = nullptr;
    sqlite3_stmt *flightStmt = nullptr;
    sqlite3_stmt *flightSourceStmt = nullptr;
    sqlite3_stmt *documentStmt = nullptr;
    sqlite3_stmt *profileStmt = nullptr;
    sqlite3_stmt *atcStmt = nullptr;
    sqlite3_prepare_v2(m_db,
        "INSERT INTO aircraft VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)", -1, &aircraftStmt, nullptr);
    sqlite3_prepare_v2(m_db, "INSERT INTO sources VALUES (?,?,?,?)", -1, &sourceStmt, nullptr);
    // The reason, read straight after the call that failed. sqlite3_errmsg reports the
    // LAST call, so asking once at the end names whichever prepare happened to run after
    // the broken one - which is how a documents insert that no longer matched its table
    // reported "not an error" and took a while to find.
    auto prepared = [this](sqlite3_stmt *stmt, const char *what)
    {
        if (!stmt) {
            qWarning() << "AircraftTracker::saveDatabase: could not prepare the" << what
                       << "insert:" << sqlite3_errmsg(m_db);
        }
        return stmt != nullptr;
    };
    sqlite3_prepare_v2(m_db, "INSERT INTO flight_sources VALUES (?,?,?,?)", -1, &flightSourceStmt, nullptr);
    sqlite3_prepare_v2(m_db, "INSERT INTO track VALUES (?,?,?,?,?,?)", -1, &trackStmt, nullptr);
    sqlite3_prepare_v2(m_db,
        "INSERT INTO flights VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
        -1, &flightStmt, nullptr);
    sqlite3_prepare_v2(m_db, "INSERT INTO documents VALUES (?,?,?,?,?,?,?)", -1, &documentStmt, nullptr);
    sqlite3_prepare_v2(m_db, "INSERT INTO profile VALUES (?,?,?,?)", -1, &profileStmt, nullptr);
    sqlite3_prepare_v2(m_db, "INSERT INTO atc VALUES (?,?,?,?,?,?,?,?,?,?,?)", -1, &atcStmt, nullptr);
    // Every one of them reported, not just the first, so a schema change that has moved
    // more than one table is fixed in one go rather than one restart at a time
    bool allPrepared = true;
    allPrepared &= prepared(aircraftStmt, "aircraft");
    allPrepared &= prepared(sourceStmt, "sources");
    allPrepared &= prepared(flightSourceStmt, "flight_sources");
    allPrepared &= prepared(trackStmt, "track");
    allPrepared &= prepared(flightStmt, "flights");
    allPrepared &= prepared(documentStmt, "documents");
    allPrepared &= prepared(profileStmt, "profile");
    allPrepared &= prepared(atcStmt, "atc");

    if (!allPrepared)
    {
        qWarning() << "AircraftTracker::saveDatabase: nothing was saved";
        execSQL(m_db, "ROLLBACK");
        sqlite3_finalize(aircraftStmt);
        sqlite3_finalize(sourceStmt);
        sqlite3_finalize(trackStmt);
        sqlite3_finalize(flightStmt);
        sqlite3_finalize(documentStmt);
        sqlite3_finalize(profileStmt);
        sqlite3_finalize(atcStmt);
        sqlite3_finalize(flightSourceStmt);
        return false;
    }

    for (const TrackedAircraft *item : m_aircraft)
    {
        sqlite3_reset(aircraftStmt);
        sqlite3_bind_int64(aircraftStmt, 1, (qint64) item->m_id);
        sqlite3_bind_int64(aircraftStmt, 2, item->m_icao);
        bindText(aircraftStmt, 3, item->m_registration);
        bindText(aircraftStmt, 4, item->m_flight);
        sqlite3_bind_int(aircraftStmt, 5, item->m_positionValid ? 1 : 0);
        sqlite3_bind_double(aircraftStmt, 6, item->m_latitude);
        sqlite3_bind_double(aircraftStmt, 7, item->m_longitude);
        sqlite3_bind_int(aircraftStmt, 8, item->m_altitudeValid ? 1 : 0);
        sqlite3_bind_double(aircraftStmt, 9, item->m_altitudeFt);
        sqlite3_bind_int(aircraftStmt, 10, item->m_headingValid ? 1 : 0);
        sqlite3_bind_double(aircraftStmt, 11, item->m_heading);
        sqlite3_bind_int(aircraftStmt, 12, item->m_speedValid ? 1 : 0);
        sqlite3_bind_double(aircraftStmt, 13, item->m_speedKts);
        sqlite3_bind_int64(aircraftStmt, 14, dateTimeValue(item->m_positionDateTime));
        sqlite3_bind_int64(aircraftStmt, 15, dateTimeValue(item->m_lastSeen));
        bindText(aircraftStmt, 16, item->m_departure);
        bindText(aircraftStmt, 17, item->m_arrival);
        bindText(aircraftStmt, 18, item->m_route);
        bindText(aircraftStmt, 19, item->m_lastDocumentText);
        sqlite3_bind_int(aircraftStmt, 20, item->m_messages);
        bindText(aircraftStmt, 21, item->m_model3D);
        sqlite3_bind_double(aircraftStmt, 22, item->m_modelAltitudeOffset);
        sqlite3_bind_double(aircraftStmt, 23, item->m_labelAltitudeOffset);
        sqlite3_bind_int(aircraftStmt, 24, item->m_modelIsOurs ? 1 : 0);
        stepInsert(m_db, aircraftStmt, "aircraft", failed);

        QHashIterator<QPair<int, qint64>, int> sourceIt(item->m_sources);
        while (sourceIt.hasNext())
        {
            sourceIt.next();
            sqlite3_reset(sourceStmt);
            sqlite3_bind_int64(sourceStmt, 1, (qint64) item->m_id);
            sqlite3_bind_int(sourceStmt, 2, sourceIt.key().first);
            sqlite3_bind_int64(sourceStmt, 3, sourceIt.key().second);
            sqlite3_bind_int(sourceStmt, 4, sourceIt.value());
            stepInsert(m_db, sourceStmt, "sources", failed);
        }

        for (TrackedFlight *flight : item->m_flights)
        {
            // Only what is not stored yet. m_trackSaved is advanced after the commit,
            // so a rolled back transaction leaves it describing what is really there.
            for (int p = flight->m_trackSaved; p < flight->m_track.size(); p++)
            {
                sqlite3_reset(trackStmt);
                sqlite3_bind_int64(trackStmt, 1, (qint64) flight->m_id);
                sqlite3_bind_int(trackStmt, 2, p);
                sqlite3_bind_double(trackStmt, 3, flight->m_track[p].latitude());
                sqlite3_bind_double(trackStmt, 4, flight->m_track[p].longitude());
                sqlite3_bind_double(trackStmt, 5, flight->m_track[p].altitude());
                sqlite3_bind_int64(trackStmt, 6, dateTimeValue(flight->m_trackTimes[p]));
                stepInsert(m_db, trackStmt, "track", failed);
            }

            sqlite3_reset(flightStmt);
            sqlite3_bind_int64(flightStmt, 1, (qint64) flight->m_id);
            sqlite3_bind_int64(flightStmt, 2, (qint64) item->m_id);
            bindText(flightStmt, 3, flight->m_flight);
            sqlite3_bind_int64(flightStmt, 4, dateTimeValue(flight->m_firstSeen));
            sqlite3_bind_int64(flightStmt, 5, dateTimeValue(flight->m_lastSeen));
            bindText(flightStmt, 6, flight->m_departure);
            bindText(flightStmt, 7, flight->m_arrival);
            bindText(flightStmt, 8, flight->m_route);
            sqlite3_bind_int(flightStmt, 9, flight == item->m_currentFlight ? 1 : 0);
            bindText(flightStmt, 10, flight->m_aliases.join(","));
            sqlite3_bind_int(flightStmt, 11, flight->m_messages);
            sqlite3_bind_int64(flightStmt, 12, dateTimeValue(flight->m_out));
            sqlite3_bind_int64(flightStmt, 13, dateTimeValue(flight->m_off));
            sqlite3_bind_int64(flightStmt, 14, dateTimeValue(flight->m_on));
            sqlite3_bind_int64(flightStmt, 15, dateTimeValue(flight->m_in));
            sqlite3_bind_int64(flightStmt, 16, dateTimeValue(flight->m_routeFiled));
            // The two filed routes, in the column order added in openDatabase()
            int column = 17;
            for (const TrackedFlight::FiledRoute *route : { &flight->m_flightPlan, &flight->m_clearance })
            {
                bindText(flightStmt, column++, route->m_waypoints);
                bindText(flightStmt, column++, encodeRoutePoints(route->m_points));
                bindText(flightStmt, column++, route->m_text);
                sqlite3_bind_int64(flightStmt, column++, dateTimeValue(route->m_filed));
            }
            // After all eight of those rather than beside each route, because that is
            // where ALTER TABLE put them - see openDatabase()
            for (const TrackedFlight::FiledRoute *route : { &flight->m_flightPlan, &flight->m_clearance })
            {
                bindText(flightStmt, column++, encodeRouteParts(
                    { route->m_runway, route->m_sid, route->m_star, route->m_approach,
                      route->m_alternate, route->m_departure, route->m_arrival,
                      // The level and speed the route is flown at, stored with the rest
                      // of it - the message that gave them is not heard again, so without
                      // this a reloaded route goes back to lying on the ground
                      route->m_flightLevel > 0 ? QString::number(route->m_flightLevel) : QString(),
                      route->m_mach > 0.0f ? QString::number(route->m_mach, 'f', 2) : QString() }));
            }
            stepInsert(m_db, flightStmt, "flights", failed);

            QHashIterator<QPair<int, qint64>, int> flightSourceIt(flight->m_sources);
            while (flightSourceIt.hasNext())
            {
                flightSourceIt.next();
                sqlite3_reset(flightSourceStmt);
                sqlite3_bind_int64(flightSourceStmt, 1, (qint64) flight->m_id);
                sqlite3_bind_int(flightSourceStmt, 2, flightSourceIt.key().first);
                sqlite3_bind_int64(flightSourceStmt, 3, flightSourceIt.key().second);
                sqlite3_bind_int(flightSourceStmt, 4, flightSourceIt.value());
                stepInsert(m_db, flightSourceStmt, "flight_sources", failed);
            }

            for (int d = flight->m_documentsSaved; d < flight->m_documents.size(); d++)
            {
                const TrackedDocument *doc = flight->m_documents[d];
                sqlite3_reset(documentStmt);
                sqlite3_bind_int64(documentStmt, 1, (qint64) flight->m_id);
                sqlite3_bind_int(documentStmt, 2, doc->m_kind);
                bindText(documentStmt, 3, doc->m_title);
                bindText(documentStmt, 4, doc->m_text);
                sqlite3_bind_int64(documentStmt, 5, dateTimeValue(doc->m_received));
                if (doc->m_raw.isEmpty()) {
                    sqlite3_bind_null(documentStmt, 6);
                } else {
                    sqlite3_bind_blob(documentStmt, 6, doc->m_raw.constData(),
                                      doc->m_raw.size(), SQLITE_TRANSIENT);
                }
                bindText(documentStmt, 7, doc->m_source);
                stepInsert(m_db, documentStmt, "documents", failed);
            }

            for (int p = flight->m_profileSaved; p < flight->m_profileTimes.size(); p++)
            {
                sqlite3_reset(profileStmt);
                sqlite3_bind_int64(profileStmt, 1, (qint64) flight->m_id);
                sqlite3_bind_int64(profileStmt, 2, flight->m_profileTimes[p]);
                if (std::isnan(flight->m_profileAltFt[p])) {
                    sqlite3_bind_null(profileStmt, 3);
                } else {
                    sqlite3_bind_double(profileStmt, 3, flight->m_profileAltFt[p]);
                }
                if (std::isnan(flight->m_profileSpeedKts[p])) {
                    sqlite3_bind_null(profileStmt, 4);
                } else {
                    sqlite3_bind_double(profileStmt, 4, flight->m_profileSpeedKts[p]);
                }
                stepInsert(m_db, profileStmt, "profile", failed);
            }
        }
    }

    for (const AtcEvent& e : m_atcLog)
    {
        if (e.m_stored) {
            continue;
        }
        sqlite3_reset(atcStmt);
        sqlite3_bind_int64(atcStmt, 1, dateTimeValue(e.m_received));
        bindText(atcStmt, 2, e.m_protocol);
        sqlite3_bind_int(atcStmt, 3, e.m_uplink ? 1 : 0);
        bindText(atcStmt, 4, e.m_from);
        bindText(atcStmt, 5, e.m_to);
        bindText(atcStmt, 6, e.m_message);
        bindText(atcStmt, 7, e.m_tooltip);
        sqlite3_bind_int64(atcStmt, 8, (qint64) e.m_aircraftId);
        sqlite3_bind_int64(atcStmt, 9, (qint64) e.m_flightId);
        bindText(atcStmt, 10, e.m_mapName);
        bindText(atcStmt, 11, e.m_link);
        stepInsert(m_db, atcStmt, "atc", failed);
    }

    sqlite3_finalize(aircraftStmt);
    sqlite3_finalize(sourceStmt);
    sqlite3_finalize(trackStmt);
    sqlite3_finalize(flightStmt);
    sqlite3_finalize(documentStmt);
    sqlite3_finalize(profileStmt);
    sqlite3_finalize(atcStmt);
    sqlite3_finalize(flightSourceStmt);
    // Inside the transaction, so a rollback takes the statistics with everything else
    // The flag is cleared after the commit, like everything else: cleared here, a save
    // that then rolled back left the coverage unsaved until the next bin grew
    bool coverageWritten = false;
    if (!failed && m_coverageChanged)
    {
        if (saveCoverage()) {
            coverageWritten = true;
        } else {
            failed = true;
        }
    }
    if (!failed && !saveStatistics()) {
        failed = true;
    }
    if (failed)
    {
        // Leave the previous contents alone and keep the dirty flag, so the next save
        // tries the whole thing again rather than leaving a half written database behind
        qWarning() << "AircraftTracker::saveDatabase: rolling back, session not saved";
        execSQL(m_db, "ROLLBACK");
        return false;
    }
    if (!execSQL(m_db, "COMMIT"))
    {
        qWarning() << "AircraftTracker::saveDatabase: commit failed, session not saved";
        execSQL(m_db, "ROLLBACK");
        return false;
    }

    // Only now that the rows are really there. Doing this as they were written would
    // have a rolled back save leave every flight claiming points the database does not
    // hold, and the next save would append after them - a permanent hole in the track.
    //
    // And ONLY for flights whose history is actually in memory: a restored flight's
    // track is loaded lazily, so until ensureTrackLoaded() runs, m_trackSaved counts
    // rows the empty list does not hold. Overwriting it with the list length here
    // claimed the stored rows did not exist - the flight's next append then restarted
    // seq at 0 and the following save wrote a SECOND copy of the track on top of the
    // stored one. Loaded back, the two generations interleave point by point (ORDER BY
    // seq with duplicate seqs), which drew as tracks leaping back and forth between
    // two paths on both maps.
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            if (flight->m_trackLoaded) {
                flight->m_trackSaved = flight->m_track.size();
            }
            if (flight->m_profileLoaded) {
                flight->m_profileSaved = flight->m_profileTimes.size();
            }
            flight->m_documentsSaved = flight->m_documents.size();
        }
    }
    for (AtcEvent& e : m_atcLog) {
        e.m_stored = true;
    }
    forgetPendingChanges();
    m_dbTrackWipe = false;
    m_dbMessageRewrite = false;
    if (coverageWritten) {
        m_coverageChanged = false;
    }
    // Only now that the rows are committed, or a rolled back save would drop blocks the
    // database does not hold - they stay queued for the next save instead
    m_acarsBlocks.clear();
    const bool everythingDeleted = m_dbMessageWipe;
    m_dbMessageWipe = false;

    m_dbDirty = false;

    // Give the space back to the file system, which SQLite does not do on its own.
    //
    // Deleting every row leaves the pages on the database's free list, not with the
    // operating system, so the file stays exactly the size it was - which is why
    // clearing everything left the reported database size unchanged. VACUUM rebuilds the
    // file without the free pages, and the checkpoint truncates the write ahead log,
    // which is counted in that figure too and would otherwise still hold the deletes.
    //
    // Only after everything was deleted, never on an ordinary save: VACUUM rewrites the
    // whole database, which on a session of any size is slow and needs as much free disk
    // again. After a delete there is next to nothing left to rewrite, so it is quick.
    if (everythingDeleted)
    {
        execSQL(m_db, "VACUUM");
        execSQL(m_db, "PRAGMA wal_checkpoint(TRUNCATE)");
    }

    // The figure is only refreshed once a minute otherwise, so without this the size sits
    // at its old value for up to a minute after a save that changed it - and after a
    // delete, which is when it is looked at, that reads as nothing having happened
    updateDatabaseSize();
    qDebug() << "AircraftTracker::saveDatabase: wrote" << m_aircraft.size() << "aircraft,"
             << tableCount("flights") << "flights," << documentsHeld << "documents,"
             << m_atcLog.size() << "ATC messages";
    return true;
}

// The changes a save has to make beyond appending - deletes, rewrites and moves - once
// they no longer apply: they have been committed, the history is to be written out
// whole, or memory has just been read from the database and matches it
void AircraftTracker::forgetPendingChanges()
{
    m_flightsToDelete.clear();
    m_tracksToRewrite.clear();
    m_profilesToRewrite.clear();
    m_documentsToRewrite.clear();
    m_atcAircraftMoves.clear();
    m_atcFlightMoves.clear();
    m_atcAircraftToDelete.clear();
    m_atcPrunedIds.clear();
    m_atcPruneCutoff = QDateTime();
}

// What has changed in the documents and ATC tables since the last save, other than the
// rows to append: messages that moved with a merge, and those that went with discarded
// aircraft and flights or were cut down for a rewrite. Inside the caller's transaction.
bool AircraftTracker::saveMessageChanges()
{
    // Run one statement for each pair of ids, in order: a later move can start from where
    // an earlier one ended
    auto forEach = [this](const char *sql, const QList<QPair<quint64, quint64>>& pairs, bool twoIds)
    {
        if (pairs.isEmpty()) {
            return true;
        }
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        {
            qWarning() << "AircraftTracker::saveMessageChanges: could not prepare" << sql
                       << ":" << sqlite3_errmsg(m_db);
            return false;
        }
        bool ok = true;
        for (const auto& pair : pairs)
        {
            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, (qint64) pair.first);
            if (twoIds) {
                sqlite3_bind_int64(stmt, 2, (qint64) pair.second);
            }
            if (sqlite3_step(stmt) != SQLITE_DONE)
            {
                qWarning() << "AircraftTracker::saveMessageChanges:" << sql << "failed:"
                           << sqlite3_errmsg(m_db);
                ok = false;
                break;
            }
        }
        sqlite3_finalize(stmt);
        return ok;
    };
    auto ids = [](const QSet<quint64>& set)
    {
        QList<QPair<quint64, quint64>> pairs;
        for (quint64 id : set) {
            pairs.append({id, 0});
        }
        return pairs;
    };
    // (to, from) for the UPDATEs, whose first parameter is the new id
    auto swapped = [](const QList<QPair<quint64, quint64>>& moves)
    {
        QList<QPair<quint64, quint64>> pairs;
        for (const auto& move : moves) {
            pairs.append({move.second, move.first});
        }
        return pairs;
    };

    if (!forEach("UPDATE atc SET aircraft_id = ? WHERE aircraft_id = ?", swapped(m_atcAircraftMoves), true)
        || !forEach("UPDATE atc SET flight_id = ? WHERE flight_id = ?", swapped(m_atcFlightMoves), true)
        || !forEach("DELETE FROM documents WHERE flight_id = ?", ids(m_flightsToDelete + m_documentsToRewrite), false)
        || !forEach("DELETE FROM atc WHERE aircraft_id = ?", ids(m_atcAircraftToDelete), false)) {
        return false;
    }
    if (m_atcPruneCutoff.isValid())
    {
        QList<QPair<quint64, quint64>> pruned;
        for (quint64 id : m_atcPrunedIds) {
            pruned.append({id, (quint64) dateTimeValue(m_atcPruneCutoff)});
        }
        if (!forEach("DELETE FROM atc WHERE aircraft_id = ? AND time < ?", pruned, true)) {
            return false;
        }
    }
    return true;
}

// The all time records and counters, replaced wholesale - there are a few dozen rows at
// most, so there is nothing to gain from working out which changed. Inside the caller's
// transaction, so a rollback takes these with it.
// The coverage bins, one row per protocol and bearing, with the receiver position they
// were measured from.
//
// A table of its own, created with IF NOT EXISTS and WITHOUT touching the schema
// version: an existing database gains it on the next run and keeps everything it already
// holds. Bumping the version would have dropped every aircraft, flight and message
// anyone had collected, to add a feature that starts empty anyway.
bool AircraftTracker::saveCoverage()
{
    if (!execSQL(m_db, "DELETE FROM coverage")) {
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "INSERT INTO coverage VALUES (?,?,?,?,?)",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::saveCoverage:" << sqlite3_errmsg(m_db);
        return false;
    }

    bool failed = false;
    for (int protocol = 0; protocol < AircraftReport::ProtocolCount; protocol++)
    {
        if (!m_coverageOrigin[protocol].isValid()) {
            continue;
        }
        for (int bin = 0; bin < m_coverageRangeKm[protocol].size(); bin++)
        {
            // Only the bins that hold something, so a fresh protocol costs no rows
            if (m_coverageRangeKm[protocol][bin] <= 0.0f) {
                continue;
            }
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, protocol);
            sqlite3_bind_int(stmt, 2, bin);
            sqlite3_bind_double(stmt, 3, m_coverageRangeKm[protocol][bin]);
            sqlite3_bind_double(stmt, 4, m_coverageOrigin[protocol].latitude());
            sqlite3_bind_double(stmt, 5, m_coverageOrigin[protocol].longitude());
            stepInsert(m_db, stmt, "coverage", failed);
        }
    }
    sqlite3_finalize(stmt);

    return !failed;
}

bool AircraftTracker::loadCoverage()
{
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT protocol, bin, range, latitude, longitude FROM coverage",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        // An older database that has not been opened by this build yet has no such
        // table. Not an error - there is simply no coverage to restore.
        return false;
    }

    bool any = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const int protocol = sqlite3_column_int(stmt, 0);
        const int bin = sqlite3_column_int(stmt, 1);
        if ((protocol < 0) || (protocol >= AircraftReport::ProtocolCount)
            || (bin < 0) || (bin >= m_coverageBins)) {
            continue;
        }
        if (m_coverageRangeKm[protocol].isEmpty()) {
            m_coverageRangeKm[protocol].fill(0.0f, m_coverageBins);
        }
        m_coverageRangeKm[protocol][bin] = (float) sqlite3_column_double(stmt, 2);
        m_coverageOrigin[protocol] = QGeoCoordinate(sqlite3_column_double(stmt, 3),
                                                    sqlite3_column_double(stmt, 4));
        m_coverageDirty[protocol] = true;
        any = true;
    }
    sqlite3_finalize(stmt);

    m_coverageLoaded = any;
    return any;
}

bool AircraftTracker::saveStatistics()
{
    if (!execSQL(m_db, "DELETE FROM stat_records; DELETE FROM stat_counters;")) {
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "INSERT INTO stat_records VALUES (?,?,?,?,?,?,?,?,?,?,?)",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::saveStatistics:" << sqlite3_errmsg(m_db);
        return false;
    }

    bool failed = false;
    auto put = [&](int kind, int protocol, const AircraftStatistics::Record& r)
    {
        if (!r.m_valid) {
            return;         // An unset record is an absent row, so a reset writes nothing
        }
        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, kind);
        sqlite3_bind_int(stmt, 2, protocol);
        sqlite3_bind_double(stmt, 3, r.m_value);
        bindText(stmt, 4, r.m_aircraft);
        sqlite3_bind_int64(stmt, 5, dateTimeValue(r.m_when));
        sqlite3_bind_int(stmt, 6, r.m_positionValid ? 1 : 0);
        sqlite3_bind_double(stmt, 7, r.m_latitude);
        sqlite3_bind_double(stmt, 8, r.m_longitude);
        sqlite3_bind_int(stmt, 9, r.m_altitudeValid ? 1 : 0);
        sqlite3_bind_double(stmt, 10, r.m_altitudeFt);
        sqlite3_bind_int64(stmt, 11, r.m_frequency);
        stepInsert(m_db, stmt, "stat_records", failed);
    };

    const AircraftStatistics::Scope& all = m_statistics.m_allTime;
    for (int i = 0; i < AircraftReport::ProtocolCount; i++) {
        put(0, i, all.m_maxRange[i]);
    }
    put(1, -1, all.m_fastest);
    put(2, -1, all.m_highest);
    sqlite3_finalize(stmt);

    stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "INSERT INTO stat_counters VALUES (?,?)",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::saveStatistics:" << sqlite3_errmsg(m_db);
        return false;
    }
    auto count = [&](const char *name, qint64 value)
    {
        sqlite3_reset(stmt);
        bindText(stmt, 1, QString(name));
        sqlite3_bind_int64(stmt, 2, value);
        stepInsert(m_db, stmt, "stat_counters", failed);
    };
    count("maxConcurrent", all.m_maxConcurrent);
    count("maxConcurrentWhen", dateTimeValue(all.m_maxConcurrentWhen));
    count("totalMessages", (qint64) all.m_totalMessages);
    for (int i = 0; i < AircraftReport::ProtocolCount; i++) {
        count(qPrintable(QString("messages%1").arg(i)), (qint64) all.m_messagesByProtocol[i]);
    }
    count("seconds", all.m_seconds);
    count("firstStart", dateTimeValue(m_statistics.m_firstStart));
    sqlite3_finalize(stmt);

    return !failed;
}

// Returns false when the database holds no statistics at all, which is what tells the
// caller to look for a set in the preset to import - see applySettings()
bool AircraftTracker::loadStatistics()
{
    AircraftStatistics::Scope& all = m_statistics.m_allTime;
    bool any = false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM stat_records", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const int kind = sqlite3_column_int(stmt, 0);
            const int protocol = sqlite3_column_int(stmt, 1);
            AircraftStatistics::Record r;
            r.m_valid = true;
            r.m_value = (float) sqlite3_column_double(stmt, 2);
            r.m_aircraft = columnText(stmt, 3);
            r.m_when = columnDateTime(stmt, 4);
            r.m_positionValid = sqlite3_column_int(stmt, 5) != 0;
            r.m_latitude = (float) sqlite3_column_double(stmt, 6);
            r.m_longitude = (float) sqlite3_column_double(stmt, 7);
            r.m_altitudeValid = sqlite3_column_int(stmt, 8) != 0;
            r.m_altitudeFt = (float) sqlite3_column_double(stmt, 9);
            r.m_frequency = sqlite3_column_int64(stmt, 10);

            if ((kind == 0) && (protocol >= 0) && (protocol < AircraftReport::ProtocolCount)) {
                all.m_maxRange[protocol] = r;
            } else if (kind == 1) {
                all.m_fastest = r;
            } else if (kind == 2) {
                all.m_highest = r;
            } else {
                continue;   // A kind this build does not know: leave it in the table
            }
            any = true;
        }
    }
    sqlite3_finalize(stmt);

    stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT name, value FROM stat_counters", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const QString name = columnText(stmt, 0);
            const qint64 value = sqlite3_column_int64(stmt, 1);
            any = true;
            if (name == "maxConcurrent") {
                all.m_maxConcurrent = (int) value;
            } else if (name == "maxConcurrentWhen") {
                all.m_maxConcurrentWhen = value ? QDateTime::fromMSecsSinceEpoch(value) : QDateTime();
            } else if (name == "totalMessages") {
                all.m_totalMessages = (quint64) value;
            } else if (name == "seconds") {
                all.m_seconds = value;
            } else if (name == "firstStart") {
                m_statistics.m_firstStart = value ? QDateTime::fromMSecsSinceEpoch(value) : QDateTime();
            } else if (name.startsWith("messages"))
            {
                const int protocol = name.mid(8).toInt();
                if ((protocol >= 0) && (protocol < AircraftReport::ProtocolCount)) {
                    all.m_messagesByProtocol[protocol] = (quint64) value;
                }
            }
        }
    }
    sqlite3_finalize(stmt);

    return any;
}


// A restored flight's track, read the first time anything wants it. The index added in
// openDatabase() is what makes this a lookup rather than a scan of the whole table.
void AircraftTracker::ensureTrackLoaded(TrackedFlight *flight)
{
    if (!flight || flight->m_trackLoaded) {
        return;
    }
    flight->m_trackLoaded = true;       // Set first, so a failure does not retry for ever

    // What the table holds for this flight, counted when the database was opened
    const int stored = flight->m_trackSaved;
    if (!m_db || (stored == 0))
    {
        flight->m_trackSaved = flight->m_track.size();
        return;
    }

    // ORDER BY time, not seq: a database written by a build where a save could reset a
    // restored flight's saved count holds TWO generations of rows for one flight, both
    // starting at seq 0 - ordered by seq they interleave point by point, and the track
    // leaps back and forth between the two paths. Ordered by time (seq only as the
    // tiebreak within one instant) the generations come back out end to end, which is
    // the actual flight. For a healthy flight the two orders are identical.
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
            "SELECT latitude, longitude, altitude, time, seq FROM track WHERE flight_id = ? ORDER BY time, seq",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::ensureTrackLoaded:" << sqlite3_errmsg(m_db);
        return;
    }
    sqlite3_bind_int64(stmt, 1, (qint64) flight->m_id);

    QList<QGeoCoordinate> track;
    QList<QDateTime> times;
    int rowCount = 0;
    int maxSeq = -1;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        rowCount++;
        maxSeq = std::max(maxSeq, sqlite3_column_int(stmt, 4));
        // Exactly 0N 0E is a zeroed-out field saved before the filter for them existed,
        // not a position
        const double latitude = sqlite3_column_double(stmt, 0);
        const double longitude = sqlite3_column_double(stmt, 1);
        if ((latitude == 0.0) && (longitude == 0.0)) {
            continue;
        }
        track.append(QGeoCoordinate(latitude, longitude, sqlite3_column_double(stmt, 2)));
        times.append(columnDateTime(stmt, 3));
    }
    sqlite3_finalize(stmt);

    // Anything received since the flight was restored is newer than everything stored, so
    // the stored points go in front of it rather than replacing it
    flight->m_track = track + flight->m_track;
    flight->m_trackTimes = times + flight->m_trackTimes;
    flight->m_trackSaved = track.size();

    if ((track.size() != stored) || (rowCount != maxSeq + 1))
    {
        // Rows were dropped as zeroed positions - or the seq numbers are duplicated
        // (two generations, see above) or have holes - so the table no longer matches
        // what is in memory and the save cannot simply append to it. Rewriting the
        // flight is the only thing that puts the two back in step - and it quietly
        // cleans out the bad rows, which nothing else would.
        m_tracksToRewrite.insert(flight->m_id);
        flight->m_trackSaved = 0;
        m_dbDirty = true;
    }
}

void AircraftTracker::ensureProfileLoaded(TrackedFlight *flight)
{
    if (!flight || flight->m_profileLoaded) {
        return;
    }
    flight->m_profileLoaded = true;

    const int stored = flight->m_profileSaved;
    if (!m_db || (stored == 0))
    {
        flight->m_profileSaved = flight->m_profileTimes.size();
        return;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
            "SELECT time, altitude, speed FROM profile WHERE flight_id = ? ORDER BY time",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::ensureProfileLoaded:" << sqlite3_errmsg(m_db);
        return;
    }
    sqlite3_bind_int64(stmt, 1, (qint64) flight->m_id);

    QList<qint64> times;
    QList<float> altitude;
    QList<float> speed;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        times.append(sqlite3_column_int64(stmt, 0));
        altitude.append(sqlite3_column_type(stmt, 1) == SQLITE_NULL
            ? std::numeric_limits<float>::quiet_NaN() : (float) sqlite3_column_double(stmt, 1));
        speed.append(sqlite3_column_type(stmt, 2) == SQLITE_NULL
            ? std::numeric_limits<float>::quiet_NaN() : (float) sqlite3_column_double(stmt, 2));
    }
    sqlite3_finalize(stmt);

    flight->m_profileTimes = times + flight->m_profileTimes;
    flight->m_profileAltFt = altitude + flight->m_profileAltFt;
    flight->m_profileSpeedKts = speed + flight->m_profileSpeedKts;
    flight->m_profileSaved = times.size();
}

// Everything at once, for the two places that genuinely need the whole history in memory:
// writing the session into a different database, and wiping the stored points to write
// them all again. Both are rare and deliberate; everything else takes one flight.
void AircraftTracker::ensureAllHistoryLoaded()
{
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            ensureTrackLoaded(flight);
            ensureProfileLoaded(flight);
        }
    }
}

// Nothing stored relates to what is in memory any more, so the next save empties the
// track, profile, documents and ATC tables and writes everything again. Everything has
// to be read in first, or the points that were never read would be wiped without ever
// having been written back.
void AircraftTracker::forgetSavedHistory()
{
    ensureAllHistoryLoaded();
    m_dbTrackWipe = true;
    m_dbMessageRewrite = true;
    forgetPendingChanges();
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            flight->m_trackSaved = 0;
            flight->m_profileSaved = 0;
            flight->m_documentsSaved = 0;
        }
    }
    for (AtcEvent& e : m_atcLog) {
        e.m_stored = false;
    }
}

// Restore the previous session. Aircraft older than the removal timeout are
// pruned afterwards; the ATC log is restored in full. The GUI, if any, gets the
// restored state when it connects and requests a resync.
void AircraftTracker::loadDatabase()
{
    if (!m_db) {
        return;
    }

    m_loadingDatabase = true;
    QHash<qint64, TrackedAircraft *> aircraftById;
    QHash<qint64, TrackedFlight *> flightsById;
    sqlite3_stmt *stmt = nullptr;

    // Ids are never reused, so the next one has to clear every id still referenced -
    // not only those of the aircraft and flights loaded below. ATC and document rows
    // outlive the aircraft they were heard from, and taking the next id from the two
    // tables alone handed their ids to new flights, which then claimed old messages.
    if (sqlite3_prepare_v2(m_db,
            "SELECT MAX(id) FROM ("
                "SELECT MAX(id) AS id FROM aircraft UNION ALL SELECT MAX(id) FROM flights "
                "UNION ALL SELECT MAX(aircraft_id) FROM atc UNION ALL SELECT MAX(flight_id) FROM atc "
                "UNION ALL SELECT MAX(flight_id) FROM documents UNION ALL SELECT MAX(flight_id) FROM track "
                "UNION ALL SELECT MAX(flight_id) FROM profile UNION ALL SELECT MAX(flight_id) FROM flight_sources "
                "UNION ALL SELECT MAX(aircraft_id) FROM sources)",
            -1, &stmt, nullptr) == SQLITE_OK)
    {
        if ((sqlite3_step(stmt) == SQLITE_ROW) && (sqlite3_column_type(stmt, 0) != SQLITE_NULL)) {
            m_nextId = std::max(m_nextId, (quint64) sqlite3_column_int64(stmt, 0) + 1);
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // Aircraft
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM aircraft", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TrackedAircraft *item = new TrackedAircraft();
            qint64 id = sqlite3_column_int64(stmt, 0);
            item->m_id = (quint64) id;
            m_nextId = std::max(m_nextId, item->m_id + 1);
            item->m_icao = (quint32) sqlite3_column_int64(stmt, 1);
            item->m_registration = columnText(stmt, 2);
            item->m_flight = columnText(stmt, 3);
            item->m_positionValid = sqlite3_column_int(stmt, 4) != 0;
            item->m_latitude = (float) sqlite3_column_double(stmt, 5);
            item->m_longitude = (float) sqlite3_column_double(stmt, 6);
            item->m_altitudeValid = sqlite3_column_int(stmt, 7) != 0;
            item->m_altitudeFt = (float) sqlite3_column_double(stmt, 8);
            item->m_headingValid = sqlite3_column_int(stmt, 9) != 0;
            item->m_heading = (float) sqlite3_column_double(stmt, 10);
            item->m_speedValid = sqlite3_column_int(stmt, 11) != 0;
            item->m_speedKts = (float) sqlite3_column_double(stmt, 12);
            item->m_positionDateTime = columnDateTime(stmt, 13);
            item->m_lastSeen = columnDateTime(stmt, 14);
            item->m_departure = columnText(stmt, 15);
            item->m_arrival = columnText(stmt, 16);
            item->m_route = columnText(stmt, 17);
            item->m_lastDocumentText = columnText(stmt, 18);
            item->m_messages = sqlite3_column_int(stmt, 19);
            item->m_model3D = columnText(stmt, 20);
            // Added by ensureColumn(), so a database written before they existed reads
            // them as the defaults - which is the same as never having had a model
            item->m_modelAltitudeOffset = (float) sqlite3_column_double(stmt, 21);
            item->m_labelAltitudeOffset = (float) sqlite3_column_double(stmt, 22);
            const int modelIsOurs = sqlite3_column_int(stmt, 23);
            item->m_modelIsOurs = modelIsOurs > 0;
            // A row written before these columns existed has the model but nothing that
            // goes with it, so the model is dropped and chosen again on the next report
            // rather than drawn at the wrong height and never replaced
            if (modelIsOurs < 0) {
                item->m_model3D.clear();
            }
            // Nor is one that is no longer installed: the map would ask for a file that
            // isn't there, and draw nothing at all for the aircraft
            if (!item->m_model3D.isEmpty() && !Aircraft3DModels::instance().isInstalled(item->m_model3D)) {
                item->m_model3D.clear();
            }

            m_aircraft.append(item);
            aircraftById.insert(id, item);
            if (item->m_icao) {
                m_byKey.insert(QString("I%1").arg(item->m_icao, 6, 16, QChar('0')), item);
            }
            if (!item->m_registration.isEmpty()) {
                m_byKey.insert("R" + item->m_registration, item);
            }
            if (!item->m_flight.isEmpty()) {
                m_byKey.insert("F" + item->m_flight, item);
            }
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // Protocols/frequencies heard on
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM sources", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TrackedAircraft *item = aircraftById.value(sqlite3_column_int64(stmt, 0));
            if (item) {
                item->m_sources.insert(
                    qMakePair(sqlite3_column_int(stmt, 1), (qint64) sqlite3_column_int64(stmt, 2)),
                    sqlite3_column_int(stmt, 3));
            }
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // Flights
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM flights ORDER BY id", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TrackedAircraft *item = aircraftById.value(sqlite3_column_int64(stmt, 1));
            if (!item) {
                continue;
            }
            TrackedFlight *flight = new TrackedFlight();
            flight->m_id = (quint64) sqlite3_column_int64(stmt, 0);
            m_nextId = std::max(m_nextId, flight->m_id + 1);
            flight->m_aircraft = item;
            flight->m_flight = columnText(stmt, 2);
            flight->m_firstSeen = columnDateTime(stmt, 3);
            flight->m_lastSeen = columnDateTime(stmt, 4);
            flight->m_departure = columnText(stmt, 5);
            flight->m_arrival = columnText(stmt, 6);
            flight->m_route = columnText(stmt, 7);
            flight->m_messages = sqlite3_column_int(stmt, 10);
            flight->m_out = columnDateTime(stmt, 11);
            flight->m_off = columnDateTime(stmt, 12);
            flight->m_on = columnDateTime(stmt, 13);
            flight->m_in = columnDateTime(stmt, 14);
            flight->m_routeFiled = columnDateTime(stmt, 15);
            int routeColumn = 16;
            for (TrackedFlight::FiledRoute *route : { &flight->m_flightPlan, &flight->m_clearance })
            {
                route->m_waypoints = columnText(stmt, routeColumn++);
                route->m_points = decodeRoutePoints(columnText(stmt, routeColumn++));
                route->m_text = columnText(stmt, routeColumn++);
                route->m_filed = columnDateTime(stmt, routeColumn++);
            }
            // decodeRouteParts writes into QStrings, so the two numeric parts land here
            // first and are converted after
            QString flightLevelText;
            QString machText;
            for (TrackedFlight::FiledRoute *route : { &flight->m_flightPlan, &flight->m_clearance })
            {
                const QList<QString *> parts {
                    &route->m_runway, &route->m_sid, &route->m_star, &route->m_approach,
                    &route->m_alternate, &route->m_departure, &route->m_arrival,
                    &flightLevelText, &machText };
                flightLevelText.clear();
                machText.clear();
                decodeRouteParts(columnText(stmt, routeColumn++), parts);
                route->m_flightLevel = flightLevelText.toInt();
                route->m_mach = machText.toFloat();
                // Whatever the row could not carry, taken back out of the message
                recoverRoutePartsFromText(route->m_text, parts);
            }
            QString aliases = columnText(stmt, 9);
            if (!aliases.isEmpty()) {
                flight->m_aliases = aliases.split(',');
            }
            item->m_flights.append(flight);
            if (sqlite3_column_int(stmt, 8)) {
                item->m_currentFlight = flight;
            }
            flightsById.insert(sqlite3_column_int64(stmt, 0), flight);
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // Protocols/frequencies each flight was heard on. After the flights, because it is
    // keyed on their ids.
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM flight_sources", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TrackedFlight *flight = flightsById.value(sqlite3_column_int64(stmt, 0));
            if (flight) {
                flight->m_sources.insert(
                    qMakePair(sqlite3_column_int(stmt, 1), (qint64) sqlite3_column_int64(stmt, 2)),
                    sqlite3_column_int(stmt, 3));
            }
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // Tracks and profiles are NOT read here, only how many rows each flight has - which
    // is exactly what the incremental save needs in order to append. The rows themselves
    // are fetched when something wants them, one flight at a time.
    for (auto it = flightsById.cbegin(); it != flightsById.cend(); ++it)
    {
        it.value()->m_trackLoaded = false;
        it.value()->m_profileLoaded = false;
    }
    for (int pass = 0; pass < 2; pass++)
    {
        const char *query = pass == 0
            ? "SELECT flight_id, COUNT(*) FROM track GROUP BY flight_id"
            : "SELECT flight_id, COUNT(*) FROM profile GROUP BY flight_id";
        if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) == SQLITE_OK)
        {
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                TrackedFlight *flight = flightsById.value(sqlite3_column_int64(stmt, 0));
                if (!flight) {
                    continue;
                }
                // Standing in for the points until they are read, so a flight nobody
                // looks at is never rewritten
                if (pass == 0) {
                    flight->m_trackSaved = sqlite3_column_int(stmt, 1);
                } else {
                    flight->m_profileSaved = sqlite3_column_int(stmt, 1);
                }
            }
        }
        sqlite3_finalize(stmt);
        stmt = nullptr;
    }

    // Documents
    int rerendered = 0;
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM documents ORDER BY received", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            TrackedFlight *flight = flightsById.value(sqlite3_column_int64(stmt, 0));
            if (!flight) {
                continue;
            }
            TrackedDocument *doc = new TrackedDocument();
            doc->m_kind = sqlite3_column_int(stmt, 1);
            doc->m_title = columnText(stmt, 2);
            doc->m_text = columnText(stmt, 3);
            doc->m_received = columnDateTime(stmt, 4);
            const void *blob = sqlite3_column_blob(stmt, 5);
            const int bytes = sqlite3_column_bytes(stmt, 5);
            if (blob && (bytes > 0)) {
                doc->m_raw = QByteArray((const char *) blob, bytes);
            }
            doc->m_source = columnText(stmt, 6);
            // Rendered again by whatever the parser does now, rather than shown as it was
            // written. The stored text is the fallback: a document with no raw came from a
            // decoder this feature cannot run, and a raw that no longer decodes should not
            // lose the text it once had.
            if (!doc->m_raw.isEmpty())
            {
                const QString rendered = renderDocument(doc->m_raw, doc->m_source);
                if (!rendered.isEmpty())
                {
                    if (rendered != doc->m_text) {
                        rerendered++;
                    }
                    doc->m_text = rendered;
                }
            }
            flight->m_documents.append(doc);
        }
    }
    sqlite3_finalize(stmt);
    stmt = nullptr;
    if (rerendered > 0) {
        qDebug() << "AircraftTracker::loadDatabase:" << rerendered
                 << "documents came out differently under the current decoder";
    }

    // Flight profiles are fetched on demand as well - see the row counts read above
    sqlite3_finalize(stmt);
    stmt = nullptr;

    // ATC log
    if (sqlite3_prepare_v2(m_db, "SELECT * FROM atc ORDER BY time", -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            AtcEvent e;
            e.m_received = columnDateTime(stmt, 0);
            e.m_protocol = columnText(stmt, 1);
            e.m_uplink = sqlite3_column_int(stmt, 2) != 0;
            e.m_from = columnText(stmt, 3);
            e.m_to = columnText(stmt, 4);
            e.m_message = columnText(stmt, 5);
            e.m_tooltip = columnText(stmt, 6);
            e.m_aircraftId = (quint64) sqlite3_column_int64(stmt, 7);
            e.m_flightId = (quint64) sqlite3_column_int64(stmt, 8);
            e.m_mapName = columnText(stmt, 9);
            e.m_link = columnText(stmt, 10);
            e.m_stored = true;
            m_atcLog.append(e);
        }
    }
    sqlite3_finalize(stmt);

    // Everything just read is, by definition, already in the database - without this the
    // first save would write the whole restored history out again.
    //
    // Before EVERY merge below, not after: absorbAircraft() runs one of its own through
    // mergeLoadedFlights(), so putting this between the two loops still erased what the
    // first had recorded - merging two restored flights deletes one and rewrites the
    // other, and says so through m_flightsToDelete, m_tracksToRewrite and a zeroed
    // m_trackSaved.
    //
    // The track and profile counts are NOT set from the lists here: the points have not
    // been read, so the lists are empty while the table is full, and taking the sizes
    // would tell the next save that the whole stored history had never been written. They
    // come from the row counts read above instead.
    forgetPendingChanges();
    m_dbTrackWipe = false;
    // Unlike the track, the documents were read in full, so the lists are what is stored
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights) {
            flight->m_documentsSaved = flight->m_documents.size();
        }
    }
    // Memory now reflects this file, so any pending clear belonged to the last one. It
    // matters because changeDatabase() calls deleteAll() before opening the new database.
    m_dbMessageWipe = false;
    m_dbMessageRewrite = false;

    // Flights stored before the airport fields were trimmed hold whatever the airline
    // wrote after the code, because the field ran to the next : or / and a route holds
    // neither: EGPD(34O)..AVANT.UM184.HEMEL.. is an airport, a landing runway and a whole
    // route in what should be four characters. The demodulator splits them now, and this
    // splits what was stored before it did - so the route it swallowed comes back rather
    // than waiting for the aircraft to file again.
    int airportsTrimmed = 0;
    int routesRecovered = 0;
    {
        static const QRegularExpression airportRe(R"(^([A-Z0-9]{4})[^A-Z0-9])");
        static const QRegularExpression bracketRe(R"(^\([^)]*\))");
        static const QRegularExpression separatorRe("[ .]+");

        for (auto it = flightsById.cbegin(); it != flightsById.cend(); ++it)
        {
            TrackedFlight *flight = it.value();

            for (int end = 0; end < 2; end++)
            {
                QString& airport = (end == 0) ? flight->m_departure : flight->m_arrival;
                const QRegularExpressionMatch match = airportRe.match(airport);
                if (!match.hasMatch()) {
                    continue;
                }

                QString tail = airport.mid(match.captured(1).size());
                airport = match.captured(1);
                airportsTrimmed++;

                // The runway comes first where it is given at all, then the route, joined
                // to the airport by the dots that mean "direct to"
                const QRegularExpressionMatch bracket = bracketRe.match(tail);
                if (bracket.hasMatch()) {
                    tail = tail.mid(bracket.captured(0).size());
                }
                while (tail.startsWith('.')) {
                    tail.remove(0, 1);
                }
                if ((end == 0) || tail.isEmpty()) {
                    continue;       // Only the arrival carries the route, in this form
                }

                // Longer wins, counted in names rather than placed, so that this needs
                // none of the databases - which are not what was being read here. It is
                // what keeps a plan filed properly since from being replaced by the tail
                // of an older message, while still replacing the "ILS 34" that an
                // approach-only insert left behind.
                const int recovered = tail.split(separatorRe, Qt::SkipEmptyParts).size();
                if (recovered < 2) {
                    continue;
                }
                if (flight->m_route.split(separatorRe, Qt::SkipEmptyParts).size() < recovered)
                {
                    flight->m_route = tail;
                    routesRecovered++;
                }
                TrackedFlight::FiledRoute& plan = flight->m_flightPlan;
                if (plan.m_waypoints.split(separatorRe, Qt::SkipEmptyParts).size() < recovered)
                {
                    plan.m_waypoints = tail;
                    plan.m_points.clear();      // Names only; nothing placed them
                }
            }
        }
    }
    // Position reports stored as OOOI events, from before a label 13 to 18 message had
    // to carry an actual event to be filed as one. They are not documents under the
    // current rules, so they are dropped from the list rather than left claiming to be
    // something they are not. Nothing is lost: the messages themselves are in the ACARS
    // block archive, and their positions are already in the track.
    static const QRegularExpression poxShape("^POX,");
    static const QRegularExpression companyShape(R"RX(^\d{6},\s*\d+,\d{4},)RX");
    int positionsUnfiled = 0;
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            QList<TrackedDocument *> kept;
            const int stored = flight->m_documents.size();
            for (TrackedDocument *doc : flight->m_documents)
            {
                const bool misfiled = (doc->m_kind == AircraftReport::OooiEvent)
                    && (poxShape.match(doc->m_source).hasMatch()
                        || companyShape.match(doc->m_source).hasMatch());
                if (misfiled)
                {
                    positionsUnfiled++;
                    delete doc;
                }
                else
                {
                    kept.append(doc);
                }
            }
            flight->m_documents = kept;
            if (kept.size() != stored)
            {
                // Saved incrementally, so what was dropped has to be taken out of the table
                flight->m_documentsSaved = 0;
                m_documentsToRewrite.insert(flight->m_id);
            }
        }
    }
    if (positionsUnfiled > 0)
    {
        m_dbDirty = true;
        qDebug() << "AircraftTracker::loadDatabase: dropped" << positionsUnfiled
                 << "position reports that had been stored as OOOI events";
    }

    // Pre-departure clearances stored before they had a kind of their own were filed as
    // flight plans, which is what they are for drawing purposes but not for the Flights
    // table's clearance column. Reclassified by their title, which is the only thing
    // that distinguishes them, so the ones already held tick CL like new ones do.
    int clearancesReclassified = 0;
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            for (TrackedDocument *doc : flight->m_documents)
            {
                if ((doc->m_kind == AircraftReport::FlightPlan)
                    && (doc->m_title == "Pre-departure clearance"))
                {
                    doc->m_kind = AircraftReport::DepartureClearance;
                    clearancesReclassified++;
                }
            }
        }
    }
    if (clearancesReclassified > 0)
    {
        m_dbDirty = true;
        qDebug() << "AircraftTracker::loadDatabase: reclassified" << clearancesReclassified
                 << "stored pre-departure clearances";
    }

    // Destinations out of stored VDL-2 logons.
    //
    // The XID destination was not read until recently, so every flight heard before that
    // has the logon text stored and nothing taken from it. In a week of traffic that is
    // 662 flights whose arrival is empty while their own logons state it outright.
    //
    // It also replaces an arrival that matches the callsign to route database, because
    // that is a guess rather than something anyone said: of 606 flights whose stored
    // arrival disagreed with their own logons, 605 were holding exactly the database's
    // answer. An arrival that differs from the database was set by a message, and is
    // left alone - a logon does not outrank a filed plan.
    int destinationsRecovered = 0;
    for (TrackedAircraft *item : m_aircraft)
    {
        for (TrackedFlight *flight : item->m_flights)
        {
            // One destination across all of its logons, or none at all. An aircraft that
            // named two is not a case to choose between here.
            QString destination;
            bool ambiguous = false;
            for (const TrackedDocument *doc : flight->m_documents)
            {
                if (doc->m_kind != AircraftReport::Logon) {
                    continue;
                }
                const QString found = xidDestinationAirport(doc->m_text);
                if (found.isEmpty()) {
                    continue;
                }
                if (destination.isEmpty()) {
                    destination = found;
                } else if (destination != found) {
                    ambiguous = true;
                    break;
                }
            }
            if (ambiguous || destination.isEmpty() || (flight->m_arrival == destination)) {
                continue;
            }

            if (!flight->m_arrival.isEmpty())
            {
                // Held already, so only a database guess may be overruled. Provenance is
                // not stored, so it is recovered the only way it can be: a value equal to
                // what the database says for this callsign either came from there or is
                // indistinguishable from it, and either way the logon agrees or improves.
                const AircraftRouteInformation *entry = nullptr;
                QStringList names;
                names.append(flight->m_flight);
                names.append(flight->m_aliases);
                for (const QString& name : names)
                {
                    if (!name.isEmpty() && m_routeInfo && m_routeInfo->contains(name))
                    {
                        entry = m_routeInfo->value(name);
                        break;
                    }
                }
                if (!entry || (entry->m_arr != flight->m_arrival)) {
                    continue;       // A message set this; a logon does not outrank it
                }
            }

            flight->m_arrival = destination;
            flight->m_arrivalFromDatabase = false;
            destinationsRecovered++;
        }
    }
    if (destinationsRecovered > 0)
    {
        m_dbDirty = true;
        qDebug() << "AircraftTracker::loadDatabase: took" << destinationsRecovered
                 << "destinations from stored VDL-2 logons";
    }

    if (airportsTrimmed > 0)
    {
        m_dbDirty = true;
        qDebug() << "AircraftTracker::loadDatabase: trimmed" << airportsTrimmed
                 << "airport fields that had a runway or a route written after the code,"
                 << "recovering" << routesRecovered << "routes";
    }

    // Flights saved before the filed routes had columns of their own kept only the route
    // STRING and the document that filed it, so nothing could be drawn for them - the
    // clearance was in the database and on screen, and the line was not. Both halves are
    // stored, so the route is rebuilt from them: the waypoints from the string, the text
    // and which of the two routes it is from the document, and the time from route_filed.
    //
    // Only where the flight has a route document, because m_route is also filled in from
    // the callsign to route database, and what that holds is the airports rather than the
    // waypoints. The positions a message gave for its waypoints are NOT recoverable, only
    // their names - which for an oceanic route names them anyway.
    //
    // It runs once per flight in practice: the next save writes the columns, and a flight
    // that has them is skipped.
    int routesRebuilt = 0;
    for (auto it = flightsById.cbegin(); it != flightsById.cend(); ++it)
    {
        TrackedFlight *flight = it.value();

        if (flight->m_route.isEmpty()
            || !flight->m_flightPlan.m_waypoints.isEmpty()
            || !flight->m_clearance.m_waypoints.isEmpty()) {
            continue;
        }

        // The most recent of them, because that is the one m_route came from
        const TrackedDocument *filedBy = nullptr;
        for (const TrackedDocument *doc : flight->m_documents)
        {
            if ((doc->m_kind != AircraftReport::FlightPlan)
                && (doc->m_kind != AircraftReport::Clearance)) {
                continue;
            }
            if (!filedBy || (doc->m_received > filedBy->m_received)) {
                filedBy = doc;
            }
        }
        if (!filedBy) {
            continue;
        }

        TrackedFlight::FiledRoute& route = (filedBy->m_kind == AircraftReport::Clearance)
                                         ? flight->m_clearance : flight->m_flightPlan;
        route.m_waypoints = flight->m_route;
        route.m_text = filedBy->m_text;
        route.m_filed = flight->m_routeFiled.isValid() ? flight->m_routeFiled : filedBy->m_received;
        routesRebuilt++;
    }
    if (routesRebuilt > 0)
    {
        m_dbDirty = true;       // So they are written into the columns they now have
        qDebug() << "AircraftTracker::loadDatabase: rebuilt" << routesRebuilt
                 << "filed routes from the route string and the document that filed it";
    }

    // What came back, against what is in the file. The two disagreeing is the only
    // warning there will be that something is being dropped on the way in - and it is
    // read back rather than counted as it goes, so a row the load skipped still shows.
    {
        int documentsRead = 0;
        for (const TrackedAircraft *item : m_aircraft)
        {
            for (const TrackedFlight *flight : item->m_flights) {
                documentsRead += flight->m_documents.size();
            }
        }
        qDebug() << "AircraftTracker::loadDatabase: read" << m_aircraft.size() << "of"
                 << tableCount("aircraft") << "aircraft," << flightsById.size() << "of"
                 << tableCount("flights") << "flights," << documentsRead << "of"
                 << tableCount("documents") << "documents," << m_atcLog.size() << "of"
                 << tableCount("atc") << "ATC messages";
    }

    // The all time figures belong to this database, so they arrive with it. Where there
    // are none, an older preset's blob is imported once - see applySettings().
    loadCoverage();
    if (loadStatistics())
    {
        m_statisticsLoaded = true;
        // The stored seconds are what was listened to BEFORE now, and flush() reports the
        // all time clock as that base plus the time since. Without setting the base here
        // the loaded figure was overwritten on the very first flush by this session's own
        // elapsed time - so the all time listening time read as the session's, and the all
        // time messages per minute, a lifetime total of messages over a few seconds of
        // clock, came out in the millions.
        m_allTimeSecondsBase = m_statistics.m_allTime.m_seconds;
        // From now rather than from the start of the session, because this also runs when
        // the user switches to a different database part way through one, and that
        // database has not been listening for the time already spent on the last
        m_allTimeSince = QDateTime::currentDateTime();
    }

    // The ACARS archive stays on disk. It used to be read in whole here and then held for
    // the life of the session, though nothing reads it back - the saves only ever append
    // - so every launch paid for months of blocks in memory to no purpose. Not even
    // counted here: COUNT(*) on a table this size is a scan of the whole thing.

    updateDatabaseSize();
    // The airframe count cannot be a running total, so it is counted from the table -
    // and re-counted on every load, not only at startup, or switching databases would
    // leave the previous one's figure beside this one's records
    m_statistics.m_allTime.m_distinctAircraft = seenCount();
    m_statisticsDirty = true;

    // Fold weak flight-only aircraft into the identified airframe flying the
    // same flight, and merge flights that are the same flight under its ACARS
    // and ADS-B names (sessions saved before these existed live)
    QList<TrackedAircraft *> weakItems;
    for (TrackedAircraft *a : m_aircraft)
    {
        if ((a->m_icao == 0) && a->m_registration.isEmpty()) {
            weakItems.append(a);
        }
    }
    for (TrackedAircraft *weak : weakItems)
    {
        TrackedAircraft *strong = nullptr;
        for (TrackedAircraft *cand : m_aircraft)
        {
            if ((cand == weak) || ((cand->m_icao == 0) && cand->m_registration.isEmpty())) {
                continue;
            }
            for (TrackedFlight *weakFlight : weak->m_flights)
            {
                if (weakFlight->m_flight.isEmpty()) {
                    continue;
                }
                for (TrackedFlight *candFlight : cand->m_flights)
                {
                    // Exact name only: number-based matching is only safe
                    // within a single airframe
                    if ((candFlight->m_flight != weakFlight->m_flight)
                        && !candFlight->m_aliases.contains(weakFlight->m_flight)) {
                        continue;
                    }
                    // ... and only when the two were flying it at the same time. A
                    // callsign is flown again day after day, by a different airframe as
                    // often as by the same one, so a flight-only record from last week
                    // must not be folded into whichever aircraft flew that number most
                    // recently - which is what an unbounded name match did.
                    if (!weakFlight->m_lastSeen.isValid() || !candFlight->m_lastSeen.isValid()
                        || (std::abs(weakFlight->m_lastSeen.secsTo(candFlight->m_lastSeen))
                            >= m_flightKeyValidSecs)) {
                        continue;
                    }
                    strong = cand;
                    break;
                }
                if (strong) {
                    break;
                }
            }
            if (strong) {
                break;
            }
        }
        if (strong) {
            absorbAircraft(strong, weak);
        }
    }
    for (TrackedAircraft *item : m_aircraft) {
        mergeLoadedFlights(item);
    }

    // Prune aircraft not heard from within the removal timeout, then send the
    // survivors to the Map and register their current flight names
    removeOldAircraft();

    m_loadingDatabase = false;
    for (TrackedAircraft *item : m_aircraft)
    {
        if (item->m_currentFlight)
        {
            if (!item->m_currentFlight->m_flight.isEmpty())
            {
                item->m_flight = item->m_currentFlight->m_flight;
                m_byKey.insert("F" + item->m_flight, item);
            }
            for (const QString& alias : item->m_currentFlight->m_aliases) {
                m_byKey.insert("F" + alias, item);
            }
        }
        // Restored aircraft start inactive: they appear in the archive tables and are
        // not drawn until data arrives for them again
        item->m_active = false;
        m_dirtyAircraft.insert(item);
        for (TrackedFlight *flight : item->m_flights) {
            m_dirtyFlights.insert(flight);
        }
    }

    // Anything restored that is already past its retention goes now
    discardOldAircraft();
}


// The key an airframe is counted under for all time. The ICAO address where there is
// one, because it is the only identifier that is genuinely the airframe; otherwise the
// registration, which is nearly as good, prefixed to keep the two namespaces apart.
//
// A flight number is deliberately NOT accepted. It identifies a service rather than an
// airframe, so counting one would both inflate the total and count the same aircraft
// again the day it is heard with an address - and an aircraft with neither an address
// nor a registration is rare, since ADS-B always carries the first and an ACARS block
// almost always carries the second.
static QString seenKey(quint32 icao, const QString& registration)
{
    if (icao) {
        return QString("%1").arg(icao, 6, 16, QChar('0')).toUpper();
    }
    if (!registration.isEmpty()) {
        return "R:" + registration;
    }
    return QString();
}

int AircraftTracker::recordSeen(TrackedAircraft *item, int& countDelta)
{
    countDelta = 0;
    const QString key = seenKey(item->m_icao, item->m_registration);
    if (key.isEmpty()) {
        return -1;              // Nothing to recognise it by yet: ask again next report
    }
    if (!m_db) {
        return -1;              // Not written: ask again rather than counting it as done
    }

    // A registration-only airframe is recorded under its registration and moves to its
    // ICAO address the day one arrives. The old row has to move with it: left behind, a
    // later session hears the same aircraft, inserts the address as a key it has never
    // seen, and counts one airframe as two for ever.
    //
    // Written as "is there a stale registration row" rather than "did the key just
    // change", because the change may have happened in an earlier session - the aircraft
    // is restored already holding both identities, with nothing to say which of them it
    // was last recorded under.
    //
    // The two rows are folded into one that spans both: the earlier first_seen and the
    // later last_seen. That has to be done explicitly - an UPDATE that simply rekeys the
    // row cannot merge, and one that rekeys onto an address that already has a row
    // cannot even run.
    if (item->m_icao && !item->m_registration.isEmpty())
    {
        const QString stale = "R:" + item->m_registration;
        qint64 staleFirst = 0;
        qint64 staleLast = 0;
        bool staleExists = false;

        sqlite3_stmt *moveStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "SELECT first_seen, last_seen FROM seen WHERE key = ?",
                -1, &moveStmt, nullptr) == SQLITE_OK)
        {
            bindText(moveStmt, 1, stale);
            if (sqlite3_step(moveStmt) == SQLITE_ROW)
            {
                staleExists = true;
                staleFirst = sqlite3_column_int64(moveStmt, 0);
                staleLast = sqlite3_column_int64(moveStmt, 1);
            }
        }
        sqlite3_finalize(moveStmt);

        if (staleExists)
        {
            // Whether the address already had a row of its own decides whether this is a
            // rename or a merge - and a merge is one airframe fewer than the table held
            bool addressExists = false;
            moveStmt = nullptr;
            if (sqlite3_prepare_v2(m_db, "SELECT 1 FROM seen WHERE key = ?",
                    -1, &moveStmt, nullptr) == SQLITE_OK)
            {
                bindText(moveStmt, 1, key);
                addressExists = sqlite3_step(moveStmt) == SQLITE_ROW;
            }
            sqlite3_finalize(moveStmt);

            bool folded = false;
            moveStmt = nullptr;
            if (sqlite3_prepare_v2(m_db, "DELETE FROM seen WHERE key = ?",
                    -1, &moveStmt, nullptr) == SQLITE_OK)
            {
                bindText(moveStmt, 1, stale);
                folded = sqlite3_step(moveStmt) == SQLITE_DONE;
            }
            sqlite3_finalize(moveStmt);
            if (!folded) {
                qWarning() << "AircraftTracker::recordSeen: could not fold" << stale
                           << "into" << key << ":" << sqlite3_errmsg(m_db);
            }

            if (folded)
            {
                // Carry the span across. INSERT OR IGNORE below cannot widen a row it
                // does not create, so where the address had one already this is the only
                // thing that keeps the airframe's real first_seen.
                moveStmt = nullptr;
                if (sqlite3_prepare_v2(m_db,
                        "INSERT INTO seen (key, first_seen, last_seen) VALUES (?,?,?)"
                        " ON CONFLICT(key) DO UPDATE SET"
                        " first_seen = MIN(first_seen, excluded.first_seen),"
                        " last_seen = MAX(last_seen, excluded.last_seen)",
                        -1, &moveStmt, nullptr) == SQLITE_OK)
                {
                    bindText(moveStmt, 1, key);
                    sqlite3_bind_int64(moveStmt, 2, staleFirst);
                    sqlite3_bind_int64(moveStmt, 3, staleLast);
                    if (sqlite3_step(moveStmt) != SQLITE_DONE) {
                        qWarning() << "AircraftTracker::recordSeen:" << sqlite3_errmsg(m_db);
                    }
                }
                sqlite3_finalize(moveStmt);

                // Two rows became one, so the all time count the caller holds is now one
                // too high. It is only read from the table at startup, so without this it
                // would stay wrong until the next run.
                if (addressExists) {
                    countDelta--;
                }
            }
        }
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    sqlite3_stmt *stmt = nullptr;

    // INSERT OR IGNORE, then ask how many rows changed: exactly 1 when this airframe
    // was new and 0 when it was already there, which is the answer the count needs. An
    // upsert would report 1 either way, and last_insert_rowid cannot be used to tell
    // them apart because the aircraft and flight inserts move it too
    if (sqlite3_prepare_v2(m_db,
            "INSERT OR IGNORE INTO seen (key, first_seen, last_seen) VALUES (?,?,?)",
            -1, &stmt, nullptr) != SQLITE_OK)
    {
        qWarning() << "AircraftTracker::recordSeen:" << sqlite3_errmsg(m_db);
        return -1;              // A failure is not "already counted": try again
    }
    bindText(stmt, 1, key);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_int64(stmt, 3, now);

    bool inserted = false;
    bool stepped = false;
    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        stepped = true;
        inserted = sqlite3_changes(m_db) > 0;
    }
    else
    {
        qWarning() << "AircraftTracker::recordSeen:" << sqlite3_errmsg(m_db);
    }
    sqlite3_finalize(stmt);
    if (!stepped) {
        return -1;              // Nothing was written, so nothing has been counted
    }

    if (!inserted)
    {
        // Heard before, so only the last seen time moves
        stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, "UPDATE seen SET last_seen = ? WHERE key = ?",
                -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, now);
            bindText(stmt, 2, key);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }
    if (inserted) {
        countDelta++;
    }
    return 1;
}

// How many different airframes have ever been heard
// How many rows a table holds. Used to compare what is stored against what is in
// memory, which is the only way to notice that a save is about to destroy something.
int AircraftTracker::tableCount(const char *table)
{
    if (!m_db) {
        return 0;
    }
    const QString sql = QString("SELECT COUNT(*) FROM %1").arg(table);
    sqlite3_stmt *stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(m_db, sql.toUtf8().constData(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

int AircraftTracker::seenCount()
{
    if (!m_db) {
        return 0;
    }
    sqlite3_stmt *stmt = nullptr;
    int count = 0;
    if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM seen", -1, &stmt, nullptr) == SQLITE_OK)
    {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

// Only from an explicit "reset all time statistics", never from clearing the session
void AircraftTracker::resetSeen()
{
    if (m_db) {
        execSQL(m_db, "DELETE FROM seen");
    }
}
