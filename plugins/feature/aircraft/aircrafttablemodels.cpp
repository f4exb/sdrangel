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

#include <algorithm>
#include <cmath>
#include <functional>
#include <QColor>
#include <QDateTime>
#include <QLocale>
#include "util/airlines.h"
#include "util/osndb.h"
#include "util/ourairportsdb.h"

#include "aircrafttablemodels.h"
#include "aircraftsettings.h"

// The saved column width and order arrays are sized by these, and a header signal
// indexes them with a logical column straight from the model. Adding a column to a
// model without growing the array would write past the end of it.
static_assert(AircraftTableModel::COL_COUNT == AIRCRAFT_COLUMNS,
              "AIRCRAFT_COLUMNS must match AircraftTableModel::COL_COUNT");
static_assert(FlightTableModel::COL_COUNT == FLIGHT_COLUMNS,
              "FLIGHT_COLUMNS must match FlightTableModel::COL_COUNT");

AircraftTableModel::AircraftTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int AircraftTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int AircraftTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COL_COUNT;
}

QVariant AircraftTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return QVariant();
    }
    // What each column holds. Several headings are abbreviated to fit, and the table's
    // own tooltip says nothing about the columns.
    if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case COL_ICAO: return "The aircraft's ICAO 24 bit address as hex. Unique to the airframe.";
        case COL_REG: return "Registration, from a message or looked up from the ICAO address";
        case COL_TYPE: return "ICAO aircraft type designator, e.g. B77W";
        case COL_FLIGHT: return "Callsign the aircraft is transmitting - not the flight number";
        case COL_FLIGHT_NO: return "Flight number, the form a passenger is sold. Often differs from the callsign.";
        case COL_AIRLINE: return "Operator, from the callsign's airline designator";
        case COL_SIDEVIEW: return "Side view of the type in the operator's livery, where one is known";
        case COL_COUNTRY: return "Country of registration, from the ICAO address block";
        case COL_LATITUDE: return "Latitude last reported, in degrees";
        case COL_LONGITUDE: return "Longitude last reported, in degrees";
        case COL_DISTANCE: return "Distance from the receiver that heard it, in km";
        case COL_ALTITUDE: return "Altitude last reported, in feet";
        case COL_HEADING: return "Heading or track last reported, in degrees";
        case COL_SPEED: return "Speed last reported, in knots";
        case COL_PROTOCOLS: return "Which protocols this aircraft has been heard on, and on what frequencies";
        case COL_MESSAGES: return "Messages received from this aircraft";
        case COL_LAST_SEEN: return "When a message was last received from this aircraft";
        case COL_SELCAL: return "SELCAL code: the one it was heard called on, otherwise what the"
                                " SELCAL database assigns it. Download the database from the"
                                " SELCAL Demodulator";
        default: return QVariant();
        }
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    switch (section)
    {
    case COL_ICAO: return "ICAO";
    case COL_REG: return "Reg";
    case COL_TYPE: return "Type";
    case COL_FLIGHT: return "Callsign";
    case COL_FLIGHT_NO: return "Flight No";
    case COL_AIRLINE: return "Airline";
    case COL_SIDEVIEW: return "Sideview";
    case COL_COUNTRY: return "Country";
    case COL_LATITUDE: return "Lat (°)";
    case COL_LONGITUDE: return "Lon (°)";
    case COL_DISTANCE: return "Dist (km)";
    case COL_ALTITUDE: return "Alt (ft)";
    case COL_HEADING: return "Hdg (°)";
    case COL_SPEED: return "Spd (kn)";
    case COL_PROTOCOLS: return "Protocols";
    case COL_MESSAGES: return "Msgs";
    case COL_LAST_SEEN: return "Last seen";
    case COL_SELCAL: return "SELCAL";
    default: return QVariant();
    }
}

