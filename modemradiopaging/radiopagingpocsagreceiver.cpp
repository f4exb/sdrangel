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

#include "radiopagingpocsag.h"
#include "radiopagingpocsagreceiver.h"

const int RadioPagingPocsagReceiver::m_preambleLockBits;

RadioPagingPocsagReceiver::RadioPagingPocsagReceiver() :
    m_baud(1200),
    m_normalEnabled(true),
    m_invertedEnabled(true),
    m_normal(RadioPagingPocsagDecoder::Normal),
    m_inverted(RadioPagingPocsagDecoder::Inverted)
{
    create(38400.0, 1200);
}

void RadioPagingPocsagReceiver::create(double sampleRate, int baud)
{
    m_baud = baud;
    m_demod.create(sampleRate, baud, 2);

    // Quality limit of the codewords of a batch no sync codeword follows (rms symbol error, outer levels +/-1),
    // measured per speed with the FSK modulator through noise: the lowest value of one codeword of noise only,
    // less a margin
    float wordNoise = (baud <= 512) ? 0.72f : (baud <= 1200) ? 0.59f : 0.50f;
    m_normal.setNoiseQuality(wordNoise);
    m_inverted.setNoiseQuality(wordNoise);

    reset();
}

void RadioPagingPocsagReceiver::reset()
{
    m_demod.reset();
    m_normal.reset();
    m_inverted.reset();
    m_lastBit = -1;
    m_reversals = 0;
    m_preambleLocked = false;
    m_bitsSincePreamble = 0;
    m_bitsSinceReversals = m_preambleRecentBits;
    m_messages.clear();
    m_dedup.reset();
    m_time = 0.0;
    m_levelsMeasured = false;
    m_measuredDc = 0.0f;
}

void RadioPagingPocsagReceiver::dropLock()
{
    m_normal.reset();
    m_inverted.reset();
    m_preambleLocked = false;
    m_bitsSincePreamble = 0;
    m_bitsSinceReversals = m_preambleRecentBits;
}

// The last 32 symbols were the sync codeword, sent most significant bit first; a binary 1
// is a negative deviation in normal polarity
void RadioPagingPocsagReceiver::fitSyncCodeword(bool inverted)
{
    std::vector<float> known;

    for (int i = 31; i >= 0; i--)
    {
        bool one = (RadioPagingPocsag::m_syncWord >> i) & 1;
        known.push_back((one != inverted) ? -1.0f : 1.0f);
    }

    if (m_demod.fitLevels(known))
    {
        m_levelsMeasured = true;
        m_measuredDc = m_demod.getDC();
    }
}

bool RadioPagingPocsagReceiver::takeLevelsMeasured(float& dc)
{
    if (!m_levelsMeasured) {
        return false;
    }

    m_levelsMeasured = false;
    dc = m_measuredDc;
    return true;
}

bool RadioPagingPocsagReceiver::isSynced() const
{
    return (m_normalEnabled && m_normal.isSynced()) || (m_invertedEnabled && m_inverted.isSynced());
}

