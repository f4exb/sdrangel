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

#ifndef MODEMDAB_DABCOHERENT_H_
#define MODEMDAB_DABCOHERENT_H_

#include <array>
#include <complex>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "export.h"
#include "dabensemble.h"
#include "dabmsc.h"
#include "dabofdm.h"

namespace modemdab
{

// Decoder aided (near coherent) demodulation, a pass after differential demodulation. DAB has
// no pilots, but once the pass before has decoded every sub-channel and the FIC, re-encoding
// gives the transmitted symbols. For each carrier, the channel times the previous symbol is
// then estimated from the symbols before and after, each turned by its known symbol, rather
// than from the one symbol before. Those references have much less noise, so the whole MSC
// can be demodulated again almost as well as with a known channel.
//
// A wrongly decoded bit gives a wrong known symbol, which spoils the references of its
// carrier for many symbols. So the Viterbi decoder's soft output (SOVA) is used, and bits
// whose reliability is below a threshold stay unknown. The output can feed another pass,
// whose decoding starts from better soft bits.
//
// A frame is ready once every codeword that crosses it is decoded: 15 CIFs after its last
// CIF, so about 0.4 s after the pass before.
class MODEMDAB_API DABCoherentDecoder
{
public:
    // A demodulated frame: the input frame with its MSC soft bits from this pass, so that it
    // can be the input of another pass
    struct Output
    {
        int cifCount = 0;               // Of the frame's first CIF
        DABFrame frame;
        std::array<std::vector<uint8_t>, 4> ficCoded;
        std::array<bool, 4> ficValid{};
    };

    static constexpr float DefaultReliabilityThreshold = 2.0f;

    DABCoherentDecoder();
    DABCoherentDecoder(const DABCoherentDecoder&) = delete;
    DABCoherentDecoder& operator=(const DABCoherentDecoder&) = delete;

    void reset();
    // Every sub-channel of the ensemble (each is decoded in the first pass)
    void setSubchannels(const std::vector<DABSubchannel>& subchannels);
    // A first pass frame (with its carriers) and its first CIF count; ficCoded holds the
    // re-encoded FIC blocks, valid where their FIBs passed their CRCs.
    void push(const DABFrame& frame, int cifCount, const std::array<std::vector<uint8_t>, 4>& ficCoded,
        const std::array<bool, 4>& ficValid);
    // A frame whose symbols are known, demodulated again
    bool pop(Output& output);

    float knownFraction() const { return m_knownFraction; }  // Of the symbols in the last frame
    float feedback() const { return m_feedback; }            // Reference memory chosen
    // Re-encoded bits whose reliability (see DABViterbi, in mean soft value magnitudes) is
    // below this are not used: wrong ones would spoil the references for many symbols
    void setReliabilityThreshold(float threshold);
    float drift() const { return m_drift; }                  // Phase drift per symbol (radians)

private:
    struct Subchannel
    {
        DABSubchannel config;
        ProtectionProfile profile;
        DABSubchannelDecoder decoder;
        std::map<int64_t, std::vector<int8_t>> codewords;   // Re-encoded, by logical frame (-1 unreliable)
    };

    struct Frame
    {
        int64_t cif = 0;                // Running count of its first CIF
        int cifCount = 0;
        std::vector<std::complex<float>> carriers;
        std::vector<float> weights;
        std::vector<float> mscSoft;     // From the pass before
        std::array<std::vector<uint8_t>, 4> fic;
        std::array<bool, 4> ficValid{};
    };

    std::vector<std::unique_ptr<Subchannel>> m_subchannels;
    std::deque<Frame> m_frames;
    int64_t m_cif;                      // Running count of the next CIF
    int m_lastCIFCount;
    int64_t m_decodedUpTo;              // Codewords of logical frames up to this are known
    std::vector<int> m_softCarrier;
    std::vector<int8_t> m_known;        // Coded bits of a frame: 0, 1 or -1 (unknown)
    // Demodulation buffers, by symbol then carrier
    std::vector<std::complex<float>> m_y;       // Known symbols, zero where unknown
    std::vector<float> m_k;                     // One where known
    std::vector<std::complex<float>> m_decision;
    std::vector<float> m_gain;
    std::vector<std::complex<float>> m_drifts;
    float m_knownFraction;
    float m_feedback;
    float m_reliabilityThreshold;
    float m_drift;
    float m_driftSlope;                 // Per carrier

    // Demodulates the frame again; its carriers move to the output
    void demodulate(Frame& frame, Output& output);
};

} // namespace modemdab

#endif // MODEMDAB_DABCOHERENT_H_