// Country flag, airline logo and sideview silhouette, as in the ADS-B
// demodulator's table, resolved once per aircraft
void AircraftTableModel::resolveIcons(Row& row) const
{
    row.m_iconsResolved = true;
    row.m_country = QIcon();
    row.m_airline = QIcon();
    row.m_airlineText.clear();
    row.m_sideview = QIcon();
    row.m_type.clear();

    QSharedPointer<const QHash<QString, AircraftInformation *>> aircraftInfo = OsnDB::getAircraftInformationByReg();
    const AircraftInformation *info = nullptr;
    if (aircraftInfo && aircraftInfo->contains(row.m_display.m_registration)) {
        info = aircraftInfo->value(row.m_display.m_registration);
    }

    if (info)
    {
        row.m_type = info->m_type;
        QIcon *icon = nullptr;
        if (!info->m_operatorICAO.isEmpty()) {
            icon = AircraftInformation::getAirlineIcon(info->m_operatorICAO);
        }
        if (icon) {
            row.m_airline = *icon;
        } else if (!info->m_operator.isEmpty()) {
            row.m_airlineText = info->m_operator;
        } else {
            row.m_airlineText = info->m_owner;
        }
        icon = AircraftInformation::getSideviewIcon(info->m_registration, info->m_operatorICAO, info->m_type);
        if (icon) {
            row.m_sideview = *icon;
        }
        QString flag = info->getFlag();
        if (!flag.isEmpty())
        {
            icon = AircraftInformation::getFlagIcon(flag);
            if (icon) {
                row.m_country = *icon;
            }
        }
    }
    else if (!row.m_display.m_flight.isEmpty())
    {
        // No database entry: airline logo and country from the flight's
        // airline designator
        const Airline *airline = Airline::getByICAO(row.m_display.m_flight.left(3));
        if (airline)
        {
            QIcon *icon = AircraftInformation::getAirlineIcon(airline->m_icao);
            if (icon) {
                row.m_airline = *icon;
            } else {
                row.m_airlineText = airline->m_name;
            }
            QString flag = airline->m_country.toLower().replace(" ", "_");
            icon = AircraftInformation::getFlagIcon(flag);
            if (icon) {
                row.m_country = *icon;
            }
        }
    }
}

// How long ago, coarsely - the tooltip says how stale a figure is, not when exactly
static QString ageText(qint64 secs)
{
    if (secs < 90) {
        return QString("%1 s").arg(secs);
    } else if (secs < 90 * 60) {
        return QString("%1 min").arg((secs + 30) / 60);
    } else if (secs < 48 * 60 * 60) {
        return QString("%1 h %2 min").arg(secs / 3600).arg((secs % 3600) / 60);
    } else {
        return QString("%1 days").arg(secs / 86400);
    }
}

// The age in seconds of a figure in a column that no longer describes where the aircraft
// is, or -1 when it is current. The table keeps a position for the whole flight, but the
// Map drops it after "ADS-B position for" or "ACARS position for", and Find on map then
// has nothing to find - so a row would show as current a position the Map has given up
// on. Lat, Lon and Dist go with the position. An altitude, heading or speed can have
// arrived after it - a Mode S altitude reply carries no position - so each of those is
// judged by its own time against the same window, which makes one that came with the
// position stale with it and leaves a later one alone while it is still fresh.
// Only for aircraft being heard: an archived one is off the Map by definition.
static qint64 staleAge(const AircraftDisplay& d, int column)
{
    if (!d.m_active || !d.m_positionValid
        || !d.m_mapAvailableUntil.isValid() || !d.m_positionDateTime.isValid()) {
        return -1;
    }
    // Milliseconds since the epoch throughout: the times are UTC, so none of this touches
    // the time zone code, which is behind a lock shared by every thread
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 until = d.m_mapAvailableUntil.toMSecsSinceEpoch();
    if (until > now) {
        return -1;
    }
    const qint64 positionTime = d.m_positionDateTime.toMSecsSinceEpoch();
    const qint64 window = until - positionTime;

    // A value with no time of its own is taken to have come with the position
    auto valueAge = [&](bool valid, const QDateTime& when) -> qint64 {
        if (!valid) {
            return -1;
        }
        const qint64 time = when.isValid() ? when.toMSecsSinceEpoch() : positionTime;
        return (time + window <= now) ? (now - time) / 1000 : -1;
    };

    switch (column)
    {
    case AircraftTableModel::COL_LATITUDE:
    case AircraftTableModel::COL_LONGITUDE:
        return (now - positionTime) / 1000;
    case AircraftTableModel::COL_DISTANCE:
        return d.m_distanceValid ? (now - positionTime) / 1000 : -1;
    case AircraftTableModel::COL_ALTITUDE: return valueAge(d.m_altitudeValid, d.m_altitudeDateTime);
    case AircraftTableModel::COL_HEADING: return valueAge(d.m_headingValid, d.m_headingDateTime);
    case AircraftTableModel::COL_SPEED: return valueAge(d.m_speedValid, d.m_speedDateTime);
    default: return -1;
    }
}

