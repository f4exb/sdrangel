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
#include <chrono>
#include <cstdio>
#include <cstring>

#include "dvangel61file.h"
#include "dvangel61.h"
#include "dvcrc.h"

// ---- The offer (content type 3) ---------------------------------------------------------------

std::vector<uint8_t> DVAngel61FileOffer::encode() const
{
    std::vector<uint8_t> bytes;
    bytes.push_back((uint8_t) m_transferId);
    bytes.push_back((uint8_t) m_flags);
    bytes.push_back((uint8_t) (m_blockSize >> 8));
    bytes.push_back((uint8_t) m_blockSize);
    bytes.push_back((uint8_t) (m_fileSize >> 24));
    bytes.push_back((uint8_t) (m_fileSize >> 16));
    bytes.push_back((uint8_t) (m_fileSize >> 8));
    bytes.push_back((uint8_t) m_fileSize);
    bytes.push_back((uint8_t) (m_crc >> 24));
    bytes.push_back((uint8_t) (m_crc >> 16));
    bytes.push_back((uint8_t) (m_crc >> 8));
    bytes.push_back((uint8_t) m_crc);
    std::string name = m_name.substr(0, 255);
    bytes.push_back((uint8_t) name.size());
    bytes.insert(bytes.end(), name.begin(), name.end());
    bytes.insert(bytes.end(), m_mediaType.begin(), m_mediaType.end());
    return bytes;
}

bool DVAngel61FileOffer::decode(const std::vector<uint8_t>& bytes, DVAngel61FileOffer& offer)
{
    if (bytes.size() < 13) {
        return false;
    }

    offer.m_transferId = bytes[0];
    offer.m_flags = bytes[1];
    offer.m_blockSize = (bytes[2] << 8) | bytes[3];
    offer.m_fileSize = ((uint32_t) bytes[4] << 24) | ((uint32_t) bytes[5] << 16) | ((uint32_t) bytes[6] << 8) | bytes[7];
    offer.m_crc = ((uint32_t) bytes[8] << 24) | ((uint32_t) bytes[9] << 16) | ((uint32_t) bytes[10] << 8) | bytes[11];
    int nameLength = bytes[12];

    if ((int) bytes.size() < 13 + nameLength) {
        return false;
    }

    offer.m_name.assign(bytes.begin() + 13, bytes.begin() + 13 + nameLength);
    offer.m_mediaType.assign(bytes.begin() + 13 + nameLength, bytes.end());

    // A block size that does not fit a message field, or a file that needs more blocks than the
    // 16 bit index holds, is not a transfer this protocol can carry
    if ((offer.m_blockSize <= 0) || (offer.m_blockSize > DVAngel61FileOffer::MaxBlockSize)
        || (offer.blockCount() > DVAngel61FileOffer::MaxBlocks)) {
        return false;
    }

    return true;
}

// ---- The control message (content type 5) -----------------------------------------------------

std::vector<uint8_t> DVAngel61FileControl::encode() const
{
    std::vector<uint8_t> bytes;
    bytes.push_back((uint8_t) m_kind);
    bytes.push_back((uint8_t) m_transferId);
    bytes.push_back((uint8_t) (m_crc >> 24));
    bytes.push_back((uint8_t) (m_crc >> 16));
    bytes.push_back((uint8_t) (m_crc >> 8));
    bytes.push_back((uint8_t) m_crc);

    if ((m_kind == Reject) || (m_kind == Abort)) {
        bytes.push_back((uint8_t) m_reason);
    }
    else if (m_kind == Missing)
    {
        bytes.push_back((uint8_t) std::min((size_t) 255, m_ranges.size()));

        for (const auto& range : m_ranges)
        {
            bytes.push_back((uint8_t) (range.first >> 8));
            bytes.push_back((uint8_t) range.first);
            bytes.push_back((uint8_t) (range.second >> 8));
            bytes.push_back((uint8_t) range.second);
        }
    }

    return bytes;
}

