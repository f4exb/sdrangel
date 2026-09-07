///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
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

#ifndef INCLUDE_PACKETDEMODFRAMER_H
#define INCLUDE_PACKETDEMODFRAMER_H

// HDLC deframing for AX.25, with Chase decoding. Header only and free of any dependency on
// the channel, the device or Qt beyond QByteArray, so the demodulator and the offline test
// harness run the SAME framing code rather than two copies of it. There were four
// implementations of this state machine before - two here and two in the harness - which
// had to agree for Chase to accept only what the primary path would have framed, and
// nothing enforced that they did.
//
// The detector is deliberately not part of this: it hands in one hard symbol and its
// confidence at a time, and gets frames back through a callback.

#include <QByteArray>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "dsp/dsptypes.h"
#include "util/ax25.h"
#include "util/crc.h"
#include "util/popcount.h"

// The shortest legal AX.25 frame: 7 byte destination, 7 byte source, 1 control, 2 FCS.
// Accepting anything shorter lets m_byteCount == 1 index m_bytes[-1] when the CRC is
// extracted, and admits junk frames that can only fabricate CRC matches.
#define PACKETDEMOD_AX25_MIN_BYTES 17

// Soft decision history for Chase decoding. A maximum length frame is 512 bytes, which with
// bit stuffing and flags is under 5000 symbols.
#define PACKETDEMOD_SYM_HIST 8192

// Largest Chase depth honoured, matching the GUI spin box. The work is 2^n - 1 rebuilt
// and re-CRC'd frames for every CRC failure on every detector, so 12 is already 4095;
// the value reaches here from presets and the REST API, neither of which bounds it,
// and 1u << n is undefined once n reaches 32.
#define PACKETDEMOD_CHASE_MAX  12

class PacketDemodFramer
{
public:

    // Returns true if the frame was accepted - the caller's deduplication decides, and the
    // framer reports that back so callers can count distinct packets rather than the
    // copies many parallel detectors make of one transmission.
    typedef std::function<bool (const QByteArray& packet, bool viaChase)> FrameHandler;

    // What the framing saw. The harness prints these; the demodulator has had no such
    // visibility at all, which is part of why its framing bugs were found by inspection.
    struct Counters
    {
        quint64 m_crcValid = 0;
        quint64 m_crcInvalid = 0;
        quint64 m_implausible = 0;      // passed CRC, failed the callsign check
        quint64 m_chaseCalls = 0;
        quint64 m_chaseRecovered = 0;
    };

    const Counters& counters() const { return m_counters; }
    void resetCounters() { m_counters = Counters(); }

    struct State
    {
        int m_symbolPrev;
        unsigned char m_bits;
        int m_bitCount;
        int m_onesCount;
        bool m_gotSOP;
        unsigned char m_bytes[512]{};     // Info field can be 256 bytes
        int m_byteCount;

        std::vector<Real> m_symHist;
        quint64 m_symPos;
        quint64 m_sopPos;

        State() :
            m_symHist(PACKETDEMOD_SYM_HIST, 0.0f)
        {
            reset();
        }

        void reset()
        {
            m_symbolPrev = 0;
            m_bits = 0;
            m_bitCount = 0;
            m_onesCount = 0;
            m_gotSOP = false;
            m_byteCount = 0;
            m_symPos = 0;
            m_sopPos = 0;
        }

        int symbolAt(quint64 pos) const
        {
            return m_symHist[pos % PACKETDEMOD_SYM_HIST] >= 0.0f ? 1 : 0;
        }
    };

    void setFrameHandler(const FrameHandler& handler) { m_onFrame = handler; }

    // Chase depth, clamped on use rather than trusted
    void setChaseDepth(int depth) { m_chaseDepth = depth; }

    // Whether a CRC pass alone is enough. One detector is rarely offered a wrong frame,
    // but the MLSE runs a hundred or more over the same noise and a 16 bit CRC is not
    // enough on its own at that rate.
    void setRequirePlausible(bool require) { m_requirePlausible = require; }

