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

#include "radiopagingpocsagdecoder.h"

#include <algorithm>
#include <cmath>

#include "radiopagingbch.h"
#include "radiopagingpocsag.h"

const int RadioPagingPocsagDecoder::m_maxSyncErrors;
const int RadioPagingPocsagDecoder::m_maxMessageBits;
const int RadioPagingPocsagDecoder::m_maxSoftSyncErrors;
const int RadioPagingPocsagDecoder::m_maxIdleErrors;
constexpr float RadioPagingPocsagDecoder::m_softSyncCorrelation;

namespace {

int popcount32(uint32_t x)
{
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0f0f0f0fu;
    return (int) ((x * 0x01010101u) >> 24);
}

} // namespace

RadioPagingPocsagDecoder::RadioPagingPocsagDecoder(Polarity polarity) :
    m_polarity(polarity),
    m_batchWords{},
    m_batchQuality{},
    m_wordError2(0.0f),
    m_wordQualityKnown(false),
    m_wordNoiseQuality(1e9f),
    m_softBits{},
    m_softPos(0),
    m_softKnown(false),
    m_preambleBefore(false)
{
    reset();
}

void RadioPagingPocsagDecoder::reset()
{
    m_state = Hunting;
    m_reg = 0;
    m_regBits = 0;
    m_word = 0;
    m_wordBits = 0;
    m_slot = 0;
    m_messageEndsBatch = false;
    m_wordError2 = 0.0f;
    m_wordQualityKnown = false;
    m_softPos = 0;
    m_softKnown = false;
    m_preambleBefore = false;
    m_leadingConfirmed = false;
    m_batch = 0;
    m_active = false;
    m_currentBits.clear();
    m_uncorrectableRun = 0;
    m_codewords = 0;
    m_codewordsCorrected = 0;
    m_codewordsUncorrectable = 0;
    m_transmissions = 0;
    m_candidatesDropped = 0;
    m_syncMissed = 0;
    m_syncMissedLast = false;
    m_ber.reset();
    m_messages.clear();
}

RadioPagingPocsagDecoder::Status RadioPagingPocsagDecoder::getStatus() const
{
    Status status;

    status.m_state = m_state;
    status.m_polarity = m_polarity;
    status.m_confirmed = m_leadingConfirmed;
    status.m_batches = m_batch;
    status.m_codewords = m_codewords;
    status.m_codewordsCorrected = m_codewordsCorrected;
    status.m_codewordsUncorrectable = m_codewordsUncorrectable;
    status.m_transmissions = m_transmissions;
    status.m_candidatesDropped = m_candidatesDropped;
    status.m_syncMissed = m_syncMissed;
    status.m_ber = m_ber;

    return status;
}

RadioPagingPocsagDecoder::Message RadioPagingPocsagDecoder::takeMessage()
{
    Message message = m_messages.front();
    m_messages.pop_front();
    return message;
}

bool RadioPagingPocsagDecoder::syncMatch() const
{
    return (m_regBits >= 32) && ((popcount32(m_reg ^ RadioPagingPocsag::m_syncWord) <= m_maxSyncErrors) || softSyncMatch(m_reg));
}

// The last 32 symbols (word: their bits, newest in bit 0) as a sync codeword with up to m_maxSoftSyncErrors bit errors:
// their normalised values correlate with the sync codeword (the bits in error are uncertain ones)
bool RadioPagingPocsagDecoder::softSyncMatch(uint32_t word) const
{
    if (!m_softKnown || (popcount32(word ^ RadioPagingPocsag::m_syncWord) > m_maxSoftSyncErrors)) {
        return false;
    }

    float correlation = 0.0f;
    float magnitude = 0.0f;

    for (int k = 0; k < 32; k++)
    {
        // Bit k of the sync codeword (bit 0 is the newest symbol)
        float s = m_softBits[(m_softPos + 31 - k) % 32];
        correlation += ((RadioPagingPocsag::m_syncWord >> k) & 1) ? s : -s;
        magnitude += std::fabs(s);
    }

    return (magnitude > 0.0f) && (correlation >= m_softSyncCorrelation * magnitude);
}

bool RadioPagingPocsagDecoder::dropUntrustedCandidate()
{
    if ((m_state == Hunting) || m_preambleBefore || m_leadingConfirmed || (m_batch > 0) || m_syncMissedLast) {
        return false;
    }

    m_candidatesDropped++;
    m_state = Hunting;
    m_slot = 0;
    m_word = 0;
    m_wordBits = 0;
    m_wordError2 = 0.0f;
    return true;
}

