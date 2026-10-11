///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef MODEMDVB_RESAMPLERKERNEL_H
#define MODEMDVB_RESAMPLERKERNEL_H

// Internal to modemdvb: the resampler's loop over output samples, for a dot product of the input with a phase of
// the filter (Dot::dot), compiled for each instruction set. Only raw buffers here: a file compiled for AVX2 mustn't
// instantiate library code (as std::vector's, or even std::complex's constructors) that the linker could then use for
// every caller. Samples are std::complex only as pointers, reinterpreted as float pairs, as std::complex allows

#include <complex>

namespace dvbt {

typedef std::complex<float> ResamplerCf;

//! Outputs that can be made from what's available for them, up to maxOut (resampleMaxOutputs) into out. Input sample
//! j (from the one time is from) is samples[j - offset], for j from offset (the first output's first tap must be at
//! or after it) to available. pairs: the filter, [phase][tap][2], each tap twice to multiply interleaved I/Q. taps is
//! a multiple of 4. time: of the next output, in input samples from sample 0, advanced. Returns the number made.
//! Dot::dot(x, h, taps, out): the output's real and imaginary parts to out[0] and out[1]
template <class Dot>
int resampleRun(const ResamplerCf *samples, int offset, int available, const float *pairs, int taps, int phases,
    double& time, double step, ResamplerCf *out, int maxOut)
{
    const int half = taps / 2 - 1;
    int n = 0;

    while (n < maxOut)
    {
        int i0 = (int) time;
        int phase = (int) ((time - i0) * phases + 0.5);

        if (phase == phases)
        {
            i0++;
            phase = 0;
        }

        const int first = i0 - half;

        if (first + taps > available) {
            break;
        }

        Dot::dot(reinterpret_cast<const float *>(samples + (first - offset)), pairs + 2 * phase * taps, taps,
            reinterpret_cast<float *>(out + n));
        n++;
        time += step;
    }

    return n;
}

//! Room for every output resampleRun can make from available input samples
inline int resampleMaxOutputs(int available, double time, double step)
{
    return available > time ? (int) ((available - time) / step) + 2 : 2;
}

#if defined(MODEMDVB_X86)
int resampleAVX2(const ResamplerCf *samples, int offset, int available, const float *pairs, int taps, int phases,
    double& time, double step, ResamplerCf *out, int maxOut);
#endif

} // namespace dvbt

#endif // MODEMDVB_RESAMPLERKERNEL_H
