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

#ifndef MODEMDVB_DVBTINNER_H
#define MODEMDVB_DVBTINNER_H

#include <cstdint>
#include <vector>

#include "dvbt.h"

namespace dvbt {

// Inner de-interleaving and de-mapping (4.3.4, 4.3.5) and de-puncturing (4.3.3). Turns the equalized data cells of one
// OFDM symbol into soft X/Y pairs for the Viterbi decoder. In hierarchical modes, of one of the two streams: the high
// priority one, in the first two bits of each cell (the quadrant), or the low priority one, in the others, each with its
// own code rate, and with alpha 2 or 4 the constellation's points are further from the axes (non-uniform)
class MODEMDVB_API InnerDecoder
{
public:
    InnerDecoder();

    //! codeRate is that of the stream decoded: the low priority one if lowPriority, in a hierarchical mode
    void configure(Mode mode, Constellation constellation, CodeRate codeRate, Hierarchy hierarchy = Hierarchy::None,
        bool lowPriority = false);
    int infoBitsPerSymbol() const { return m_infoBitsPerSymbol; }

    //! cells are equalized and normalised to unit average power, csi the per-cell reliability (1 = average).
    //! Appends infoBitsPerSymbol() X/Y pairs to llrs
    void decode(int symbolIndex, const Cf *cells, const float *csi, std::vector<int8_t>& llrs);

    double getMER() const { return m_mer; }     //!< dB, averaged over recent symbols

    // Transmitter side, for testing: punctured bits of one symbol into cells, in carrier order. In hierarchical modes,
    // bits are the high priority stream's and lpBits the low priority one's
    void encode(int symbolIndex, const uint8_t *bits, Cf *cells, const uint8_t *lpBits = nullptr) const;
    //! Punctured bits a symbol carries of the high (or the only) and the low priority streams
    int hpBitsPerSymbol() const { return m_nbData * (m_hierarchy == Hierarchy::None ? m_bitsPerCell : 2); }
    int lpBitsPerSymbol() const { return m_hierarchy == Hierarchy::None ? 0 : m_nbData * (m_bitsPerCell - 2); }
    static int alpha(Hierarchy hierarchy) { return hierarchy == Hierarchy::Alpha2 ? 2 : hierarchy == Hierarchy::Alpha4 ? 4 : 1; }
    //! What normalises the constellation to unit average power (its points being odd integers, plus alpha - 1)
    static float normalisation(Constellation constellation, Hierarchy hierarchy);
    static void puncture(CodeRate codeRate, const uint8_t *xy, int nbInfoBits, std::vector<uint8_t>& out);

private:
    struct PunctureEntry
    {
        int m_bit;  //!< Information bit within the period
        int m_y;    //!< 0 for X, 1 for Y
    };

    static void getPuncturePattern(CodeRate codeRate, std::vector<PunctureEntry>& pattern, int& period);
    static inline float nearestPoint(float x, float maxPoint);
    static void quantize(const float *in, int8_t *out, int n);

    //! Soft bit scale: an LLR of m_llrScale for a point 1 away from the decision boundary (points are 2 apart)
    static const float m_llrScale;

    Mode m_mode;
    Constellation m_constellation;
    CodeRate m_codeRate;
    Hierarchy m_hierarchy;
    bool m_lowPriority;
    int m_nbData;
    int m_bitsPerCell;
    int m_infoBitsPerSymbol;
    std::vector<int> m_permutation[2];      //!< [symbol parity][cell * bits + bit] -> punctured bit index, in its stream
    std::vector<int> m_softIndex[2];        //!< [symbol parity][i] -> cell * bits + bit, of the stream decoded
    std::vector<int> m_llrIndex[2];         //!< [symbol parity][i] -> index in X/Y pairs
    std::vector<PunctureEntry> m_puncture;
    int m_puncturePeriod;
    std::vector<float> m_softBits;          //!< Soft bits of one symbol, in cell order
    std::vector<int8_t> m_quantized;
    double m_mer;
};

} // namespace dvbt

#endif // MODEMDVB_DVBTINNER_H