void RadioPagingPocsagDecoder::startCandidate()
{
    m_syncWords++;
    m_candidateSyncErrors = popcount32(m_reg ^ RadioPagingPocsag::m_syncWord);
    m_state = InBatch;
    m_slot = 0;
    m_word = 0;
    m_wordBits = 0;
    m_wordError2 = 0.0f;
    m_preambleBefore = false;
    m_leadingConfirmed = false;
    m_syncMissedLast = false;
    m_batch = 0;
}

// A codeword of the batch is complete: kept with its quality
void RadioPagingPocsagDecoder::storeWord(int slot, uint32_t word)
{
    m_batchWords[slot] = word;
    m_batchQuality[slot] = m_wordQualityKnown ? std::sqrt(m_wordError2 / 32.0f) : -1.0f;
    m_wordError2 = 0.0f;
}

void RadioPagingPocsagDecoder::feed(int symbol, float error)
{
    m_wordQualityKnown = true;
    m_wordError2 += error * error;
    feed(symbol);
}

void RadioPagingPocsagDecoder::feed(int symbol, float error, float soft)
{
    // Binary 1 is a negative deviation for normal polarity
    m_softBits[m_softPos] = (m_polarity == Normal) ? -soft : soft;
    m_softPos = (m_softPos + 1) % 32;
    m_softKnown = true;
    feed(symbol, error);
}

void RadioPagingPocsagDecoder::feed(int symbol)
{
    // Binary 1 is a negative deviation for normal polarity
    int bit = (m_polarity == Normal) ? (symbol ? 0 : 1) : (symbol ? 1 : 0);

    m_reg = (m_reg << 1) | (uint32_t) bit;
    if (m_regBits < 32) {
        m_regBits++;
    }

    switch (m_state)
    {
    case Hunting:
        if (syncMatch()) {
            startCandidate();
        }
        break;

    case InBatch:
        // An unconfirmed batch gives way to a sync codeword at another position. The first batch of a transmission
        // only to one with fewer bit errors (its candidate was found in noise or in the data of another signal), and
        // not after the preamble: that is the start of a transmission, and a sync like pattern in the codewords of its
        // first batch is their data (a soft match within 4 bit errors straddling two message codewords is found in
        // real text). The batch read after a missed sync codeword gives way to any (a transmission that follows)
        if (!m_leadingConfirmed && syncMatch()
            && (m_syncMissedLast || (m_batch > 0)
                || (!m_preambleBefore && (popcount32(m_reg ^ RadioPagingPocsag::m_syncWord) < m_candidateSyncErrors))))
        {
            m_candidatesDropped++;
            startCandidate();
            break;
        }

        m_word = (m_word << 1) | (uint32_t) bit;
        if (++m_wordBits == 32)
        {
            storeWord(m_slot, m_word);
            m_wordBits = 0;
            m_word = 0;
            if (++m_slot == RadioPagingPocsag::m_codewordsPerBatch) {
                m_state = AwaitSync;
            }
        }
        break;

    case AwaitSync:
        m_word = (m_word << 1) | (uint32_t) bit;
        if (++m_wordBits == 32)
        {
            bool sync = (popcount32(m_word ^ RadioPagingPocsag::m_syncWord) <= m_maxSyncErrors) || softSyncMatch(m_word);

            m_wordBits = 0;
            m_word = 0;
            m_wordError2 = 0.0f;

            if (sync)
            {
                // The batch lies between two sync codewords one batch apart
                m_syncWords++;
                decodeBatch();
                m_leadingConfirmed = true;
                m_syncMissedLast = false;
                m_slot = 0;
                m_state = InBatch;
            }
            else if (!m_syncMissedLast)
            {
                // The following sync codeword was missed: the batch is decoded up to the end of the signal in it, which
                // ends the transmission. Otherwise (a fade over the sync codeword) reception goes on at the expected
                // position, as a pager decoder does. Two batches in a row without their sync codeword end the
                // transmission (sync loss)
                if (decodeUnconfirmedBatch())
                {
                    endTransmission();
                }
                else
                {
                    m_syncMissed++;
                    m_syncMissedLast = true;
                    m_leadingConfirmed = false;
                    m_slot = 0;
                    m_state = InBatch;
                }
            }
            else
            {
                decodeUnconfirmedBatch();
                endTransmission();
            }
        }
        break;
    }
}