bool DVAngel61FileControl::decode(const std::vector<uint8_t>& bytes, DVAngel61FileControl& control)
{
    if (bytes.size() < 6) {
        return false;
    }

    control.m_kind = bytes[0];
    control.m_transferId = bytes[1];
    control.m_crc = ((uint32_t) bytes[2] << 24) | ((uint32_t) bytes[3] << 16) | ((uint32_t) bytes[4] << 8) | bytes[5];
    control.m_reason = 0;
    control.m_ranges.clear();

    if ((control.m_kind == Reject) || (control.m_kind == Abort))
    {
        if (bytes.size() < 7) {
            return false;
        }

        control.m_reason = bytes[6];
    }
    else if (control.m_kind == Missing)
    {
        if (bytes.size() < 7) {
            return false;
        }

        int count = bytes[6];

        if ((int) bytes.size() < 7 + 4 * count) {
            return false;
        }

        for (int i = 0; i < count; i++)
        {
            int at = 7 + 4 * i;
            int first = (bytes[at] << 8) | bytes[at + 1];
            int blocks = (bytes[at + 2] << 8) | bytes[at + 3];
            control.m_ranges.push_back(std::make_pair(first, blocks));
        }
    }
    else if ((control.m_kind != Accept) && (control.m_kind != Complete))
    {
        return false;
    }

    return true;
}

// ---- The sender -------------------------------------------------------------------------------

DVAngel61FileSender::DVAngel61FileSender() :
    m_state(Idle),
    m_offerFragment(0),
    m_offerFieldBytes(-1),
    m_framesSinceOffer(0),
    m_offerInterval(OfferIntervalFrames),
    m_rounds(0),
    m_blocksSent(0),
    m_rejectReason(-1),
    m_maxRounds(DefaultMaxRounds)
{
}

bool DVAngel61FileSender::start(const std::string& name, const std::vector<uint8_t>& data, int transferId, int blockSize,
    const std::string& mediaType)
{
    stop();

    if (data.empty() || (blockSize <= 0) || (blockSize > DVAngel61FileOffer::MaxBlockSize)) {
        return false;
    }

    m_offer = DVAngel61FileOffer();
    m_offer.m_transferId = transferId & 0xFF;
    m_offer.m_blockSize = blockSize;
    m_offer.m_fileSize = (uint32_t) data.size();
    m_offer.m_crc = DVCRC::crc32(data.data(), (int) data.size());
    m_offer.m_name = name;
    m_offer.m_mediaType = mediaType;

    if (m_offer.blockCount() > DVAngel61FileOffer::MaxBlocks) {
        return false;
    }

    m_data = data;

    for (int i = 0; i < m_offer.blocks(); i++) {
        m_pending.push_back(i);
    }

    m_offerFragments.clear();
    m_offerFragment = 0;
    m_offerFieldBytes = -1;
    m_framesSinceOffer = m_offerInterval; // the offer leads the transfer
    m_rounds = 0;
    m_blocksSent = 0;
    m_rejectReason = -1;
    m_state = Sending;
    return true;
}

void DVAngel61FileSender::stop()
{
    m_state = Idle;
    m_pending.clear();
    m_offerFragments.clear();
    m_data.clear();
    m_offerFragment = 0;
    m_framesSinceOffer = 0;
}

bool DVAngel61FileSender::buildOffer(int fieldBytes)
{
    if ((m_offerFieldBytes == fieldBytes) && !m_offerFragments.empty()) {
        return true;
    }

    std::vector<uint8_t> bytes = m_offer.encode();

    // Fragmented for the field's size: revision 3's fragmenter, the same fragments as revision 2's
    // for its frame types
    if (DVAngel61::fragmentMessage(bytes, DVAngel61::ContentFileOffer, m_offer.m_transferId, fieldBytes, m_offerFragments) == 0) {
        return false;
    }

    m_offerFieldBytes = fieldBytes;
    return true;
}

