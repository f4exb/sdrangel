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

#ifndef INCLUDE_UTIL_MESSAGERECORD_H
#define INCLUDE_UTIL_MESSAGERECORD_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QHash>
#include <QVariantMap>
#include <QJsonObject>

#include "export.h"

// A message received or transmitted by a channel, in a protocol independent form.
//
// Records are exchanged between channels and features through the "messages"
// message pipe (see MainCore::MsgMessageRecord). Producers fill the protocol
// facts they decoded or sent; consumers such as the message center add their own
// interpretation, keeping the raw payload so a message can be decoded again.
//
// Null handling: a null QString (QString()) means "not applicable", which is
// different from an empty string. Integer fields use -1 for "not applicable".
// Serialisations write every field, using JSON null where not applicable.
struct SDRBASE_API MessageRecord
{
    enum Direction {
        DirectionRX,
        DirectionTX
    };

    enum Kind {
        KindMessage,        //!< A message for one or more addresses
        KindEvent,          //!< A state change, e.g. transmission started or finished
        KindDiagnostic      //!< Statistics or decoder diagnostics
    };

    enum Polarity {
        PolarityUnknown,
        PolarityNormal,
        PolarityInverted
    };

    enum Duplicate {
        DuplicateNone,
        DuplicateDup,       //!< Same content seen before
        DuplicateDupPlus    //!< Same message seen before, this copy has fewer errors
    };

    //! How well the message came through, set by its producer (only it knows which of its checks matter)
    enum Quality {
        QualityUnknown,     //!< Not set by the producer (records of before, imported files)
        QualityNone,        //!< Not applicable: nothing to check (a tone, a message without error counts or checks)
        QualityClean,       //!< No bit errors, every check present passed
        QualityCorrected,   //!< Bit errors, all corrected: nothing lost
        QualityDamaged      //!< Something lost: uncorrectable errors, a failed check, an incomplete message
    };

    static const int m_schemaVersion = 1;
    static const char * const m_pipeName;   //!< Name of the message pipe carrying records

    QString m_id;                   //!< Stable id; updates of a partial message reuse it
    qint64 m_timestamp;             //!< Milliseconds since the epoch, UTC, when first seen
    Direction m_direction;
    QString m_protocol;             //!< Lower case protocol name: pocsag, flex, gsc, rds, ...
    QString m_mode;                 //!< Protocol mode, e.g. "1200" or "3200/4"
    Kind m_kind;
    QString m_msgType;              //!< e.g. alphanumeric, numeric, tone
    QString m_address;              //!< Protocol formatted address, null for events
    Polarity m_polarity;
    int m_speed;                    //!< Bits per second, -1 if not applicable
    QString m_text;                 //!< Text of the current interpretation, null if none
    QByteArray m_rawData;           //!< Payload as transmitted, protocol specific packing
    int m_rawBits;                  //!< Number of valid bits in m_rawData, -1 if no payload
    int m_errorsCorrected;          //!< -1 if not applicable
    int m_errorsUncorrectable;      //!< -1 if not applicable
    float m_ber;                    //!< Bit error rate, negative if not applicable
    QString m_charStatus;           //!< One of 'o' (ok), 'c' (corrected), 'b' (bad) per text character, null if unknown
    qint64 m_frequency;             //!< Absolute frequency in Hz, -1 if unknown
    QString m_channel;              //!< Channel title at the time of the message
    int m_deviceSetIndex;           //!< -1 if unknown
    int m_channelIndex;             //!< -1 if unknown
    qint64 m_centerFrequency;       //!< Device centre frequency in Hz, -1 if unknown
    qint64 m_channelOffset;         //!< Channel offset from the centre frequency in Hz
    Duplicate m_duplicate;
    bool m_partial;                 //!< Part of a message, an update with the same id follows
    QVariantMap m_protocolData;     //!< Protocol specific fields
    Quality m_quality;              //!< QualityUnknown if the producer did not set it

    MessageRecord();
    void resetToDefaults();

    //!< Key identifying the same subscriber and message: protocol, address, polarity and,
    //!< when present in m_protocolData, "function" and "msgNumber"
    QString dedupKey() const;
    //!< The addresses the record is for: its address and, for a group message, the addresses of the members of the
    //!< group (protocolData group_capcodes)
    QStringList addresses() const;

    QJsonObject toJson() const;
    //!< One field of toJson() (the same value), undefined for a key it does not have: a field read without the
    //!< whole JSON
    QJsonValue jsonValue(const QString& key) const;
    static bool fromJson(const QJsonObject& json, MessageRecord& record);
    //!< Compact JSON followed by a newline, for JSON lines files and UDP
    QByteArray toJsonLine() const;

    //!< CSV column names, in the order used by toCSV()
    static QStringList csvHeader();
    //!< One CSV line without a trailing newline. protocolData is written as compact JSON
    QString toCSV() const;
    //!< The record of a row read with CSV::readRow, the columns as found by CSV::readHeader (csvHeader() names);
    //!< false if the row has no id, timestamp or protocol
    static bool fromCSV(const QHash<QString, int>& columns, const QStringList& row, MessageRecord& record);

    static QString directionToString(Direction direction);
    static Direction directionFromString(const QString& string);
    static QString kindToString(Kind kind);
    static Kind kindFromString(const QString& string);
    static QString polarityToString(Polarity polarity);
    static Polarity polarityFromString(const QString& string);
    static QString duplicateToString(Duplicate duplicate);
    static Duplicate duplicateFromString(const QString& string);
    //!< none, clean, corrected, damaged; null for unknown
    static QString qualityToString(Quality quality);
    static Quality qualityFromString(const QString& string);
    //!< From the common fields only: uncorrectable errors or a lost character damaged, corrected errors or a corrected
    //!< character corrected, error counts of 0 clean, else none. A producer with checks of its own sets the quality
    //!< itself
    static Quality qualityFromErrors(int errorsCorrected, int errorsUncorrectable, const QString& charStatus);
};

#endif // INCLUDE_UTIL_MESSAGERECORD_H
