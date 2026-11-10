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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODCHANNELACCESS_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODCHANNELACCESS_H_

#include <cstdint>

/**
 * Channel access for the Angel61 tunnel (ANGEL61.md 3.5): when a station with something queued for
 * its peer may key up. Two stations share one half duplex channel and each keys by itself, so who
 * goes next is defined, or two busy stations collide and an answer owed never gets through.
 * - The end of a transmission hands the turn to its destination, which keys no sooner than the
 *   turnaround and no later than the reply window after it (its reply slot).
 * - The station that has just transmitted waits the reply window for its peer; if the peer
 *   starts, it waits for its end, and then has the reply slot itself.
 * - Otherwise it listens first (nothing heard) and backs off 0 to 2^k - 1 slots, k from 2 and
 *   rising to 6 with every transmission that expected an answer and got none in the reply
 *   window; hearing the peer resets it.
 * - After MaxUnanswered such transmissions in a row the datagrams are held until the peer is
 *   heard again (what is owed - tunnel control, the advertise - still goes).
 * Times are milliseconds on any monotonic clock; the events come from the station's receiver (a
 * transmission heard begins, and ends: whose, to whom) and its transmitter.
 */
class DSEModChannelAccess
{
public:
    struct Timing
    {
        int m_turnaroundMs = 100;     //!< TunnelTurnaround
        int m_replyWindowMs = 500;    //!< TunnelReplyWindow
        int m_slotMs = 80;            //!< a backoff slot: a frame (VHF 80 ms, HF 240)
        int m_maxUnanswered = 3;      //!< TunnelMaxUnanswered
        int m_kMin = 2;
        int m_kMax = 6;
    };

    explicit DSEModChannelAccess(uint32_t seed = 61);
    void setTiming(const Timing& timing) { m_timing = timing; }
    const Timing& getTiming() const { return m_timing; }
    //! Everything forgotten: a new peer, or the tunnel opened
    void reset();

    //! The receiver heard a transmission begin
    void heardStart(int64_t nowMs);
    //! ... and end: whether it was the peer's, and addressed to this station
    void heardEnd(int64_t nowMs, bool fromPeer, bool toMe);
    //! This station's transmission ended; `expectsAnswer`: it carried datagrams for the peer
    void transmitted(int64_t nowMs, bool expectsAnswer);
    //! This station keyed up: the backoff drawn for it, and the reply slot, are spent
    void keyed(int64_t nowMs);

    /**
     * When this station may key up: `nowMs` or later, or -1 while it may not - a transmission is
     * being heard (its end decides), or, for datagrams, the peer has stopped answering (held)
     */
    int64_t mayKeyAt(int64_t nowMs, bool datagrams = true);
    bool isBusy() const { return m_busy; }
    bool isHeld() const { return m_unanswered >= m_timing.m_maxUnanswered; }
    int getUnanswered() const { return m_unanswered; }
    int getBackoffExponent() const { return m_k; }
    //! In the reply slot the peer handed this station
    bool inReplySlot(int64_t nowMs) const { return (m_replySlotToMs >= 0) && (nowMs <= m_replySlotToMs); }

private:
    Timing m_timing;
    uint32_t m_rng;
    bool m_busy;
    int64_t m_replySlotFromMs;       //!< the reply slot owed this station, -1 for none
    int64_t m_replySlotToMs;
    int64_t m_waitUntilMs;           //!< just transmitted: the peer's turn until then
    bool m_expectAnswer;             //!< ... and the transmission wanted an answer
    int64_t m_backoffUntilMs;        //!< the backoff drawn for the next key up, -1 for none drawn
    int m_unanswered;
    int m_k;

    //! A reply window that passed with nothing heard from the peer
    void settle(int64_t nowMs);
    uint32_t nextRandom();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODCHANNELACCESS_H_
