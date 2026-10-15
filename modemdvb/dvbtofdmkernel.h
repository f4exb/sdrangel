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

#ifndef MODEMDVB_DVBTOFDMKERNEL_H
#define MODEMDVB_DVBTOFDMKERNEL_H

// Internal to modemdvb: DVB-T OFDM demodulation's kernels, for instruction sets chosen at run time

namespace dvbt {

#if defined(MODEMDVB_X86)
//! Interior outputs of a centred convolution of complex values (as interleaved floats) with real taps: floats
//! [f, end) of y, from x, with taps wc[-half..half]. Symmetric taps (wc[-i] == wc[i]) are folded. Returns where it
//! stopped, as it works a block at a time, leaving the rest to the caller
int convolveAVX2(const float *x, const float *wc, int half, bool symmetric, int f, int end, float *y);
//! As convolveAVX2, with two sets of taps at once, into y1 and y2, sharing the loads
int convolve2AVX2(const float *x, const float *wc1, const float *wc2, int half, int f, int end, float *y1, float *y2);
//! y[i] = x[i] exp(j (phase + step i)) for n complex values (as interleaved floats), with table[i] = exp(j step i)
//! for i < 64, and blockStep exp(j 64 step)
void rotateAVX2(const float *x, float *y, int n, const float *table, double phase, double blockStepRe,
    double blockStepIm);
//! Equalise n carriers: e = y conj(h) derotate / |h|^2 (0 where h is), power = |h|^2
void equaliseAVX2(const float *y, const float *h, float derotateRe, float derotateIm, int n, float *e, float *power);
//! The noise estimate's update, for carriers [0, n) of e (equalised, interleaved) with power |h|^2: where mask > 0,
//! noise (-1 if not yet measured) moves by alpha towards the squared distance to the nearest point (of a square
//! constellation, after scaling by scale, points up to +/-maxPoint) times invScale2 * power. Adds the sum and number
//! of the measured noise values (>= 0) to total and count. Returns the carriers done, a multiple of 8
int updateNoiseAVX2(const float *e, const float *power, const float *mask, int n, float scale, float maxPoint,
    float invScale2, float alpha, float *noise, float& total, float& count);
//! The noise smoothed over carriers k - 2..k + 2 of those measured (noise[-2..n + 1] readable), at least floor, and
//! its inverse. Returns the carriers done, a multiple of 8
int smoothNoiseAVX2(const float *noise, int n, float floor, float *smoothed, float *inverse);
//! out = x (complex, interleaved) * window, for n values
void windowAVX2(const float *x, const float *window, int n, float *out);
//! power = |x|^2 for n complex values, returning the largest
float powerAVX2(const float *x, int n, float *power);
//! The indices of values >= threshold, in order, into indices (room for n), returning how many. No library
//! containers, as code instantiated here (compiled for AVX2) could be shared with other files
int findAboveAVX2(const float *values, int n, float threshold, int *indices);
#endif

} // namespace dvbt

#endif // MODEMDVB_DVBTOFDMKERNEL_H