QVariant AircraftTableModel::data(const QModelIndex& index, int role) const
{

    if (role == ActiveRole)
    {
        const int r = index.row();
        return ((r >= 0) && (r < m_rows.size())) ? m_rows[r].m_display.m_active : false;
    }
    if (!index.isValid() || (index.row() >= m_rows.size())) {
        return QVariant();
    }
    const Row& row = m_rows[index.row()];
    const AircraftDisplay& d = row.m_display;

    if ((role == Qt::DecorationRole) || ((role == Qt::DisplayRole) && ((index.column() == COL_TYPE) || (index.column() == COL_AIRLINE))))
    {
        if (!row.m_iconsResolved) {
            resolveIcons(const_cast<Row&>(row));
        }
    }

    if (role == Qt::DecorationRole)
    {
        switch (index.column())
        {
        case COL_COUNTRY: return row.m_country.isNull() ? QVariant() : row.m_country;
        case COL_AIRLINE: return row.m_airline.isNull() ? QVariant() : row.m_airline;
        case COL_SIDEVIEW: return row.m_sideview.isNull() ? QVariant() : row.m_sideview;
        default: return QVariant();
        }
    }

    // Greyed and explained rather than hidden: where the aircraft was is still worth
    // knowing, as long as it does not read as where it is
    if ((role == Qt::ForegroundRole) || (role == Qt::ToolTipRole))
    {
        const qint64 age = staleAge(d, index.column());
        if (age < 0) {
            return QVariant();
        }
        if (role == Qt::ForegroundRole) {
            return QColor(128, 128, 128);
        }
        QString what;
        switch (index.column())
        {
        case COL_ALTITUDE: what = "Altitude"; break;
        case COL_HEADING: what = "Heading"; break;
        case COL_SPEED: what = "Speed"; break;
        default: what = "Position"; break;
        }
        return QString("%1 last updated %2 ago - the aircraft is no longer on the map")
            .arg(what, ageText(age));
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (index.column())
    {
    case COL_ICAO:
        return d.m_icao ? QString("%1").arg(d.m_icao, 6, 16, QChar('0')).toUpper() : QVariant();
    case COL_REG: return d.m_registration;
    case COL_TYPE: return row.m_type;
    // The ICAO callsign the transponder broadcasts and the IATA number ACARS carries,
    // for the flight the airframe is on now - see AircraftTracker::flightNameForms()
    case COL_FLIGHT: return d.m_callsign;
    case COL_FLIGHT_NO: return d.m_flightNumber;
    case COL_AIRLINE: return row.m_airline.isNull() ? QVariant(row.m_airlineText) : QVariant();
    case COL_LATITUDE: return d.m_positionValid ? QVariant((double) d.m_latitude) : QVariant();
    case COL_LONGITUDE: return d.m_positionValid ? QVariant((double) d.m_longitude) : QVariant();
    // Rounded to 100 m but still a number, so the column sorts by magnitude rather
    // than by the leading character of a formatted string
    case COL_DISTANCE: return d.m_distanceValid
        ? QVariant(std::round((double) d.m_distanceKm * 10.0) / 10.0) : QVariant();
    // Altitude alone, not altitude and a position. The tracker deliberately accepts an
    // altitude from a report that carried no position - a Mode S altitude reply has one
    // and not the other - so requiring both hid a figure it had gone to the trouble of
    // keeping.
    case COL_ALTITUDE: return d.m_altitudeValid ? QVariant((int) d.m_altitudeFt) : QVariant();
    case COL_HEADING: return d.m_headingValid ? QVariant((int) d.m_heading) : QVariant();
    case COL_SPEED: return d.m_speedValid ? QVariant((int) d.m_speedKts) : QVariant();
    case COL_PROTOCOLS: return d.m_protocols;
    case COL_MESSAGES: return d.m_messages;
    case COL_LAST_SEEN: return d.m_lastSeen;
    case COL_SELCAL: return d.m_selcal;
    default: return QVariant();
    }
}

void AircraftTableModel::upsert(const QList<AircraftDisplay>& aircraft)
{
    QList<Row> added;

    for (const AircraftDisplay& d : aircraft)
    {
        int row = m_rowById.value(d.m_id, -1);
        if (row >= 0)
        {
            // Re-resolve icons if the identity changed
            if ((m_rows[row].m_display.m_registration != d.m_registration)
                || (m_rows[row].m_display.m_flight != d.m_flight)) {
                m_rows[row].m_iconsResolved = false;
            }
            m_rows[row].m_display = d;
            emit dataChanged(index(row, 0), index(row, COL_COUNT - 1));
        }
        else
        {
            // Collected and inserted in one go below: a beginInsertRows per aircraft makes
            // the proxy re-sort and the view relayout once per row, and a feed delivers
            // these in batches of dozens
            Row newRow;
            newRow.m_display = d;
            added.append(newRow);
        }
    }

    if (!added.isEmpty())
    {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + added.size() - 1);
        for (const Row& r : added)
        {
            m_rowById.insert(r.m_display.m_id, m_rows.size());
            m_rows.append(r);
        }
        endInsertRows();
    }
}