bool DVAngel61FileSender::nextFieldBytes(int fieldBytes, DVAngel61::MessageFragment& fragment)
{
    if ((m_state != Sending) || (fieldBytes < m_offer.m_blockSize)) {
        return false;
    }

    // The offer leads the transfer and is repeated as it runs, so that a station that joined
    // late knows what is arriving
    if ((m_offerFragment < (int) m_offerFragments.size()) || (m_framesSinceOffer >= m_offerInterval))
    {
        if (!buildOffer(fieldBytes)) {
            m_state = Failed;
            return false;
        }

        if (m_offerFragment >= (int) m_offerFragments.size())
        {
            m_offerFragment = 0;
            m_framesSinceOffer = 0;
        }

        fragment = m_offerFragments[m_offerFragment++];

        if (m_offerFragment >= (int) m_offerFragments.size()) {
            m_framesSinceOffer = 0;
        }

        return true;
    }

    if (m_pending.empty())
    {
        m_state = m_group ? Done : Waiting;
        return false;
    }

    int index = m_pending.front();
    m_pending.pop_front();
    int start = index * m_offer.m_blockSize;
    int length = std::min(m_offer.m_blockSize, (int) m_data.size() - start);
    fragment = DVAngel61::MessageFragment();
    fragment.m_contentType = DVAngel61::ContentFileData;
    fragment.m_id = m_offer.m_transferId;
    fragment.setBlockIndex(index);
    fragment.m_payload.assign(m_data.begin() + start, m_data.begin() + start + length);
    fragment.m_payload.resize(m_offer.m_blockSize, 0);
    m_blocksSent++;
    m_framesSinceOffer++;

    // Sent: a receiver answers what it missed; a group does not, so the transfer is over
    if (m_pending.empty()) {
        m_state = m_group ? Done : Waiting;
    }

    return true;
}

void DVAngel61FileSender::handleControl(const DVAngel61FileControl& control)
{
    if ((m_state == Idle) || (control.m_transferId != m_offer.m_transferId) || (control.m_crc != m_offer.m_crc)) {
        return;
    }

    switch (control.m_kind)
    {
    case DVAngel61FileControl::Complete:
        m_state = Done;
        break;

    case DVAngel61FileControl::Reject:
    case DVAngel61FileControl::Abort:
        m_rejectReason = control.m_reason;
        m_state = Failed;
        break;

    case DVAngel61FileControl::Missing:
    {
        if (++m_rounds > m_maxRounds)
        {
            m_rejectReason = DVAngel61FileControl::ReasonTimeout;
            m_state = Failed;
            break;
        }

        m_pending.clear();
        int blocks = m_offer.blocks();

        for (const auto& range : control.m_ranges)
        {
            for (int i = 0; i < range.second; i++)
            {
                int index = range.first + i;

                if ((index >= 0) && (index < blocks)) {
                    m_pending.push_back(index);
                }
            }
        }

        if (m_pending.empty())
        {
            m_state = Waiting; // nothing was asked for: the receiver will say complete or ask again
        }
        else
        {
            m_offerFragment = 0;
            m_framesSinceOffer = m_offerInterval;
            m_state = Sending;
        }

        break;
    }

    default:
        break;
    }
}

bool DVAngel61FileSender::handleNoAnswer()
{
    if ((m_state != Waiting) || m_group) {
        return false;
    }

    if (++m_rounds > m_maxRounds)
    {
        m_rejectReason = DVAngel61FileControl::ReasonTimeout;
        m_state = Failed;
        return false;
    }

    // The offer alone: no blocks are pending, so the transmission is the offer and it waits again
    m_offerFragment = 0;
    m_framesSinceOffer = m_offerInterval;
    m_state = Sending;
    return true;
}

// ---- The receiver -----------------------------------------------------------------------------

DVAngel61FileReceiver::DVAngel61FileReceiver() :
    m_state(Idle),
    m_hasOffer(false),
    m_source(0),
    m_blocksReceived(0),
    m_maxFileSize(0),
    m_rejectReason(-1),
    m_controlPending(false),
    m_group(false),
    m_earlyTransferId(-1)
{
}

void DVAngel61FileReceiver::reset()
{
    m_state = Idle;
    m_hasOffer = false;
    m_source = 0;
    m_offer = DVAngel61FileOffer();
    m_data.clear();
    m_received.clear();
    m_blocksReceived = 0;
    m_rejectReason = -1;
    m_controlPending = false;
    m_group = false;
    m_earlyTransferId = -1;
    m_early.clear();
    m_confirmed = true;
    m_blocksResumed = 0;
}

