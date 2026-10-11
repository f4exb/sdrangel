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

#ifndef MODEMDVB_CWCANCELLER_H
#define MODEMDVB_CWCANCELLER_H

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

#include "export.h"

namespace dvbt {

// Notches out strong narrowband interferers (CW), before the FFT. An OFDM receiver's FFT leaks a tone into
// carriers far from it (falling only 20 dB a decade), so one much stronger than the signal spoils most of the
// carriers, which weighting them by their noise can't help. The input's spectrum is measured now and then (a
// windowed FFT, averaged over a few blocks), and a narrow peak well above the signal's level gets a complex IIR
// notch (a zero on the unit circle and a pole just inside it), a few kHz wide, which follows its frequency
class MODEMDVB_API CWCanceller
{
public:
    typedef std::complex<float> Cf;

    struct Notch
    {
        double m_frequency;     //!< Cycles per sample
        double m_level;         //!< dB above the signal's level, in the measurement
        int m_missed;           //!< Measurements since it was last seen
        float m_zr, m_zi;       //!< Zero, e^jw
        float m_pr, m_pi;       //!< Pole, r e^jw
        float m_wr, m_wi;       //!< State
    };

    CWCanceller();

    //! Look within +/- bandwidth/2 of the centre, of samples at sampleRate
    void configure(double sampleRate, double bandwidth);
    void reset();
    void setEnabled(bool enabled);
    //! The samples to use: samples itself if no notch is active, else out's data (out is resized to fit)
    const Cf *process(const Cf *samples, int nbSamples, std::vector<Cf>& out);
    const std::vector<Notch>& getNotches() const { return m_notches; }

private:
    friend class NotchedCarriers;           //!< For m_twoPi and m_poleRadius

    static const int m_fftSize = 4096;
    static const int m_blocks = 8;          //!< Averaged in each measurement
    static const int m_maxNotches = 4;
    static const double m_twoPi;
    static const double m_thresholdDb;      //!< Above the signal's median level in a bin, to notch (a CW at -23 dBc)
    static const double m_poleRadius;       //!< Notch about 0.0003 of the sample rate wide (3 kHz at 9.14 MS/s)
    static const int m_matchBins = 3;       //!< A peak this close to a notch, or a candidate, is the same one
    static const int m_missesToRemove = 3;  //!< Measurements without the interferer

    void measure(const Cf *block);
    void decide();
    void setFrequency(Notch& notch, double frequency);

    bool m_enabled;
    double m_sampleRate;
    double m_bandwidth;
    int m_interval;                 //!< Samples between blocks measured
    int m_untilNext;                //!< Samples until the next block is measured
    std::vector<Cf> m_block;        //!< Being gathered, as a block may span calls
    int m_blockFill;
    std::vector<double> m_window;
    std::vector<double> m_power;    //!< Averaged over m_blockCount blocks
    int m_blockCount;
    std::vector<std::complex<double>> m_spectra[2];    //!< Of the two blocks measured
    std::vector<int> m_band;        //!< Bins within the bandwidth
    std::vector<double> m_levels;
    std::vector<std::complex<double>> m_twiddles;
    std::vector<int> m_bitReverse;
    std::vector<double> m_candidates;  //!< Frequencies seen in the last measurement, not yet notched
    std::vector<Notch> m_notches;
};

// The notches' effect on the carriers near them, undone: each carrier within 50 kHz of a notch is divided by the
// notch's response at its frequency (known exactly), as the notch's phase changes too fast across them for the
// channel estimate to follow (26 degrees 3 kHz away, still 10 at 8 kHz), and those where the response is below half
// are erased (a carrier or two, or a few in 32K)
class MODEMDVB_API NotchedCarriers
{
public:
    typedef std::complex<float> Cf;

    //! Hz from the input's centre, of the input at inputRate
    void setNotches(const std::vector<double>& frequencies, double inputRate);
    bool empty() const { return m_notches.empty(); }
    //! For carriers 0..kMax, carrier k being at (k - kMax / 2) spacing + offset Hz
    void prepare(int kMax, double spacing, double offset);
    //! Divide the carriers near the notches by its response
    void compensate(Cf *y) const;
    bool erased(int k) const { return (k < (int) m_erased.size()) && m_erased[k]; }

private:
    std::vector<double> m_notches;
    double m_inputRate = 1.0;
    std::vector<int> m_carriers;        //!< Near notches
    std::vector<Cf> m_factors;          //!< For each of them
    std::vector<uint8_t> m_erased;      //!< Per carrier
};

} // namespace dvbt

#endif // MODEMDVB_CWCANCELLER_H
