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

#ifndef MODEMRADIOPAGING_RADIOPAGINGFSKDEMOD_H
#define MODEMRADIOPAGING_RADIOPAGINGFSKDEMOD_H

#include <cstdint>
#include <vector>       // used by util/movingaverage.h

#include "util/movingaverage.h"
#include "export.h"

// Symbol recovery for 2-level and 4-level FSK from FM discriminator output.
//
// Input samples are discriminator output scaled so that the outer symbol deviation
// is about +/-1. The chain is:
//   matched filter   boxcar over one symbol (integrate and dump for NRZ symbols)
//   symbol timing    Gardner timing error detector on the matched filter output,
//                    sampled twice per symbol with linear interpolation, so any
//                    (fractional) samples per symbol ratio is supported
//   levels           DC offset (carrier frequency offset) and deviation. While hunting: the
//                    mean of the symbol values over a window (the preamble or the carrier
//                    averages to the offset) and the mean distance from it. On a known pattern
//                    (sync word, preamble codewords): a least squares fit of the symbol values
//                    to the known levels, exact for unbalanced patterns, then held. Data
//                    symbols never move the levels
//   decision         2-level: sign; 4-level: thresholds at 0 and +/-2/3 of deviation
//
// The symbol clock free runs at the nominal rate and is only nudged by the timing
// error, so an isolated noise transition cannot insert or delete a symbol.
// Acquisition uses higher loop gains than tracking (setLocked()).
class MODEMRADIOPAGING_API RadioPagingFskDemod
{
public:
    struct Symbol
    {
        int m_value;        //!< 2-level: 0 or 1 (1 = positive deviation); 4-level: 0..3 (3 = highest frequency)
        float m_soft;       //!< Normalised symbol value: outer levels +/-1, inner +/-1/3
        float m_error;      //!< Distance from the decided level, normalised
        float m_offset;     //!< The symbol instant lies this many samples (0 to 1) before the last input sample
        //! The decision on the mean of the centre half of the symbol (the same levels): without the transitions,
        //! better when the band of the signal is narrow for the symbol rate (intersymbol interference), the whole
        //! symbol (m_value) better in noise
        int m_centreValue;
        float m_centreSoft;
    };

    RadioPagingFskDemod();

    //!< sampleRate in Hz, symbolRate in symbols/s, levels 2 or 4
    void create(double sampleRate, double symbolRate, int levels);
    void reset();
    //!< Process one discriminator sample. Returns true when a symbol has been decided
    bool process(float sample, Symbol& symbol);

    //!< Use tracking (low) timing loop gains once a sync word has been found, acquisition gains otherwise
    void setLocked(bool locked) { m_locked = locked; }
    //!< Keep the DC offset and deviation as they are (a transmission is being decoded); when
    //!< released they follow the hunting means again
    void setHoldLevels(bool hold) { m_holdLevels = hold; }
    bool getHoldLevels() const { return m_holdLevels; }
    //!< Set DC offset and outer level deviation, e.g. measured by another demodulator
    void setLevels(float dc, float deviation);
    //!< Symbols of the hunting means (default 64)
    void setHuntWindow(int symbols);
    //!< The input moved by delta (a frequency correction before the discriminator): the DC
    //!< offset and the symbol history move with it, the hunting means continue over the
    //!< moved history
    void offsetLevels(float delta);
    //!< Fit DC offset and deviation to known.size() symbols whose levels (outer +/-1, inner
    //!< +/-1/3) are known, oldest first, the last of them symbolsAgo symbols before the last
    //!< symbol, and hold them. False (nothing changed) if they are no longer in the history
    //!< or the levels do not vary
    bool fitLevels(const std::vector<float>& known, size_t symbolsAgo = 0);
    //!< Place the next symbol instant samplesToNextSymbol samples after the last input sample,
    //!< e.g. to continue the symbol clock of another demodulator at a new symbol rate
    void startSymbols(double samplesToNextSymbol);

    float getDC() const { return m_dc; }
    float getDeviation() const { return m_deviation; }
    double getSamplesPerSymbol() const { return m_samplesPerSymbol; }
    int getLevels() const { return m_levels; }

private:
    double m_samplesPerSymbol;
    int m_levels;
    bool m_locked;
    bool m_holdLevels;

    MovingAverageUtilVar<float, double> m_matchedFilter;
    float m_prevFiltered;           //!< Matched filter output of the previous input sample
    double m_timeToNext;            //!< Samples until the next half symbol instant
    bool m_nextIsSymbol;            //!< The next half symbol instant is a symbol (not a mid point)
    float m_prevSymbolValue;        //!< Matched filter value at the previous symbol instant
    float m_midValue;               //!< Matched filter value midway between the last two symbols
    bool m_haveMid;
    double m_timingPower;           //!< Running mean square of symbol values, normalises the timing error

    float m_dc;
    float m_deviation;
    MovingAverageUtilVar<float, double> m_huntDc;          //!< Symbol values
    MovingAverageUtilVar<float, double> m_huntDeviation;   //!< Their distance from the DC offset
    std::vector<float> m_history;                           //!< Last symbol values (DC included), ring
    size_t m_historyPos;
    size_t m_historyCount;
    std::vector<float> m_input;                             //!< Last input samples, ring (the centre of the symbol)
    size_t m_inputPos;

    //! The mean of the input samples from back1 to back2 samples before the last one (back1 < back2)
    float inputMean(double back1, double back2) const;

    int decide(float normalised) const;
    static float levelValue(int symbol, int levels);
};

#endif // MODEMRADIOPAGING_RADIOPAGINGFSKDEMOD_H
