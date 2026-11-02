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

#include "util/acarsmultipartassembler.h"

#include <QMap>

namespace {

const QChar KEY_SEPARATOR(0x1f);
const int MAX_MESSAGE_BLOCKS = 16;

}

int AcarsMultipartAssembler::Assembly::partNumber(quint64 sourceId) const
{
    int number = 0;

    for (const Part& part : m_parts)
    {
        if (!m_uplink && (part.m_sourceId == sourceId))
        {
            const ushort sequence = part.m_blockSequence.unicode();
            return ((sequence >= 'A') && (sequence <= 'Z'))
                ? sequence - 'A' + 1
                : 0;
        }

        if (!part.m_duplicate) {
            ++number;
        }
        if (part.m_sourceId == sourceId) {
            return number;
        }
    }

    return 0;
}

bool AcarsMultipartAssembler::Assembly::isMultipart() const
{
    return (m_uniquePartCount > 1)
        || (m_duplicateCount > 0)
        || (m_status != Complete);
}

AcarsMultipartAssembler::AcarsMultipartAssembler(int timeoutSeconds) :
    m_timeoutSeconds(timeoutSeconds),
    m_nextId(1)
{
}

AcarsMultipartAssembler::Result AcarsMultipartAssembler::add(const Part& part)
{
    expire(part.m_received);
    return part.m_uplink ? addUplink(part) : addDownlink(part);
}

QSharedPointer<const AcarsMultipartAssembler::Assembly>
AcarsMultipartAssembler::assemblyFor(quint64 sourceId) const
{
    return m_sourceAssemblies.value(sourceId);
}

void AcarsMultipartAssembler::clear()
{
    m_downlinks.clear();
    m_uplinks.clear();
    m_uplinkReferences.clear();
    m_sourceAssemblies.clear();
    m_nextId = 1;
}

QString AcarsMultipartAssembler::statusText(Status status)
{
    switch (status)
    {
    case InProgress:
        return "In progress";
    case Complete:
        return "Complete";
    case Incomplete:
        return "Incomplete";
    case Ambiguous:
        return "Ambiguous";
    }

    return QString();
}

QString AcarsMultipartAssembler::downlinkKey(const Part& part)
{
    return part.m_address + KEY_SEPARATOR
        + part.m_originator + KEY_SEPARATOR
        + part.m_messageNumber;
}

QString AcarsMultipartAssembler::uplinkKey(const Part& part)
{
    return part.m_address + KEY_SEPARATOR
        + part.m_label + KEY_SEPARATOR
        + part.m_subLabel;
}

bool AcarsMultipartAssembler::validMessageNumber(const QString& number)
{
    return (number.size() == 2)
        && number[0].isDigit()
        && number[1].isDigit();
}

int AcarsMultipartAssembler::sequenceIndex(QChar sequence)
{
    const ushort value = sequence.unicode();
    return ((value >= 'A') && (value <= 'Z')) ? value - 'A' : -1;
}

bool AcarsMultipartAssembler::sameBlock(const Part& lhs, const Part& rhs)
{
    return (lhs.m_blockId == rhs.m_blockId)
        && (lhs.m_text == rhs.m_text)
        && (lhs.m_label == rhs.m_label)
        && (lhs.m_subLabel == rhs.m_subLabel)
        && (lhs.m_originator == rhs.m_originator)
        && (lhs.m_messageNumber == rhs.m_messageNumber)
        && (lhs.m_blockSequence == rhs.m_blockSequence)
        && (lhs.m_more == rhs.m_more);
}

QSharedPointer<AcarsMultipartAssembler::Assembly>
AcarsMultipartAssembler::createAssembly(const Part& part)
{
    QSharedPointer<Assembly> assembly(new Assembly);
    assembly->m_id = m_nextId++;
    assembly->m_uplink = part.m_uplink;
    assembly->m_firstReceived = part.m_received;
    assembly->m_lastReceived = part.m_received;
    return assembly;
}