std::string DVAngel61FileReceiver::sanitiseName(const std::string& name)
{
    std::string out;

    for (char c : name)
    {
        unsigned char u = (unsigned char) c;

        if ((u < 0x20) || (c == '/') || (c == '\\') || (c == ':') || (c == '*') || (c == '?')
            || (c == '"') || (c == '<') || (c == '>') || (c == '|')) {
            out += '_';
        } else {
            out += c;
        }
    }

    // Nothing that looks like a path or a hidden file is left at the front ("..", ".config")
    while (!out.empty() && ((out.front() == '.') || (out.front() == '_') || (out.front() == ' '))) {
        out.erase(out.begin());
    }

    while (!out.empty() && (out.back() == ' ')) {
        out.pop_back();
    }

    if (out.empty()) {
        out = "angel61.bin";
    }

    return out.substr(0, 128);
}

bool DVAngel61FileReceiver::setOffer(uint64_t source, const std::vector<uint8_t>& bytes, bool group)
{
    DVAngel61FileOffer offer;

    if (!DVAngel61FileOffer::decode(bytes, offer)) {
        return false;
    }

    // The same offer again is the transmitter saying the transfer is still running: nothing
    // changes, but there is something to answer at the end of this transmission
    if (m_hasOffer && (m_source == source) && (m_offer.m_transferId == offer.m_transferId)
        && (m_offer.m_crc == offer.m_crc) && (m_offer.m_fileSize == offer.m_fileSize))
    {
        if (m_state == Receiving) {
            m_controlPending = true;
        }

        return false;
    }

    // A transfer that was completed or refused is not started again by a repeated offer
    if ((m_state == Complete) || (m_state == Rejected))
    {
        if (m_hasOffer && (m_offer.m_crc == offer.m_crc) && (m_offer.m_fileSize == offer.m_fileSize)) {
            return false;
        }
    }

    // Another transfer replaces this one: what arrived of it is kept for a resume
    keepPartial();

    std::vector<std::pair<int, std::vector<uint8_t>>> early;
    int earlyTransferId = m_earlyTransferId;
    early.swap(m_early);
    reset();
    m_source = source;
    m_offer = offer;
    m_hasOffer = true;
    m_group = group;
    m_state = Receiving;
    m_confirmed = !m_confirmRequired;

    if (((m_maxFileSize > 0) && (offer.m_fileSize > m_maxFileSize))
        || (offer.blockCount() > DVAngel61FileOffer::MaxBlocks))
    {
        // The block count is checked even when the operator set no limit of their own: the
        // 16 bit block index cannot address more, and the allocation below is the file's size
        reject(DVAngel61FileControl::ReasonTooLarge);
        return true;
    }

    m_data.assign(offer.m_fileSize, 0);
    m_received.assign(offer.blocks(), false);
    m_controlPending = true;    // even if no block arrives, the answer says which ones are missing

    // What a kept partial holds of the same file (3.4: a resume), then the blocks that arrived
    // before the offer, which belong to it when the transfer id matches
    if (m_partials)
    {
        if (const DVAngel61FilePartial *partial = m_partials->find(source, offer.m_fileSize, offer.m_crc)) {
            resume(*partial);
        }
    }

    if ((earlyTransferId == offer.m_transferId) && (m_state == Receiving))
    {
        for (const auto& block : early) {
            addBlock(source, offer.m_transferId, block.first, block.second.data(), (int) block.second.size());
        }
    }

    return true;
}

void DVAngel61FileReceiver::resume(const DVAngel61FilePartial& partial)
{
    if (partial.m_data.size() != m_data.size()) {
        return;
    }

    for (int index = 0; index < (int) m_received.size(); index++)
    {
        const uint32_t start = (uint32_t) index * (uint32_t) m_offer.m_blockSize;
        const uint32_t length = std::min((uint32_t) m_offer.m_blockSize, (uint32_t) m_data.size() - start);

        if (!m_received[index] && partial.covers(start, length))
        {
            std::memcpy(m_data.data() + start, partial.m_data.data() + start, length);
            m_received[index] = true;
            m_blocksReceived++;
            m_blocksResumed++;
        }
    }

    if (m_blocksReceived == (int) m_received.size()) {
        finish();
    }
}

void DVAngel61FileReceiver::finish()
{
    // Every block is in: the CRC-32 of the offer says whether the file is the one that was sent
    if (DVCRC::crc32(m_data.data(), (int) m_data.size()) == m_offer.m_crc)
    {
        m_state = Complete;

        if (m_partials) {
            m_partials->remove(m_source, m_offer.m_fileSize, m_offer.m_crc);
        }
    }
    else
    {
        // (and a partial that held a wrong block is not offered again)
        if (m_partials) {
            m_partials->remove(m_source, m_offer.m_fileSize, m_offer.m_crc);
        }

        reject(DVAngel61FileControl::ReasonCrc);
    }
}

