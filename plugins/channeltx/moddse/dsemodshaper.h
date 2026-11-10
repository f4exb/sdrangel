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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODSHAPER_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODSHAPER_H_

#include <algorithm>
#include <cstdint>
#include <vector>

#include <complex>
#include <vector>

#include "dsp/dsptypes.h"
#include "dsp/gaussian.h"
#include "dsp/rootraisedcosine.h"
#include "dsp/dv/dvangel61.h"

/**
 * Pulse shaping of symbol levels into a baseband modulating signal at
 * samplesPerSymbol samples per symbol. Output is in [-1, 1] where 1 is the
 * peak deviation.
 */
class DSEModShaper
{
public:
    static const int m_sampleRate = 48000;

    DSEModShaper(int samplesPerSymbol) : m_samplesPerSymbol(samplesPerSymbol) {}
    virtual ~DSEModShaper() {}
    virtual void reset() = 0;
    //! Shape levels in {-3,-1,1,3}. Appends samplesPerSymbol samples per level to out.
    virtual void shape(const int8_t *levels, int nbLevels, std::vector<float>& out) = 0;
    //! Flush the filter memory with zero level symbols. Appends to out.
    virtual void flush(std::vector<float>& out) = 0;
    int getSamplesPerSymbol() const { return m_samplesPerSymbol; }

    static const int8_t m_levelOff = 127;     //!< Level value meaning "carrier off" for one symbol period (burst protocols)
    static constexpr float m_sampleOff = -8.0f; //!< Output sample meaning "carrier off" (far outside the shaped signal)

    //! The FM FIFO carries the frequency over this many peak deviations: a 4FSK symbol stream through the
    //! transmit RRC overshoots its outer level between symbol instants (1.73 times at roll-off 0.2, 1.46 at
    //! 0.5), and a FIFO full scale at the peak deviation clipped a fifth of its samples
    static constexpr float m_fmHeadroom = 2.0f;
    //! A shaped FM sample (in peak deviations) as the FIFO carries it, and back. DSEModProcessor writes with
    //! the one and DSEModSource modulates with the other; the DSD harness passes its synthetic signal through
    //! both, so that the FIFO's scaling is what the receiver tests see
    static int16_t fmToFifo(float v) { return (int16_t) (std::max(-1.0f, std::min(1.0f, v / m_fmHeadroom)) * 32767.0f); }
    static float fifoToFm(int16_t s) { return (s / 32767.0f) * m_fmHeadroom; }

    //! Shape a level stream that may contain m_levelOff runs: on runs are shaped, the filter is flushed
    //! into the start of each off run and the rest of the off run is m_sampleOff, so timing is kept.
    void shapeWithGaps(const int8_t *levels, int nbLevels, std::vector<float>& out);

protected:
    int m_samplesPerSymbol;
};

//! Gaussian filtered NRZ (GMSK / GFSK) for D-STAR: BT = 0.5
class DSEModGMSKShaper : public DSEModShaper
{
public:
    DSEModGMSKShaper(int samplesPerSymbol = 10, double bt = 0.5, int symbolSpan = 4);
    virtual void reset();
    virtual void shape(const int8_t *levels, int nbLevels, std::vector<float>& out);
    virtual void flush(std::vector<float>& out);

private:
    double m_bt;
    int m_symbolSpan;
    Gaussian<float> m_filter;
};

//! Root raised cosine filtered 4FSK (C4FM) for DMR, YSF, NXDN, dPMR, P25: alpha = 0.2
class DSEModRRC4FSKShaper : public DSEModShaper
{
public:
    DSEModRRC4FSKShaper(int samplesPerSymbol = 10, double alpha = 0.2, int symbolSpan = 8);
    virtual void reset();
    virtual void shape(const int8_t *levels, int nbLevels, std::vector<float>& out);
    virtual void flush(std::vector<float>& out);

private:
    double m_alpha;
    int m_symbolSpan;
    float m_scale; //!< Impulse amplitude giving unity output at symbol instants
    RootRaisedCosine<float> m_filter;

    void create();
};

/**
 * Linear pulse shaping of complex symbols (Angel61, sdrbase/dsp/dv/ANGEL61.md 2.1):
 * the protocol's 241 tap root raised cosine at 10 samples per symbol, sample
 * exact: the first output sample is the filter's response to the first symbol,
 * flush() appends the 24 zero symbols that empty it. Output is interleaved I, Q
 * with the RMS level given (unit power symbols in), so that the peaks of 16-QAM
 * (6.5 dB above the mean) stay inside [-1, 1].
 */
class DSEModLinearShaper
{
public:
    explicit DSEModLinearShaper(int samplesPerSymbol = 10);
    //! The profile's pulse (2.1): 10 samples a symbol in NB, 5 in WB; clears the history
    void setSamplesPerSymbol(int samplesPerSymbol);
    void reset();
    //! The output RMS for unit power symbols (QPSK 0.5 and 16-QAM 0.35 keep the peaks under 0.8)
    void setLevel(float rms) { m_level = rms; }
    void shape(const std::complex<float> *symbols, int nbSymbols, std::vector<float>& iq);
    void flush(std::vector<float>& iq);

private:
    int m_sps;                                      //!< Samples per symbol at 48 kS/s
    std::vector<float> m_taps;                      //!< The RRC, unit energy
    std::vector<std::complex<float>> m_history;     //!< The last symbols, newest first
    float m_level;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODSHAPER_H_