void RadioPagingPocsagDecoder::decodeBatch()
{
    if (m_batch == 0)
    {
        m_transmissions++;
        m_ber.reset();
    }

    for (m_slot = 0; m_slot < RadioPagingPocsag::m_codewordsPerBatch; m_slot++) {
        processCodeword(m_batchWords[m_slot]);
    }

    m_batch++;
}

// A codeword of a batch no sync codeword follows is decipherable when it decodes and its symbols are better than
// noise gives (noise passes the code a quarter of the time)
bool RadioPagingPocsagDecoder::decipherable(int slot) const
{
    return (RadioPagingBCH::decode(m_batchWords[slot]).m_bitErrors >= 0) && (m_batchQuality[slot] < m_wordNoiseQuality);
}

// Decodes the first received codewords of the batch up to the end of the signal in it. Indecipherable codewords are lost
// codewords of the message, never addresses. After a confirmed sync codeword the batch is the last one of the
// transmission, which ends at the end of a batch (M.584-2 1): it is decoded whole. Otherwise (after a missed sync
// codeword, or a sync codeword found alone) the signal ends at its last decipherable codeword (noise has none). True
// if the batch is cut there, or not all its codewords were received: the message in progress ends cut and the
// transmission is over
bool RadioPagingPocsagDecoder::decodeUnconfirmedBatch(int received)
{
    int cut = 0;

    // A sync codeword alone (no preamble before it, no sync codeword one batch after it) is all that says the signal is
    // POCSAG: in noise, a carrier or the data of another protocol one is found now and then, and the code corrects half
    // of the words that follow it to some codeword. A codeword that needs a correction ends it: the codewords are taken
    // as received (an undecodable one is a lost codeword of the message, it says nothing). So is the batch received
    // after a missed sync codeword (a fade over it, or the end of the transmission): no sync codeword at either end
    // says it is POCSAG, and after the end of a transmission it is another signal (a transmission at another speed
    // that follows, read at this one)
    const bool alone = !m_leadingConfirmed && (((m_batch == 0) && !m_preambleBefore) || m_syncMissedLast);

    for (int slot = 0; slot < received; slot++)
    {
        if (alone)
        {
            RadioPagingBCH::Result result = RadioPagingBCH::decode(m_batchWords[slot]);

            if ((result.m_bitErrors > 0) || ((result.m_bitErrors == 0) && result.m_parityError)) {
                break;
            }
        }
        if (decipherable(slot)) {
            cut = slot + 1;
        }
    }

    if (m_leadingConfirmed) {
        cut = received;
    }

    // The codeword the signal was lost in is partly the transmission's, partly noise or another signal: the code
    // corrects it to some other codeword. Codewords before the cut that needed a correction are not taken
    while ((cut < received) && (cut > 0))
    {
        RadioPagingBCH::Result result = RadioPagingBCH::decode(m_batchWords[cut - 1]);

        if ((result.m_bitErrors == 0) && !result.m_parityError) {
            break;
        }

        cut--;
    }

    if (cut == 0)
    {
        if (m_batch == 0) {
            m_candidatesDropped++;
        }
        // Nothing of the signal in this batch: the transmission ended with the batch before. A message that filled
        // it ended with it
        finishMessage(m_messageEndsBatch ? EndBatch : EndCut);
        return true;
    }

    if (m_batch == 0)
    {
        m_transmissions++;
        m_ber.reset();
    }

    for (m_slot = 0; m_slot < cut; m_slot++) {
        processCodeword(m_batchWords[m_slot], !decipherable(m_slot));
    }

    m_batch++;

    if (cut < RadioPagingPocsag::m_codewordsPerBatch)
    {
        finishMessage(EndCut);      // The signal ended in the middle of the batch
        return true;
    }

    return false;
}

void RadioPagingPocsagDecoder::flush()
{
    // The batch received so far, decoded as a batch no sync codeword follows
    int received = (m_state == AwaitSync) ? RadioPagingPocsag::m_codewordsPerBatch : m_slot;

    if ((m_state != Hunting) && (received > 0)) {
        decodeUnconfirmedBatch(received);
    }

    endTransmission();
    m_word = 0;
    m_wordBits = 0;
    m_wordError2 = 0.0f;
    m_preambleBefore = false;
}