void DVAngel61FileReceiver::accept()
{
    if (!m_hasOffer || m_confirmed) {
        return;
    }

    m_confirmed = true;

    if ((m_state == Receiving) || (m_state == Complete)) {
        m_controlPending = true;
    }
}

void DVAngel61FileReceiver::keepPartial()
{
    if (!m_partials || !m_hasOffer || !m_confirmed || (m_state != Receiving) || (m_blocksReceived == 0)) {
        return;
    }

    DVAngel61FilePartial partial;
    partial.m_source = m_source;
    partial.m_name = m_offer.m_name;
    partial.m_fileSize = m_offer.m_fileSize;
    partial.m_crc = m_offer.m_crc;
    partial.m_savedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    partial.m_data = m_data;

    // What an earlier partial of the same file held that this transfer's blocks did not (its
    // block size may have been another: its ranges are not whole blocks of this one's)
    const DVAngel61FilePartial *earlier = m_partials->find(m_source, m_offer.m_fileSize, m_offer.m_crc);
    std::vector<bool> held(m_data.size(), false);

    for (int index = 0; index < (int) m_received.size(); index++)
    {
        if (m_received[index])
        {
            const uint32_t start = (uint32_t) index * (uint32_t) m_offer.m_blockSize;
            const uint32_t length = std::min((uint32_t) m_offer.m_blockSize, (uint32_t) m_data.size() - start);
            std::fill(held.begin() + start, held.begin() + start + length, true);
        }
    }

    if (earlier && (earlier->m_data.size() == m_data.size()))
    {
        for (const auto& range : earlier->m_ranges)
        {
            for (uint32_t i = range.first; i < range.first + range.second; i++)
            {
                if (!held[i])
                {
                    held[i] = true;
                    partial.m_data[i] = earlier->m_data[i];
                }
            }
        }
    }

    for (uint32_t i = 0; i < (uint32_t) held.size(); )
    {
        if (!held[i]) {
            i++;
            continue;
        }

        uint32_t first = i;

        while ((i < (uint32_t) held.size()) && held[i]) {
            i++;
        }

        partial.m_ranges.push_back(std::make_pair(first, i - first));
    }

    m_partials->put(partial);
}

void DVAngel61FileReceiver::noteOfferFragment(int transferId)
{
    if (m_hasOffer && (m_state == Receiving) && (transferId == m_offer.m_transferId)) {
        m_controlPending = true;
    }
}

bool DVAngel61FileReceiver::addBlock(uint64_t source, int transferId, int blockIndex, const uint8_t *data, int nbBytes)
{
    if (!m_hasOffer || (m_state != Receiving))
    {
        // Keep what arrives before the offer does, so that a late entrant loses nothing
        if ((m_state == Idle) && (nbBytes > 0))
        {
            if (m_earlyTransferId != transferId)
            {
                m_earlyTransferId = transferId;
                m_early.clear();
            }

            if (m_early.size() < 256) {
                m_early.push_back(std::make_pair(blockIndex, std::vector<uint8_t>(data, data + nbBytes)));
            }
        }

        return false;
    }

    if ((source != m_source) || (transferId != m_offer.m_transferId)
        || (blockIndex < 0) || (blockIndex >= (int) m_received.size()) || m_received[blockIndex]) {
        return false;
    }

    int start = blockIndex * m_offer.m_blockSize;
    int length = std::min(m_offer.m_blockSize, (int) m_data.size() - start);

    if ((length <= 0) || (nbBytes < length)) {
        return false;
    }

    std::memcpy(m_data.data() + start, data, length);
    m_received[blockIndex] = true;
    m_blocksReceived++;

    if (m_blocksReceived == (int) m_received.size()) {
        finish();
    }

    m_controlPending = true;
    return true;
}

void DVAngel61FileReceiver::reject(int reason)
{
    m_rejectReason = reason;
    m_state = Rejected;
    m_controlPending = true;
}

