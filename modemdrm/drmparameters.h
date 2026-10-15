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

#ifndef MODEMDRM_DRMPARAMETERS_H_
#define MODEMDRM_DRMPARAMETERS_H_

#include <array>
#include <cstdint>
#include <vector>

#include "export.h"

namespace modemdrm
{

enum class Mode : std::uint8_t
{
    A,
    B,
    C,
    D,
    E
};

struct MODEMDRM_API ModeParameters
{
    Mode mode;
    int usefulTimeUnits;       // Units of 1/12000 second (ETSI ES 201 980 table 2)
    int guardTimeUnits;
    int symbolsPerFrame;
    int framesPerSuperframe;
    int internalSampleRate;

    double usefulDurationSeconds() const;
    double guardDurationSeconds() const;
    double carrierSpacingHz() const;
    int usefulSamples(int sampleRate) const;
    int guardSamples(int sampleRate) const;
    int symbolSamples(int sampleRate) const;
};

struct MODEMDRM_API SpectrumParameters
{
    Mode mode;
    int occupancy;
    int nominalBandwidthHz;
    int minimumCarrier;
    int maximumCarrier;
};

class MODEMDRM_API Parameters
{
public:
    static constexpr int TimeUnitsPerSecond = 12000;
    static constexpr int DefaultSearchRangeADHz = 2000;
    static constexpr int DefaultSearchRangeEHz = 5000;
    static constexpr int MinimumSearchRangeHz = 100;
    static constexpr int MaximumSearchRangeHz = 5000;

    static const ModeParameters& forMode(Mode mode);
    static const SpectrumParameters* forSpectrum(Mode mode, int occupancy);
    static const std::array<ModeParameters, 5>& modes();
    static const std::vector<SpectrumParameters>& spectra();
    static int sdcDataFieldBytes(Mode mode, int occupancy, int sdcMode);
    static int effectiveSearchRangeHz(Mode mode, int configuredRangeHz);
    static bool isSearchRangeValid(int configuredRangeHz);
    // Frequencies relative to the DRM reference frequency fR. The acquisition
    // band is the occupied carrier span widened by the effective search range
    // plus half a carrier spacing: the span the input must preserve.
    static bool acquisitionBand(Mode mode, int occupancy, int configuredRangeHz,
        bool spectralInversion, double& lowerHz, double& upperHz);
    // True when the acquisition band lies strictly inside (lowHz, highHz).
    static bool spectrumWithinBand(Mode mode, int occupancy, double lowHz, double highHz,
        int configuredRangeHz = 0, bool spectralInversion = false);
    static char modeName(Mode mode);
};

} // namespace modemdrm

#endif // MODEMDRM_DRMPARAMETERS_H_