    // One symbol. allowChase lets the caller withhold Chase where it cannot afford it.
    // Returns true if a frame was emitted AND accepted by the handler.
    bool process(State& d, int sample, Real soft, bool allowChase)
    {
        bool accepted = false;

        // Keep the soft decision for every symbol, so a frame that fails CRC can be retried
        // with its least confident symbols inverted
        d.m_symHist[d.m_symPos % PACKETDEMOD_SYM_HIST] = soft;

        feedSymbol(d, sample, [this, allowChase, &accepted](State& st)
        {
            QByteArray candidate;

            if (frameCrcOk(st.m_bytes, st.m_byteCount, candidate))
            {
                m_counters.m_crcValid++;

                // One detector is rarely offered a wrong frame, but the MLSE runs a
                // hundred or more in parallel over the same noise and a 16 bit CRC is
                // not enough on its own at that rate
                if (!m_requirePlausible || plausible(candidate)) {
                    accepted = m_onFrame && m_onFrame(candidate, false);
                } else {
                    m_counters.m_implausible++;
                }
            }
            else
            {
                // Counted whether or not Chase runs, so the totals mean the same thing
                // on the live chains and in the replay
                m_counters.m_crcInvalid++;

                // Chase is the caller's policy: the replay already searches timing, rate,
                // metric and tone-pair hypotheses, so it does not also pay for up to 63
                // rebuilt frames per CRC failure per detector.
                if (allowChase)
                {
                    QByteArray fixed;

                    if (chaseDecode(st, fixed)) {
                        accepted = m_onFrame && m_onFrame(fixed, true);
                    }
                }
            }
        });

        return accepted;
    }

    bool plausible(const QByteArray& packet)
    {
        AX25Packet ax25;

        if (!ax25.decode(packet)) {
            return false;
        }

        for (const QString& call : {ax25.m_from, ax25.m_to})
        {
            QString base = call.section('-', 0, 0);

            if (base.isEmpty() || (base.length() > 6)) {
                return false;
            }

            for (QChar c : base)
            {
                if (!((c >= 'A') && (c <= 'Z')) && !((c >= '0') && (c <= '9'))) {
                    return false;
                }
            }
        }

        return true;
    }

private:
    // The bit level HDLC state machine: NRZI, destuffing, flag detection and byte assembly.
    // Both the live path and Chase's reframing drive THIS one implementation, so a frame
    // Chase offers the CRC is by construction one the live path would have framed the same
    // way. They used to be separate copies and they disagreed: on a flag arriving with fewer
    // than the minimum bytes this restarts the frame, where the copy inside redeframe()
    // stored the flag byte as data and carried on with the bytes it already had.
    //
    // onFrame(d) is called at the instant a closing flag completes a frame, with the state
    // untouched, so it sees d.m_bytes, d.m_byteCount and the span [d.m_sopPos, d.m_symPos).
    template <typename OnFrame>
    void feedSymbol(State& d, int symbol, const OnFrame& onFrame)
    {
        d.m_symPos++;

        // NRZI decoding
        int bit;

        if (symbol != d.m_symbolPrev) {
            bit = 0;
        } else {
            bit = 1;
        }
        d.m_symbolPrev = symbol;

        d.m_bits |= bit << d.m_bitCount;
        d.m_bitCount++;

        if (bit == 1)
        {
            d.m_onesCount++;

            // Shouldn't ever get 7 1s in a row
            if ((d.m_onesCount == 7) && d.m_gotSOP)
            {
                d.m_gotSOP = false;
                d.m_byteCount = 0;
            }
        }
        else
        {
            if (d.m_onesCount == 5)
            {
                // Remove bit-stuffing (5 1s followed by a 0)
                d.m_bitCount--;
            }
            else if (d.m_onesCount == 6)
            {
                // A flag. With a whole frame behind it this closes one - anything shorter
                // than the minimum cannot be a frame, and lets m_byteCount == 1 index
                // m_bytes[-1] when the CRC is extracted, so that only opens a new one.
                if ((d.m_bitCount == 8) && (d.m_bits == 0x7e)
                    && (d.m_byteCount >= PACKETDEMOD_AX25_MIN_BYTES)) {
                    onFrame(d);
                }

                // A closing flag is also the next frame's opening flag
                d.m_gotSOP = true;
                d.m_sopPos = d.m_symPos;
                d.m_bits = 0;
                d.m_bitCount = 0;
                d.m_byteCount = 0;
            }

            d.m_onesCount = 0;
        }

        if (d.m_gotSOP && (d.m_bitCount == 8))
        {
            if (d.m_byteCount >= (int) sizeof(d.m_bytes))
            {
                // Too many bytes
                d.m_gotSOP = false;
                d.m_byteCount = 0;
            }
            else
            {
                d.m_bytes[d.m_byteCount] = d.m_bits;
                d.m_byteCount++;
            }

            d.m_bits = 0;
            d.m_bitCount = 0;
        }
    }