std::vector<std::pair<int, int>> DVAngel61FileReceiver::missingRanges(int maxRanges) const
{
    std::vector<std::pair<int, int>> ranges;
    int index = 0;
    int blocks = (int) m_received.size();

    while ((index < blocks) && ((int) ranges.size() < maxRanges))
    {
        while ((index < blocks) && m_received[index]) {
            index++;
        }

        if (index >= blocks) {
            break;
        }

        int first = index;

        while ((index < blocks) && !m_received[index]) {
            index++;
        }

        ranges.push_back(std::make_pair(first, index - first));
    }

    return ranges;
}

bool DVAngel61FileReceiver::nextControl(DVAngel61FileControl& control)
{
    // A transfer to a group has no control loop (3.4): nobody answers it, whatever arrived
    if (m_group) {
        m_controlPending = false;
    }

    if (!m_hasOffer || !m_controlPending) {
        return false;
    }

    // Awaiting the operator: no answer yet (a refusal of the station's own, a limit, still goes)
    if (!m_confirmed && (m_state != Rejected)) {
        return false;
    }

    control = DVAngel61FileControl();
    control.m_transferId = m_offer.m_transferId;
    control.m_crc = m_offer.m_crc;

    if (m_state == Rejected)
    {
        control.m_kind = DVAngel61FileControl::Reject;
        control.m_reason = m_rejectReason;
    }
    else if (m_state == Complete)
    {
        control.m_kind = DVAngel61FileControl::Complete;
    }
    else
    {
        control.m_kind = DVAngel61FileControl::Missing;
        control.m_ranges = missingRanges(MaxRangesPerControl);

        if (control.m_ranges.empty()) {
            return false;
        }
    }

    m_controlPending = false;
    return true;
}

float DVAngel61FileReceiver::getProgress() const
{
    int blocks = (int) m_received.size();
    return blocks > 0 ? (float) m_blocksReceived / blocks : 0.0f;
}

// ---- Unfinished transfers ---------------------------------------------------------------------

bool DVAngel61FilePartial::covers(uint32_t first, uint32_t count) const
{
    for (const auto& range : m_ranges)
    {
        if ((range.first <= first) && ((uint64_t) first + count <= (uint64_t) range.first + range.second)) {
            return true;
        }
    }

    return count == 0;
}

uint32_t DVAngel61FilePartial::bytesHeld() const
{
    uint32_t bytes = 0;

    for (const auto& range : m_ranges) {
        bytes += range.second;
    }

    return bytes;
}

std::string DVAngel61FilePartial::keyOf(uint64_t source, uint32_t fileSize, uint32_t crc)
{
    char key[48];
    std::snprintf(key, sizeof(key), "%016llx-%08x-%08x", (unsigned long long) source, (unsigned) crc, (unsigned) fileSize);
    return std::string(key);
}

static void putLE(std::vector<uint8_t>& out, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++) {
        out.push_back((uint8_t) (value >> (8 * i)));
    }
}

static bool getLE(const std::vector<uint8_t>& in, size_t& at, int bytes, uint64_t& value)
{
    if (at + bytes > in.size()) {
        return false;
    }

    value = 0;

    for (int i = 0; i < bytes; i++) {
        value |= (uint64_t) in[at + i] << (8 * i);
    }

    at += bytes;
    return true;
}

std::vector<uint8_t> DVAngel61FilePartial::encode() const
{
    std::vector<uint8_t> out = { 'A', '6', '1', 'P', 1 };
    putLE(out, m_source, 8);
    putLE(out, m_fileSize, 4);
    putLE(out, m_crc, 4);
    putLE(out, (uint64_t) m_savedSec, 8);
    const std::string name = m_name.substr(0, 255);
    putLE(out, name.size(), 2);
    out.insert(out.end(), name.begin(), name.end());
    putLE(out, m_ranges.size(), 4);

    for (const auto& range : m_ranges)
    {
        putLE(out, range.first, 4);
        putLE(out, range.second, 4);
    }

    for (const auto& range : m_ranges) {
        out.insert(out.end(), m_data.begin() + range.first, m_data.begin() + range.first + range.second);
    }

    return out;
}