void RadioPagingPocsagReceiver::process(float sample)
{
    RadioPagingFskDemod::Symbol symbol;

    if (!m_demod.process(sample, symbol)) {
        return;
    }

    m_time += 1.0 / m_baud;

    // Reversals of the preamble lock the demodulator after a part of the preamble
    if ((m_lastBit >= 0) && (symbol.m_value != m_lastBit)) {
        m_reversals++;
    } else {
        m_reversals = 0;
    }
    m_lastBit = symbol.m_value;

    if (m_reversals >= m_preambleLockBits)
    {
        m_preambleLocked = true;
        m_bitsSincePreamble = 0;
        m_bitsSinceReversals = 0;
    }
    else if (m_bitsSinceReversals < m_preambleRecentBits)
    {
        m_bitsSinceReversals++;
    }

    uint32_t normalSyncs = m_normal.syncWords();
    uint32_t invertedSyncs = m_inverted.syncWords();

    if (m_normalEnabled) {
        m_normal.feed(symbol.m_value, symbol.m_error, symbol.m_soft);
    }
    if (m_invertedEnabled) {
        m_inverted.feed(symbol.m_value, symbol.m_error, symbol.m_soft);
    }

    // A sync codeword was just received (the candidate of a batch or the codeword of the
    // next batch): DC offset and deviation are measured on it and held for the batch. A
    // candidate that follows the preamble is marked so: a run of reversals ended at most a
    // codeword before the sync codeword started (bit errors in the last bits of the preamble
    // break the run before its end)
    const bool preambleBefore = m_bitsSinceReversals < m_preambleRecentBits;

    // A transmitter sends one polarity: a candidate without a preamble in one polarity, while the decoder of the other
    // one is in a transmission that started after its preamble or that its sync codewords confirmed, is in that
    // transmission's data, not a sync codeword. The end of the sync codeword (18 bits) is the complement of the start
    // of the idle codeword: where a codeword is followed by an idle codeword, the other polarity sees a window that
    // matches the sync codeword but for the last 14 bits of that codeword, and now and then again one batch later
    if (m_normalEnabled && (m_normal.syncWords() != normalSyncs))
    {
        if (preambleBefore && !m_normal.getStatus().m_confirmed) {
            m_normal.setPreambleBefore();
        }
        if (!(m_invertedEnabled && m_inverted.inTrustedTransmission() && m_normal.dropUntrustedCandidate()))
        {
            fitSyncCodeword(false);
            // and a candidate of the other polarity found before is given up
            if (m_invertedEnabled && m_normal.inTrustedTransmission()) {
                m_inverted.dropUntrustedCandidate();
            }
        }
    }
    else if (m_invertedEnabled && (m_inverted.syncWords() != invertedSyncs))
    {
        if (preambleBefore && !m_inverted.getStatus().m_confirmed) {
            m_inverted.setPreambleBefore();
        }
        if (!(m_normalEnabled && m_normal.inTrustedTransmission() && m_inverted.dropUntrustedCandidate()))
        {
            fitSyncCodeword(true);
            // and a candidate of the other polarity found before is given up
            if (m_normalEnabled && m_inverted.inTrustedTransmission()) {
                m_normal.dropUntrustedCandidate();
            }
        }
    }

    bool synced = (m_normalEnabled && m_normal.isSynced()) || (m_invertedEnabled && m_inverted.isSynced());

    // Hunting: the levels follow the means of the preamble (or carrier)
    if (!synced) {
        m_demod.setHoldLevels(false);
    }

    // The preamble lock is handed over to the decoders once a batch is being received. The
    // preamble is followed by the sync codeword, so the lock is kept for one codeword after
    // the last reversal and released if no sync codeword came
    if (m_preambleLocked)
    {
        if (synced) {
            m_preambleLocked = false;
        } else if ((m_reversals < m_preambleLockBits) && (++m_bitsSincePreamble > m_preambleLockBits)) {
            m_preambleLocked = false;
        }
    }

    m_demod.setLocked(m_preambleLocked || synced);

    while (m_normal.hasMessage()) {
        collect(m_normal.takeMessage());
    }
    while (m_inverted.hasMessage()) {
        collect(m_inverted.takeMessage());
    }
}

void RadioPagingPocsagReceiver::flush()
{
    m_normal.flush();
    m_inverted.flush();

    while (m_normal.hasMessage()) {
        collect(m_normal.takeMessage());
    }
    while (m_inverted.hasMessage()) {
        collect(m_inverted.takeMessage());
    }
}

// The same message to the same pager (address and function) again within the window is a
// duplicate. The other polarity is another network
void RadioPagingPocsagReceiver::collect(RadioPagingPocsagDecoder::Message message)
{
    std::string address = std::to_string(message.m_address) + (char) ('A' + (message.m_function & 3));
    int polarity = (message.m_polarity == RadioPagingPocsagDecoder::Inverted) ? 1 : 0;

    message.m_duplicate = m_dedup.check(polarity, address, message.m_rawData, message.m_rawBits, m_time);
    m_messages.push_back(message);
}

RadioPagingPocsagDecoder::Message RadioPagingPocsagReceiver::takeMessage()
{
    RadioPagingPocsagDecoder::Message message = m_messages.front();
    m_messages.pop_front();
    return message;
}

RadioPagingPocsagReceiver::Status RadioPagingPocsagReceiver::getStatus() const
{
    Status status;

    status.m_locked = m_preambleLocked || m_normal.isSynced() || m_inverted.isSynced();
    status.m_preamble = m_reversals >= m_preambleLockBits;
    status.m_normal = m_normal.getStatus();
    status.m_inverted = m_inverted.getStatus();
    status.m_deviation = m_demod.getDeviation();
    status.m_dc = m_demod.getDC();

    return status;
}
