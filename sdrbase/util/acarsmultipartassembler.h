///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ACARSMULTIPARTASSEMBLER_H
#define INCLUDE_ACARSMULTIPARTASSEMBLER_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

#include "export.h"

// ARINC 618 gives downlinks an explicit MSN block sequence, but deliberately gives
// uplinks no block sequence. Keep the two assembly rules in one place so the GUI never
// has to infer message identity from table row order or from the retransmission IDs.
class SDRBASE_API AcarsMultipartAssembler
{
public:
    enum Status
    {
        InProgress,
        Complete,
        Incomplete,
        Ambiguous
    };

    struct SDRBASE_API Part
    {
        // An opaque identity for the message this part came from, NOT a pointer to it.
        // It used to be the AcarsMessage*, which forced the worker to keep every decoded
        // message alive for the life of the channel purely so the addresses stayed
        // unique - an unbounded leak, and one that headless operation paid for too.
        quint64 m_sourceId = 0;
        QDateTime m_received;
        bool m_uplink = false;
        QString m_address;
        QString m_label;
        QString m_subLabel;
        QChar m_blockId;
        QString m_originator;
        QString m_messageNumber;
        QChar m_blockSequence;
        QString m_text;
        bool m_more = false;
        bool m_duplicate = false;
        bool m_conflict = false;
    };

    struct SDRBASE_API Assembly
    {
        quint64 m_id = 0;
        bool m_uplink = false;
        Status m_status = InProgress;
        QList<Part> m_parts;
        QDateTime m_firstReceived;
        QDateTime m_lastReceived;
        QString m_combinedText;
        QStringList m_missingSequences;
        QString m_decodedText;
        QString m_libAcarsDecodedText;
        int m_finalSequence = -1;
        int m_uniquePartCount = 0;
        int m_duplicateCount = 0;

        int partNumber(quint64 sourceId) const;
        bool isMultipart() const;
    };

    struct SDRBASE_API Result
    {
        QSharedPointer<Assembly> m_assembly;
        int m_partNumber = 0;
        bool m_duplicate = false;
    };

    explicit AcarsMultipartAssembler(int timeoutSeconds = 90);

    Result add(const Part& part);
    QSharedPointer<const Assembly> assemblyFor(quint64 sourceId) const;
    void clear();

    static QString statusText(Status status);

private:
    struct SDRBASE_API UplinkReference
    {
        Part m_part;
        QSharedPointer<Assembly> m_assembly;
    };

    int m_timeoutSeconds;
    quint64 m_nextId;
    QHash<QString, QSharedPointer<Assembly>> m_downlinks;
    QHash<QString, QSharedPointer<Assembly>> m_uplinks;
    QHash<QString, UplinkReference> m_uplinkReferences;
    QHash<quint64, QSharedPointer<Assembly>> m_sourceAssemblies;

    static QString downlinkKey(const Part& part);
    static QString uplinkKey(const Part& part);
    static bool validMessageNumber(const QString& number);
    static int sequenceIndex(QChar sequence);
    static bool sameBlock(const Part& lhs, const Part& rhs);

    QSharedPointer<Assembly> createAssembly(const Part& part);
    Result addDownlink(Part part);
    Result addUplink(Part part);
    void appendPart(const QSharedPointer<Assembly>& assembly, const Part& part);
    void refreshDownlink(const QSharedPointer<Assembly>& assembly);
    void refreshUplink(const QSharedPointer<Assembly>& assembly);
    void expire(const QDateTime& received);
};

#endif // INCLUDE_ACARSMULTIPARTASSEMBLER_H
