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

#ifndef MODEMDRM_DRMFACACQUISITION_H_
#define MODEMDRM_DRMFACACQUISITION_H_

#include <atomic>
#include <complex>
#include <cstddef>
#include <vector>

#include "export.h"
#include "drmacquisition.h"
#include "drmfac.h"

namespace modemdrm
{

struct MODEMDRM_API FACAcquisitionResult
{
    bool valid = false;
    AcquisitionCandidate coarse;
    int occupancy = -1;
    int integerCarrierOffset = 0;
    bool spectralInversion = false;
    std::size_t frameStartOffsetSamples = 0;
    // Tracking: where the FFT window is placed relative to the guard window
    // of greatest cyclic-prefix correlation, smoothed over superframes.
    double windowPlacementSamples = 0.0;
    int validatedFACBlockCount = 0;
    // Chronological observations; false means cached signalling, not a new
    // CRC-valid block. Kept even when a tracked superframe is rejected.
    std::vector<bool> facBlockValid;
    std::vector<FACData> facBlocks;
};

struct MODEMDRM_API FACSearchOptions
{
    int configuredSearchRangeHz = 0;
    // -1 (automatic): the normal spectrum, then the mirrored one (I and Q
    // swapped, as some receivers and converters deliver it); 0 or 1 forces
    // normal or mirrored.
    int spectralInversion = -1;
    int forcedMode = -1;
    int forcedOccupancy = -1;
    // fR-relative span the input preserves. Mode/occupancy candidates whose
    // acquisition band does not fit are not tried. low >= high: unlimited.
    double passbandLowHz = 0.0;
    double passbandHighHz = 0.0;
    // Reacquisition hint from the lock just lost: that mode and occupancy are
    // tried first, on the ranked candidates only; equal coherences prefer the
    // integer offsets nearest the previous total offset.
    bool haveHint = false;
    int hintMode = -1;
    int hintOccupancy = -1;
    double hintCarrierOffsetHz = 0.0;
    bool hintSpectralInversion = false; // tried first when automatic
    // Set by the caller to abandon a search between its stages (a channel
    // being stopped); the search then returns no lock.
    const std::atomic_bool* cancel = nullptr;
};

class MODEMDRM_API FACAcquisition
{
public:
    static constexpr int RequiredConsecutiveBlocks = 3;
    static constexpr int CoarseAcquisitionSeconds = 4;

    static FACAcquisitionResult search(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        const FACSearchOptions& options
    );

    static FACAcquisitionResult searchWindow(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        int maximumStartOffsetSeconds,
        const FACSearchOptions& options
    );

    static FACAcquisitionResult search(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        int configuredSearchRangeHz = 0,
        bool spectralInversion = false,
        int forcedMode = -1,
        int forcedOccupancy = -1
    );

    // Searches at one-second start offsets when the first candidate sequence
    // is not coherent. Returned timing offsets are relative to the original
    // sample span rather than the successful subspan.
    static FACAcquisitionResult searchWindow(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        int configuredSearchRangeHz = 0,
        bool spectralInversion = false,
        int maximumStartOffsetSeconds = 3,
        int forcedMode = -1,
        int forcedOccupancy = -1
    );

    // Validates FAC at an expected transmission-superframe boundary using
    // the previously locked mode, occupancy and carrier estimate. A fresh
    // cyclic-prefix estimate tracks fractional carrier and symbol timing;
    // it does not scan other modes, occupancies or integer carriers. Once
    // locked, two freshly validated blocks may bridge erased FAC blocks by
    // retaining that slot's last coherent signalling.
    static FACAcquisitionResult track(
        const std::complex<float>* samples,
        std::size_t sampleCount,
        int sampleRate,
        const FACAcquisitionResult& previous,
        std::size_t expectedFrameStartOffsetSamples = 0
    );
};

} // namespace modemdrm

#endif // MODEMDRM_DRMFACACQUISITION_H_