    // True if the assembled bytes carry a matching CRC, in which case packet is the frame
    bool frameCrcOk(const unsigned char *bytes, int byteCount, QByteArray& packet)
    {
        m_crc.init();
        m_crc.calculate(bytes, byteCount - 2);
        uint16_t calcCrc = m_crc.get();
        uint16_t rxCrc = bytes[byteCount-2] | (bytes[byteCount-1] << 8);

        if (calcCrc != rxCrc) {
            return false;
        }

        packet = QByteArray((const char *) bytes, byteCount);

        return true;
    }

    bool chaseDecode(State& d, QByteArray& packet)
    {
        m_counters.m_chaseCalls++;

        // Clamped rather than trusted. The GUI cannot offer more than the maximum, but this
        // value also arrives from a saved preset and from the REST API, neither of which
        // bounds it - and the cost is 2^n - 1 rebuilt frames per CRC failure per detector,
        // so a large value does not degrade the baseband thread, it stops it.
        const int chase = std::max(0, std::min(m_chaseDepth, PACKETDEMOD_CHASE_MAX));

        if (chase == 0) {
            return false;
        }

        quint64 sopPos = d.m_sopPos;
        quint64 endPos = d.m_symPos;

        if ((endPos <= sopPos) || (endPos - sopPos > PACKETDEMOD_SYM_HIST)) {
            return false;
        }

        // The last 8 symbols are the closing flag, which by definition decoded correctly.
        // Replay the whole span, but rank only the body - flipping a flag symbol stops
        // redeframe() finding the flag, and a flag symbol ranking among the weakest displaces
        // a real data error from the search depth.
        quint64 rankEnd = endPos - 8;

        if (rankEnd <= sopPos) {
            return false;
        }

        std::vector<std::pair<Real, quint64>> ranked;
        ranked.reserve((size_t)(rankEnd - sopPos));

        for (quint64 p = sopPos; p < rankEnd; p++) {
            ranked.push_back(std::make_pair(
                std::fabs(d.m_symHist[p % PACKETDEMOD_SYM_HIST]), p));
        }

        int n = std::min(chase, (int) ranked.size());

        std::partial_sort(ranked.begin(), ranked.begin() + n, ranked.end());

        std::vector<quint64> flips;

        for (int count = 1; count <= n; count++)
        {
            for (uint32_t mask = 1; mask < (1u << n); mask++)
            {
                // popcount is a macro resolving to std::popcount, a compiler intrinsic or a
                // fallback, so the return type varies - MSVC's __popcnt is unsigned
                if ((int) popcount(mask) != count) {
                    continue;
                }

                flips.clear();

                for (int b = 0; b < n; b++)
                {
                    if (mask & (1u << b)) {
                        flips.push_back(ranked[b].second);
                    }
                }

                std::sort(flips.begin(), flips.end());

                if (redeframe(d, sopPos, endPos, flips, packet)) {
                    m_counters.m_chaseRecovered++;
                    return true;
                }
            }
        }

        return false;
    }

    bool redeframe(const State& d, quint64 sopPos, quint64 endPos,
                   const std::vector<quint64>& flips, QByteArray& out)
    {
        // Reusing one scratch state rather than building one per attempt: Chase runs this
        // up to 2^n - 1 times per CRC failure, and State carries a symbol history.
        State& t = m_scratch;
        t.reset();

        // The span starts mid stream, just after an opening flag: the symbol before it is
        // the NRZI reference, and the machine is already inside a frame.
        t.m_symbolPrev = d.symbolAt(sopPos - 1);
        t.m_gotSOP = true;

        size_t nextFlip = 0;
        bool found = false;

        for (quint64 p = sopPos; (p < endPos) && !found; p++)
        {
            int symbol = d.symbolAt(p);

            if ((nextFlip < flips.size()) && (flips[nextFlip] == p))
            {
                symbol = !symbol;
                nextFlip++;
            }

            feedSymbol(t, symbol, [this, &out, &found](State& st)
            {
                QByteArray candidate;

                // Chase offers the CRC up to 2^n - 1 frames per failure, so the callsign
                // check is not optional
                if (frameCrcOk(st.m_bytes, st.m_byteCount, candidate) && plausible(candidate))
                {
                    out = candidate;
                    found = true;
                }
            });
        }

        return found;
    }

    State m_scratch;                // see redeframe
    FrameHandler m_onFrame;
    int m_chaseDepth = 6;
    bool m_requirePlausible = true;
    crc16x25 m_crc;
    Counters m_counters;
};

#endif // INCLUDE_PACKETDEMODFRAMER_H
