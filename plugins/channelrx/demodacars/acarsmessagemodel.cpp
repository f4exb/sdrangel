///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                      //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "acarsmessagemodel.h"

AcarsMessageModel::AcarsMessageModel(QObject *parent) :
    QAbstractTableModel(parent),
    m_showDate(true)
{
}

int AcarsMessageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int AcarsMessageModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : MESSAGE_COL_COUNT;
}

QString AcarsMessageModel::dateTimeText(const QDateTime& dateTime) const
{
    if (m_showDate)
    {
        return QString("%1 %2").arg(m_locale.toString(dateTime.date(), QLocale::ShortFormat),
                                    dateTime.time().toString());
    }
    return dateTime.time().toString();
}

// The Mode character, per ARINC 618-7 Attachment 6.
//
// Category A is the single character "2" - "transmitted bi-directional, no preferred
// air/ground data path selected", which ARINC 620 calls the broadcast mode.
//
// Category B identifies a path, and Attachment 6 pairs the two directions: an uplink
// character in ` (6/0) through } (7/13) and the downlink character the aircraft then
// answers on, @ (4/0) through ] (5/13), are the same access code. The two ranges are
// 0x20 apart - the case bit - so subtracting the base of whichever range the character
// is in gives the access code, 0 to 29, for either direction.
//
// Attachment 6 does NOT name the ground stations. Its column is headed "LOGICAL PATH
// (GROUND STATION)" and holds only #0 to #29, because section 2.3.2.2 leaves the
// assignment to the service provider: it is regional operational data, not a standard,
// and only thirty codes exist for the whole world. This used to be decoded against
// per-country tables of airport names taken from an enthusiast's handbook, which named
// stations the specification never does and which were as likely wrong as right - so
// the path number is reported and nothing is invented around it.
//
// The satellite column of Attachment 6 IS normative, but names only five of the thirty,
// and applies to Satcom alone.
QString AcarsMessageModel::modeDecode(const AcarsRowEvent& e)
{
    // Every protocol but plain ACARS names its ground station in the worker, where the
    // station tables are. Only an ACARS block carries an ARINC 618 Mode character.
    if (!e.m_modeDecode.isEmpty()) {
        return e.m_modeDecode;
    }
    if ((e.m_frameType != 0) || e.m_mode.isEmpty()) {
        return QString();
    }
    // Not used on HF: 618 section 2.2.2.5 says to enter the default 2 and ignore it
    if (e.m_protocol == "HFDL") {
        return "";
    }
    if (e.m_mode == "2") {
        return "All"; // Category A
    }

    const ushort c = e.m_mode.at(0).unicode();
    int path = -1;
    if ((c >= 0x40) && (c <= 0x5D)) {
        path = c - 0x40;            // Air to ground
    } else if ((c >= 0x60) && (c <= 0x7D)) {
        path = c - 0x60;            // Ground to air
    } else {
        return "Invalid";
    }

    // Attachment 6's satellite service provider column. Sparse on purpose: the other
    // twenty-five codes have no provider assigned.
    if (e.m_protocol.startsWith("Aero"))
    {
        const char *provider = (path == 1)  ? "ARINC"
                             : (path == 3)  ? "Air Canada"
                             : (path == 10) ? "AVICOM"
                             : (path == 12) ? "AlliedSignal"
                             : (path == 19) ? "SITA" : nullptr;
        if (provider) {
            return QString("%1").arg(provider);
        }
    }
    return QString("#%1").arg(path);
}

