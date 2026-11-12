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

#ifndef INCLUDE_DSDSLICER_H
#define INCLUDE_DSDSLICER_H

#include <cstdint>

/**
 * Symbol decisions with soft outputs.
 *
 * Tracks the outer level A (hertz), the DC offset B (hertz) and the noise
 * variance of the symbol instant samples. A sample y is normalised to
 * u = (y - B) / (A / 3) where the 4FSK levels are +3, +1, -1, -3 (2FSK: +3, -3)
 * and sliced. Two log likelihood ratios per symbol come from the max-log rule,
 * positive favouring bit 0, in the dibit mapping 01 = +3, 00 = +1, 10 = -1,
 * 11 = -3 (bit 1 is the sign, bit 0 the outer/inner choice).
 */
class DSDSlicer
{
public:
    struct Symbol
    {
        float m_u;          //!< Normalised sample
        int m_level;        //!< Decided level: +3, +1, -1, -3
        uint8_t m_dibit;    //!< 0..3
        float m_llrMsb;     //!< LLR of the first (sign) bit
        float m_llrLsb;     //!< LLR of the second (magnitude) bit
        float m_reliability;//!< 0 .. 1: distance from the nearest decision boundary
    };

    DSDSlicer();

    void setFourLevel(bool fourLevel) { m_fourLevel = fourLevel; }
    //! Initialise from a sync fit: outer level, DC offset, noise variance (all hertz based)
    void initialise(float outerLevelHz, float offsetHz, float sigma2);
    //! Correct the DC offset (e.g. from a later sync fit)
    void setOffset(float offsetHz) { m_offset = offsetHz; }
    //! Slice one sample. weight scales the LLRs (fading); track enables decision directed updates.
    Symbol slice(float y, float weight, bool track);

    float getOuterLevel() const { return m_outer; }
    float getOffset() const { return m_offset; }
    float getSigma2() const { return m_sigma2; }
    //! Estimated symbol SNR in dB from the outer level and noise variance
    float getSnrDB() const;

private:
    bool m_fourLevel;
    float m_outer;      //!< Hz
    float m_offset;     //!< Hz
    float m_sigma2;     //!< Hz^2
    float m_trackRate;
};

#endif // INCLUDE_DSDSLICER_H