// Rows for ids we hold, highest first, so removing one never moves the next
template <typename T>
static QList<int> rowsDescending(const QHash<quint64, int>& rowById, const QList<quint64>& ids, T&& forget)
{
    QList<int> rows;
    for (quint64 id : ids)
    {
        const int row = rowById.value(id, -1);
        if (row >= 0)
        {
            rows.append(row);
            forget(id);
        }
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    return rows;
}

void AircraftTableModel::remove(const QList<quint64>& ids)
{
    // Removed in contiguous runs and reindexed once. One row at a time reindexed
    // everything below each removal, so the retention sweep discarding a few hundred
    // archived aircraft cost hundreds of passes over the whole table.
    const QList<int> rows = rowsDescending(m_rowById, ids, [this](quint64 id) { m_rowById.remove(id); });
    if (rows.isEmpty()) {
        return;
    }
    for (int i = 0; i < rows.size(); )
    {
        const int last = rows[i];
        int first = last;
        while ((++i < rows.size()) && (rows[i] == first - 1)) {
            first--;
        }
        beginRemoveRows(QModelIndex(), first, last);
        m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
        endRemoveRows();
    }
    reindex(rows.last());
}

void AircraftTableModel::reindex(int from)
{
    for (int i = from; i < m_rows.size(); i++) {
        m_rowById.insert(m_rows[i].m_display.m_id, i);
    }
}

void AircraftTableModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_rowById.clear();
    endResetModel();
}

const AircraftDisplay *AircraftTableModel::aircraftAt(int row) const
{
    return ((row >= 0) && (row < m_rows.size())) ? &m_rows[row].m_display : nullptr;
}

// Both the current and the archived aircraft views are proxies over this one model, so
// this finds an aircraft whichever of them is showing it - and returns nullptr for one
// that has been discarded altogether
const AircraftDisplay *AircraftTableModel::aircraftById(quint64 id) const
{
    const auto it = m_rowById.constFind(id);
    return (it != m_rowById.constEnd()) ? aircraftAt(it.value()) : nullptr;
}

int AircraftTableModel::rowOf(quint64 id) const
{
    return m_rowById.value(id, -1);
}

FlightTableModel::FlightTableModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int FlightTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int FlightTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COL_COUNT;
}

// The airport's name, for a tooltip. Looked up here rather than carried on every
// flight display: it is wanted only while the pointer rests on a cell, and the database
// is a shared hash that is loaded already.
static QString airportName(const QString& icao)
{
    if (icao.size() != 4) {
        return QString();
    }
    QSharedPointer<const QHash<QString, AirportInformation *>> airports =
        OurAirportsDB::getAirportsByIdent();
    const AirportInformation *airport = airports ? airports->value(icao, nullptr) : nullptr;
    return airport ? airport->m_name : QString();
}

