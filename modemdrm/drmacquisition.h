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

#ifndef MODEMDRM_DRMACQUISITION_H_
#define MODEMDRM_DRMACQUISITION_H_

#include <complex>
#include <cstddef>
#include <vector>

#include "drmparameters.h"

namespace modemdrm
{

struct MODEMDRM_API AcquisitionCandidate
{
    Mode mode = Mode::A;
    bool valid = false;
    float cyclicPrefixCorrelation = 0.0f;
    // 10th percentile of the guard-window correlation over symbol offsets: the level
    // noise or interference correlated at the useful-period lag gives.
    float cyclicPrefixFloor = 0.0f;
    int symbolStartOffsetSamples = 0;
    // Tracking only (alignFirstPath false): the span of FFT-window starts that
    // hold nearly all the paths' power, as its last offset in the symbol and
    // its width; width 0 when not measured.
    int windowEndOffsetSamples = 0;
    int windowSpanSamples = 0;
    double fractionalCarrierOffsetHz = 0.0;
    int usefulSamples = 0;
    int guardSamples = 0;
    int symbolSamples = 0;
    int symbolsObserved = 0;
};

class MODEMDRM_API Acquisition
{
public:
    // Coarse cyclic-prefix gate used only to reject implausible mode/timing
    // candidates. Repeated CRC-valid FAC blocks are still required for lock.
    static constexpr float MinimumCyclicPrefixCorrelation = 0.10f;
    // A cyclic prefix concentrates its correlation in one guard-length span of
    // the symbol; noise correlates about equally at every offset, as does
    // interference correlated at the useful-period lag (a carrier, say), which
    // raises the floor. The best guard window must exceed the floor (10th
    // percentile over offsets) by this share of the headroom above it: noise
    // with interference reached 0.09 and the corpus is at 0.83 or more; 0.15
    // is about -7 dB SNR, below where FAC decodes. Without it every noisy
    // capture window that passes the correlation gate costs a full FAC search.
    static constexpr float MinimumCyclicPrefixExcess = 0.15f;
    // Fraction of the way from the noise floor to the peak of the cyclic-prefix
    // correlation profile above which a path counts for timing (see
    // evaluateMode).
    static constexpr double FirstPathFraction = 0.05;
    // Multiple of the profile's noise floor (10th percentile) a path must exceed.
    static constexpr double NoiseMargin = 2.2;
    // Share of the best achievable path power the FFT window start must hold.
    static constexpr double WindowPowerFraction = 0.9;

    // Timing is the start of the guard window of greatest cyclic-prefix
    // correlation, moved to the first significant path when alignFirstPath.
    static AcquisitionCandidate evaluateMode(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        Mode mode,
        bool alignFirstPath = true
    );

    // Produces carrier indices whose absolute offset from fR is within the
    // configured symmetric range. Ordering starts nearest the fractional
    // estimate so a receiver can validate cheap hypotheses first.
    static std::vector<int> integerCarrierHypotheses(
        Mode mode,
        double fractionalCarrierOffsetHz,
        int configuredSearchRangeHz
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMACQUISITION_H_