QVariant AcarsMessageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !isValidRow(index.row())) {
        return QVariant();
    }
    const Row& r = m_rows.at(index.row());
    const AcarsRowEvent& e = r.m_event;
    const int col = index.column();

    if ((role == Qt::DisplayRole) || (role == Qt::EditRole))
    {
        switch (col)
        {
        case MESSAGE_COL_DATETIME: return dateTimeText(e.m_received);
        case MESSAGE_COL_DIR: return QString(QChar(e.m_uplink ? 0x2191 : 0x2193));
        case MESSAGE_COL_GS: return e.m_mode;
        case MESSAGE_COL_GS_DECODE: return modeDecode(e);
        case MESSAGE_COL_ADDRESS: return e.m_address;
        case MESSAGE_COL_ACK: return e.m_ack;
        case MESSAGE_COL_LABEL: return e.m_label;
        case MESSAGE_COL_LABEL_DECODE: return e.m_labelDecode;
        case MESSAGE_COL_ID: return e.m_blockId;
        case MESSAGE_COL_ORIGIN: return e.m_originator;
        case MESSAGE_COL_ORIGIN_DECODE: return e.m_originatorDecode;
        case MESSAGE_COL_MSG_NUM: return e.m_messageNumber;
        case MESSAGE_COL_BLOCK_SEQUENCE: return e.m_blockSequence;
        case MESSAGE_COL_FLIGHT: return e.m_flight;
        case MESSAGE_COL_TEXT: return e.m_text;
        case MESSAGE_COL_TEXT_DECODE: return e.m_textDecode;
        case MESSAGE_COL_ATC: return e.m_atc;
        case MESSAGE_COL_HEX: return e.m_hex;
        case MESSAGE_COL_PROTOCOL: return e.m_protocol;
        // A number, so it sorts and aligns as one
        case MESSAGE_COL_RATE: return e.m_bitRate ? QVariant(e.m_bitRate) : QVariant();
        // Numbers rather than strings, so they sort and align as numbers. An empty
        // QVariant leaves the cell blank where there is no position at all.
        case MESSAGE_COL_LATITUDE: return e.m_hasPosition ? QVariant(e.m_latitude) : QVariant();
        case MESSAGE_COL_LONGITUDE: return e.m_hasPosition ? QVariant(e.m_longitude) : QVariant();
        case MESSAGE_COL_ALTITUDE:
            return (e.m_hasPosition && e.m_hasAltitude) ? QVariant(e.m_altitudeFt) : QVariant();
        default: return QVariant();
        }
    }
    else if (role == Qt::ToolTipRole)
    {
        // The flattened decode is what fits in the cell; the full one is worth having on
        // hover, and is often many lines
        if ((col == MESSAGE_COL_TEXT_DECODE) && !e.m_fullDecode.isEmpty()) {
            return e.m_fullDecode;
        }
    }
    return QVariant();
}

QVariant AcarsMessageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) {
        // The row numbers come from the proxy, which numbers what is on show rather
        // than mapping back to a source row - see AcarsMessageFilter::headerData
        return QVariant();
    }
    if ((section < 0) || (section >= MESSAGE_COL_COUNT)) {
        return QVariant();
    }

    static const char *names[MESSAGE_COL_COUNT] = {
        "Date/Time", "Dir", "GS", "GS Decode", "Registration", "Ack", "Label",
        "Label Decode", "ID", "Origin", "Origin Decode", "Msg No.", "Block Seq.",
        "Flight", "Message", "Message Decode", "ATC", "Lat (°)",
        "Lon (°)", "Alt (ft)", "Hex", "Protocol", "Rate (bps)"
    };
    static const char *tips[MESSAGE_COL_COUNT] = {
        "Date and time the message was received. Right click the table to show the time alone",
        "Direction of the message relative to the aircraft. ↑ uplink, ground station to aircraft; ↓ downlink, aircraft to ground",
        "Ground Station Id.\n\nACARS: Mode character.\nVDL-2: Ground Station (GS)\nHFDL: Ground Station (GS)\nAero: Ground Earth Station (GES)",
        "Ground Station name",
        "Address / Aircraft registration",
        "Technical acknowledgement",
        "Message type",
        "Description of message type",
        "",
        "Origin of message",
        "Description of origin of message",
        "Message number",
        "Block sequence",
        "Flight number",
        "Message text",
        "Decoded message",
        "Air Traffic Control Communications",
        "Aircraft latitude in decimal degrees. North positive",
        "Aircraft longitude in decimal degrees. East positive",
        "Aircraft altitude in feet",
        "",
        "Which link the frame arrived on",
        "Bit rate the frame was sent at, in bits per second"
    };

    if (role == Qt::DisplayRole) {
        return QString(names[section]);
    }
    if (role == Qt::ToolTipRole)
    {
        const QString tip(tips[section]);
        return tip.isEmpty() ? QVariant() : QVariant(tip);
    }
    return QVariant();
}

void AcarsMessageModel::addRow(const AcarsRowEvent& event)
{
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
    Row r;
    r.m_event = event;
    m_rows.append(r);
    endInsertRows();
}