AcarsMultipartAssembler::Result AcarsMultipartAssembler::addDownlink(Part part)
{
    const int sequence = sequenceIndex(part.m_blockSequence);
    const bool sequenced = validMessageNumber(part.m_messageNumber)
        && !part.m_originator.isEmpty()
        && (sequence >= 0);

    // A legal no-text block has no MSN. Treat other unsequenced blocks as standalone but
    // make an ETB block ambiguous because there is no safe way to attach its successor.
    if (!sequenced)
    {
        QSharedPointer<Assembly> assembly = createAssembly(part);
        appendPart(assembly, part);
        assembly->m_combinedText = part.m_text;
        assembly->m_uniquePartCount = 1;
        assembly->m_status = part.m_more ? Ambiguous : Complete;
        return {assembly, 1, false};
    }

    const QString key = downlinkKey(part);
    QSharedPointer<Assembly> assembly = m_downlinks.value(key);

    if (assembly)
    {
        const Part *firstPart = nullptr;
        for (const Part& existing : assembly->m_parts)
        {
            if (!existing.m_duplicate)
            {
                firstPart = &existing;
                break;
            }
        }
        if (firstPart
            && ((firstPart->m_label != part.m_label)
                || (firstPart->m_subLabel != part.m_subLabel)))
        {
            part.m_conflict = true;
        }

        for (const Part& existing : assembly->m_parts)
        {
            if ((existing.m_blockSequence == part.m_blockSequence) && sameBlock(existing, part))
            {
                part.m_duplicate = true;
                appendPart(assembly, part);
                refreshDownlink(assembly);
                return {assembly, assembly->partNumber(part.m_sourceId), true};
            }
        }

        // A new A with a different DBI/payload is a restart, not the successor of A.
        if (sequence == 0)
        {
            if (assembly->m_status == InProgress) {
                assembly->m_status = Incomplete;
            }
            assembly = QSharedPointer<Assembly>();
        }
    }

    if (!assembly)
    {
        assembly = createAssembly(part);
        m_downlinks.insert(key, assembly);
    }
    else
    {
        for (const Part& existing : assembly->m_parts)
        {
            if (!existing.m_duplicate && (existing.m_blockSequence == part.m_blockSequence))
            {
                part.m_conflict = true;
                break;
            }
        }
    }

    appendPart(assembly, part);
    refreshDownlink(assembly);
    return {assembly, assembly->partNumber(part.m_sourceId), part.m_duplicate};
}

AcarsMultipartAssembler::Result AcarsMultipartAssembler::addUplink(Part part)
{
    const QString referenceKey = part.m_address;
    const auto referenceIt = m_uplinkReferences.constFind(referenceKey);
    const bool reusedUbi = (referenceIt != m_uplinkReferences.constEnd())
        && (referenceIt->m_part.m_blockId == part.m_blockId);

    if (reusedUbi
        && sameBlock(referenceIt->m_part, part))
    {
        part.m_duplicate = true;
        appendPart(referenceIt->m_assembly, part);
        refreshUplink(referenceIt->m_assembly);
        return {
            referenceIt->m_assembly,
            referenceIt->m_assembly->partNumber(part.m_sourceId),
            true
        };
    }

    const QString key = uplinkKey(part);
    QSharedPointer<Assembly> assembly = m_uplinks.value(key);

    if (reusedUbi) {
        part.m_conflict = true;
    }

    if (!assembly)
    {
        assembly = createAssembly(part);
        if (part.m_more) {
            m_uplinks.insert(key, assembly);
        }
    }

    appendPart(assembly, part);
    refreshUplink(assembly);

    if (!part.m_more) {
        m_uplinks.remove(key);
    }

    UplinkReference reference;
    reference.m_part = part;
    reference.m_assembly = assembly;
    m_uplinkReferences.insert(referenceKey, reference);

    return {assembly, assembly->partNumber(part.m_sourceId), false};
}

void AcarsMultipartAssembler::appendPart(
    const QSharedPointer<Assembly>& assembly,
    const Part& part)
{
    assembly->m_parts.append(part);
    assembly->m_lastReceived = part.m_received;
    m_sourceAssemblies.insert(part.m_sourceId, assembly);
}