QVariant FlightTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        return QVariant();
    }
    // Four of these headings are two letters, so without this there is nothing anywhere
    // that says what they are
    if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
        case COL_FLIGHT: return "Callsign the flight was flown under - what the aircraft transmits, not the flight number";
        case COL_FLIGHT_NO: return "Flight number, the form a passenger is sold. Often unrelated to the callsign.";
        case COL_REG: return "Registration of the aircraft that flew it";
        case COL_FROM: return "Departure airport. Green where a message said so, plain where it is only the route database's guess.";
        case COL_TO: return "Arrival airport. Green where a message said so, plain where it is only the route database's guess.";
        case COL_DOCS: return "Documents collected for this flight: flight plans, clearances, loadsheets and OOOI events. Logons have their own tab and are not counted here.";
        case COL_LS: return "Loadsheet: weights, fuel and passenger figures, sent before departure";
        case COL_CL: return "Clearance: an oceanic clearance, a request for one, or a pre-departure clearance";
        case COL_FP: return "Flight plan: a filed route, from the aircraft's own FMC or from a departure clearance";
        case COL_OUT: return "OOOI: off the gate";
        case COL_OFF: return "OOOI: airborne";
        case COL_ON: return "OOOI: landed";
        case COL_IN: return "OOOI: on the gate";
        case COL_FIRST_SEEN: return "When this flight was first heard";
        case COL_LAST_SEEN: return "When this flight was last heard";
        case COL_PROTOCOLS: return "Which protocols this flight was heard on, and on what frequencies";
        case COL_ROUTE_SOURCE: return "Where From and To came from: a received message or the callsign to route database, and whether the two agree";
        default: return QVariant();
        }
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    switch (section)
    {
    case COL_FLIGHT: return "Callsign";
    case COL_FLIGHT_NO: return "Flight No";
    case COL_REG: return "Reg";
    case COL_FROM: return "From";
    case COL_TO: return "To";
    case COL_OUT: return "Out";
    case COL_OFF: return "Off";
    case COL_ON: return "On";
    case COL_IN: return "In";
    case COL_FIRST_SEEN: return "First seen";
    case COL_LAST_SEEN: return "Last seen";
    case COL_LS: return "LS";
    case COL_CL: return "CL";
    case COL_FP: return "FP";
    case COL_DOCS: return "Docs";
    case COL_PROTOCOLS: return "Protocols";
    case COL_ROUTE_SOURCE: return "Route src";
    default: return QVariant();
    }
}

