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

#include "dsemodchannelaccess.h"

DSEModChannelAccess::DSEModChannelAccess(uint32_t seed) :
    m_rng(seed ? seed : 61)
{
    reset();
}

void DSEModChannelAccess::reset()
{
    m_busy = false;
    m_replySlotFromMs = -1;
    m_replySlotToMs = -1;
    m_waitUntilMs = -1;
    m_expectAnswer = false;
    m_backoffUntilMs = -1;
    m_unanswered = 0;
    m_k = m_timing.m_kMin;
}

uint32_t DSEModChannelAccess::nextRandom()
{
    // xorshift32: the backoff needs no more
    m_rng ^= m_rng << 13;
    m_rng ^= m_rng >> 17;
    m_rng ^= m_rng << 5;
    return m_rng;
}

void DSEModChannelAccess::settle(int64_t nowMs)
{
    if (m_expectAnswer && !m_busy && (nowMs >= m_waitUntilMs))
    {
        m_expectAnswer = false;
        m_unanswered++;
        m_k = std::min(m_k + 1, m_timing.m_kMax);
    }
}

void DSEModChannelAccess::heardStart(int64_t nowMs)
{
    settle(nowMs);
    m_busy = true;
    m_backoffUntilMs = -1;      // drawn again once the channel is free
}

void DSEModChannelAccess::heardEnd(int64_t nowMs, bool fromPeer, bool toMe)
{
    m_busy = false;
    m_backoffUntilMs = -1;

    if (fromPeer)
    {
        // The peer answers: whatever went unanswered is forgotten
        m_expectAnswer = false;
        m_unanswered = 0;
        m_k = m_timing.m_kMin;
        m_waitUntilMs = -1;
    }

    settle(nowMs);

    // Its end hands the turn to its destination
    if (fromPeer && toMe)
    {
        m_replySlotFromMs = nowMs + m_timing.m_turnaroundMs;
        m_replySlotToMs = nowMs + m_timing.m_replyWindowMs;
    }
    else
    {
        m_replySlotFromMs = -1;
        m_replySlotToMs = -1;
    }
}

void DSEModChannelAccess::transmitted(int64_t nowMs, bool expectsAnswer)
{
    m_busy = false;
    m_waitUntilMs = nowMs + m_timing.m_replyWindowMs;
    m_expectAnswer = expectsAnswer;
    m_replySlotFromMs = -1;
    m_replySlotToMs = -1;
    m_backoffUntilMs = -1;
}

void DSEModChannelAccess::keyed(int64_t nowMs)
{
    (void) nowMs;
    m_backoffUntilMs = -1;
    m_replySlotFromMs = -1;
    m_replySlotToMs = -1;
}

int64_t DSEModChannelAccess::mayKeyAt(int64_t nowMs, bool datagrams)
{
    settle(nowMs);

    if (m_busy || (datagrams && isHeld())) {
        return -1;
    }

    // The reply slot the peer handed this station
    if ((m_replySlotToMs >= 0) && (nowMs <= m_replySlotToMs)) {
        return std::max(nowMs, m_replySlotFromMs);
    }

    // Just transmitted: the peer's turn first
    if (m_waitUntilMs > nowMs) {
        return m_waitUntilMs;
    }

    // Listen first (nothing is heard), then a random backoff of 0 to 2^k - 1 slots
    if (m_backoffUntilMs < 0) {
        m_backoffUntilMs = nowMs + (int64_t) (nextRandom() % (1u << m_k)) * m_timing.m_slotMs;
    }

    return std::max(nowMs, m_backoffUntilMs);
}
