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

#ifndef INCLUDE_DSDSYMBOLCLOCK_H
#define INCLUDE_DSDSYMBOLCLOCK_H

#include <cstdint>
#include <algorithm>
#include <vector>

/**
 * Symbol timing: a cubic (Farrow) interpolator over the matched filter output
 * and a second order Mueller and Muller loop.
 *
 * The strobe position is a continuous sample index. It is set from a sync
 * detection and then advanced by the symbol period plus the loop's rate term,
 * the loop being driven by decision directed timing errors. A symbol is
 * available once the sample after the strobe's integer part has arrived.
 */
class DSDSymbolClock
{
public:
    DSDSymbolClock();

    void setSamplesPerSymbol(int sps);
    //! Loop bandwidth as a fraction of the symbol rate
    void setLoopBandwidth(float bandwidth);
    void reset();
    //! Clear the loop state (rate, hold, last decision) but keep the sample buffer
    void resetLoop();

    //! Store a matched filter sample
    void push(float y, int64_t sampleIndex);
    //! Set the strobe for the next symbol: the given symbol centre plus one period. The rate term is kept.
    void setPhase(double symbolCentre);
    //! Correct the strobe by a fraction of a sample (bounded), without touching the rate
    void adjust(double samples);
    //! True when the next symbol can be interpolated from the samples pushed so far
    bool ready() const;
    //! Interpolate the next symbol and advance the strobe. Returns the symbol value.
    float next(double& centre);
    //! Interpolate at any position held in the buffer, without touching the strobe
    float interpolateAt(double position) const;
    //! Newest sample index in the buffer
    int64_t newest() const { return m_newest; }
    //! Oldest sample index still in the buffer
    int64_t oldest() const { return std::max<int64_t>(std::max<int64_t>(0, m_first), m_newest - (int64_t) m_buffer.size() + 1); }
    //! Feed the loop with a decision: normalised sample u (levels +-1, +-3), its decision d and
    //! the decision reliability (0 .. 1)
    void update(float u, int d, float reliability);
    //! Hold the loop (no rate or phase changes from decisions). Releasing the hold may start a short
    //! acquisition average followed by one timing correction (see setAcquisitionLength).
    void setHold(bool hold);
    void setAcquisitionLength(int symbols) { m_acquisitionLength = symbols; }
    //! Phase corrections applied by the loop since the last call, and the symbols they span
    double takeCorrection(int& symbols);

    double getRate() const { return m_rate; }          //!< Extra samples per symbol (clock error)
    void setRate(double rate);
    double getStrobe() const { return m_strobe; }

private:
    int m_sps;
    double m_strobe;
    double m_rate;
    float m_gainPhase;
    float m_gainRate;
    bool m_hold;
    int m_acquisitionLength;   //!< Symbols averaged after a gap before one timing correction (0: none)
    int m_acquisition;      //!< Symbols left in the acquisition average
    double m_acquisitionSum;
    float m_slope;          //!< Timing error per sample of offset
    double m_correction;    //!< Accumulated phase corrections
    int m_correctionSymbols;
    std::vector<float> m_buffer;
    int64_t m_newest;      //!< Sample index of the newest sample in the buffer
    int64_t m_first;       //!< Of the first pushed since reset(): the buffer's zeros before it were never samples (a chain resumed from idle took them for history)
    float m_lastU;
    int m_lastD;
    float m_lastReliability;
    bool m_lastReliable;

    float at(int64_t sampleIndex) const;
};

#endif // INCLUDE_DSDSYMBOLCLOCK_H