QVariant FlightTableModel::data(const QModelIndex& index, int role) const
{

    if (role == ActiveRole)
    {
        const int r = index.row();
        return ((r >= 0) && (r < m_rows.size())) ? m_rows[r].m_active : false;
    }
    if (!index.isValid() || (index.row() >= m_rows.size())) {
        return QVariant();
    }
    const FlightDisplay& d = m_rows[index.row()];

    // OOOI columns show the time and sort on the instant. Qt sorts on the display role
    // by default, so the full value goes out as the sort role and the short form as the
    // display - otherwise a flight that went out at 23:55 sorts after one at 00:10.
    if ((index.column() >= COL_OUT) && (index.column() <= COL_IN))
    {
        const QDateTime *when = (index.column() == COL_OUT) ? &d.m_out
                              : (index.column() == COL_OFF) ? &d.m_off
                              : (index.column() == COL_ON)  ? &d.m_on : &d.m_in;
        if (role == Qt::DisplayRole) {
            return when->isValid() ? when->toUTC().toString("hh:mm") : QString();
        }
        if (role == Qt::ToolTipRole) {
            return when->isValid() ? when->toUTC().toString("yyyy-MM-dd hh:mm:ss' UTC'") : QString();
        }
        if (role == SortRole) {
            return *when;
        }
    }

    // Green where an aircraft itself said so, plain where it is only the callsign to
    // route database's guess. That is the distinction worth seeing at a glance, and it
    // does not depend on the database holding the callsign at all - most do not, so
    // colouring by agreement with it left confirmed routes looking unconfirmed.
    //
    // No red: where a message and the database disagree, what is displayed is the
    // message's own answer, so marking it suspect said the opposite of the truth. The
    // disagreement is reported in the Route src column instead.
    if ((role == Qt::ForegroundRole)
        && ((index.column() == COL_FROM) || (index.column() == COL_TO)))
    {
        const bool fromMessage = (index.column() == COL_FROM) ? d.m_departureFromMessage
                                                              : d.m_arrivalFromMessage;
        return fromMessage ? QVariant(QColor(0, 192, 0)) : QVariant();
    }

    if (role == Qt::ToolTipRole)
    {
        if (((index.column() == COL_FLIGHT) || (index.column() == COL_FLIGHT_NO))
            && !d.m_aliases.isEmpty()) {
            return QString("Also known as: %1").arg(d.m_aliases);
        }
        if ((index.column() == COL_FROM) || (index.column() == COL_TO)
            || (index.column() == COL_ROUTE_SOURCE))
        {
            QStringList lines;
            // Named, because a four letter ident is only recognisable if you happen to
            // know it. Both ends whichever cell is hovered, since the pair is what the
            // row is about.
            for (int end = 0; end < 2; end++)
            {
                const QString ident = end ? d.m_arrival : d.m_departure;
                if (ident.isEmpty()) {
                    continue;
                }
                const QString name = airportName(ident);
                lines.append(QString("%1: %2").arg(end ? "To" : "From")
                    .arg(name.isEmpty() ? ident : QString("%1 - %2").arg(ident).arg(name)));
            }
            if ((index.column() == COL_FROM) && !d.m_route.isEmpty()) {
                lines.append(QString("Route: %1").arg(d.m_route));
            }
            if (d.m_routeSource.isEmpty()) {
                return lines.isEmpty() ? QVariant() : QVariant(lines.join("\n"));
            }
            QString tip = (d.m_routeSource == "Database")
                ? "From the callsign to route database, which is a guess until a message says otherwise"
                : "From a received message";
            if (d.m_routeChecked)
            {
                tip += d.m_routeAgrees
                    ? " - the route database agrees"
                    : " - the route database says something different";
            }
            lines.append(tip);
            return lines.join("\n");
        }
        return QVariant();
    }
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (index.column())
    {
    // The ICAO callsign the transponder broadcasts, and the IATA number ACARS carries.
    // One flight, two names - see AircraftTracker::flightNameForms()
    case COL_FLIGHT: return d.m_callsign;
    case COL_FLIGHT_NO: return d.m_flightNumber;
    case COL_REG: return d.m_reg;
    case COL_FROM: return d.m_departure;
    case COL_TO: return d.m_arrival;
    // Displayed as the time alone - these are all within a few hours of each other and
    // the date is noise - but returned as a QDateTime so sorting still works across
    // midnight, which a bare "23:55" against "00:10" would not
    case COL_OUT: return d.m_out;
    case COL_OFF: return d.m_off;
    case COL_ON: return d.m_on;
    case COL_IN: return d.m_in;
    case COL_FIRST_SEEN: return d.m_firstSeen;
    case COL_LAST_SEEN: return d.m_lastSeen;
    // Sorted and displayed on the same value, so sorting groups the flights that have
    // one against those that do not
    // A tick where the flight has one, and nothing where it does not. A column of
    // crosses is as loud as a column of ticks, which makes the ticks - the thing worth
    // spotting - harder to pick out than an empty cell does.
    case COL_LS: return d.m_hasLoadsheet ? QString(QChar(0x2713)) : QString();
    case COL_CL: return d.m_hasClearance ? QString(QChar(0x2713)) : QString();
    case COL_FP: return d.m_hasFlightPlan ? QString(QChar(0x2713)) : QString();
    case COL_DOCS: return d.m_documents;
    // What THIS flight has been heard on. The aircraft table's column of the same
    // name is the airframe's, over every flight it has ever made.
    case COL_PROTOCOLS: return d.m_protocols;
    // "Message" on its own where the database has never heard of the callsign, and with
    // a verdict where it has
    case COL_ROUTE_SOURCE:
        if (d.m_routeSource.isEmpty() || !d.m_routeChecked) {
            return d.m_routeSource;
        }
        return d.m_routeAgrees ? QString("Message =DB") : QString("Message <>DB");
    default: return QVariant();
    }
}

