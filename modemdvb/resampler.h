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

#ifndef MODEMDVB_RESAMPLER_H
#define MODEMDVB_RESAMPLER_H

#include <complex>
#include <functional>
#include <vector>

#include "export.h"

namespace dvbt {

// Polyphase windowed sinc resampler with an adjustable ratio, for any ratio between about 0.5 and 2.
// The filter passes up to passband Hz and rejects what would alias onto it
class MODEMDVB_API Resampler
{
public:
    typedef std::complex<float> Cf;

    Resampler();

    void configure(double inputRate, double outputRate, double passband);
    void setRatioCorrection(double correction); //!< Multiplies the input samples per output sample
    double getRatioCorrection() const { return m_correction; }
    void reset();
    //! Start with the given previous input, so the first output is at the next input sample, without a gap or jump
    void prime(const std::vector<Cf>& previous);
    int getTaps() const { return m_taps; }
    void process(const Cf *in, int nbIn, std::vector<Cf>& out); //!< Appends to out
    //! Room for the outputs of nbIn more input samples
    int maxOutputs(int nbIn) const;
    //! Into out, room for maxOut (maxOutputs(nbIn)). Returns the number made
    int process(const Cf *in, int nbIn, Cf *out, int maxOut);

private:
    static constexpr int m_phases = 512;
    static const double m_pi;
    static const double m_attenuation;  //!< dB
    static const double m_kaiserBeta;

    static double besselI0(double x);

    int m_taps;                     //!< Per phase
    std::vector<float> m_filter;    //!< [phase][tap]
    bool m_avx2;                    //!< Use the AVX2 filter
    std::vector<float> m_filterPairs; //!< [phase][tap][2], each tap twice, to multiply interleaved I/Q
    std::vector<Cf> m_history;      //!< Input samples not yet used
    std::vector<Cf> m_staging;      //!< The history and the start of the input, for the outputs that need both
    double m_step;                  //!< Input samples per output sample
    double m_nominalStep;
    double m_correction;
    double m_time;                  //!< Of the next output sample, in input samples from m_history[0]

    int run(const Cf *samples, int offset, int available, Cf *out, int maxOut);
};

//! A polyphase filter for resampleFilter(): phases phases of taps taps (a multiple of 4), from impulse(x), x the
//! distance in input samples from the output's time. Each phase normalised to unity gain at DC. pairs: [phase][tap][2]
MODEMDVB_API void makePolyphaseFilter(int taps, int phases, const std::function<double(double)>& impulse, std::vector<float>& pairs);

//! The resampler's loop over outputs, with the best kernel for the CPU, for a caller with its own buffer and filter
//! (from makePolyphaseFilter()). Input sample j (counting from the one time is from) is samples[j - offset], for j from
//! offset to available. time: of the next output, in input samples, advanced by step for each. Makes up to maxOut,
//! returning how many: it stops when the next needs input sample available or beyond
MODEMDVB_API int resampleFilter(const std::complex<float> *samples, int offset, int available, const float *pairs, int taps,
    int phases, double& time, double step, std::complex<float> *out, int maxOut);

} // namespace dvbt

#endif // MODEMDVB_RESAMPLER_H
