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

#ifndef MODEMRADIOPAGING_RADIOPAGINGPAGERFORMAT_H
#define MODEMRADIOPAGING_RADIOPAGINGPAGERFORMAT_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>

#include "export.h"
#include "util/messagerecord.h"

// The formats of the Pager demodulator, for the messages of the radio paging channels: the UDP datagram, and the
// rows of its CSV log
class MODEMRADIOPAGING_API RadioPagingPagerFormat
{
public:
    //! An update that only completes an earlier record (the end of a GSC voice message, without text)
    static bool isCompletion(const MessageRecord& record);
    //! The datagram of a message: date, time, address, function bits, alphanumeric, numeric, protocol and type, each
    //! ended by a NUL. The address has 7 digits for POCSAG and 9 for FLEX, the function bits are only for POCSAG. A
    //! numeric message is in the numeric field and its alphanumeric reading in the other, and the other way round.
    //! Empty for what is not a message (an event, a diagnostic, the end of a voice message)
    static QByteArray datagram(const MessageRecord& record);
    //! The header of the CSV log the Pager demodulator writes (csvColumns, then baud, protocol, type and frame)
    static QStringList csvHeader();
    //! The row of a message in that log: BCH parity errors are its uncorrectable errors, even parity errors none.
    //! Empty for what is not a message
    static QString csvRow(const MessageRecord& record);
    //! The message of a datagram of a Pager demodulator; the time is that of the datagram, else now (ms since the epoch)
    static bool fromDatagram(const QByteArray& datagram, qint64 now, MessageRecord& record);
    //! The header of the CSV log of the Pager demodulator has these columns
    static QStringList csvColumns();
    //! The message of a row of that log (columns: what CSV::readHeader found)
    static bool fromCsvRow(const QHash<QString, int>& columns, const QStringList& row, MessageRecord& record);

private:
    //! Date, time, address, function bits, alphanumeric, numeric, protocol and type of a message; empty if not one
    static QStringList fields(const MessageRecord& record);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGPAGERFORMAT_H