void RadioPagingPocsagDecoder::endTransmission()
{
    // A message in progress is cut (sync loss, the end of the input); one that filled the last batch before the signal
    // ended was finished (end batch) when the batch after it was found empty
    finishMessage(EndCut);
    m_messageEndsBatch = false;
    m_state = Hunting;
    m_leadingConfirmed = false;
    m_syncMissedLast = false;
    m_batch = 0;
    m_uncorrectableRun = 0;
}

// lost: the codeword is taken as undecodable (a codeword of a batch no sync codeword follows that needs two corrected
// bits)
void RadioPagingPocsagDecoder::processCodeword(uint32_t word, bool lost)
{
    RadioPagingBCH::Result result = RadioPagingBCH::decode(word);
    m_codewords++;
    m_messageEndsBatch = false;
    m_ber.add((result.m_bitErrors > 0 ? result.m_bitErrors : 0) + (result.m_parityError ? 1 : 0), 32, result.m_bitErrors < 0);

    if ((result.m_bitErrors < 0) || lost)
    {
        m_codewordsUncorrectable++;
        m_uncorrectableRun++;

        if (m_active)
        {
            m_current.m_errorsUncorrectable++;

            if (m_uncorrectableRun >= 2) {
                finishMessage(EndCut);      // Two codewords in a row that cannot be decoded (M.584-2 2.5.1)
            } else {
                // Received but not corrected: kept as received, marked uncertain ('u'); a word taken for
                // noise after the transmission is lost ('X')
                appendMessageBits(RadioPagingBCH::data(word) & 0xfffff, lost ? 'X' : 'u');
            }
        }
        return;
    }

    m_uncorrectableRun = 0;

    int corrections = result.m_bitErrors + (result.m_parityError ? 1 : 0);
    if (corrections > 0) {
        m_codewordsCorrected++;
    }

    uint32_t corrected = result.m_word;

    // An idle codeword received with 3 or 4 bit errors is corrected to an address codeword at distance 6 (the code's
    // minimum distance) from it: a codeword corrected to an address while within m_maxIdleErrors of the idle codeword
    // is the idle codeword
    if ((corrected != word) && ((corrected & 0x80000000u) == 0) && (popcount32(word ^ RadioPagingPocsag::m_idleWord) <= m_maxIdleErrors)) {
        corrected = RadioPagingPocsag::m_idleWord;
    }

    if (corrected == RadioPagingPocsag::m_idleWord)
    {
        finishMessage(EndIdle);
    }
    else if ((corrected & 0x80000000u) == 0)
    {
        // Address
        finishMessage(EndAddress);

        uint32_t data = RadioPagingBCH::data(corrected);

        m_active = true;
        m_current = Message();
        m_current.m_address = (((data >> 2) & 0x3ffff) << 3) | (uint32_t) (m_slot >> 1);
        m_current.m_function = data & 3;
        m_current.m_batch = m_batch;
        m_current.m_polarity = m_polarity;
        m_current.m_errorsCorrected = corrections;
        m_currentBits.clear();
    }
    else if (m_active)
    {
        appendMessageBits(RadioPagingBCH::data(corrected) & 0xfffff, (corrections > 0) ? 'c' : '.');
        m_current.m_errorsCorrected += corrections;
        m_messageEndsBatch = (m_slot == RadioPagingPocsag::m_codewordsPerBatch - 1);
    }
}

void RadioPagingPocsagDecoder::appendMessageBits(uint32_t data, char status)
{
    if ((int) m_currentBits.size() >= m_maxMessageBits) {
        return;
    }

    for (int i = 19; i >= 0; i--) {
        m_currentBits.push_back((data >> i) & 1);
    }

    m_current.m_wordStatus += status;
}

const char *RadioPagingPocsagDecoder::endName(End end)
{
    switch (end)
    {
    case EndIdle: return "idle";
    case EndAddress: return "address";
    case EndBatch: return "batch";
    case EndCut:
    default: return "cut";
    }
}

void RadioPagingPocsagDecoder::finishMessage(End end)
{
    if (!m_active) {
        return;
    }

    m_active = false;
    m_current.m_end = end;
    m_current.m_rawBits = (int) m_currentBits.size();
    m_current.m_rawData.assign((m_currentBits.size() + 7) / 8, 0);

    for (size_t i = 0; i < m_currentBits.size(); i++)
    {
        if (m_currentBits[i]) {
            m_current.m_rawData[i >> 3] |= (uint8_t) (0x80 >> (i & 7));
        }
    }

    m_messages.push_back(m_current);
    m_currentBits.clear();
}