bool DVAngel61FilePartial::decode(const std::vector<uint8_t>& bytes, DVAngel61FilePartial& partial)
{
    if ((bytes.size() < 5) || (bytes[0] != 'A') || (bytes[1] != '6') || (bytes[2] != '1') || (bytes[3] != 'P') || (bytes[4] != 1)) {
        return false;
    }

    size_t at = 5;
    uint64_t source, size, crc, saved, nameLength, count;

    if (!getLE(bytes, at, 8, source) || !getLE(bytes, at, 4, size) || !getLE(bytes, at, 4, crc)
        || !getLE(bytes, at, 8, saved) || !getLE(bytes, at, 2, nameLength) || (at + nameLength > bytes.size())) {
        return false;
    }

    // (a file off the disk: a size past what an offer may carry, or ranges outside the file or out
    // of order, and it is not one of ours)
    if ((size == 0) || (size > (uint64_t) DVAngel61FileOffer::MaxBlocks * DVAngel61FileOffer::MaxBlockSize)) {
        return false;
    }

    DVAngel61FilePartial p;
    p.m_source = source;
    p.m_fileSize = (uint32_t) size;
    p.m_crc = (uint32_t) crc;
    p.m_savedSec = (int64_t) saved;
    p.m_name.assign(bytes.begin() + at, bytes.begin() + at + nameLength);
    at += nameLength;

    if (!getLE(bytes, at, 4, count) || (count > size)) {
        return false;
    }

    uint64_t end = 0, total = 0;

    for (uint64_t i = 0; i < count; i++)
    {
        uint64_t first, length;

        if (!getLE(bytes, at, 4, first) || !getLE(bytes, at, 4, length) || (length == 0)
            || (first < end) || (first + length > size)) {
            return false;
        }

        p.m_ranges.push_back(std::make_pair((uint32_t) first, (uint32_t) length));
        end = first + length;
        total += length;
    }

    if (bytes.size() - at != total) {
        return false;
    }

    p.m_data.assign(size, 0);

    for (const auto& range : p.m_ranges)
    {
        std::copy(bytes.begin() + at, bytes.begin() + at + range.second, p.m_data.begin() + range.first);
        at += range.second;
    }

    partial = p;
    return true;
}

const DVAngel61FilePartial *DVAngel61FilePartials::find(uint64_t source, uint32_t fileSize, uint32_t crc) const
{
    return get(DVAngel61FilePartial::keyOf(source, fileSize, crc));
}

const DVAngel61FilePartial *DVAngel61FilePartials::get(const std::string& key) const
{
    auto it = m_partials.find(key);
    return it == m_partials.end() ? nullptr : &it->second;
}

void DVAngel61FilePartials::insert(const DVAngel61FilePartial& partial, bool changed)
{
    const std::string key = partial.key();

    if ((m_partials.find(key) == m_partials.end()) && ((int) m_partials.size() >= MaxPartials))
    {
        auto oldest = m_partials.begin();

        for (auto it = m_partials.begin(); it != m_partials.end(); ++it) {
            if (it->second.m_savedSec < oldest->second.m_savedSec) { oldest = it; }
        }

        m_removed.insert(oldest->first);
        m_changed.erase(oldest->first);
        m_partials.erase(oldest);
    }

    m_partials[key] = partial;
    m_removed.erase(key);

    if (changed) {
        m_changed.insert(key);
    }
}

void DVAngel61FilePartials::put(const DVAngel61FilePartial& partial)
{
    insert(partial, true);
}

void DVAngel61FilePartials::load(const DVAngel61FilePartial& partial)
{
    insert(partial, false);
}

void DVAngel61FilePartials::remove(uint64_t source, uint32_t fileSize, uint32_t crc)
{
    const std::string key = DVAngel61FilePartial::keyOf(source, fileSize, crc);

    if (m_partials.erase(key) > 0)
    {
        m_changed.erase(key);
        m_removed.insert(key);
    }
}

void DVAngel61FilePartials::expire(int64_t nowSec)
{
    for (auto it = m_partials.begin(); it != m_partials.end(); )
    {
        if (nowSec - it->second.m_savedSec > MaxAgeSec)
        {
            m_changed.erase(it->first);
            m_removed.insert(it->first);
            it = m_partials.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<std::string> DVAngel61FilePartials::takeChanged()
{
    std::vector<std::string> keys(m_changed.begin(), m_changed.end());
    m_changed.clear();
    return keys;
}

std::vector<std::string> DVAngel61FilePartials::takeRemoved()
{
    std::vector<std::string> keys(m_removed.begin(), m_removed.end());
    m_removed.clear();
    return keys;
}