void FlightTableModel::upsert(const QList<FlightDisplay>& flights)
{
    // New flights are collected and inserted in one go, as AircraftTableModel::upsert()
    // does and for the same reason: a resync delivers every flight in the session at once,
    // and a beginInsertRows per flight re-sorted both proxies and relaid out both views
    // once per row. Indexed as they are collected, so a flight that appears twice in the
    // batch updates the row it is about to get rather than getting two.
    QList<FlightDisplay> added;
    QHash<quint64, int> addedIndex;

    for (const FlightDisplay& d : flights)
    {
        int row = m_rowById.value(d.m_id, -1);
        if (row >= 0)
        {
            m_rows[row] = d;
            emit dataChanged(index(row, 0), index(row, COL_COUNT - 1));
        }
        else
        {
            const int pending = addedIndex.value(d.m_id, -1);
            if (pending >= 0)
            {
                added[pending] = d;
            }
            else
            {
                addedIndex.insert(d.m_id, added.size());
                added.append(d);
            }
        }
    }

    if (!added.isEmpty())
    {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + added.size() - 1);
        for (const FlightDisplay& d : added)
        {
            m_rowById.insert(d.m_id, m_rows.size());
            m_rows.append(d);
        }
        endInsertRows();
    }
}

void FlightTableModel::remove(const QList<quint64>& ids)
{
    // In contiguous runs and reindexed once - see AircraftTableModel::remove()
    const QList<int> rows = rowsDescending(m_rowById, ids, [this](quint64 id) { m_rowById.remove(id); });
    if (rows.isEmpty()) {
        return;
    }
    for (int i = 0; i < rows.size(); )
    {
        const int last = rows[i];
        int first = last;
        while ((++i < rows.size()) && (rows[i] == first - 1)) {
            first--;
        }
        beginRemoveRows(QModelIndex(), first, last);
        m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
        endRemoveRows();
    }
    reindex(rows.last());
}

void FlightTableModel::reindex(int from)
{
    for (int i = from; i < m_rows.size(); i++) {
        m_rowById.insert(m_rows[i].m_id, i);
    }
}

void FlightTableModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_rowById.clear();
    endResetModel();
}

const FlightDisplay *FlightTableModel::flightAt(int row) const
{
    return ((row >= 0) && (row < m_rows.size())) ? &m_rows[row] : nullptr;
}

int FlightTableModel::rowOf(quint64 id) const
{
    return m_rowById.value(id, -1);
}

MessageTableModel::MessageTableModel(const QStringList& headings, const QStringList& tooltips, QObject *parent) :
    QAbstractTableModel(parent),
    m_headings(headings),
    m_tooltips(tooltips)
{
}

int MessageTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int MessageTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_headings.size();
}

QVariant MessageTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation != Qt::Horizontal) || (section < 0) || (section >= m_headings.size())) {
        return QVariant();
    }
    if (role == Qt::DisplayRole) {
        return m_headings[section];
    }
    if (role == Qt::ToolTipRole) {
        return m_tooltips.value(section);
    }
    return QVariant();
}

QVariant MessageTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (index.row() >= m_rows.size())) {
        return QVariant();
    }
    const MessageRow& row = m_rows[index.row()];
    const int column = index.column();

    if (role == Qt::DisplayRole)
    {
        if (column == 0)
        {
            // Local time, worked out only for the rows being drawn. The date keeps the
            // locale's own format so the column looks as it always has; the time is
            // spelled out to the second
            static const QString format = QLocale().dateFormat(QLocale::ShortFormat) + " hh:mm:ss";
            return row.m_receivedMs ? QDateTime::fromMSecsSinceEpoch(row.m_receivedMs).toString(format) : QString();
        }
        return row.m_cells.value(column - 1);
    }
    if ((role == Qt::ToolTipRole) && (column == m_headings.size() - 1) && !row.m_tooltip.isEmpty()) {
        return row.m_tooltip;
    }
    return QVariant();
}