void AcarsMultipartAssembler::refreshDownlink(const QSharedPointer<Assembly>& assembly)
{
    QMap<int, const Part *> ordered;
    bool conflict = false;
    bool ended = false;
    int finalSequence = assembly->m_finalSequence;
    int duplicates = 0;

    for (const Part& part : assembly->m_parts)
    {
        if (part.m_duplicate)
        {
            ++duplicates;
            continue;
        }

        const int sequence = sequenceIndex(part.m_blockSequence);
        conflict = conflict || part.m_conflict || (sequence < 0);

        if ((sequence >= 0) && !ordered.contains(sequence)) {
            ordered.insert(sequence, &part);
        }

        if (!part.m_more)
        {
            ended = true;
            finalSequence = sequence;
        }
    }

    assembly->m_combinedText.clear();
    for (const Part *part : ordered) {
        assembly->m_combinedText.append(part->m_text);
    }

    assembly->m_missingSequences.clear();
    const int lastExpected = ended ? finalSequence : (ordered.isEmpty() ? -1 : ordered.lastKey());
    for (int sequence = 0; sequence <= lastExpected; ++sequence)
    {
        if (!ordered.contains(sequence)) {
            assembly->m_missingSequences.append(QString(QChar('A' + sequence)));
        }
    }

    assembly->m_finalSequence = finalSequence;
    assembly->m_uniquePartCount = ordered.size();
    assembly->m_duplicateCount = duplicates;

    if (conflict
        || (ordered.size() > MAX_MESSAGE_BLOCKS)
        || (!ordered.isEmpty() && (ordered.lastKey() >= MAX_MESSAGE_BLOCKS)))
    {
        assembly->m_status = Ambiguous;
    } else if (ended) {
        assembly->m_status = assembly->m_missingSequences.isEmpty() ? Complete : Incomplete;
    } else {
        assembly->m_status = InProgress;
    }
}

void AcarsMultipartAssembler::refreshUplink(const QSharedPointer<Assembly>& assembly)
{
    bool conflict = false;
    bool ended = false;
    int uniqueParts = 0;
    int duplicates = 0;

    assembly->m_combinedText.clear();

    for (const Part& part : assembly->m_parts)
    {
        if (part.m_duplicate)
        {
            ++duplicates;
            continue;
        }

        ++uniqueParts;
        assembly->m_combinedText.append(part.m_text);
        conflict = conflict || part.m_conflict;
        ended = ended || !part.m_more;
    }

    assembly->m_uniquePartCount = uniqueParts;
    assembly->m_duplicateCount = duplicates;

    if (conflict || (uniqueParts > MAX_MESSAGE_BLOCKS)) {
        assembly->m_status = Ambiguous;
    } else {
        assembly->m_status = ended ? Complete : InProgress;
    }
}

void AcarsMultipartAssembler::expire(const QDateTime& received)
{
    auto expireAssemblies = [this, &received](QHash<QString, QSharedPointer<Assembly>>& active)
    {
        auto it = active.begin();
        while (it != active.end())
        {
            const QSharedPointer<Assembly>& assembly = it.value();
            if (assembly->m_lastReceived.secsTo(received) > m_timeoutSeconds)
            {
                if (assembly->m_status == InProgress) {
                    assembly->m_status = Incomplete;
                }
                it = active.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    expireAssemblies(m_downlinks);
    expireAssemblies(m_uplinks);

    // ... and the source-to-assembly lookup with them. It was never pruned at all, so it
    // grew by one entry per message received for the life of the channel - the same leak
    // the retained AcarsMessage list was, one indirection further along.
    auto sourceIt = m_sourceAssemblies.begin();
    while (sourceIt != m_sourceAssemblies.end())
    {
        if (sourceIt.value()->m_lastReceived.secsTo(received) > m_timeoutSeconds) {
            sourceIt = m_sourceAssemblies.erase(sourceIt);
        } else {
            ++sourceIt;
        }
    }

    auto reference = m_uplinkReferences.begin();
    while (reference != m_uplinkReferences.end())
    {
        if (reference->m_part.m_received.secsTo(received) > m_timeoutSeconds) {
            reference = m_uplinkReferences.erase(reference);
        } else {
            ++reference;
        }
    }
}