void AcarsMessageModel::clear()
{
    if (m_rows.isEmpty()) {
        return;
    }
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

// One column's worth of every row has changed, which is a single signal rather than a
// walk over the table writing new strings in
void AcarsMessageModel::columnChanged(int column)
{
    if (m_rows.isEmpty()) {
        return;
    }
    emit dataChanged(index(0, column), index(m_rows.size() - 1, column),
                     { Qt::DisplayRole });
}

void AcarsMessageModel::setShowDate(bool showDate)
{
    if (showDate == m_showDate) {
        return;
    }
    m_showDate = showDate;
    columnChanged(MESSAGE_COL_DATETIME);
}



// beginFilterChange() was added in Qt 6.9 and endFilterChange() in Qt 6.10, and
// invalidateFilter() is deprecated in favour of the pair. Bracketing the change is
// what newer Qt wants - beginFilterChange() before the filter state is touched - so
// the call sites are written that way and these stand in on older Qt, where a single
// invalidateFilter() after the change is the whole operation.
#if QT_VERSION < QT_VERSION_CHECK(6, 10, 0)
void AcarsMessageFilter::beginFilterChange()
{
}

void AcarsMessageFilter::endFilterChange()
{
    invalidateFilter();
}
#endif

AcarsMessageFilter::AcarsMessageFilter(QObject *parent) :
    QSortFilterProxyModel(parent),
    m_hideNoInfo(true),
    m_filterColumn(-1),
    m_filterValid(false)
{
}

void AcarsMessageFilter::setHideNoInfo(bool hide)
{
    if (hide == m_hideNoInfo) {
        return;
    }
    beginFilterChange();
    m_hideNoInfo = hide;
    endFilterChange();
}

void AcarsMessageFilter::setFilter(int column, const QString& pattern)
{
    beginFilterChange();
    m_filterColumn = column;
    // Anchored, as the table's filter has always been - the pattern has to match the
    // whole cell rather than appear somewhere in it
    m_filterRe = QRegularExpression(QRegularExpression::anchoredPattern(pattern));
    m_filterValid = !pattern.isEmpty() && m_filterRe.isValid()
                 && (column >= 0) && (column < MESSAGE_COL_COUNT);
    endFilterChange();
}

// The base class would map a vertical section back to its source row, which with a
// filter applied gives a broken looking 2, 4, 6... Numbering the displayed position
// instead keeps them 1..N, and the last one is the number of messages shown.
QVariant AcarsMessageFilter::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Vertical) && (role == Qt::DisplayRole)) {
        return section + 1;
    }
    return QSortFilterProxyModel::headerData(section, orientation, role);
}

bool AcarsMessageFilter::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    (void) sourceParent;
    const AcarsMessageModel *model = qobject_cast<const AcarsMessageModel *>(sourceModel());
    if (!model || !model->isValidRow(sourceRow)) {
        return true;
    }
    if (m_hideNoInfo && model->event(sourceRow).m_noInfo) {
        return false;
    }
    if (m_filterValid)
    {
        const QString text = model->data(model->index(sourceRow, m_filterColumn),
                                         Qt::DisplayRole).toString();
        if (!m_filterRe.match(text).hasMatch()) {
            return false;
        }
    }
    return true;
}

bool AcarsMessageFilter::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const AcarsMessageModel *model = qobject_cast<const AcarsMessageModel *>(sourceModel());
    if (model && model->isValidRow(left.row()) && model->isValidRow(right.row())
        && (left.column() == MESSAGE_COL_DATETIME))
    {
        // Never on the displayed text: the date can be hidden, and the locale's date
        // string does not sort chronologically even when it is not
        return model->event(left.row()).m_received < model->event(right.row()).m_received;
    }

    const QVariant l = left.data(Qt::DisplayRole);
    const QVariant r = right.data(Qt::DisplayRole);
    switch (left.column())
    {
    case MESSAGE_COL_LATITUDE:
    case MESSAGE_COL_LONGITUDE:
    case MESSAGE_COL_ALTITUDE:
    case MESSAGE_COL_RATE:
        // Blank sorts before any value rather than comparing as the string ""
        if (!l.isValid() || !r.isValid()) {
            return !l.isValid() && r.isValid();
        }
        return l.toDouble() < r.toDouble();
    default:
        return QString::localeAwareCompare(l.toString(), r.toString()) < 0;
    }
}
