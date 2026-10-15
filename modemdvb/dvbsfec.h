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

#ifndef MODEMDVB_DVBSFEC_H
#define MODEMDVB_DVBSFEC_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <vector>

#include "dvbs.h"
#include "dvbtouter.h"
#include "viterbi27.h"

namespace dvbs {

//! DVB-S error correction (EN 300 421 4.4): from the soft bits of the QPSK symbols, as demodulated, to transport stream
//! packets. Depuncturing, K = 7 Viterbi decoding, sync byte search, Forney de-interleaving, RS(204,188) and
//! descrambling, the last four DVB-T's (dvbt::OuterDecoder).
//!
//! The carrier loop can lock at any of four phases, and the code rate and where its puncturing periods start aren't
//! known. A rotation of 180 degrees inverts every bit, which the code is transparent to (its generators have odd
//! weight): the decoded stream is inverted, its sync bytes 0xB8 and every eighth 0x47. A rotation of 90 degrees swaps
//! X and Y and inverts one, which nothing decodes. So 2 rotations, each code rate (or the one set) and each start of its
//! period are tried on a block of symbols: each is depunctured, decoded, re-encoded and compared with what was
//! received. The right one agrees with all but the channel's errors, the others about half. Then the sync bytes of what
//! it decoded say whether it's inverted.
//!
//! The spectrum may be inverted too (a high side LO, I and Q swapped): the symbols are conjugated, (I, -Q), which isn't
//! a rotation, though the carrier loop locks to it as well (QPSK conjugated is QPSK). So if nothing's found, the block's
//! tried again conjugated (before the rotation), costing a second pass only of searches that fail. The way that last
//! locked is tried first.
class MODEMDVB_API FECDecoder
{
public:
    typedef std::function<void(const uint8_t *packet)> PacketCallback;

    enum class State { Searching = 0, Locked = 1 };

    struct Status
    {
        State m_state = State::Searching;
        bool m_tsSync = false;
        bool m_codeRateKnown = false;
        CodeRate m_codeRate = CodeRate::R1_2;
        int m_rotation = 0;                 //!< Quarter turns undone (after any conjugation)
        bool m_inverted = false;            //!< The spectrum's inverted: the symbols are conjugated
        double m_agreement = 0.0;           //!< Of the hypothesis found, with what was received
        double m_lastAgreement = 0.0;       //!< Of the best hypothesis of the last search
        int m_lastSyncs = 0;                //!< Sync bytes it decoded, of
        int m_lastPositions = 0;
        dvbt::OuterDecoder::Stats m_outer;
        double m_preViterbiBER = 0.0;       //!< Recent average, from re-encoding the decoded bits
        double m_postViterbiBER = 0.0;      //!< Recent average, from what Reed-Solomon corrects
        uint64_t m_viterbiBits = 0;
        uint64_t m_searches = 0;
        uint64_t m_locks = 0;
        double m_seconds[3] = {0.0, 0.0, 0.0}; //!< Searching, Viterbi (and BER), outer decoding
    };

    FECDecoder();

    void setPacketCallback(PacketCallback callback) { m_outer.setPacketCallback(callback); }
    //! For the search's CPU budget: symbols are skipped between searches that find nothing, so it uses a fixed share
    //! of real time. 0 (the default) for no skipping
    //! The search's skip is in symbols at the rate it was measured at: none at another. From any thread: taken (and the
    //! skip cleared) by the thread decoding, which alone uses the skip
    void setSymbolRate(double rate) { m_requestedSymbolRate = rate; }
    //! The code rate, or -1 to find it (as is any other value out of range). Searches again
    void setCodeRate(int rate);
    void reset();
    //! The soft I and Q of each symbol (int8, positive for 0, 0 for nothing known), as demodulated: whichever rotation
    //! the carrier locked at
    void decode(const int8_t *iq, int nbSymbols);
    const Status& getStatus() const { return m_status; }

    static constexpr int m_searchSymbols = 8192;    //!< Tried at a time

private:
    struct Hypothesis
    {
        int m_rotation = 0;
        bool m_conjugate = false;   //!< Before the rotation
        CodeRate m_rate = CodeRate::R1_2;
        int m_offset = 0;           //!< Of the first bit of a puncturing period in the block's serial bits
        double m_agreement = 0.0;
    };

    static constexpr double m_minAgreement = 0.75;          //!< Of a hypothesis with what was received, to look for its sync bytes
    static constexpr int m_skipSteps = 32;                  //!< At the start of a test, while the decoder's metrics settle
    static constexpr uint64_t m_maxUnsyncedPackets = 24;    //!< Decoded without the outer decoder's sync, before searching again
    static constexpr int m_maxFailedRun = 6;                //!< Consecutive packets RS can't correct (as after a cycle slip), the same
    static constexpr double m_searchBudget = 0.25;          //!< Of a core, at most, searching without finding (symbols are skipped)
    static constexpr uint64_t m_berWindow = 2000000;        //!< Bits compared for the BER before Viterbi

    static double now();
    void search();
    //! Depuncture serial soft bits from offset, decode, re-encode and compare. bits: the decoded bits, if not null
    double test(const int8_t *serial, int nbBits, CodeRate rate, int offset, std::vector<uint8_t> *bits);
    //! The most sync bytes (0x47 or 0xB8) at one alignment, and whether 0xB8 were the most of them
    static int syncBytes(const std::vector<uint8_t>& bits, bool& inverted, int& positions);
    //! Conjugated (if conjugate), then quarter turns undone
    static void rotate(const int8_t *iq, int nbSymbols, int rotation, bool conjugate, int8_t *out);
    void lock(const Hypothesis& hypothesis);
    void decodeSerial(const int8_t *serial, int nbBits);
    void measureBER(const std::vector<uint8_t>& bits);

    dvbt::Viterbi27 m_viterbi;
    dvbt::Viterbi27 m_testViterbi;  //!< For the search's tests
    dvbt::OuterDecoder m_outer;
    std::atomic<double> m_requestedSymbolRate{0.0};    //!< Set from any thread
    double m_symbolRate = 0.0;      //!< The decoding thread's
    int64_t m_skipSymbols = 0;      //!< Still to skip before the next search (the decoding thread's)
    int m_codeRateSetting;      //!< -1 to find it
    bool m_conjugateFirst;      //!< Searched first: as last locked (kept through reset(), as the signal likely is)
    Status m_status;

    std::vector<int8_t> m_search;   //!< Serial soft bits (I, Q of each symbol) being searched, unrotated
    std::vector<int8_t> m_history;  //!< The last block's, when locked, to search again at once if lock's lost
    std::vector<int8_t> m_rotated;
    std::vector<int8_t> m_xy;       //!< Depunctured, X and Y of each step
    std::vector<uint8_t> m_bits;
    std::vector<uint8_t> m_encoded;

    // Locked
    Hypothesis m_hypothesis;
    std::vector<PunctureEntry> m_pattern;
    int m_period;
    std::vector<int8_t> m_periodBits;   //!< Of the period being received
    int m_periodFill;
    std::vector<uint8_t> m_hard;    //!< Of each step not yet decoded: X, Y, X known, Y known in bits 0..3
    uint8_t m_encoderState;
    uint64_t m_unsyncedBits;        //!< Decoded since the outer decoder was last synchronised
    int m_failedRun;                //!< Consecutive packets RS couldn't correct
    uint64_t m_berErrors;
    uint64_t m_berTotal;
    dvbt::OuterDecoder::Stats m_berOuter;   //!< The outer decoder's counts at the start of the BER window
};

} // namespace dvbs

#endif // MODEMDVB_DVBSFEC_H