void MessageTableModel::append(const QList<MessageRow>& rows)
{
    if (rows.isEmpty()) {
        return;
    }
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + rows.size() - 1);
    m_rows.append(rows);
    endInsertRows();
}

void MessageTableModel::clear()
{
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

const MessageRow *MessageTableModel::rowAt(int row) const
{
    return ((row >= 0) && (row < m_rows.size())) ? &m_rows[row] : nullptr;
}

void MessageTableModel::retagAircraft(quint64 from, quint64 to)
{
    for (MessageRow& row : m_rows)
    {
        if (row.m_aircraftId == from) {
            row.m_aircraftId = to;
        }
    }
}

void MessageTableModel::retagFlight(quint64 from, quint64 to)
{
    for (MessageRow& row : m_rows)
    {
        if (row.m_flightId == from) {
            row.m_flightId = to;
        }
    }
}

// From the bottom up and a run of adjacent rows at a time, so each removal moves only the
// rows below it once
void MessageTableModel::removeIf(const std::function<bool(const MessageRow& row)>& matches, qint64 olderThanMs)
{
    auto rowMatches = [this, &matches, olderThanMs](int i) {
        const MessageRow& row = m_rows[i];
        if (olderThanMs && (!row.m_receivedMs || (row.m_receivedMs >= olderThanMs))) {
            return false;
        }
        return matches(row);
    };
    int i = m_rows.size() - 1;
    while (i >= 0)
    {
        if (!rowMatches(i))
        {
            i--;
            continue;
        }
        const int last = i;
        while ((i >= 0) && rowMatches(i)) {
            i--;
        }
        beginRemoveRows(QModelIndex(), i + 1, last);
        m_rows.erase(m_rows.begin() + i + 1, m_rows.begin() + last + 1);
        endRemoveRows();
    }
}

MessageFilterProxy::MessageFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_aircraftId(0),
    m_flightId(0)
{
    // Row numbers count what is displayed, as in ActiveFilterProxy
    auto renumber = [this]()
    {
        if (rowCount() > 0) {
            emit headerDataChanged(Qt::Vertical, 0, rowCount() - 1);
        }
    };
    connect(this, &QAbstractItemModel::rowsInserted, this, renumber);
    connect(this, &QAbstractItemModel::rowsRemoved, this, renumber);
    connect(this, &QAbstractItemModel::layoutChanged, this, renumber);
    connect(this, &QAbstractItemModel::modelReset, this, renumber);
}

void MessageFilterProxy::setSelection(quint64 aircraftId, quint64 flightId)
{
    beginFilterChange();
    m_aircraftId = aircraftId;
    m_flightId = flightId;
    endFilterChange();
}

const MessageRow *MessageFilterProxy::rowAt(const QModelIndex& index) const
{
    const MessageTableModel *model = qobject_cast<const MessageTableModel *>(sourceModel());
    return (model && index.isValid()) ? model->rowAt(mapToSource(index).row()) : nullptr;
}

QVariant MessageFilterProxy::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical) {
        return (role == Qt::DisplayRole) ? QVariant(section + 1) : QVariant();
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

bool MessageFilterProxy::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    (void) parent;
    const MessageRow *r = static_cast<const MessageTableModel *>(sourceModel())->rowAt(row);
    if (!r) {
        return false;
    }
    if (m_flightId) {
        return r->m_flightId == m_flightId;
    }
    if (m_aircraftId) {
        return r->m_aircraftId == m_aircraftId;
    }
    // Nothing else is ever logged without an aircraft, and these would otherwise have
    // nowhere to be seen
    return r->m_aircraftId == 0;
}

// Equal times go in arrival order, which is the order of the source rows
bool MessageFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (left.column() == 0)
    {
        const MessageTableModel *model = static_cast<const MessageTableModel *>(sourceModel());
        const MessageRow *l = model->rowAt(left.row());
        const MessageRow *r = model->rowAt(right.row());
        if (l && r)
        {
            return (l->m_receivedMs < r->m_receivedMs)
                || ((l->m_receivedMs == r->m_receivedMs) && (left.row() < right.row()));
        }
    }
    return QSortFilterProxyModel::lessThan(left, right);
}
